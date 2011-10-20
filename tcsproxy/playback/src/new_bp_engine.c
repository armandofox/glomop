/*
 *     Author: Steve Gribble
 *       Date: Feb 12th, 1997
 *       File: new_bp_engine.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/dirent.h>

#include "config_tr.h"
#ifdef HAVE_PTHREAD_H
#include "pthread.h"
#endif

#include "bp_timer.h"
#include "http_task.h"
#include "logparse.h"
#include "simclient.h"
#include "playback_sched.h"

#define MAX_DIR_ENTRIES 256
#define FILENAME_BUFSIZE 1024
#define NUM_PARALLEL_READS 250

typedef struct rf_st {
  char filename_buf[FILENAME_BUFSIZE];
  int  start_random;
  simclient_list *clist;
} readfile_thrproc_st;

void *readfile_thrproc(void *arg);

int main(int argc, char **argv)
{
  simclient_list clist;
  unsigned long int i, j, cur_client=0;
  struct timeval max_delay;
  pthread_t           ids[NUM_PARALLEL_READS];
  readfile_thrproc_st sts[NUM_PARALLEL_READS];
  pthread_attr_t      attr;
  void               *status;
  double              speedup;

  pthread_attr_init(&attr);
  if (argc <= 5) {
    fprintf(stderr, 
	    "usage: new_bp_engine host port max_delay speedup directory [directories]\n");
    exit(1);
  }

  /* Initialize all of the subsystems of our cool app */

  if (bp_timer_init()) {
    fprintf(stderr, "timer subsystem init failed\n");
    exit(1);
  }

  if (initialize_http_tasks("UC Berkeley Trace Playback Engine (please contact gribble@cs.berkeley.edu for inquiries regarding this access)",
			    "Steven Gribble, http://www.cs.berkeley.edu/~gribble, gribble@cs.berkeley.edu")) {
    fprintf(stderr, "http_tasks subsystem init failed\n");
    exit(1);
  }
  
  if (!init_list_o_clients(&clist, 6000)) {
    fprintf(stderr, "init_list_o_clients failed\n");
    exit(1);
  }

  if (sscanf(argv[3], "%lu", &(max_delay.tv_sec)) != 1) {
    fprintf(stderr, "Bogus max delay %s\n", argv[3]);
    exit(1);
  }
  max_delay.tv_usec = 0;
  set_dest_addr_and_port(argv[1], argv[2]);
  set_max_delay(max_delay);
  if (sscanf(argv[4], "%lf", &speedup) != 1) {
    fprintf(stderr, "Bogus speedup factor %f\n", speedup);
    exit(1);
  }
  set_speedup_factor(speedup);

  /* Now prep all of the client entry files */
  for (i=5; i<argc; i++) {
    DIR *thedir;
    struct dirent *direntp;

    thedir = opendir(argv[i]);
    if (thedir == NULL) {
      fprintf(stderr, "Couldn't open directory %s\n", argv[i]);
      continue;
    }
    while ((direntp = readdir(thedir)) != NULL) {
      if ( (strcmp(direntp->d_name, ".") != 0) &&
	   (strcmp(direntp->d_name, "..") != 0)) {
	
	sprintf(sts[cur_client].filename_buf, "%s/%s", 
		argv[i], direntp->d_name);
	sts[cur_client].start_random = 0;
	sts[cur_client].clist = &clist;
	fprintf(stderr, "Adding %s\n", sts[cur_client].filename_buf);
	pthread_create(&(ids[cur_client]), &attr, readfile_thrproc,
		       (void *) &(sts[cur_client]));
	cur_client++;

	if (cur_client == NUM_PARALLEL_READS) {
	  for (j=0; j<cur_client; j++) {
	    pthread_join(ids[j], &status);
	  }
	  cur_client = 0;
	}
      }
    }
    closedir(thedir);
  }
  for (j=0; j<cur_client; j++) {
    pthread_join(ids[j], &status);
  }

  while(1)
    sleep(500);
  exit(0);
}

void *readfile_thrproc(void *arg)
{
  readfile_thrproc_st *st = (readfile_thrproc_st *) arg;
  unsigned long int cid;

  cid = add_client_to_list(st->filename_buf, st->start_random, st->clist);
  initialize_scheduler(cid, st->clist);
  inject_client(cid, st->clist);

  return NULL;
}
