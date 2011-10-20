#include <config_tr.h>
#include <stdio.h>
#include <string.h>
#include <frontend.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <proxyinterface.h>

#include "config_tr.h"
#include "clib.h"
#include "icp.h"
#include "utils.h"
#include "thr_cntl.h"
#include "libmon.h"

extern int errno;
extern int tasks_done;
static int gSock2;
static UINT32 nreqs2;
static pthread_t thr_http2;

pthread_t  harvest_httpreq_init(int port);
static void *harvest_http_eventloop_proc(void *arg);

int main(int argc, char **argv)
{
  int port;
  clib_response resp;
  int  nthr;
  char filename[64];

  sprintf(filename, "part.config");
  if (argc == 1) {
    fprintf(stderr, "Usage: test_harvest num_threads [portnum [cfgfile]]\n");
    exit(1);
  }
  if (argc == 2)
    port = 5676;
  else if (argc > 2) {
    port = atoi(argv[2]);
    if (argc >= 4) {
      sprintf(filename, "%s", argv[3]);
    }
  }
  if ((resp = Clib_Initialize(filename, NULL) != CLIB_OK)) {
    exit(1);
  }
  if (sscanf(argv[1], "%d", &nthr) != 1) {
    proxy_errlog_1("Usage: test_harvest num_threads [portnum [configfile]]");
    exit(1);
  }
  harvest_httpreq_init(port);
  (void)make_threads(nthr, 1);
  while(1)
    sleep(10);
}

void *harvest_http_go_proc(void *arg)
{
  task_t   *t = (task_t *) arg;
  int       fd = (int)TASK_DATA(t), rv;
  unsigned  inlen, outlen;
  char     *inbuf=NULL, *outbuf=NULL;

  if (do_http_client_receive(fd, &inlen, &inbuf) != 0) {
    close(fd);
    return (void *) 0;
  }

  if (inlen <= 0) {
    close(fd);
    return (void *) 0;
  }
  INST_set_thread_state(t->thrindex, THR_CACHE);
  if (Clib_Lowlevel_Op(inbuf, inlen, &outbuf, &outlen) != CLIB_OK) {
    if (inbuf)
      free(inbuf);
    if (outbuf)
      free(outbuf);
    close(fd);
    return (void *) 0;
  }
  INST_set_thread_state(t->thrindex, THR_WRITEBACK);

  if ((rv = correct_write(fd, outbuf, outlen)) != outlen) {
    if (inbuf)
      free(inbuf);
    if (outbuf)
      free(outbuf);
    close(fd);
    return (void *) 0;
  }

  if (inbuf)
    free(inbuf);
  if (outbuf)
    free(outbuf);
  close(fd);
  return (void *) 0;
}

static void *harvest_http_eventloop_proc(void *arg)
{
  int newconn;
  task_t *request;    

  proxy_debug_2(DBG_HTTP, "Accepting connections");

  while (1) {
    newconn = saccept(gSock2);
    if (newconn == -1) {
      proxy_errlog_2("Accept connection: %s", strerror(errno));
      continue;
    }
    /* Put the connection on the queue. */

    new_task(&request);
    SET_TASK_ID(request,nreqs2);
    SET_TASK_GO_PROC(request, harvest_http_go_proc);
    SET_TASK_DATA(request, newconn);
    assert(dispatch(request) == 0);
    nreqs2++;
  }
}

pthread_t  harvest_httpreq_init(int port)
{
  char portnum[8];
  pthread_attr_t attr;
  struct sched_param param;
  
  nreqs2 = 0;
  sprintf(portnum,"%d",port);
  gSock2 = slisten(portnum);
  assert(gSock2 != 0);

#if 0
  /* BUG:: */
  proxy_debug_2(DBG_HTTP, "Setting up sock options");

  if (setsockopt(gSock2, SOL_SOCKET, SO_REUSEADDR, 
		 (const char *)1, sizeof(char)) != 0)
    proxy_errlog_2("setsockopt SO_REUSEADDR: %s", strerror(errno));  

  if (setsockopt(gSock2, SOL_SOCKET, SO_KEEPALIVE, (const char *)0, 
		 sizeof(char)) != 0)
    proxy_errlog_2("setsockopt SO_KEEPALIVE: %s", strerror(errno));
#endif

  
  proxy_debug_3(DBG_HTTP, "Listening on port %d\n", port);

  /* spawn a local worker thread  */
  THR_OP("Init attributes",
	 pthread_attr_init(&attr));
  THR_OP("Get HTTP thread params",
	 pthread_attr_getschedparam(&attr, &param));
#ifdef HAVE_PTHREAD_MIT_PTHREAD_ATTR_H
  param.prio += 30;
#else
  param.sched_priority += 30;
#endif
  THR_OP("Set HTTP thread params",
	 pthread_attr_setschedparam(&attr, &param));
  THR_OP("Thread create HTTP",
         pthread_create(&thr_http2, (pthread_attr_t *)&attr,
                        harvest_http_eventloop_proc, (void *)NULL));
  
  proxy_debug_2(DBG_HTTP, "Spawned HTTP worker thread\n");
  return thr_http2;

}
