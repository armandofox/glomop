/*
 *  This is the front end that listens for HTTP requests from the
 *  outside world.  It runs as a single thread.  When a request is
 *  received, it is placed on a work queue for other threads to grab.
 *  The queue length is periodically monitored to see if new threads
 *  should be spawned.
 */

#include "frontend.h"
#include "debug.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "utils.h"

/*
 *  Private declarations for this file
 */

static int gSock;
static pthread_t thr_http;

/*
 *  Forward declarations
 */

UINT32 n_httpreqs;
static ThrFunc http_eventloop_proc;
ThrFunc http_go_proc;

/*
 *  httpreq_init
 *  Initialize HTTPd-like front end by opening a socket for listening, and
 *  spawning a thread that will listen on that socket and farm out incoming
 *  requests by putting them on the thread work queue.
 *
 *  ARGS:
 *    i: port number to listen on
 *    i: amount by which to boost accept() thread's priority (0 = don't boost,
 *       -1=use default) 
 *  Returns: gm_True or gm_False
 *  Thread-safe: no
 */

gm_Bool
httpreq_init(int port, int boost_prio)
{
  char portnum[8];
  int i;
  pthread_attr_t att;
  struct sched_param sched;

  sprintf(portnum,"%d",port);
  gSock = slisten(portnum);

  if (gSock < 1) {
    /* failed to open listening socket */
    MON_SEND_2(MON_ERR,"Socket listen failed: %s\n", strerror(errno));
    return gm_False;
    /* NOTREACHED */
  }

  i = 1;
  if (setsockopt(gSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&i, sizeof(int))
      != 0) {
    MON_SEND_2(MON_ERR,"setsockopt SO_REUSEADDR: %s", strerror(errno));  
  }
  
  i = 0;
  if (setsockopt(gSock, SOL_SOCKET, SO_KEEPALIVE, (const char *)&i, sizeof(int))
      != 0) {
    MON_SEND_2(MON_ERR,"setsockopt SO_KEEPALIVE: %s", strerror(errno));
  }

  MON_SEND_2(MON_LOGGING,"(HTTP): listening on port %d\n", port);

  n_httpreqs = 1;

  /* spawn a local worker thread  */

  THR_OP("HTTP thread attrs init", pthread_attr_init(&att));
  THR_OP("HTTP set global scope",
         pthread_attr_setscope(&att, PTHREAD_SCOPE_SYSTEM));
  THR_OP("HTTP get sched params",
         pthread_attr_getschedparam(&att, &sched));

#ifdef _MIT_POSIX_THREADS
  sched.prio += boost_prio;
#else
  sched.sched_priority += boost_prio;
#endif

  THR_OP("HTTP boost prio",
         pthread_attr_setschedparam(&att, &sched));
  MON_SEND_2(MON_LOGGING,"Boosting HTTP accept() thread prio by %d\n",
	     boost_prio);
  THR_OP("Thread create HTTP",
         pthread_create(&thr_http, (pthread_attr_t *)&att,
                        http_eventloop_proc, (void *)NULL));
  
  proxy_debug_2(DBG_HTTP, "Spawned HTTP worker thread\n");
  return gm_True;

}

/*
 *  http_eventloop_proc
 *  This thread accepts connections (ostensibly from HTTP clients) and
 *  immediately puts the accepted connection on the work queue,specifying
 *  http_go_proc as the "continuation" procedure.  
 *
 *  REENTRANT: no - only one thread assumed to be in this eventloop
 *  RETURNS: never returns
 *  ARGS: None
 *
 */

static int newconn = 0;

static void *
http_eventloop_proc(void *arg)
{
  task_t *request;    
  
  proxy_debug_2(DBG_HTTP, "(HTTP): Accepting connections");

  while (1) {
    newconn = saccept(gSock);
    if (newconn == -1) {
      MON_SEND_2(MON_ERR,"(HTTP): Accept connection: %s", strerror(errno));
      continue;
    }
    /* Put the connection on the queue. */

    proxy_debug_3(DBG_HTTP, "(HTTP): Queueing connection %d", n_httpreqs);
    new_task(&request);
    SET_TASK_ID(request,n_httpreqs);
    n_httpreqs++;
    SET_TASK_GO_PROC(request, http_go_proc);
    /*
     *  Set parent, child_index, and metadata to null, to indicate this request
     *  is direct from client. 
     */
    SET_TASK_PARENT(request,0);
    SET_TASK_CHILD_INDEX(request,0);
    SET_TASK_METADATA(request,NULL);
    /*
     *  Set task_data to the file descriptor for this socket.  When a
     *  worker thread picks up this task, it will change task_data to a
     *  pointer to the Request struct describing this request.
     *  That structure includs a field for the file descriptor (among
     *  other thigns), but we can't fill in the structure here because
     *  the structure is managed per worker thread.
     */
    SET_TASK_DATA(request, newconn);
    assert(dispatch(request) == 0);
  }
}

