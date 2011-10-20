/**** 
** This file implements the functions necessary to create a memory based
** source and destination manager for the JPEG library.  This will let the
** JPEG library read data from memory instead of a FILE *.
**
** Steve Gribble, Jan 25/1997
**
** $Id: mem_src_dst_mgr.c,v 1.2 1997/02/08 03:11:46 gribble Exp $
****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "jpeglib.h"
#include "jpg_munge.h"
#include "distinterface.h"
#include "mem_src_dst_mgr.h"

/**** Data destination object for memory output ****/

void mem_init_destination(j_compress_ptr cinfo)
{
  mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;

  dest->copybuf = (JOCTET *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
			        COPYBUF_BLOCK_SIZE * SIZEOF(JOCTET));
  dest->pub.next_output_byte = dest->copybuf;
  dest->pub.free_in_buffer = COPYBUF_BLOCK_SIZE;
}

boolean mem_empty_output_buffer(j_compress_ptr cinfo)
{
  mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
  JOCTET      *ptr;

  /* First we must see if we need to realloc */
  if ( ( (INT32) (dest->bufsize) - dest->offset) < 
       (INT32) COPYBUF_BLOCK_SIZE) {
    JOCTET *tmp;

    tmp = realloc((void * ) *(dest->origbuf),
		  ((INT32) (dest->bufsize) + 
		   (INT32) ORIGBUF_REALLOC_BLOCK_SIZE) *
		  SIZEOF(JOCTET));
    if (tmp == NULL) {
      fprintf(stderr, "Out of memory in mem_empty_output_buffer\n");
      exit(1);
    }
    *(dest->origbuf) = tmp;
    (dest->bufsize) += (UINT32) ORIGBUF_REALLOC_BLOCK_SIZE;
  }
  ptr = *(dest->origbuf) + dest->offset;
  memcpy((void *) ptr, (void *) dest->copybuf, COPYBUF_BLOCK_SIZE);
  dest->offset += COPYBUF_BLOCK_SIZE;

  dest->pub.next_output_byte = dest->copybuf;
  dest->pub.free_in_buffer = COPYBUF_BLOCK_SIZE;
  return TRUE;
}

void mem_term_destination(j_compress_ptr cinfo)
{
  mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
  size_t datacount = COPYBUF_BLOCK_SIZE - dest->pub.free_in_buffer;
  JOCTET      *ptr;

  if (datacount > 0) {
    if ( ( (INT32) (dest->bufsize) - dest->offset) < (INT32) datacount) {
      JOCTET *tmp;

      tmp = realloc((void * ) *(dest->origbuf),
		    ((INT32) (dest->bufsize) + (INT32) COPYBUF_BLOCK_SIZE) *
		    SIZEOF(JOCTET));
      if (tmp == NULL) {
	fprintf(stderr, "Out of memory in mem_term_destination\n");
	exit(1);
      }
      *(dest->origbuf) = tmp;
      (dest->bufsize) += (UINT32) COPYBUF_BLOCK_SIZE;
    }
    ptr = *(dest->origbuf) + dest->offset;
    memcpy((void *) ptr, (void *) dest->copybuf, datacount);
    dest->offset += datacount;
  }
  *(dest->size) = (UINT32) dest->offset;
}

void jpeg_mem_dest(j_compress_ptr cinfo, void **buf, UINT32 *size)
{
  mem_dest_ptr dest;

  if (cinfo->dest == NULL) {   /* first time for this JPEG object */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT,
				 SIZEOF(mem_destination_mgr));
  }
  dest = (mem_dest_ptr) cinfo->dest;
  dest->origbuf = (JOCTET **) buf;
  dest->bufsize = (UINT32) *size;
  dest->size = (UINT32 *) size;
  dest->offset = (INT32) 0;
  dest->pub.init_destination = mem_init_destination;
  dest->pub.empty_output_buffer = mem_empty_output_buffer;
  dest->pub.term_destination = mem_term_destination;
}


/**** Data source object for memory input ****/

void mem_init_source(j_decompress_ptr cinfo)
{
  mem_src_ptr src = (mem_src_ptr) cinfo->src;

  src->offset = 0;
}

boolean mem_fill_input_buffer(j_decompress_ptr cinfo)
{
  /* This is called whenever the buffer is emptied.  It should never
     really be called, since we've provided ALL of the data up front. */
  ERREXIT(cinfo, JERR_INPUT_EOF);
  return TRUE;
}

void mem_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  mem_src_ptr src = (mem_src_ptr) cinfo->src;

  src->offset+=num_bytes;
  src->pub.bytes_in_buffer -= (size_t) num_bytes;
  src->pub.next_input_byte += (size_t) num_bytes;
}

void mem_term_source(j_decompress_ptr cinfo)
{
  /* do nothing here */
}

void jpeg_mem_src(j_decompress_ptr cinfo, void *buf, UINT32 size)
{
  mem_src_ptr src;

  if (cinfo->src == NULL) {   /* first time for this JPEG object */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT,
				 SIZEOF(mem_source_mgr));
  }
  src = (mem_src_ptr) cinfo->src;
  src->srcbuf = (JOCTET *) buf;
  src->size = (INT32) size;
  src->offset = (INT32) 0;
  src->pub.init_source = mem_init_source;
  src->pub.fill_input_buffer = mem_fill_input_buffer;
  src->pub.skip_input_data = mem_skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;  /* default */
  src->pub.term_source = mem_term_source;
  src->pub.bytes_in_buffer = size;
  src->pub.next_input_byte = (JOCTET *) buf;
}
