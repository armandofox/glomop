/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: bp_timer.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>

#include "bp_timer.h"
#include "utils.h"

/*** Elements of the task queue ***/
typedef struct taskq_el_st {
  int             tid;               /* task ID as assigned by bp_timer_add */
  struct timeval  ttime;             /* time that task will run */
  bp_timer_proc   tproc;             /* the task itself */
  void           *tproc_arg;         /* caller's args to task procedure */
} taskq_el;

/*** The task queue (and associated synchronization variables) ***/
ll              taskq = NULL;
int             taskq_len = 0;
pthread_mutex_t taskq_mutex;
pthread_cond_t  taskq_cond;

int             taskid = 0;
pthread_mutex_t taskid_mutex;

int             num_threads_running = 0;

void *bp_timer_mainloop(void *arg);
void  bp_dispatch_task(taskq_el *task);
int   bp_timevalptr_cmp(void *tvp1, void *tvp2);

/**
 ** bp_timer_init will initialize the timer subsystem by creating a thread
 ** that will loop indefinitely, processing tasks on the task queue.  This
 ** returns 0 on success, one of the integers in the pthread_create man
 ** page on failure.
 ** 
 ** Also created is a (mutex,cond) pair for use with the task queue.  The
 ** task queue service thread will do a timed sleep on the condition variable.
 ** The thread will wake up either when the next task should be dispatched,
 ** or when something changes on the task queue, so it can reevaluate its
 ** sleep time.
 **/

int bp_timer_init(void)
{
  pthread_t      t;
  pthread_attr_t attr;
  int            res;

  if ((res = pthread_mutex_init(&taskq_mutex, NULL)) != 0) {
    strerror(res);
    return res;
  }
  if ((res = pthread_mutex_init(&taskid_mutex, NULL)) != 0) {
    strerror(res);
    return res;
  }
  if ((res = pthread_cond_init(&taskq_cond, NULL)) != 0) {
    strerror(res);
    return res;
  }
  pthread_attr_init(&attr);
  res = pthread_create(&t, &attr, bp_timer_mainloop, (void *) NULL);
  pthread_detach(t);
  pthread_attr_destroy(&attr);

  return res;
}

/**
 ** bp_timer_add will schedule the procedure "start_p" to be run at
 ** the time specified by "t", with argument tproc_data.  If "type" is
 ** ABSOLUTE, then t specifies a time similar to that returned by
 ** gettimeofday() at which the procedure should be called.  If "type"
 ** is RELATIVE, then t specifies an offset from now at which time the
 ** procedure should be called.
 **
 ** This procedure will NOT signal the queue condition variable - the
 ** caller is responsible for doing that if he/she desires, using
 ** bp_dispatch_signal().
 **/

int bp_timer_add(bp_ttype type, struct timeval t, bp_timer_proc start_p,
		 void *tproc_data)
{
  taskq_el       *newtask;
  int             res;
  struct timeval  now;

  if (type == RELATIVE) {
    gettimeofday(&now, NULL);
    t = tv_timeadd(now, t);
  }

  /* Build up the task data structure */
  newtask = (taskq_el *) malloc(sizeof(taskq_el));
  if (newtask == NULL) {
    fprintf(stderr, "malloc failed in bp_timer_add.\n");
    exit(1);
  }
  if (pthread_mutex_lock(&taskid_mutex) < 0) {
    fprintf(stderr, "mutex lock failed in bp_timer_add\n");
    exit(1);
  }
  newtask->tid = taskid++;
  if (pthread_mutex_unlock(&taskid_mutex) < 0) {
    fprintf(stderr, "mutex unlock failed in bp_timer_add\n");
    exit(1);
  }
  newtask->ttime = t;
  newtask->tproc = start_p;
  newtask->tproc_arg = tproc_data;

  if (pthread_mutex_lock(&taskq_mutex) < 0) {
    fprintf(stderr, "mutex q lock failed in bp_timer_add\n");
    exit(1);
  }
  res = ll_sorted_insert(&taskq, (void *) newtask, bp_timevalptr_cmp);
  taskq_len++;
  if (pthread_mutex_unlock(&taskq_mutex) < 0) {
    fprintf(stderr, "mutex q unlock failed in bp_timer_add\n");
    exit(1);
  }
  if (res != 1) {
    fprintf(stderr, "ll_sorted_insert failed in bp_timer_add\n");
    exit(1);
  }
  return 0;
}

/**
 ** bp_dispatch_signal will send a signal to the taskq_cond condition
 ** variable, telling the dispatcher to wake up and check on the first
 ** element in the queue.  This function should be called after a batch
 ** of bp_timer_add's have been done.
 **/
void bp_dispatch_signal(void)
{
  if (pthread_cond_signal(&taskq_cond) != 0) {
    fprintf(stderr, "pthread_cond_signal failed in bp_dispatch_signal.\n");
    exit(1);
  }
  return;
}

/**
 ** Here's the main loop of the task dispatch thread.  It's job is to
 ** do a timed wait on a condition variable, where the time elapsed is until
 ** the next task to be run.  When it wakes up, it should check the front
 ** of the task queue, see if it's time to dispatch the event, and if not,
 ** go back to sleep.
 **/

void *bp_timer_mainloop(void *arg)
{
  struct timespec  sleeptime;
  struct timeval   now;
  taskq_el        *first;

  /* Loop forever, dispatching items from the task queue. */
  if (pthread_mutex_lock(&taskq_mutex) != 0) {
    fprintf(stderr, "mutex lock failed in bp_timer_mainloop\n");
    exit(1);
  }

  while(1) {
    if (taskq_len > 0) {   /* assumes only add to queue */
      /*      printf("(timerd) len %d\n", taskq_len); */
      /* There is stuff on the queue.  Let's look at the first element
	   and see if it is time to dispatch it.  If not, then go back to
	   sleep until it is. */

      gettimeofday(&now, NULL);
      first = (taskq_el *) taskq->data;
      if (tv_timecmp(now, first->ttime) >= 0) {
	/* the time has come to dispatch this puppy */
	if (ll_delfirst(&taskq, NULL) == 0) {
	  fprintf(stderr, "ll_delfirst failed in bp_timer_mainloop.\n");
	  exit(1);
	}
	taskq_len--;
	if (pthread_mutex_unlock(&taskq_mutex) < 0) {
	  fprintf(stderr, "mutex unlock failed in bp_timer_mainloop\n");
	  exit(1);
	}
	bp_dispatch_task(first);
	if (pthread_mutex_lock(&taskq_mutex) < 0) {
	  fprintf(stderr, "mutex lock2 failed in bp_timer_mainloop\n");
	  exit(1);
	}

	/* We'll not do a cond_timedwait;  instead we'll roll back to the
           top of the loop and check for more stuff to do. */

      } else {
	/* Not time to dispatch, go back to sleep until it is */
	sleeptime.tv_sec = first->ttime.tv_sec;
	sleeptime.tv_nsec = 1000L*(first->ttime.tv_usec);
	pthread_cond_timedwait(&taskq_cond, &taskq_mutex, &sleeptime);
      }
    } else {
      /* Nothing on the queue.  Let's sleep for 5 seconds. */
      gettimeofday(&now, NULL);
      sleeptime.tv_sec = now.tv_sec + 5L;
      sleeptime.tv_nsec = 0L;
/*      fprintf(stdout, "(timerd) nothing on task queue. sleeping for 5 more\n");*/
      pthread_cond_timedwait(&taskq_cond, &taskq_mutex, &sleeptime);
    }
  }  /* end while(1) */
}

/** The following two local functions dispatch task queue elements **/
void *bp_runtask(void *arg)
{
  taskq_el        *task = (taskq_el *) arg;

  /* This is running inside a new thread */
  num_threads_running++;
  task->tproc(task->tid, task->tproc_arg);
  num_threads_running--;
  free(task);
  pthread_exit((void *) 0);
  return (void *) 0;
}
void bp_dispatch_task(taskq_el *task)
{
  pthread_t        t;
  pthread_attr_t   attr;
  int              res;

  /* This should start a new thread for the task */
  pthread_attr_init(&attr);
  res = pthread_create(&t, &attr, bp_runtask, (void *) task);
  if (res != 0) {
    fprintf(stderr, "pthread_create failed in bp_dispatch_task\n");
    exit(1);
  }
  pthread_detach(t);
  pthread_attr_destroy(&attr);
}

/* Convenience wrapper for the timeval comparison utility procedure */
int   bp_timevalptr_cmp(void *tvp1, void *tvp2)
{
  taskq_el *t1, *t2;

  t1 = (taskq_el *) tvp1;
  t2 = (taskq_el *) tvp2;

  if ( (t1 == NULL) || (t2 == NULL)){
    fprintf(stderr, "NULLage in bp_timevalptr_cmp\n");
    return 0;
  }

  return tv_timecmp(t1->ttime, t2->ttime);
}
