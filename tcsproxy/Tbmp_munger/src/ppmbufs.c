/*
 *  Some routines to read a pnm description from a buffer, instead of  a FILE*.
 *  Usage:  pixels = ppmbuf_readppm(bufptr, &cols, &rows, &maxval)
 *
 *  The statically-declared support routines here are basically copies
 *  of all the ppm_foo() routines.  The only differences are that these
 *  are called ppmbuf_foo() and that the FILE* argument is replaced by
 *  an argument of type 'struct ppmbuf *', which is basically a buffer
 *  that remembers where its cursor is.
 */

#include <assert.h>
#include "ppmbufs.h"
#include "../include/ARGS.h"

#define PPMBUF_MALLOC malloc
#define PPMBUF_FREE   free

static int
pbmbuf_readmagicnumber(struct ppmbuf *buf)
{
  int magic = (buf->bufp[0] << 8) + buf->bufp[1];
  buf->bufp += 2;
  return magic;
}

void
ppmbuf_writeppminit(struct ppmbuf *b, int cols, int rows, pixval maxval, int forceplain)
{
  int nchars;
  
  /*
   *  Note - forceplain is included for function-prototype compatibility
   *  with libppm, but we ALWAYS force RAW WRITES since we usually only
   *  allocate enough bytes to hold the array (plus another 20 or so
   *  bytes for the PPM header). 
   */
  b->rows = rows;
  b->cols = cols;
  nchars = sprintf((char *)(b->bufstart), "%c%c\n%d %d\n%d\n", PPM_MAGIC1,
                   (forceplain ? PPM_MAGIC2 : RPPM_MAGIC2),
                   cols, rows, (int)maxval);
  b->bufp = b->bufstart + nchars;
}

static void
ppmbuf_writeppmrowraw( struct ppmbuf *b, pixel *pixelrow, int cols, pixval maxval )
{
  register int col;
  register pixel* pP;
  register pixval val;

  for ( col = 0, pP = pixelrow; col < cols; ++col, ++pP )
    {
      val = PPM_GETR( *pP );
#ifdef DEBUGGING
      if ( val > maxval )
        pm_error( "r value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUGGING*/
      (void) ppmbuf_putc(b, val );
      val = PPM_GETG( *pP );
#ifdef DEBUGGING
      if ( val > maxval )
        pm_error( "g value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUGGING*/
      (void) ppmbuf_putc(b, val );
      val = PPM_GETB( *pP );
#ifdef DEBUGGING
      if ( val > maxval )
        pm_error( "b value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUGGING*/
      (void) ppmbuf_putc(b, val );
    }
}

static void
ppmbuf_writeppmrowplain(struct ppmbuf *b,pixel *pixelrow, int cols, pixval maxval )
{
  register int col, charcount;
  register pixel* pP;
  register pixval val;

  charcount = 0;
  for ( col = 0, pP = pixelrow; col < cols; ++col, ++pP )
    {
      if ( charcount >= 65 )
        {
          (void) ppmbuf_putc(b, '\n' );
          charcount = 0;
        }
      else if ( charcount > 0 )
        {
          (void) ppmbuf_putc(b, ' ' );
          (void) ppmbuf_putc(b, ' ' );
          charcount += 2;
        }
      val = PPM_GETR( *pP );
#ifdef DEBUGGING
      if ( val > maxval )
        pm_error( "r value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUGGING*/
      ppmbuf_putshort(b, (unsigned short) val );
      (void) ppmbuf_putc(b, ' ' );
      val = PPM_GETG( *pP );
#ifdef DEBUGGING
      if ( val > maxval )
        pm_error( "g value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUGGING*/
      ppmbuf_putshort(b, (unsigned short) val );
      (void) ppmbuf_putc(b, ' ' );
      val = PPM_GETB( *pP );
#ifdef DEBUGGING
      if ( val > maxval )
        pm_error( "b value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUGGING*/
      ppmbuf_putshort(b, (unsigned short) val );
      charcount += 11;
    }
  if ( charcount > 0 )
    (void) ppmbuf_putc(b, '\n' );
}

void
ppmbuf_writeppmrow( struct ppmbuf *b, pixel* pixelrow, int cols, pixval maxval, int forceplain )
{
  forceplain = 0;               /* since we alloc based on ppm "raw" representation */
  if ( maxval <= 255 && ! forceplain )
    ppmbuf_writeppmrowraw( b, pixelrow, cols, maxval );
  else
    ppmbuf_writeppmrowplain(b, pixelrow, cols, maxval );
}


int
pbmbuf_getint(struct ppmbuf *buf)
{
  register char ch;
  int i;
  
  do {
    ch = ppmbuf_getc(buf);
  } while ( ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' );

  if ( ch < '0' || ch > '9' )
    pm_error( "junk in file where an integer should be" );
  
  i = 0;
  do
    {
      i = i * 10 + ch - '0';
      ch = pbmbuf_getc( buf );
    }
  while ( ch >= '0' && ch <= '9' );

  return i;
}

static void
ppmbuf_readppminitrest(struct ppmbuf *b, int *colsP, int *rowsP, pixval *maxvalP)
{
  /* read size */
  *colsP = pbmbuf_getint(b);
  *rowsP = pbmbuf_getint(b);
  *maxvalP = pbmbuf_getint(b);
  if (*maxvalP > PPM_MAXMAXVAL)
    pm_error("maxval is too large - try reconfiguring with *  PGM_BIGGRAYS\n    or without PPM_PACKCOLORS" );
  
  b->rows = *rowsP;
  b->cols = *colsP;
}

void
ppmbuf_readppminit(struct ppmbuf *buf, int *colsp, int *rowsp, pixval *maxvalp, int *format)
{
  *format  = pbmbuf_readmagicnumber(buf);
  if (PPM_FORMAT_TYPE(*format) != PPM_TYPE) {
    Message("ppmbuf_readppminit: Only ppm supported (not p[gnb]m)");
    *format = 0;
    return;
  }
  ppmbuf_readppminitrest(buf, colsp, rowsp, maxvalp);
}

void
ppmbuf_readppmrow(struct ppmbuf *buf, pixel *pixelrow, int cols, pixval maxval, int format)
{
  register int col;
  register pixel* pP;
  register pixval r, g, b;

  switch ( format )
    {
    case PPM_FORMAT:
      for ( col = 0, pP = pixelrow; col < cols; ++col, ++pP )
        {
          r = pbmbuf_getint( buf );
#ifdef PPM_DEBUG
          if ( r > maxval )
            pm_error( "r value out of bounds (%u > %u)", r, maxval );
#endif /*PPM_DEBUG*/
          g = pbmbuf_getint( buf );
#ifdef PPM_DEBUG
          if ( g > maxval )
            pm_error( "g value out of bounds (%u > %u)", g, maxval );
#endif /*PPM_DEBUG*/
          b = pbmbuf_getint( buf );
#ifdef PPM_DEBUG
          if ( b > maxval )
            pm_error( "b value out of bounds (%u > %u)", b, maxval );
#endif /*PPM_DEBUG*/
          PPM_ASSIGN( *pP, r, g, b );
        }
      break;
    case RPPM_FORMAT:
      for ( col = 0, pP = pixelrow; col < cols; ++col, ++pP )
        {
          r = pbmbuf_getrawbyte( buf );
#ifdef DEBUG
          if ( r > maxval )
            pm_error( "r value out of bounds (%u > %u)", r, maxval );
#endif /*DEBUG*/
          g = pbmbuf_getrawbyte( buf );
#ifdef DEBUG
          if ( g > maxval )
            pm_error( "g value out of bounds (%u > %u)", g, maxval );
#endif /*DEBUG*/
          b = pbmbuf_getrawbyte( buf );
#ifdef DEBUG
          if ( b > maxval )
            pm_error( "b value out of bounds (%u > %u)", b, maxval );
#endif /*DEBUG*/
          PPM_ASSIGN( *pP, r, g, b );
        }
      break;

    default:
      Message("ppmbuf_readppmrow: Only ppm is supported right now (not p[gnb]m)");
      break;
    }
}
  
pixel **
ppmbuf_readppm(void *origbuf, int *colsP, int *rowsP, pixval *maxvalP)
{
  struct ppmbuf buf;
  int row;
  int format;
  pixel** pixels;

  buf.bufstart = buf.bufp = origbuf;
  buf.rows = *rowsP;
  ppmbuf_readppminit(&buf,colsP,rowsP,maxvalP,&format);
  if (format == 0) {
    /* format error reading ppm */
    return NULL;
  }
  
  pixels = ppm_allocarray(*colsP,*rowsP);
  for (row = 0; row < *rowsP; row++)
    ppmbuf_readppmrow(&buf,pixels[row],*colsP,*maxvalP,format);

  return pixels;
}

void
ppmbuf_allocarray(struct ppmbuf *b, int cols,int rows)
{
  pixel** p;

  p = ppm_allocarray(cols,rows);
  assert(p != NULL);
  b->bufstart = b->bufp = (unsigned char *)p;
  b->rows = rows;
  b->cols = cols;
}

void
ppmbuf_alloc(struct ppmbuf *b,size_t size)
{
  b->bufstart = b->bufp = PPMBUF_MALLOC(size);
  assert(b->bufstart != NULL);

  memset(b->bufstart, 0xee, size);
}

void
ppmbuf_freeppm(pixel **p, int rows)
{
  ppm_freearray(p,rows);
}

void
ppmbuf_free(struct ppmbuf *b)
{
  assert(b->bufstart);
  PPMBUF_FREE(b->bufstart);
}

