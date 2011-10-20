/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "config_tr.h"
#ifdef HAVE_PTHREAD_H
#include "pthread.h"
#endif

#include "http_task.h"

int numthreads = 0;
extern int numconnecting;
pthread_mutex_t numt_mutex;

int   port;
char *ip_addr;
FILE *outfile = stdout;

typedef struct data_st {
  char data[2000];
  int  len;
  struct data_st *next;
} udpdgm;

udpdgm *heap_o_dgm = NULL;

#define HEAP_ALLOCSIZE 500

char *hname, *sname;

udpdgm *get_next_dgm(void);
void    release_dgm(udpdgm *releaseme);
void   *dispatch_http(void *me);

int main(int argc, char **argv)
{
  struct sockaddr_in local_addr;
  int                local_fd;
  pthread_t t;
  pthread_attr_t attr;
  int       res;

  if (argc != 3) {
    fprintf(stderr, "usage: hip_engine target_IP target_port\n");
    exit(1);
  }
  if (sscanf(*(argv+2), "%d", &port) != 1) {
    fprintf(stderr, "usage: bp_engine target_IP target_port numpersec [of]\n");
    exit(1);
  }

  hname = ip_addr = *(argv+1);
  sname = *(argv+2);

  if (initialize_http_tasks("UC Berkeley Trace Playback Engine (please contact gribble@cs.berkeley.edu for inquiries regarding this access)",
			    "Steven Gribble, http://www.cs.berkeley.edu/~gribble, gribble@cs.berkeley.edu")) {
    fprintf(stderr, "http_tasks subsystem init failed\n");
    exit(1);
  }

  if (pthread_mutex_init(&numt_mutex, NULL) != 0)
    exit(1);

  make_inetaddr(NULL, "3123", &local_addr);

  local_fd = socket(AF_INET, SOCK_DGRAM, protonumber("udp"));
  bind(local_fd, (struct sockaddr *) &local_addr, sizeof(local_addr));

  pthread_attr_init(&attr);

  while(1) {
    udpdgm *nextdgm;
    int     retlen;

    while(numthreads > 400)
      sleep(1);
   
    nextdgm = get_next_dgm();
    retlen = recv(local_fd, nextdgm->data, 1990, 0);
    if (retlen == 0) {
      release_dgm(nextdgm);
      continue;
    }
    if (retlen < 0) {
      if (errno == EINTR) {
	release_dgm(nextdgm);
	continue;
      } else {
	fprintf(stderr, "recv failed!!!!!!!!!!!!!!!!!!!!!!!!!!!!.\n");
	perror("reason: ");
	exit(1);
      }
    } else {
      /* Dispatch this puppy to a thread */
      nextdgm->data[retlen] = '\0';
      nextdgm->len = retlen;
      if (pthread_mutex_lock(&numt_mutex) < 0)
	exit(1);
      numthreads++;
      if (pthread_mutex_unlock(&numt_mutex) < 0)
	exit(1);
      fprintf(stderr, "num threads: %d\n", numthreads);
      res = pthread_create(&t, &attr, dispatch_http, (void *) nextdgm);
      pthread_detach(t);
    }
  }
}

udpdgm *get_next_dgm(void)
{
  int i;
  udpdgm *retval;

  if (heap_o_dgm == NULL) {
    heap_o_dgm = (udpdgm *) calloc(HEAP_ALLOCSIZE, sizeof(udpdgm));
    if (heap_o_dgm == NULL) {
      fprintf(stderr, "out of memory in get_next_gdm\n");
      exit(1);
    }
    for (i=0; i<HEAP_ALLOCSIZE; i++) {
      if (i != HEAP_ALLOCSIZE-1)
	heap_o_dgm[i].next = &(heap_o_dgm[i+1]);
      else
	heap_o_dgm[i].next = NULL;
    }
  }
  retval = heap_o_dgm;
  heap_o_dgm = heap_o_dgm->next;
  retval->next = NULL;
  return retval;
}

void    release_dgm(udpdgm *releaseme)
{
  releaseme->next = heap_o_dgm;
  heap_o_dgm = releaseme;
}

void    *dispatch_http(void *me)
{
  udpdgm *tdgm = (udpdgm *) me;
  int     dest_fd;
  char    inbuffer[4000], *tok, *tok2;

  if (me == NULL)
    return NULL;

  tok = strtok(tdgm->data, " ");
  if (tok == NULL) {
    release_dgm(me);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    return NULL;
  }
  tok2 = strtok(NULL, " ");
  if (tok2 == NULL) {
    release_dgm(me);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    return NULL;
  }
  sprintf(inbuffer, "%s http://%s", tok2, tok);
  tok = strtok(NULL, "");
  if (tok == NULL) {
    release_dgm(me);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    return NULL;
  }
  strcat(inbuffer, tok);

  dest_fd = sconnect(hname, sname);
  if (dest_fd < 0) {
    fprintf(stderr, "Connect failed!\n");
    perror("reason");
    release_dgm(me);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    return NULL;
  }

  fprintf(stderr, "%s", inbuffer);
  if (correct_write(dest_fd, inbuffer, strlen(inbuffer)) <= 0) {
    fprintf(stderr, "write failed!\n");
    perror("reason");
    release_dgm(me);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    close(dest_fd);
    return NULL;
  }

  while (correct_read(dest_fd, inbuffer, 3500) > 0)
    ;
  if (pthread_mutex_lock(&numt_mutex) < 0)
    exit(1);
  numthreads--;
  if (pthread_mutex_unlock(&numt_mutex) < 0)
    exit(1);
  release_dgm(me);
  close(dest_fd);
  return NULL;
}
