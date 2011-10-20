/*
 *  gifjpg_munge.c
 *
 *  $Id: gifjpg_munge.c,v 1.21 1997/08/30 21:34:37 fox Exp $
 *
 *  This code acts as a GIF->JPG distillation server for tcsproxy.
 */

#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <setjmp.h>
#include "jpeglib.h"
#include "gifjpg_munge.h"
#include "distinterface.h"
#include "gj_mem_src_dst_mgr.h"
#include "cdjpeg.h"

#include "../include/ARGS.h"
#include "../../frontend/include/ARGS.h"

typedef struct gifjpg_error_mgr_st {
  struct  jpeg_error_mgr pub;
} gifjpg_error_mgr, *gifjpg_error_ptr;

/* Some long-lived globals;  will be reused for each distillation
   for the lifetime of this distiller. */

jmp_buf jumpbuffer;                                 /* for setjmp */
struct jpeg_compress_struct   dstinfo;              /* gif->jpg, bytes->jpg */
struct jpeg_decompress_struct srcinfo;              /* jpg->bytes */
gifjpg_error_mgr              jsrc_err, jdst_err;   /* error handlers */
int                           bailout_now;          /* early bail for spec cases */
MonitorClientID               monID;

/* Quality tables for the beginner's interface */
int ismap_denom[5] = {1,1,1,1,1};    /* Don't want to lose text detail */
int ismap_qual[5] = {10, 15, 25, 50, 75};  /* Want high quality ismaps */
int noscale_denom[5] = {1,1,1,1,1};    /* Don't scale down this image */
int noscale_qual[5] = {8, 11, 15, 40, 75};  /* Just a guess */
int norm_denom[5] = {2, 2, 1, 1, 1}; /* Scale away! */
int norm_qual[5] =  {15, 35, 15, 40, 75};  /* Again, just a guess! */
int bailout_thresh[] = {1<<10, 2<<10, 4<<10, 8<<10, 16<<10};

C_DistillerType dType;

void gifjpg_error_exit(j_common_ptr cinfo);
void gifjpg_output_message(j_common_ptr cinfo);
int compute_scale_denom(int max_x, int max_y, int min_x, int min_y, 
			int in_x, int int_y, double scale);
DistillerStatus gjpg_passthrough(DistillerInput *din, DistillerOutput *dout);

DistillerStatus
ComputeDistillationCost(Argument *args, int numArgs,
			DistillerInput *din,
                        DistillationCost *cost)
{
  cost->estimatedTime_ms = DataLength(din);
  return distOk;
}

DistillerStatus
DistillerInit(C_DistillerType distType, int argc, const char * const *argv)
{
  if (0)
    return (distFatalError);

  dType = distType;
  /* Tell the JPEG library to report errors on STDERR, and call the
     gifjpg_error_exit() routine if a fatal error occurs.  This handler
     will do a longjmp back to the beginning... */

  srcinfo.err =  jpeg_std_error(&jsrc_err.pub);
  jsrc_err.pub.error_exit = gifjpg_error_exit;
  jsrc_err.pub.output_message = gifjpg_output_message;
  dstinfo.err =  jpeg_std_error(&jdst_err.pub);
  jdst_err.pub.error_exit = gifjpg_error_exit;
  jdst_err.pub.output_message = gifjpg_output_message;

  /* Create the compression/decompression objects.  This will allocate
     a whole bunch of stuff buried inside these structures. */

  jpeg_create_compress(&dstinfo);
  jpeg_create_decompress(&srcinfo);

  monID = DistillerGetMonitorClientID();

  DEBUG("distiller initialized ok\n");
  return distOk;
}

void
DistillerExit(void)
{
  jpeg_destroy_decompress(&srcinfo);
  jpeg_destroy_compress(&dstinfo);
}
  
DistillerStatus
DistillerMain(Argument *args, int nargs,
	      DistillerInput *din,
	      DistillerOutput *dout)
{
  JSAMPROW handoff = NULL;
  JDIMENSION handoff_height, handoff_width, num_scanlines;
  int        max_x=-1, max_y=-1, min_x=-1, min_y=-1, qual=55, i;
  int        expert=0, resize=0, quality=3, nodistill=0, ismap=0;
  double     scale = 0.5;
  cjpeg_source_ptr src_mgr;
  void       *phase1_data;
  INT32       phase1_length;
  int        fin_denom, fin_qual;
  DistillerStatus result = distBadInput;

  SetData(dout, NULL);
  bailout_now = 0;

  if ( (setjmp(jumpbuffer) != 0) ) {
    /*
     *  fatal error occurred, so return immediately.
     */
    MonitorClientSend(monID, "Distiller Errors", 
		      "Resetting distiller...\n", "Log");
    DistillerExit();
    DistillerInit(dType, 0, NULL);

    if (DataPtr(dout) != NULL)
      DataNeedsFree(dout,gm_True);
    else
      DataNeedsFree(dout,gm_False);
    return distFatalError;
  }

  /*
   *  parse distillation arguments.  set default values for some
   *  things, then override them if args are specified.  Default will
   *  be to scale each axis by 0.5, turn quality down to 55%.
   */

  for (i=0; i<nargs; i++) {
    INT32 argval = ARG_INT(args[i]);

    switch(ARG_ID(args[i])) {
    case GJPG_MAX_X:
      max_x = (int) argval;
      break;
    case GJPG_MAX_Y:
      max_y = (int) argval;
      break;
    case GJPG_MIN_X:
      min_x = (int) argval;
      break;
    case GJPG_MIN_Y:
      min_y = (int) argval;
      break;
    case GJPG_SCALE:
      scale = (double) ARG_DOUBLE(args[i]);
      break;
    case GJPG_QUAL:
      qual = (int) argval;
      break;
    case FRONT_RESIZE:
      resize = (int) argval;
      break;
    case FRONT_NO_DISTILL:
      nodistill = (int) argval;
      break;
    case FRONT_QUALITY:
      if (argval >= 1 && argval <= 5)
        quality = (int) argval;
      break;
    case FRONT_EXPERT:
      expert = (int) argval;
      break;
    case FRONT_ISMAP:
      ismap = (int) argval;
      break;
    default:
      break;
    }
  }

  if (nodistill) {
    return gjpg_passthrough(din, dout);
  }

  /* First pass through, we're just going to convert the GIF to JPEG */
  phase1_data = NULL;
  phase1_length = 0;
  dstinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&dstinfo);
  src_mgr = jinit_read_gif(&dstinfo, (JOCTET *) DataPtr(din),
			   (INT32) DataLength(din));
  (*src_mgr->start_input)(&dstinfo, src_mgr);
  jpeg_default_colorspace(&dstinfo);
  jpeg_mem_dest(&dstinfo, (void **) &phase1_data, (UINT32 *) &phase1_length);
  jpeg_start_compress(&dstinfo, TRUE);

  while (dstinfo.next_scanline < dstinfo.image_height) {
    num_scanlines = (*src_mgr->get_pixel_rows)(&dstinfo, src_mgr);
    jpeg_write_scanlines(&dstinfo, src_mgr->buffer, num_scanlines);
  }

  (*src_mgr->finish_input)(&dstinfo, src_mgr);
  jpeg_finish_compress(&dstinfo);

  /* early bailout because of animated or transparent gif? */
  if (bailout_now
      &&  DataLength(din) <= bailout_thresh[quality]) {
    result = distBadInput;
    goto JPGMUNGE_RETURN;
  }
    

  /* Now we're into the second pass.  Let's do our JPEG->JPEG
   * distillation. We need to compute the denominator and the quality
   * knob setting. */

  if (expert) {
    /* Do expert-like things here.  Need to work out still. */
    fin_qual = qual;
    fin_denom = compute_scale_denom(max_x, max_y, min_x, min_y,
				    srcinfo.image_width,
				    srcinfo.image_height, scale);
  } else {
    /* We're in beginner mode.  Life is easier. */
    if (ismap) {
      fin_qual = ismap_qual[quality-1];
      fin_denom = ismap_denom[quality-1];
    } else if (resize) {
      fin_qual = noscale_qual[quality-1];
      fin_denom = noscale_denom[quality-1];
    } else {
      fin_qual = norm_qual[quality-1];
      fin_denom = norm_denom[quality-1];
    }
  }

  /* Prep the input decompressor */
  jpeg_mem_src(&srcinfo, phase1_data, phase1_length);
  jpeg_read_header(&srcinfo, TRUE);

  srcinfo.scale_num = 1;
  srcinfo.scale_denom = fin_denom;
  srcinfo.dither_mode = JDITHER_ORDERED;  
  srcinfo.dct_method = JDCT_FASTEST;      
  jpeg_start_decompress(&srcinfo);

  /* Prep the output compressor */
  SetDataLength(dout,0);
  SetData(dout, NULL);
  sprintf(dout->mimeType, "image/jpeg");
  jpeg_mem_dest(&dstinfo, (void **) &(DataPtr(dout)),
                (UINT32 *) &(DataLength(dout)));
  dstinfo.image_width = srcinfo.output_width;
  dstinfo.image_height = srcinfo.output_height;
  dstinfo.input_components = srcinfo.output_components;
  dstinfo.in_color_space = srcinfo.out_color_space;
  jpeg_set_defaults(&dstinfo);
  jpeg_set_quality(&dstinfo, fin_qual, TRUE);
  jpeg_start_compress(&dstinfo, TRUE);

  handoff_height = (JDIMENSION) 1;
  handoff_width = 
    srcinfo.output_width*sizeof(JSAMPLE)*srcinfo.output_components;
  handoff = (JSAMPROW) malloc(handoff_width);

  /* We're going to need some buffer space to hand off data
     from the decompressor to the compressor. */
  while (srcinfo.output_scanline < srcinfo.output_height) {
    num_scanlines = jpeg_read_scanlines(&srcinfo, &handoff, handoff_height);
    jpeg_write_scanlines(&dstinfo, &handoff, num_scanlines);
  }

  jpeg_finish_decompress(&srcinfo);
  jpeg_finish_compress(&dstinfo);

  result = distOk;

JPGMUNGE_RETURN:
  if (handoff)
    free(handoff);
  DataNeedsFree(dout,gm_True);
  if (phase1_data)
    free(phase1_data);
  if (DataLength(dout) > DataLength(din)) {
    SetDataLength(dout, DataLength(din));
    memcpy(DataPtr(dout), DataPtr(din), DataLength(din));
    sprintf(dout->mimeType, "image/gif");
  }
  DEBUG("finished processing\n");
  return result;
}

int compute_scale_denom(int max_x, int max_y, int min_x, int min_y,
			int in_x, int in_y, double scale)
{
  int int_scale=1, outx, outy;

  if (scale == 1.0)
    int_scale = 1;
  else if (scale == 0.5)
    int_scale = 2;
  else if (scale == 0.25)
    int_scale = 4;
  else if (scale == 0.125)
    int_scale = 8;
  else
    int_scale = 2;

  /* test null arg case */
  if ((max_x == -1) && (max_y == -1) && (min_x == -1) && (min_y == -1))
    return int_scale;

  /* if image smaller than min, we're done already */
  if ((in_x <= min_x) || (in_x <= min_y))
    return 1;

  /* compute theoretically what the requested scale factor would give us */
  outx = in_x / int_scale;
  outy = in_y / int_scale;

  if ((min_x != -1) && (outx < min_x)) {  /* must try to satisfy min_x */
    while ((outx < min_x) && (int_scale > 1)) {
      int_scale >>= 1;
      outx = in_x / int_scale;
      outy = in_y / int_scale;
    }
    return int_scale;
  }

  if ((min_y != -1) && (outy < min_y)) {  /* must try to satisfy min_y */
    while ((outy < min_y) && (int_scale > 1)) {
      int_scale >>= 1;
      outx = in_x / int_scale;
      outy = in_y / int_scale;
    }
    return int_scale;
  }

  if ((max_x != -1) && (outx > max_x)) {  /* must try to satisfy max_x */
    while ((outx > max_x) && (int_scale < 8)) {
      int_scale <<= 1;
      outx = in_x / int_scale;
      outy = in_y / int_scale;
    }
    return int_scale;
  }

  if ((max_y != -1) && (outy > max_y)) {  /* must try to satisfy max_y */
    while ((outy > max_y) && (int_scale < 8)) {
      int_scale <<= 1;
      outx = in_x / int_scale;
      outy = in_y / int_scale;
    }
    return int_scale;
  }

  return int_scale;
}

void gifjpg_error_exit(j_common_ptr cinfo)
{
  (*cinfo->err->output_message)(cinfo);
  longjmp(jumpbuffer, 1);
}

void gifjpg_output_message(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX+1];

  (*cinfo->err->format_message)(cinfo, buffer);
  strcat(buffer, "\n");
  MonitorClientSend(monID, "Distiller Errors", buffer, "Log");
}

DistillerStatus gjpg_passthrough(DistillerInput *din, DistillerOutput *dout)
{
  strcpy(dout->mimeType, din->mimeType);
  SetDataLength(dout,DataLength(din));
  SetData(dout, DataPtr(din));
  DataNeedsFree(dout,gm_False);
  return distOk;
}
