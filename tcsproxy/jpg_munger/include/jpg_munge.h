#ifndef JPG_MUNG_H
#define JPG_MUNG_H

#define JPGMUNGE_FATAL_ERROR 1

#include <stdio.h>
#include <setjmp.h>
extern jmp_buf jumpbuffer;

#ifdef DEBUGGING
#define DistillerMalloc malloc
#define DistillerFree free
#define DEBUG(x) fprintf(stderr,x)
#define DEBUG2(x,y) fprintf(stderr,x,y)
#define pmerror(x) fprintf(stderr,x)
#else /* DEBUGGING */
#define DEBUG(x)
#define DEBUG2(x,y)
#endif

#endif

