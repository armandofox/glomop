#define GIFMUNGE_TYP_MAX_X  640
#define GIFMUNGE_TYP_MAX_Y  480

#define GIFMUNGE_FATAL_ERROR 1

#include "ARGS.h"
#include <stdio.h>
#include <setjmp.h>
#include "distinterface.h"
extern jmp_buf jumpbuffer;

typedef struct _ppmbuf {
  pixel **rowarray;
  pixel *buf;
  int rows, cols;
  int format;
  unsigned long size;
} ppmbuf;

extern void munge_init_ppmarray(ppmbuf *b, int cols, int rows);
extern unsigned long ppmtogif_main(int interlace, int sort, int transparent,
                         pixel *transcolorpixel, ppmbuf *inbuf, void *outbuf);
extern void ReadGIF (unsigned char *fd, int imageNum, int *transparent_p, 
                     pixel *transcolorpixel, ppmbuf *buf);
extern int pnmscale_main(int r, int c, int newpixels, ppmbuf *g, ppmbuf *buf);
extern int ppmquant_main(int colrs, char *palet, int floyd,
                         pixel *transcolorpixel, ppmbuf *buf);

extern MonitorClientID mon;

extern unsigned char hex[];
#define HEX(i)  hex[((int)i)]

#ifdef DEBUGGING
#define DEBUG(x) fprintf(stderr,x)
#define DEBUG2(x,y) fprintf(stderr,x,y)
#define pmerror(x) fprintf(stderr,x)
#else /* DEBUGGING */
#define DEBUG(x)
#define DEBUG2(x,y)
#endif

