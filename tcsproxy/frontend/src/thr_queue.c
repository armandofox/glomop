/*
 *  thr_queue.c
 *  Cache of running threads that are spin-blocked waiting on a
 *  condition.  When we need a new thread to do some work, we select one
 *  by signaling its condition variable.
 */

#include "config_tr.h"
#include "thr_cntl.h"
#include "debug.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

static pthread_t        *threadids; /* array of thread id's */
static pthread_t        threadid_mon;
static task_t *         *task_queue;
pthread_key_t           index_key;

/*
 *  Visible from other files
 */
int                     tasks_done;
Monitor                 *gMon = NULL;
Inst                    *thread_inst = NULL;
int                     nthreads;

#define DEFAULT_CONDATTR (const pthread_condattr_t *)NULL
#define DEFAULT_MTXATTR (const pthread_mutexattr_t *)NULL

#define QUEUE_EMPTY() (qhead == qtail)
#define QUEUE_FULL()  ((qtail+1) % nthreads == qhead)

/*
 *  Queue head and queue tail protected by mutex
 */
static pthread_mutex_t   mtx_queue;
static pthread_cond_t    cond_queue;
static int               qhead, qtail;

/*
 *  private forward declarations
 */

static void *   thread_go_proc(void *thrindex_cast_to_void);
static void     thread_cleanup(void *dummy_arg);
static void     thread_cleanup_sighnd(int sig);
static task_t * get_task(int thrindex);
/*
 *  make_threads
 *  Args: none
 *  Returns: 0 success, otherwise error code from errno.h
 *  Description:
 *    Create a condition variable, a bunch of threads, a task queue
 *  (circular FIFO) protected by a mutex.
 *  The dispatcher will add a task to the queue under mutex and signal
 *  the queue's condition variable.  A thread will wake up and remove work
 *  from the queue, and signal a different condition variable to put its
 *  result on the result queue.
 */

int
make_threads(int nth, int report_interval)
{
  int i;
  pthread_attr_t default_thread_attr;
  extern void *thread_mon_proc(void *arg);
  /*pthread_t max_index=0;
    int *tmp_index_array;*/

  tasks_done = nth;
  /*
   *  Useful log info
   */

  nthreads = nth;
#ifdef PTHREAD_THREADS_MAX
  if (nthreads > PTHREAD_THREADS_MAX) {
    MON_SEND_3(MON_ERR,"Warning: num threads pinned to %d from %d\n",
               PTHREAD_THREADS_MAX, nth);
    nthreads = PTHREAD_THREADS_MAX;
  }
#endif /* ifdef PTHREAD_THREADS_MAX */

  threadids = SAFEMALLOC(nthreads, pthread_t);
  task_queue = SAFEMALLOC(nthreads, task_t *);
  thread_inst = SAFEMALLOC(nthreads, Inst);
  inst_init();                  /* initialize the instrumentation buffers.
                                   Perhaps sadly, both thread_inst and nthreads
                                   are declared extern there. */
            

  /*
   *  create the queue.
   *  Queue invariants:
   *  - qhead is index of slot containing next task to be done
   *  - qtail is index of slot beyond which there are no more tasks
   *  - if qhead==qtail, there are no tasks to be done.
   *  - if (qhead+1)%qsize==qtail, the queue is full; a task must be
   *  removed before one can be added.
   */

  qhead = qtail = 0;

  THR_OP("attr init", pthread_attr_init(&default_thread_attr));
  THR_OP("attr set",
         pthread_attr_setscope(&default_thread_attr, PTHREAD_SCOPE_SYSTEM));
  THR_OP("condinit queue", pthread_cond_init(&cond_queue, DEFAULT_CONDATTR));
  THR_OP("mtxinit queue", pthread_mutex_init(&mtx_queue, DEFAULT_MTXATTR));
  THR_OP("index key", pthread_key_create(&index_key, NULL));

  for (i=0; i < nthreads; i++) {
    THR_OP("Thread create",
           pthread_create(&threadids[i], &default_thread_attr,
           thread_go_proc,
           (void *)i));
    /*if (threadids[i] > max_index) max_index = threadids[i];*/
  }
  /*tmp_index_array = SAFEMALLOC(((unsigned)max_index)+1, int);
  for (i=0; i < nthreads; i++) {
    tmp_index_array[(unsigned)threadids[i]] = i;
  }
  index_array__ = tmp_index_array;*/

  proxy_debug_3(DBG_THREAD, "Created %d threads", (void *)nthreads);
  /*
   *  Create the monitoring thread
   */

  if (report_interval) {
    THR_OP("set detached for monitor thread",
           pthread_attr_setdetachstate(&default_thread_attr,
                                       PTHREAD_CREATE_DETACHED));
    THR_OP("create monitor thread",
           pthread_create(&threadid_mon, &default_thread_attr,
                          thread_mon_proc, (void *)report_interval));
    proxy_errlog_2("Statistics reporting every %d seconds", report_interval);
  }
  return 0;
}

/*
 *  thread_go_proc is the entry point for all threads.  A thread will
 *  enter this routine, spin on a condition variable which is signaled
 *  when there's work for the thread to do, wake up and do its work,
 *  then reblock on the condition variable.  The arg is the index (0 to
 *  nthreads-1) of this worker thread.
 */

static void *
thread_go_proc(void *arg)
{
  int thrindex = (int)arg;
  task_t *curr_task;

  /* initialize the thread id for this thread */
  pthread_setspecific(index_key, (void*)thrindex);
					 
#ifdef PTHREAD_CANCEL_ENABLE
  THR_OP("make cancellable",
         pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
#endif
#ifdef PTHREAD_CANCEL_ASYNCHRONOUS
  THR_OP("async cancellable",
         pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL));
#endif

  (void)signal(SIGPIPE, SIG_IGN);
  (void)signal(SIGTERM, thread_cleanup_sighnd);
  while (1) {
    
    /* wait for a task to become available */
    
    INST_set_thread_state(thrindex, THR_IDLE);
    curr_task = get_task(thrindex);

    INST_set_thread_state(thrindex, THR_ACCEPTED);
    proxy_debug_4(DBG_THREAD, "Thread %d working on task %lu...",
                  thrindex, TASK_ID(curr_task));

    /* to work on a task, call its go proc. */

    pthread_cleanup_push(thread_cleanup, TASK_DATA(curr_task));
    CALL_TASK_GO_PROC(curr_task);
    pthread_cleanup_pop(0);

    /*
     *  Done working, requeue ourselves after freeing task structure
     */

#if 0
    if ((UINT32)(curr_task->task_result_ptr)) {
      /* more work to be done....requeue for next stage */
      dispatch(curr_task);
    } else {
      release_task(curr_task);
      /*    curr_task->task_result_ptr = (void *)(curr_task->task_id); */
    }
#else
    release_task(curr_task);
#endif
    proxy_debug_3(DBG_THREAD, "Thread %d rejoining pool", thrindex);
  }
  /* should never get here */
  /*NOTREACHED*/
  return ((void *)0);
}

/*
 *  thread_cleanup
 *
 *  Called when the proxy is killed or shutdown: each thread that is
 *  working will try to shutdown its socket in an orderly way.  Note
 *  that some of the shutdown's may be spurious, but who cares, we are
 *  exiting anyway.  The arg is a socket filedescriptor number.
 */

void
thread_cleanup_sighnd(int sig)
{
  extern void proxy_shutdown(int sig);
  thread_cleanup((void *)0);
  proxy_shutdown(sig);
}

void
thread_cleanup(void *arg)
{
  if (arg) {
    shutdown((int)arg,2);
    close((int)arg);
  }
}

/*
 *  get_task
 *  Under mutex, check the queue for work; if no work, sleep and wait
 *  for dispatcher to signal condition variable indicating that there is
 *  new work.
 *  When work is found, move the qhead under mutex to dequeue the task,
 *  and return the pointer to the new task structure.
 */

static task_t *
get_task(int thrindex)
{
  task_t *task = NULL;

  THR_OP("lock to check queue", pthread_mutex_lock(&mtx_queue));
  tasks_done--;
  while (QUEUE_EMPTY()) {
    /* no work to do */
    proxy_debug_3(DBG_THREAD, "Thr %d Waiting for work", thrindex);
    THR_OP("waiting for work", pthread_cond_wait(&cond_queue, &mtx_queue));
  }
  assert(! QUEUE_EMPTY());
  tasks_done++;
  task = task_queue[qhead++];
  qhead %= nthreads;
  /*
   *  Signal condition variable in case other threads want to grab it,
   *  or in case dispatcher is waiting for a queue slot.
   */
  THR_OP("signal queue", pthread_cond_signal(&cond_queue));
  THR_OP("unlock queue", pthread_mutex_unlock(&mtx_queue));
  assert(task != (task_t *)NULL);
  task->thrindex = thrindex;
  return(task);
}

/*
 *  dispatch
 *  Args: task ID
 *  Returns: 0 success, otherwise errno
 *  Thread-safe: yes
 *  Description:
 *    Put a new task in the work queue, and signal an available thread
 *    to pick up the task.  
 *  If no threads are available, wait for one to become available (it
 *  will signal the condition variable protected by the same mutex that
 *  protects the threads-available counter).
 *    When the thread completes or exits, it will free the task structure.
 */

int
dispatch(task_t *task_state)
{
  /*
   *  Should periodically check the value of did_stall to determine how
   *  many consecutive thread-dispatches had to wait for a thread to be
   *  available; when it exceeds some threshold, spawn a new thread.
   */

  THR_OP("dispatcher lock queue", pthread_mutex_lock(&mtx_queue));
  while (QUEUE_FULL()) {
    /* no queue slots available! wait for a thread to complete */
    proxy_debug_2(DBG_THREAD, "dispatcher waiting for queueslot");
    THR_OP("dispatcher wait for q", pthread_cond_wait(&cond_queue, &mtx_queue));
  }
  assert(! QUEUE_FULL());
  assert(task_state != (task_t *)NULL);
  /*
   *  put the task in the queue and signal any idling threads
   */

  task_queue[qtail++] = task_state;
  qtail %= nthreads;
  
  /* signal a thread - somebody will wake up to do the work. */
  proxy_debug_3(DBG_THREAD, "task %lu queued, signaling a thread...\n",
                task_state->task_id);
  THR_OP("signal wakeup", pthread_cond_signal(&cond_queue));
  THR_OP("Unlock queue", pthread_mutex_unlock(&mtx_queue));
  return (0);
}

