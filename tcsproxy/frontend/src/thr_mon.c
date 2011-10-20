/*
 *  thr_mon.c: Functions for instrumentation and thread monitoring.
 */

/*
 *  Thread queue monitoring function.  Periodically wakes up and reports
 *  some thread queue statistics via the reporting interface.
 *
 *  ARGS:  number specifying how many seconds between reports.  If
 *  negative, specifies (negative) reciprocal of this value, i.e. -4
 *  means 4 reports per second, but +2 means a report every 2 seconds.
 *  RETURNS: never
 *  REENTRANT: no (monitors global variables related to thread queue)
 */

#include <unistd.h>
#include "config_tr.h"
#include "thr_cntl.h"
#include "userpref.h"
#include "debug.h"

static void compute_ewma_reqs_sec(int report_interval, int asymptote);
static void compute_thread_state_deltas(int asymptote);

extern int nthreads;
extern Monitor *gMon;
extern Inst *thread_inst;

#define NUM_ASYMPTOTE 5
#define LOADAVG_WINDOWSIZE 8
#define TIME_DIF_TO_USEC(t2,t1) \
    (((t1).tv_sec-(t2).tv_sec)*1000000.0+((t1).tv_usec-(t2).tv_usec))


void *
thread_mon_proc(void *arg)
{
  int report_interval = (int) arg;

  int asymptote=0; 
  unsigned char report_this_time = 0;

  while (1) {

    report_this_time ^= 1;      /* toggle this flag */

    if (report_interval >= 1)
      sleep(report_interval);
    else
      usleep(-1000000/report_interval);

    /*    compute_loadavg(); */
    compute_thread_state_deltas(asymptote);

    if (report_this_time) {
      compute_ewma_reqs_sec(report_interval, asymptote);
    }

    /* next time, we'll do the next set. */
    asymptote = (asymptote+NUM_ASYMPTOTE) % nthreads;

  } /* end while(1) */
}

/*
 *  Set the run state of a thread.  Could optionally insert
 *  instrumentation here to record each state change, but for now we
 *  simply allow the thread_mon_proc function to periodically read and
 *  dump out the entire state array.
 *  Not MT-safe, but any given thread will be reading/writing only its
 *  own array entry.
 */

void
INST_set_thread_state(int thrindex, thread_state_t state)
{
  assert(thrindex >= 0 && thrindex < nthreads);
  thread_inst[thrindex].state = state;
}

/*
 *  Get ready to start collecting timestamp info.  If one of the buffers
 *  is marked "not readable", use it to collect info; otherwise pick one
 *  to use and mark it unreadable.
 *  Not MT-safe, but any given thread will be reading/writing only its
 *  own array entry.
 */

void
INST_begin_timestamp(int thrindex)
{
  int buf = thread_inst[thrindex].which_buffer & 1;

  if (thread_inst[thrindex].readable[buf]) {
    /* pick the other buffer */
    buf ^= 1;
    thread_inst[thrindex].readable[buf] = 0;
  }
  thread_inst[thrindex].which_buffer = buf;
  thread_inst[thrindex].last_report++;
  memset(&thread_inst[thrindex].ms[buf], 0, sizeof(struct inst_milestones));
}

/*
 *  Indicate a set of timestamps has been recorded: set its readable
 *  field to 1.  
 *  Not MT-safe, but any given thread will be reading/writing only its
 *  own array entry.
 */
void
INST_end_timestamp(int thrindex)
{
  int buf = thread_inst[thrindex].which_buffer & 1;

  Inst *i = &thread_inst[thrindex];
  
  i->readable[buf] = 1;
  i->which_buffer = buf ^ 1;

}

/*
 *  Initialize instrumentation
 */

void
inst_init(void)
{
  memset(thread_inst, 0, nthreads*sizeof(Inst));
}

#if 0
static void
compute_loadavg(void)
{
  int bogus;
  int i,j,thridx;
  Inst *in;
  struct inst_milestones avg_ms;
  struct inst_milestones local_ms[LOADAVG_WINDOWSIZE];
  double inputsize;
  double e2eavg = 0.0;
  UINT32 moving_e2eavg = 0;

  static UINT32 old_e2eavg = 0;
  static double w = 0.85;
  static int changed = 0;
  static int wptr = 0;

  /*
   *  send the "performance" information.  For each thread, see whether the
   * thread has something to report  (the "buffer readable" flag is set for 
   * one of the thread's two reporting buffers).  If so, aggregate the
   * information, and turn off the readable flag (so we won't report stale
   * information again next time).
   */

  bogus = 0;

  if (changed == 0) {           /* first time through a new aggregation */
    memset(&avg_ms, 0, sizeof(avg_ms));
  }
  for (i=0; i<nthreads; i++) {
    /*    int thridx = (asymptote+i) % nthreads; */
    thridx = i;
    in = &thread_inst[thridx];

    /*
     *  Check the readable flag; if set, make a copy of the data, and test
     *  the readable flag again in case it changed under us.
     */
    j = in->which_buffer;
    if ((in->readable[j])
        || (in->readable[j^=1])) {
      local_ms = in->ms[j];
      if (! in->readable[j]) {
        /* BUG::really want test-and-set(in->readable[j]) */
        MON_SEND(MON_LOGGING,
                 "Measurement suppressed: potential race condition\n");
      } else if (local_ms.inputsize == 0) {
        bogus++;
        in->last_report = 0;
      } else {
        /* hasn't changed since we started reading, and appears to be valid
         *  measurement. 
         * note we set readable to 0, which might (race condition)
         *  invalidate a 
         * good measurement currently being taken, but certainly prevents us
         * from reporting the same measurement twice.
         */
        in->readable[j] = 0;
        inputsize = (double)local_ms.inputsize;
        changed++;
        if (in->last_report > 1) {
          MON_SEND_2(MON_LOGGING,
                     "WARNING! thread %d updated more than once\n", i);
          /*                  i, in->last_report); */
        }
        in->last_report = 0;

#if 0
        avg_ms.m_headersdone.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_arrival, local_ms.m_headersdone)
           / inputsize);
        avg_ms.m_cachestart.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_headersdone, local_ms.m_cachestart)
           / inputsize);
        avg_ms.m_cachedone.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_cachestart, local_ms.m_cachedone)
           / inputsize);
        avg_ms.m_distillstart.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_cachedone, local_ms.m_distillstart)
           / inputsize);
        avg_ms.m_wbstart.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_distillstart, local_ms.m_wbstart)
           / inputsize);
        avg_ms.m_wbdone.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_wbstart, local_ms.m_wbdone)
           / inputsize);
#else
        avg_msg.m_headersdone.tv_usec +=
          (TIME_DIF_TO_USEC(local_ms.m_arrival, local_ms.m_wbdone)
           / inputsize);
#endif
      }
    }
  }

  if (changed > 10) {
    /* compute the averages for this time period. */
#if 0
    e2eavg = 0.0;
    e2eavg += (avg_ms.m_headersdone.tv_usec / changed);
    e2eavg += (avg_ms.m_cachestart.tv_usec / changed);
    e2eavg += (avg_ms.m_cachedone.tv_usec / changed);
    e2eavg += (avg_ms.m_distillstart.tv_usec / changed);
    e2eavg += (avg_ms.m_wbstart.tv_usec / changed);
    e2eavg += (avg_ms.m_wbdone.tv_usec / changed);
#else
    e2eavg = (avg_ms.m_headersdone.tv_usec / changed);
#endif
    /* use EWMA formula */

    moving_e2eavg = (w * e2eavg) + ((1-w) * old_e2eavg) + 0.5;

    /* 0.5 added to get rounding rather than truncation */

    if (moving_e2eavg != old_e2eavg) {
      MON_STATS("End-to-end latency EWMA msec", moving_e2eavg/1000.0);
      MON_STATS("Threads successfully polled", changed);
      MON_STATS("Threads reporting bogusly", bogus);
    }
    changed = 0;
    old_e2eavg = moving_e2eavg;
  }
}
#endif

static void
compute_ewma_reqs_sec(int report_interval, int asymptote)
{
  /* for computing EWMA of requests/sec */
  static UINT32 old_n_httpreqs = 0;
  static int old_reqs_sec = 0;
  static double w = 0.85;
  extern UINT32 n_httpreqs;
  int reqs_sec;
  int reqs_sec_this_period;

  /*
   *  compute Exponentially Weighted Moving Average (EWMA) of requests
   *  per second using:
   *  Average(t) = w * Measurement(t) + (1-w) * Average(t-1)
   *
   *  We use delta_t = every other report.
   */

  if (report_interval >= 1)
    reqs_sec_this_period = (n_httpreqs - old_n_httpreqs) /
      (report_interval<<1);
  else
    reqs_sec_this_period = ((n_httpreqs - old_n_httpreqs) * 
			    -(report_interval>>1));

  reqs_sec = (w * reqs_sec_this_period) + ((1-w) * old_reqs_sec) + 0.5;
  /* note, add 0.5 to round rather than truncate.  */
  old_n_httpreqs = n_httpreqs;
  
  if (reqs_sec != old_reqs_sec) {
    MON_STATS("Requests/sec EWMA", reqs_sec);
  }
  old_reqs_sec = reqs_sec;
}

/*
 *  Compute the deltas in thread state since last time, and send them to the
 *  monitor.  Also send along thread state info for the next NUM_ASYMPTOTE
 *  threads in round-robin fashion ("asymptotically reliable updates").
 */
static void
compute_thread_state_deltas(int asymptote)
{
  static thread_state_t *old_thread_state = NULL;
  static char *msg = NULL;
  static char args[400];
  extern Inst *thread_inst;
  int i, j, len, changed;
  
  if (old_thread_state == NULL) {
    /* one-time initialization */
    old_thread_state = SAFEMALLOC(nthreads, thread_state_t);
    for (i=0; i<nthreads; i++)
      old_thread_state[i] = thread_inst[i].state;
    msg = SAFEMALLOC((nthreads + NUM_ASYMPTOTE)*8+1, char);
    sprintf(args, "Array %d %d", nthreads, THREAD_STATE_MAX);
  }

  /*
   *  Generate thread state "deltas" vector
   */

  len = changed = 0;
  for (i=0; i<nthreads; i++) {
    if (thread_inst[i].state != old_thread_state[i]) {
      len += sprintf(msg+len, "%d=%d,", i, thread_inst[i].state);
      old_thread_state[i] = thread_inst[i].state;
      changed = 1;
    }
  }

  /* 
   * send at least the asymptote information, even if no threads have 
   * changed state
   */
  for (i=0; i<NUM_ASYMPTOTE; i++) {
    j = (asymptote + i) % nthreads;
    len += sprintf(msg+len, "%d=%d,", j, thread_inst[j].state);
  }
    
  msg[len-1] = 0;
  MonitorClient_Send(gMon, "Relaxen und watchen das Blinkenlights", msg, args);
}


  
  /*
 * Stub functions: Yatin will add the real thing in a 
 * separate file
 */
void INST_start_info_munging()
{
}

void
INST_munge_info(int thrindex, struct inst_milestones *milestone)
{
}

void
INST_end_info_munging()
{
}
