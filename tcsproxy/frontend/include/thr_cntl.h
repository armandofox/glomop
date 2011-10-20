/*
 *  Declarations for multithreading control routines
 */

#ifndef _THR_CNTL_H
#define _THR_CNTL_H
#include  <sys/time.h>
/*
 *  Structure used for synchronization of worker threads
 */

typedef void *TaskArg;
typedef void *ThrFunc(void *);
typedef ThrFunc *ThrFuncPtr;

typedef struct _task_t {
  UINT32 task_age;              /* how many stages we've gone thru */
  TaskArg task_data;
  UINT32 task_id;
  UINT16 task_priority;
  int thrindex;
  UINT32 task_parent;
  UINT32 task_child_index;
  char *task_metadata;
  ThrFuncPtr task_exc_handler;
  ThrFuncPtr task_go_proc;
} task_t, *QUEUE_PTR;

typedef void *GoProc(task_t *);

#define TASK_GO_PROC(t)         (t)->task_go_proc
#define TASK_EXC_HANDLER(t)     (t)->task_exc_handler
#define TASK_DATA(t)            (t)->task_data
#define TASK_ID(t)              (t)->task_id
#define TASK_METADATA(t)        (t)->task_metadata
#define TASK_PARENT(t)          (t)->task_parent
#define TASK_CHILD_INDEX(t)     (t)->task_child_index
#define TASK_THRINDEX(t)        (t)->thrindex
#define SET_TASK_GO_PROC(t,p)   (t)->task_go_proc = (p)
#define SET_TASK_EXC_HANDLER(t,p) (t)->task_exc_handler = (p)
#define SET_TASK_DATA(t,p)      (t)->task_data = (TaskArg)(p)
#define SET_TASK_ID(t,p)        (t)->task_id = (UINT32)(p)
#define SET_TASK_METADATA(t,p)  (t)->task_metadata = (void *)(p)
#define SET_TASK_PARENT(t,p)    (t)->task_parent = (UINT32)(p)
#define SET_TASK_CHILD_INDEX(t,p) (t)->task_child_index = (UINT32)(p)
#define CALL_TASK_GO_PROC(t)    assert(TASK_GO_PROC(t) != (ThrFuncPtr)NULL); \
                                (*((t)->task_go_proc))(t);

/*
 *  Thread state instrumentation
 */

typedef enum { 
  THR_IDLE=0, THR_ACCEPTED, THR_CACHE, THR_DISTILLER, THR_WRITEBACK, 
  THR_DISTILLERSEND, THR_DISTILLERWAIT, THREAD_STATE_MAX
} thread_state_t;
/*
 *  end-to-end latency instrumentation.  Each thread maintains a pair of
 *  buffers of timestamp information ("milestones") and on every N'th  request
 *  (N is parameterizable) writes into alternating buffers.  The
 *  m_readable field indicates that this buffer contains a complete set
 *  of milestones (i.e. timestamps for each point in the thread's life);
 *  the invariant is that at most one buffer from each pair will have
 *  this field set at any time.
 *
 *  The monitoring thread periodically runs through all buffers that
 *  have the m_readable field set, and aggregates the information in
 *  those buffers for reporting running average and variance.   Since
 *  the monitoring thread does not read the buffers atomically, there is
 *  still a race condition where the monitoring thread reads
 *  m_readable, the reporting thread turns off m_readable, and the
 *  monitor starts reading buffered data even as the reporting thread is
 *  overwriting it; but this is likely to be rare, and well worth the
 *  cost of avoiding another critical section and associated mutices.
 */

struct inst_milestones {
  struct timeval m_arrival;
  struct timeval m_headersdone;
  struct timeval m_cachestart;
  struct timeval m_cachedone;
  struct timeval m_distillstart;
  struct timeval m_wbstart;
  struct timeval m_wbdone;
  UINT32 inputsize;
};

typedef struct {
  struct inst_milestones ms[2];
  int readable[2];               /* is each buffer ready to read? */
  int which_buffer;
  UINT32 last_report;
  thread_state_t state;
} Inst;

extern Inst *thread_inst;
#ifdef INST
#define INST_timestamp(ndx,what) \
 gettimeofday(&thread_inst[(ndx)].ms[thread_inst[(ndx)].which_buffer].what,NULL)
#define INST_set_size(ndx,size)  \
 thread_inst[(ndx)].ms[thread_inst[(ndx)].which_buffer].inputsize = (size)
#else
#define INST_timestamp(x,y)
#define INST_set_size(x,y)
#endif
/*
 *  Function prototypes for worker-thread interfaces
 */

int make_threads(int nthreads, int report_interval);
int reap_threads(void);
int dispatch(task_t *newtask);
/*
 *  Instrumentation functions
 */
void inst_init(void);
void INST_set_thread_state(int thrindex, thread_state_t state);
void INST_begin_timestamp(int thrindex);
void INST_end_timestamp(int thrindex);
void INST_start_info_munging();
void INST_munge_info(int thrindex, struct inst_milestones *milestone);
void INST_end_info_munging();

#ifdef __GNUC__

static int _res  __attribute__ ((unused)) = 0;
#define THR_OP(what,func) \
  if ((_res=(func))) { \
    MON_SEND_4(MON_ERR, "[%lu]%s: %s\n", (UINT32)pthread_self(), (what), strerror(_res)); }

#else /* not __GNUC__ */

#define THR_OP(what,func) { int _res=(func); if (_res) { \
  MON_SEND_4(MON_ERR, "[%lu]%s: %s\n", (UINT32)pthread_self(), (what), strerror(_res)); }}

#endif /* __GNUC__ */

#define new_task(t)      (*t) = SAFEMALLOC(1,task_t)
#define release_task(t) FREE(t)

#endif /* ifndef _THR_CNTL_H */
