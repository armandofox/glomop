/*
 * Author: Steve Gribble
 *   Date: Feb 24th, 1997
 *   File: playback_sched.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>

#include "logparse.h"
#include "bp_timer.h"
#include "http_task.h"
#include "playback_sched.h"
#include "utils.h"

void timerplayproc(int id, void *data);

int            fe_initialized = 0;
struct timeval start_firstentry;
struct timeval start_wallclock;
struct timeval tv_maxdelay;
struct sockaddr_in dest_inaddr;
double         speedup_factor;


typedef struct timer_st {
  simclient_list    *clist;
  lf_entry          *the_entry;
  unsigned long int  client_id;
  unsigned char      absolute_time;   /* if 1, preserve absolute times */
} timerplay;

void set_dest_addr_and_port(char *host, char *port)
{
  if (make_inetaddr(host, port, &dest_inaddr) == -1) {
    fprintf(stderr, "Couldn't make in_addr structure for %s %s\n",
	    host, port);
    exit(1);
  }
}

void set_max_delay(struct timeval maxdelay)
{
  tv_maxdelay = maxdelay;
}

void set_speedup_factor(double sf)
{
  speedup_factor = sf;
}
 
void initialize_scheduler(unsigned long int client_id, simclient_list *clist)
{
  lf_entry *the_entry;
  struct timeval start_fe;

  the_entry = get_client_entry(clist, client_id);
  lf_convert_order(the_entry);

  if (fe_initialized == 0) {
    fe_initialized = 1;
    start_firstentry.tv_sec = the_entry->crs;
    start_firstentry.tv_usec = the_entry->cru;
  } else {
    start_fe.tv_sec = the_entry->crs;
    start_fe.tv_usec = the_entry->cru;
    if (tv_timecmp(start_fe, start_firstentry) < 0)
      start_firstentry = start_fe;
  }
  gettimeofday(&start_wallclock, NULL);

  lf_convert_order(the_entry);
  free(the_entry->url);
  free(the_entry);
}

void inject_client(unsigned long int client_id, simclient_list *clist)
{
  timerplay *thetplay;
  lf_entry *the_entry;
  struct timeval offset, nowtime;

  thetplay = (timerplay *) malloc(sizeof(timerplay));
  if (thetplay == NULL) {
    fprintf(stderr, "Out of memory in inject_client\n");
    exit(1);
  }
  thetplay->clist = clist;
  thetplay->client_id = client_id;
  thetplay->absolute_time = 1;
  the_entry = thetplay->the_entry = get_client_entry(clist, client_id);
  
  lf_convert_order(the_entry);
  offset.tv_sec = the_entry->crs;
  offset.tv_usec = the_entry->cru;
  if (tv_timecmp(offset, start_firstentry) < 0)
      start_firstentry = offset;

  gettimeofday(&nowtime, NULL);
  nowtime = tv_timesub(nowtime, start_wallclock);
  offset = tv_timemul(tv_timesub(offset, start_firstentry), speedup_factor);
  if (tv_timecmp(offset, nowtime) > 0)
    offset = tv_timesub(offset, nowtime);
  else {
    offset.tv_sec = 0;
    offset.tv_usec = 0;
  }

  lf_convert_order(the_entry);

  /* Obey minimum time */
  if (tv_timecmp(offset, tv_maxdelay) > 0) {
    offset.tv_sec = lrand48() % tv_maxdelay.tv_sec;
    offset.tv_usec = lrand48() % 1000000;
  }

  fprintf(stderr, "Scheduling %s for %lu.%lu\n",
	  the_entry->url, offset.tv_sec, offset.tv_usec);
  bp_timer_add(RELATIVE, offset, timerplayproc, (void *) thetplay);
  bp_dispatch_signal();
  return;
}

void timerplayproc(int id, void *data)
{
  timerplay *timerdata = (timerplay *) data;
  http_task  t;
  lf_entry *lfntree;
  char      *retd = NULL;
  int        res, retl;

  /* to maintain exactly one entry for this client on the queue, inject
     the next right away */

  lfntree = timerdata->the_entry;
  inject_client(timerdata->client_id, timerdata->clist);

  t.url = (char *) lfntree->url;
  if ( (strcmp(t.url, "-") == 0) ||
       (strlen(t.url) < 5) ||
       (lfntree->sip == 0xFFFFFFFF) ||
       (lfntree->spt == 0xFFFF) ||
       !((strncmp(t.url, "GET ", 4) == 0) ||
        (strncmp(t.url, "POST ", 5) == 0) ||
        (strncmp(t.url, "HEAD ", 5) == 0) ||
        (strncmp(t.url, "PUT " , 4) == 0))
    ) {
    fprintf(stderr, "Ignoring bum url...%s\n", t.url);
    goto DONE_TIMERPLAY;
  }

  t.urllen = strlen(t.url);
  t.dst_host = lfntree->sip;
  t.dst_port = lfntree->spt;
  t.client_pragmas = lfntree->cprg;
  t.server_pragmas = lfntree->sprg;
  t.client_if_modified_date = ntohl(lfntree->cims);
  t.server_expires_date = ntohl(lfntree->sexp);
  t.server_last_modified_date = ntohl(lfntree->slmd);
  insert_host_into_url(&t);
  t.dst_host = dest_inaddr.sin_addr.s_addr;
  t.dst_port = dest_inaddr.sin_port;
  if (t.dst_host == (unsigned long) -1)
    goto DONE_TIMERPLAY;

  fprintf(stderr, "About to do_http_task on %s\n", t.url);
  res = do_http_task(t, &retd, &retl);
  if (res == 0)
    free(retd);
  else {
    fprintf(stderr, "do_http_task failed\n");
    if (retd != NULL)
      free(retd);
    goto DONE_TIMERPLAY;
  }
  
DONE_TIMERPLAY:
  free(t.url);
  free(lfntree);
  free(timerdata);
  return;
}

