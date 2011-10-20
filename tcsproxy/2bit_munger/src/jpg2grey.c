/* Convert JPGs to greyscale images; based on Steve Gribble's JPG->PPM code */

#include <stdio.h>
#include <setjmp.h>
#include "jpeglib.h"
#include "gj_mem_src_dst_mgr.h"
#include "distinterface.h"
#include "jpg2grey.h"

#include "../include/ARGS.h"

#define DEBUG(x)

typedef struct jpgd_error_mgr_st {
  struct  jpeg_error_mgr pub;
} jpgd_error_mgr, *jpgd_error_ptr;

/* Some long-lived globals;  will be reused for each distillation
   for the lifetime of this distiller. */

static jmp_buf jumpbuffer;                                 /* for setjmp */
static struct  jpeg_decompress_struct srcinfo;             /* jpg->bytes */
static jpgd_error_mgr                 jsrc_err;
static MonitorClientID                monID;
static C_DistillerType                dType;

static void jpgd_error_exit(j_common_ptr cinfo);
static void jpgd_output_message(j_common_ptr cinfo);

DistillerStatus
jpg2grey_init(C_DistillerType distType, 
		       int argc, const char * const *argv)
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
jpg2grey_clean(void)
{
  jpeg_destroy_decompress(&srcinfo);
}
  
/* Convert the jpg pointed to by injpg to an array of grayscale values */
DistillerStatus
jpg2grey_main(unsigned char *injpg, unsigned int inlen,
		unsigned char **outgrey, int *owidth, int *oheight)
{
  JDIMENSION handoff_height;
  unsigned char *outbuf, *outrow;

  if (setjmp(jumpbuffer) != 0) {
    /*
     *  fatal error occurred, so return immediately.
     */
    MonitorClientSend(monID, "Distiller Errors", "Resetting distiller...\n", "Log");
    jpg2grey_clean();
    jpg2grey_init(dType, 0, NULL);

    return distFatalError;
  }
  
  /* Prep the input decompressor */
  jpeg_mem_src(&srcinfo, injpg, inlen);
  jpeg_read_header(&srcinfo, TRUE);
  srcinfo.dither_mode = JDITHER_ORDERED;  /* moderate speed/quality */
  srcinfo.dct_method = JDCT_FASTEST;      /* rippin' fast DCT */
  srcinfo.out_color_space = JCS_GRAYSCALE;
  jpeg_start_decompress(&srcinfo);

  /* Prep the output compressor */
  outbuf = DistillerMalloc(srcinfo.output_width * srcinfo.output_height);
  if (!outbuf) {
    return distOutOfLocalMemory;
  }
  
  assert(srcinfo.output_components == 1);

  handoff_height = (JDIMENSION) 1;
  
  outrow = outbuf;
  while (srcinfo.output_scanline < srcinfo.output_height) {
    jpeg_read_scanlines(&srcinfo, (JSAMPARRAY)&outrow, handoff_height);
    outrow += srcinfo.output_width;
  }

  *outgrey = outbuf;
  *owidth = srcinfo.output_width;
  *oheight = srcinfo.output_height;

  jpeg_finish_decompress(&srcinfo);

  return distOk;
}

static void jpgd_error_exit(j_common_ptr cinfo)
{
  (*cinfo->err->output_message)(cinfo);
  longjmp(jumpbuffer, 1);
}

static void jpgd_output_message(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX+1];

  (*cinfo->err->format_message)(cinfo, buffer);
  strcat(buffer, "\n");
  MonitorClientSend(monID, "Distiller Errors", buffer, "Log");
}
