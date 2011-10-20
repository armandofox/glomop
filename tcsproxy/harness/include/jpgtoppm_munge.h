#ifndef JPGTOPPM_MUNG_H
#define JPGTOPPM_MUNG_H

#define JPTOPPMGMUNGE_FATAL_ERROR 1

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
jpgtoppm_ComputeDistillationCost(Argument *args, int numArgs,
			DistillerInput *din,
                        DistillationCost *cost);

DistillerStatus
jpgtoppm_DistillerInit(C_DistillerType distType, 
		       int argc, const char * const *argv);

void jpgtoppm_DistillerExit(void);

DistillerStatus
jpgtoppm_DistillerMain(Argument *args, int nargs,
		       DistillerInput *din,
		       DistillerOutput *dout);

#endif

