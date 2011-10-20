/*
 *  jpgtoppm_munge.c
 *
 *  $Id: jpgtoppm_munge.c,v 1.1.1.1 1997/09/23 00:02:26 tkimball Exp $
 *
 *  This code acts as a JPG->PPM distillation server.
 */

#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "cdjpeg.h"
#include "jpeglib.h"
#include "distinterface.h"
#include "gj_mem_src_dst_mgr.h"
#include "jpgtoppm_munge.h"

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
jpgd_error_mgr                 jsrc_err;
MonitorClientID                monID;
C_DistillerType                dType;
int                            bailout_now;

void jpgd_error_exit(j_common_ptr cinfo);
void jpgd_output_message(j_common_ptr cinfo);

DistillerStatus
jpgtoppm_ComputeDistillationCost(Argument *args, int numArgs,
			DistillerInput *din,
                        DistillationCost *cost)
{
  cost->estimatedTime_ms = DataLength(din);
  return distOk;
}

DistillerStatus
jpgtoppm_DistillerInit(C_DistillerType distType, 
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

  /* Create the compression/decompression objects.  This will allocate
     a whole bunch of stuff buried inside these structures. */

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
jpgtoppm_DistillerExit(void)
{
  jpeg_destroy_decompress(&srcinfo);
}
  
DistillerStatus
jpgtoppm_DistillerMain(Argument *args, int nargs,
		       DistillerInput *din,
		       DistillerOutput *dout)
{
  JSAMPROW   handoff;
  JDIMENSION handoff_height, num_scanlines;
  int        i;
  ppm_output  ppmout;

  SetData(dout, NULL);

  if (setjmp(jumpbuffer) != 0) {
    /*
     *  fatal error occurred, so return immediately.
     */
    MonitorClientSend(monID, "Distiller Errors", "Resetting distiller...\n", "Log");
    jpgtoppm_DistillerExit();
    jpgtoppm_DistillerInit(dType, 0, NULL);

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

  /* Prep the input decompressor */
  jpeg_mem_src(&srcinfo, DataPtr(din), DataLength(din));
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

    if (srcinfo.output_components == 1)
      sprintf(testbuf, "P5\n%d %d\n%d\n",
	      srcinfo.output_width, srcinfo.output_height, 255);
    else
      sprintf(testbuf, "P6\n%d %d\n%d\n",
	      srcinfo.output_width, srcinfo.output_height, 255);
    write_ppm_output(&ppmout, testbuf, strlen(testbuf));
  }
  
  /* We're going to need some buffer space to grab data
     from the decompressor */
  handoff_height = (JDIMENSION) 1;
  handoff = (JSAMPROW)
    malloc(srcinfo.output_width*sizeof(JSAMPLE)*srcinfo.output_components);
  assert(handoff);
  
  while (srcinfo.output_scanline < srcinfo.output_height) {
    num_scanlines = jpeg_read_scanlines(&srcinfo, &handoff, handoff_height);
    if (srcinfo.output_components == 3)
      write_ppm_output(&ppmout, (char *) handoff, 3*srcinfo.output_width);
    else
      write_ppm_output(&ppmout, (char *) handoff, srcinfo.output_width);
  }

  free(handoff);
  jpeg_finish_decompress(&srcinfo);
  DataNeedsFree(dout,gm_True);
  SetData(dout, ppmout.data);
  SetDataLength(dout, ppmout.length);
  DEBUG("finished processing\n");
  return distOk;
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
