/*
 *  jpg_munge.c
 *
 *  $Id: jpg_munge.c,v 1.22 1997/08/30 21:34:39 fox Exp $
 *
 *  This code acts as a JPG distillation server for tcsproxy.
 */

#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "jpeglib.h"
#include "jpg_munge.h"
#include "distinterface.h"
#include "mem_src_dst_mgr.h"

#include "../include/ARGS.h"
#include "../../frontend/include/ARGS.h"

typedef struct jpgd_error_mgr_st {
  struct  jpeg_error_mgr pub;
} jpgd_error_mgr, *jpgd_error_ptr;

/* Some long-lived globals;  will be reused for each distillation
   for the lifetime of this distiller. */

jmp_buf jumpbuffer;                                 /* for setjmp */
struct  jpeg_decompress_struct srcinfo;             /* jpg->bytes */
struct  jpeg_compress_struct   dstinfo;             /* bytes->jpg */
jpgd_error_mgr                 jsrc_err, jdst_err;  /* error handlers */
MonitorClientID                monID;

C_DistillerType dType;

/* Quality tables for the beginner's interface */
int ismap_denom[5] = {1,1,1,1,1};    /* Don't want to lose text detail */
int ismap_qual[5] = {15, 25, 35, 50, 75};  /* Want high quality ismaps */
int noscale_denom[5] = {1,1,1,1,1};    /* Don't scale down this image */
int noscale_qual[5] = {10, 15, 20, 40, 75};  /* Just a guess */
int norm_denom[5] = {2, 2, 1, 1, 1}; /* Scale away! */
int norm_qual[5] =  {15, 35, 20, 40, 75};  /* Again, just a guess! */

void jpgd_error_exit(j_common_ptr cinfo);
void jpgd_output_message(j_common_ptr cinfo);

int compute_scale_denom(int max_x, int max_y, int min_x, int min_y, 
			int in_x, int int_y, double scale);

DistillerStatus jpg_passthrough(DistillerInput *din, DistillerOutput *dout);

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

  srcinfo.err = jpeg_std_error(&jsrc_err.pub);
  jsrc_err.pub.error_exit = jpgd_error_exit;
  jsrc_err.pub.output_message = jpgd_output_message;
  dstinfo.err = jpeg_std_error(&jdst_err.pub);
  jdst_err.pub.error_exit = jpgd_error_exit;
  jdst_err.pub.output_message = jpgd_output_message;

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
  JSAMPROW handoff;
  JDIMENSION handoff_height, num_scanlines;
  int      max_x=-1, max_y=-1, min_x=-1, min_y=-1, qual=55, i;
  int      expert=0, resize=0, quality=3, nodistill=0, ismap=0;
  double   scale = 0.5;
  int      fin_denom, fin_qual;

  SetData(dout, NULL);

  if (setjmp(jumpbuffer) != 0) {
    /*
     *  fatal error occurred, so return immediately.
     */
    MonitorClientSend(monID, "Distiller Errors", "Resetting distiller...\n", "Log");
    DistillerExit();
    DistillerInit(dType, 0, NULL);

    if (DataPtr(dout) != NULL)
      DataNeedsFree(dout, gm_True);
    else
      DataNeedsFree(dout, gm_False);
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
    case JPG_MAX_X:
      max_x = (int) argval;
      break;
    case JPG_MAX_Y:
      max_y = (int) argval;
      break;
    case JPG_MIN_X:
      min_x = (int) argval;
      break;
    case JPG_MIN_Y:
      min_y = (int) argval;
      break;
    case JPG_SCALE:
      scale = (double) ARG_DOUBLE(args[i]);
      break;
    case JPG_QUAL:
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

  if (1 || expert) {
    char buffer[300];
    sprintf(buffer, "Expert=%d Quality=%d Scale=%0.2g Xmax=%d Ymax=%d\n",
            expert, quality, scale, max_x, max_y);
    MonitorClientSend(monID, "Distiller Errors", buffer, "Log");
  }
  
  if (nodistill) {
    return jpg_passthrough(din, dout);
  }

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
  jpeg_mem_src(&srcinfo, DataPtr(din), DataLength(din));
  jpeg_read_header(&srcinfo, TRUE);

  srcinfo.scale_num = 1;
  srcinfo.scale_denom = fin_denom;
  srcinfo.dither_mode = JDITHER_ORDERED;  /* moderate speed/quality */
  srcinfo.dct_method = JDCT_FASTEST;      /* rippin' fast DCT */
  jpeg_start_decompress(&srcinfo);

  /* Prep the output compressor */
  DataLength(dout) = 0;
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

  /* We're going to need some buffer space to hand off data
     from the decompressor to the compressor. */
  handoff_height = (JDIMENSION) 1;
  handoff = (JSAMPROW)
    malloc(srcinfo.output_width*sizeof(JSAMPLE)*srcinfo.output_components);
  
  while (srcinfo.output_scanline < srcinfo.output_height) {
    num_scanlines = jpeg_read_scanlines(&srcinfo, &handoff, handoff_height);
    jpeg_write_scanlines(&dstinfo, &handoff, num_scanlines);
  }

  free(handoff);
  jpeg_finish_decompress(&srcinfo);
  jpeg_finish_compress(&dstinfo);
  DataNeedsFree(dout,gm_True);
  if (DataLength(dout) > DataLength(din)) {
    SetDataLength(dout, DataLength(din));
    memcpy(DataPtr(dout), DataPtr(din), DataLength(din));
    sprintf(dout->mimeType, "image/jpeg");
  }
  DEBUG("finished processing\n");
  return distOk;

}

int compute_scale_denom(int max_x, int max_y, int min_x, int min_y,
			int in_x, int in_y, double scale)
{
  int int_scale, outx, outy;

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

void jpgd_error_exit(j_common_ptr cinfo)
{
  (*cinfo->err->output_message)(cinfo);
  longjmp(jumpbuffer, 1);
}

void jpgd_output_message(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX+1];

  (*cinfo->err->format_message)(cinfo, buffer);
  strcat(buffer, "\n");
  MonitorClientSend(monID, "Distiller Errors", buffer, "Log");
}

DistillerStatus jpg_passthrough(DistillerInput *din, DistillerOutput *dout)
{
  strcpy(dout->mimeType, din->mimeType);
  SetDataLength(dout, DataLength(din));
  SetData(dout, DataPtr(din));
  DataNeedsFree(dout,gm_False);
  return distOk;
}
