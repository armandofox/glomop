#ifndef GIFTOPPM_MUNG_H
#define GIFTOPPM_MUNG_H

#define GIFTOPPMGMUNGE_FATAL_ERROR 1

#include "distinterface.h"
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

DistillerStatus
giftoppm_ComputeDistillationCost(Argument *args, int numArgs,
			DistillerInput *din,
                        DistillationCost *cost);

DistillerStatus
giftoppm_DistillerInit(C_DistillerType distType, 
		       int argc, const char * const *argv);

void giftoppm_DistillerExit(void);

DistillerStatus
giftoppm_DistillerMain(Argument *args, int nargs,
		       DistillerInput *din,
		       DistillerOutput *dout);

#endif

