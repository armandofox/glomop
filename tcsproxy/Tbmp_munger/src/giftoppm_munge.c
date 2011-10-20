/*
 *  giftoppm_munge.c
 *
 *  $Id: giftoppm_munge.c,v 1.3 1997/09/30 04:33:48 gribble Exp $
 *
 *  This code acts as a GIF->PPM distillation server.
 */

#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "cdjpeg.h"
#include "jpeglib.h"
#include "distinterface.h"
#include "gj_mem_src_dst_mgr.h"
#include "giftoppm_munge.h"

#include "../include/ARGS.h"
#include "../../frontend/include/ARGS.h"

typedef struct _ppm_outbuf_st {
  char *data;
  unsigned int  length;
  unsigned int  bufsize;
} ppm_output;

void init_ppm_output(ppm_output *initme);
void write_ppm_output(ppm_output *writeme, char *data, int length);

typedef struct jpgd_error_mgr_st {
  struct  jpeg_error_mgr pub;
} jpgd_error_mgr, *jpgd_error_ptr;

/* Some long-lived globals;  will be reused for each distillation
   for the lifetime of this distiller. */

jmp_buf jumpbuffer;                                 /* for setjmp */
struct  jpeg_decompress_struct srcinfo;             /* jpg->bytes */
struct  jpeg_compress_struct   dstinfo;             /* gif->jpg   */
jpgd_error_mgr                 jsrc_err, jdst_err;
MonitorClientID                monID;
C_DistillerType                dType;
int                            bailout_now;

void jpgd_error_exit(j_common_ptr cinfo);
void jpgd_output_message(j_common_ptr cinfo);

DistillerStatus
giftoppm_ComputeDistillationCost(Argument *args, int numArgs,
			DistillerInput *din,
                        DistillationCost *cost)
{
  cost->estimatedTime_ms = DataLength(din);
  return distOk;
}

DistillerStatus
giftoppm_DistillerInit(C_DistillerType distType, 
		       int argc, const char * const *argv)
{
  if (0)
    return (distFatalError);

  bailout_now = 0;
  dType = distType;
  /* Tell the JPEG library to report errors on STDERR, and call the
     gifjpg_error_exit() routine if a fatal error occurs.  This handler
     will do a longjmp back to the beginning... */

  srcinfo.err = jpeg_std_error(&jsrc_err.pub);
  jsrc_err.pub.error_exit = jpgd_error_exit;
  jsrc_err.pub.output_message = jpgd_output_message;
  dstinfo.err =  jpeg_std_error(&jdst_err.pub);
  jdst_err.pub.error_exit = jpgd_error_exit;
  jdst_err.pub.output_message = jpgd_output_message;

  /* Create the compression/decompression objects.  This will allocate
     a whole bunch of stuff buried inside these structures. */

  jpeg_create_compress(&dstinfo);
  jpeg_create_decompress(&srcinfo);

  monID = DistillerGetMonitorClientID();

  if (BITS_IN_JSAMPLE != 8) {
    DEBUG("libjpeg compiled with BITS_IN_JSAMPLE != 8 - can't handle it.\n");
    exit(1);
  }
  DEBUG("distiller initialized ok\n");
  return distOk;
}

void
giftoppm_DistillerExit(void)
{
  jpeg_destroy_decompress(&srcinfo);
  jpeg_destroy_compress(&dstinfo);
}
  
DistillerStatus
giftoppm_DistillerMain(Argument *args, int nargs,
		       DistillerInput *din,
		       DistillerOutput *dout)
{
  JSAMPROW   handoff;
  JDIMENSION handoff_height, num_scanlines;
  int        i;
  ppm_output  ppmout;
  void       *phase1_data;
  INT32       phase1_length;
  cjpeg_source_ptr src_mgr;

  SetData(dout, NULL);
  bailout_now = 0;

  if (setjmp(jumpbuffer) != 0) {
    /*
     *  fatal error occurred, so return immediately.
     */
    MonitorClientSend(monID, "Distiller Errors", "Resetting distiller...\n", "Log");
    giftoppm_DistillerExit();
    giftoppm_DistillerInit(dType, 0, NULL);

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
    switch(ARG_ID(args[i])) {
    default:
      break;
    }
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

  /* Now do JPEG->PPM conversion */
  jpeg_mem_src(&srcinfo, phase1_data, phase1_length);
  jpeg_read_header(&srcinfo, TRUE);
  srcinfo.dither_mode = JDITHER_ORDERED;  /* moderate speed/quality */
  srcinfo.dct_method = JDCT_FASTEST;      /* rippin' fast DCT */
  jpeg_start_decompress(&srcinfo);

  /* Prep the output compressor */
  SetDataLength(dout, 0);
  SetData(dout, NULL);
  sprintf(dout->mimeType, "image/ppm");
  init_ppm_output(&ppmout);
  {
    char testbuf[50];

    if (srcinfo.output_components == 1) {
      /* Even though this is a grayscale JPG, we'll create a binary
	 PPM not a binary PGM */
      sprintf(testbuf, "P6\n%d %d\n%d\n",   /* was p5 for PGM */
	      srcinfo.output_width, srcinfo.output_height, 255);
    } else {
      sprintf(testbuf, "P6\n%d %d\n%d\n",
	      srcinfo.output_width, srcinfo.output_height, 255);
    }
    write_ppm_output(&ppmout, testbuf, strlen(testbuf));
  }
  
  /* We're going to need some buffer space to grab data
     from the decompressor */
  handoff_height = (JDIMENSION) 1;
/*  handoff = (JSAMPROW)
    malloc(srcinfo.output_width*sizeof(JSAMPLE)*srcinfo.output_components); */
  handoff = (JSAMPROW)
    malloc(srcinfo.output_width*sizeof(JSAMPLE)*3);  /* force PPM */
  assert(handoff);
  
  while (srcinfo.output_scanline < srcinfo.output_height) {
    num_scanlines = jpeg_read_scanlines(&srcinfo, &handoff, handoff_height);
    if (srcinfo.output_components == 3)
      write_ppm_output(&ppmout, (char *) handoff, 3*srcinfo.output_width);
    else {
      int i;
      /* write_ppm_output(&ppmout, (char *) handoff, srcinfo.output_width);*/
      for (i=0; i<srcinfo.output_width; i++) {
	char outchar[3];

	outchar[0] = outchar[1] = outchar[2] = handoff[i];
	write_ppm_output(&ppmout, outchar, 3);
      }
    }
  }

  free(handoff);
  if (phase1_data)
    free(phase1_data);
  jpeg_finish_decompress(&srcinfo);
  SetData(dout, ppmout.data);
  SetDataLength(dout, ppmout.length);
  DataNeedsFree(dout,gm_True);
  DEBUG("finished processing\n");
  return distOk;
}

/*
 *  Following code only needs to appear once, so if both giftoppm_munge
 *  and jpgtoppm_munge are being "bundled" into a single distiller,
 *  don't include it more than once.
 */

#ifndef BUNDLED

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


/* Variable length buffer stuff */

#define INCRSIZE 4096

void init_ppm_output(ppm_output *initme)
{
  initme->length = initme->bufsize = 0;
  initme->data = NULL;
}

void write_ppm_output(ppm_output *writeme, char *data, int length)
{
  /* Make sure we have enough space */
  while ((writeme->bufsize - writeme->length) < length+1) {
    char *reacbuf;

    reacbuf = realloc(writeme->data, writeme->bufsize+INCRSIZE);
    assert(reacbuf);
    writeme->data = reacbuf;
    writeme->bufsize += INCRSIZE;
  }
  /* Now copy in data */
  memcpy( (writeme->data + writeme->length), data, length);
  writeme->length += length;
}

#endif /* ifndef BUNDLED */
