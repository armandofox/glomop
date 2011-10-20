/****
** This file defines the data structures and declares the functions
** necessary to create a memory based source and destination manager
** for the JPEG library.  This will let the JPEG library read data
** from memory instead of a FILE *.
**
** Steve Gribble, Jan 25/1997
** 
** $Id: mem_src_dst_mgr.h,v 1.1 1997/01/26 01:27:56 gribble Exp $
****/

#ifndef MEM_SRC_DST_MGR_H
#define MEM_SRC_DST_MGR_H

#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

/**** Data destination object for memory output ****/

#define COPYBUF_BLOCK_SIZE 4096
#define ORIGBUF_REALLOC_BLOCK_SIZE 32768
typedef struct {
  struct jpeg_destination_mgr   pub;        /* public fields */
  JOCTET                       *copybuf;    /* transition buffer for copy */
  JOCTET                      **origbuf;    /* *origbuf is start of buffer */
  UINT32                        bufsize;    /* size of buffer */
  UINT32                        *size;      /* *size is how big image is now */
  INT32                         offset;     /* where are we now? */
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;

void mem_init_destination(j_compress_ptr cinfo);
boolean mem_empty_output_buffer(j_compress_ptr cinfo);
void mem_term_destination(j_compress_ptr cinfo);
void jpeg_mem_dest(j_compress_ptr cinfo, void **buf, UINT32 *size);


/**** Data source object for memory input ****/

typedef struct {
  struct jpeg_source_mgr    pub;     /* public fields */
  JOCTET                   *srcbuf;  /* source data buffer */
  INT32                     size;    /* size of source data buffer */
  INT32                     offset;  /* where are we now? */
} mem_source_mgr;

typedef mem_source_mgr *mem_src_ptr;

void mem_init_source(j_decompress_ptr cinfo);
boolean mem_fill_input_buffer(j_decompress_ptr cinfo);
void mem_skip_input_data(j_decompress_ptr cinfo, long num_bytes);
void mem_term_source(j_decompress_ptr cinfo);
void jpeg_mem_src(j_decompress_ptr cinfo, void *buf, UINT32 size);

#endif
