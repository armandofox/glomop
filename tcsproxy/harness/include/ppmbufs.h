#include "ppm.h"
#include "ppmcmap.h"


typedef struct ppmbuf {
  unsigned char *bufstart;
  unsigned char *bufp;
  int rows;                     /* optional */
  int cols;
} ppmbuf;

#define ppmbuf_getc(b) (*((b)->bufp++))
#define pbmbuf_getc ppmbuf_getc
#define pbmbuf_getrawbyte  ppmbuf_getc

#define ppmbuf_putc(b,v) (*((b)->bufp++) = (unsigned char)v)
#define ppmbuf_putshort(b,s)  { ppmbuf_putc((b),((s)>>8)); ppmbuf_putc((b),((s)&0xff)); }
#define ppmbuf_putlong(b,l) { ppmbuf_putshort((b),((l)>>16)); ppmbuf_putshort((b),((l)&0xffff)); }

#define ppmbuf_length(b) ((b)->bufp - (b)->bufstart)
#define ppmbuf_base(b) ((b)->bufstart)

size_t ppmbuf_totalsize(struct ppmbuf *b); /* total buf size needed to hold PPM "file" */
void ppmbuf_writeppm(void *buf,struct ppmbuf *b); /* "write" ppm array into a buffer */

void ppmbuf_writeppminit(struct ppmbuf *b, int cols, int rows, pixval m, int forceplain);
void ppmbuf_writeppmrow(struct ppmbuf *b, pixel *pixelrow, int cols, pixval maxval, int forceplain);

int pbmbuf_getint(struct ppmbuf *buf);
void ppmbuf_readppminit(struct ppmbuf *b, int *cols, int *rows, pixval *m, int *fmt);
void ppmbuf_readppmrow(struct ppmbuf *b, pixel *prow, int cols, pixval m, int fmt);
pixel ** ppmbuf_readppm(void *buf, int *cols, int *rows, pixval *maxval);
void ppmbuf_allocarray(struct ppmbuf *b, int cols, int rows);
void ppmbuf_alloc(struct ppmbuf *b, size_t size);
void ppmbuf_freeppm(pixel **ppm,int rows);
void ppmbuf_free(struct ppmbuf *b);

  

