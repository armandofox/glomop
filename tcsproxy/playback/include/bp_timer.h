/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: bp_timer.h
 */

#include <sys/time.h>
#include <unistd.h>

#ifndef BP_TIMER_H
#define BP_TIMER_H

typedef void (*bp_timer_proc)(int id, void *data);     /* must be mt-safe */
typedef enum { ABSOLUTE, RELATIVE } bp_ttype;

int bp_timer_init(void);
int bp_timer_add(bp_ttype type, struct timeval t, bp_timer_proc start_p,
		 void *tproc_data);
void bp_dispatch_signal(void);

#endif
