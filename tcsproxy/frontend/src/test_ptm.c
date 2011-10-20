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

#include "clib.h"
#include "icp.h"
#include "utils.h"

extern int errno;
extern int tasks_done;
static int gSock2;
static UINT32 nreqs2;
static pthread_t thr_http2;

pthread_t  harvest_httpreq_init(int port);
static void *harvest_http_eventloop_proc(void *arg);

void
Usage(char *progName)
{
  proxy_errlog_2("Usage: %s [-t <number-of-threads>] [-p <port-number>]\n"
		 "          [-h <harvest-config-file>] [-c <ptm-config-file>]"
		 "\n", progName);
  printf("Exiting inside Usage\n");
  exit(1);
}


void 
Abort(char *string)
{
  proxy_errlog_2("Aborting program: %s", string);
  printf("Exiting inside Abort\n");
  exit(1);
}



int main(int argc, char **argv)
{
  int port=5676;
  clib_response resp;
  int  nthr=600, optCh;
  char harvestConfig[256], ptmConfig[256];
  char     *options = "t:p:h:c:";

  strcpy(harvestConfig, "part.config");
  strcpy(ptmConfig, "../ptm/.gm_options.yatin");

  optind = 1;
  while ( (optCh = getopt(argc, argv, options))!=-1) {
    switch (optCh) {
    case 't':
      nthr = atoi(optarg);
      if (nthr <= 0) Usage(argv[0]);
      break;

    case 'p':
      port = atoi(optarg);
      if (port <= 0) Usage(argv[0]);
      break;

    case 'h':
      strcpy(harvestConfig, optarg);
      break;

    case 'c':
      strcpy(ptmConfig, optarg);
      break;

    case ':':
    case '?':
    default:
      Usage(argv[0]);
    }
  }

  if (InitializeDistillerCache(ptmConfig, "", 0)==gm_False) {
    Abort("Could not initialize the PTM");
  }
  fprintf(stderr, "Waiting for PTM to start up.\n");
  WaitForPTM();
  fprintf(stderr, "PTM found; proceeding...\n");

  if ((resp = Clib_Initialize(harvestConfig) != CLIB_OK)) {
    Abort("Could not initialize the harvest client library");
  }

  harvest_httpreq_init(port);
  (void)make_threads(nthr, 1);
  while(1) {
    printf("************ Sleeping\n");
    sleep(10);
  }

  printf("************ Returning from main\n");  
}

void *harvest_http_go_proc(void *arg)
{
  task_t   *t = (task_t *) arg;
  int       fd = (int)TASK_DATA(t), rv;
  unsigned  inlen, outlen;
  UINT32    distlen=0;
  C_DistillerType type;
  char     *inbuf=NULL, *outbuf=NULL, *distbuf=NULL;

  if (do_http_client_receive(fd, &inlen, &inbuf) != 0) {
    close(fd);
    return (void *) 0;
  }

  if (inlen <= 0) {
    close(fd);
    return (void *) 0;
  }

  if (Clib_Lowlevel_Op(inbuf, inlen, &outbuf, &outlen) != CLIB_OK) {
    if (inbuf)
      free(inbuf);
    if (outbuf)
      free(outbuf);
    close(fd);
    return (void *) 0;
  }

  strcpy(type.string, "all/passthru");
  if (Distill(&type, 0, 0, outbuf, outlen, 
	      (void**) &distbuf, &distlen) != distOk) {
    if (inbuf)
      free(inbuf);
    if (outbuf)
      free(outbuf);
    close(fd);
    return (void *) 0;    
  }

  if ((rv = correct_write(fd, distbuf, distlen)) != outlen) {
    if (inbuf)
      free(inbuf);
    if (outbuf)
      free(outbuf);
    FreeOutputBuffer(distbuf);
    close(fd);
    return (void *) 0;
  }

  if (inbuf)
    free(inbuf);
  if (outbuf)
    free(outbuf);
  FreeOutputBuffer(distbuf);
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
  param.sched_priority += 30;
  THR_OP("Set HTTP thread params",
	 pthread_attr_setschedparam(&attr, &param));
  THR_OP("Thread create HTTP",
         pthread_create(&thr_http2, (pthread_attr_t *)&attr,
                        harvest_http_eventloop_proc, (void *)NULL));
  
  proxy_debug_2(DBG_HTTP, "Spawned HTTP worker thread\n");
  return thr_http2;

}
