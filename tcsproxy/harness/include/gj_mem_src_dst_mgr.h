/****
** This file defines the data structures and declares the functions
** necessary to create a memory based source and destination manager
** for the JPEG library.  This will let the JPEG library read data
** from memory instead of a FILE *.
**
** Steve Gribble, Jan 25/1997
** 
** $Id: gj_mem_src_dst_mgr.h,v 1.1.1.1 1997/09/23 00:02:25 tkimball Exp $
****/

#ifndef MEM_SRC_DST_MGR_H
#define MEM_SRC_DST_MGR_H

#include "distinterface.h"
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"
#include "cdjpeg.h"

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

/**** Data source object for in-memory GIF source ****/

/*
 * This code is loosely based on giftoppm from the PBMPLUS distribution
 * of Feb. 1991.  That file contains the following copyright notice:
 * +-------------------------------------------------------------------+
 * | Copyright 1990, David Koblas.                                     |
 * |   Permission to use, copy, modify, and distribute this software   |
 * |   and its documentation for any purpose and without fee is hereby |
 * |   granted, provided that the above copyright notice appear in all |
 * |   copies and that both that copyright notice and this permission  |
 * |   notice appear in supporting documentation.  This software is    |
 * |   provided "as is" without express or implied warranty.           |
 * +-------------------------------------------------------------------+
 *
 * We are also required to state that
 *    "The Graphics Interchange Format(c) is the Copyright property of
 *    CompuServe Incorporated. GIF(sm) is a Service Mark property of
 *    CompuServe Incorporated."
 */

#define	MAXCOLORMAPSIZE	256	/* max # of colors in a GIF colormap */
#define NUMCOLORS	3	/* # of colors */
#define CM_RED		0	/* color component numbers */
#define CM_GREEN	1
#define CM_BLUE		2

#define	MAX_LZW_BITS	12	/* maximum LZW code size */
#define LZW_TABLE_SIZE	(1<<MAX_LZW_BITS) /* # of possible LZW symbols */

/* Macros for extracting header data --- note we assume chars may be signed */

#define LM_to_uint(a,b)		((((b)&0xFF) << 8) | ((a)&0xFF))

#define BitSet(byte, bit)	((byte) & (bit))
#define INTERLACE	0x40	/* mask for bit signifying interlaced image */
#define COLORMAPFLAG	0x80	/* mask for bit signifying colormap presence */

/* LZW decompression tables look like this:
 *   symbol_head[K] = prefix symbol of any LZW symbol K (0..LZW_TABLE_SIZE-1)
 *   symbol_tail[K] = suffix byte   of any LZW symbol K (0..LZW_TABLE_SIZE-1)
 * Note that entries 0..end_code of the above tables are not used,
 * since those symbols represent raw bytes or special codes.
 *
 * The stack represents the not-yet-used expansion of the last LZW symbol.
 * In the worst case, a symbol could expand to as many bytes as there are
 * LZW symbols, so we allocate LZW_TABLE_SIZE bytes for the stack.
 * (This is conservative since that number includes the raw-byte symbols.)
 *
 * The tables are allocated from FAR heap space since they would use up
 * rather a lot of the near data space in a PC.
 */


/* Private version of data source object */

typedef struct {
  struct cjpeg_source_struct pub; /* public fields */
  j_compress_ptr cinfo;		/* back link saves passing separate parm */
  JSAMPARRAY colormap;		/* GIF colormap (converted to my format) */

  /* State for GetCode and LZWReadByte */
  char code_buf[256+4];		/* current input data block */
  int last_byte;		/* # of bytes in code_buf */
  int last_bit;			/* # of bits in code_buf */
  int cur_bit;			/* next bit index to read */
  boolean out_of_blocks;	/* TRUE if hit terminator data block */
  int input_code_size;		/* codesize given in GIF file */
  int clear_code,end_code;	/* values for Clear and End codes */
  int code_size;		/* current actual code size */
  int limit_code;		/* 2^code_size */
  int max_code;			/* first unused code value */
  boolean first_time;		/* flags first call to LZWReadByte */

  /* Private state for LZWReadByte */
  int oldcode;			/* previous LZW symbol */
  int firstcode;		/* first byte of oldcode's expansion */

  /* LZW symbol table and expansion stack */
  UINT16 FAR *symbol_head;	/* => table of prefix symbols */
  UINT8  FAR *symbol_tail;	/* => table of suffix bytes */
  UINT8  FAR *symbol_stack;	/* => stack for symbol expansions */
  UINT8  FAR *sp;		/* stack pointer */

  /* State for interlaced image processing */
  boolean is_interlaced;	/* TRUE if have interlaced image */
  jvirt_sarray_ptr interlaced_image; /* full image in interlaced order */
  JDIMENSION cur_row_number;	/* need to know actual row number */
  JDIMENSION pass2_offset;	/* # of pixel rows in pass 1 */
  JDIMENSION pass3_offset;	/* # of pixel rows in passes 1&2 */
  JDIMENSION pass4_offset;	/* # of pixel rows in passes 1,2,3 */
} gif_source_struct;

typedef gif_source_struct *gif_source_ptr;

/* Forward declarations */
JDIMENSION get_pixel_rows(j_compress_ptr cinfo, cjpeg_source_ptr sinfo);
JDIMENSION load_interlaced_image(j_compress_ptr cinfo, cjpeg_source_ptr sinfo);
JDIMENSION get_interlaced_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo);
int ReadOK(gif_source_ptr sinfo, char *buf, int count);


#endif
