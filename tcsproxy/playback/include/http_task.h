/*
 *     Author: Steve Gribble
 *       Date: Nov. 22nd, 1996
 *       File: http_task.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "utils.h"

#ifndef HTTP_TASK_H
#define HTTP_TASK_H

#define PG_CLNT_NO_CACHE       1
#define PG_CLNT_KEEP_ALIVE     2
#define PG_CLNT_CACHE_CNTRL    4
#define PG_CLNT_IF_MOD_SINCE   8
#define PG_CLNT_UNLESS         16

#define PG_SRVR_NO_CACHE       1
#define PG_SRVR_CACHE_CNTRL    2
#define PG_SRVR_EXPIRES        4
#define PG_SRVR_LAST_MODIFIED  8

typedef struct http_task_st {
  unsigned long  dst_host;        /* IP address, network order */
  unsigned short dst_port;        /* destination port, network order */
  char *url;                      /* URL to be fetched, including command,
				     but not including \r\n, e.g.
				     "GET http://foo.bar/ HTTP/1.0" */
  int   urllen;
  unsigned char  client_pragmas;
  unsigned char  server_pragmas;
  time_t         client_if_modified_date;    /* localtime (host order) */
  time_t         server_expires_date;        /* localtime (host order) */
  time_t         server_last_modified_date;  /* localtime (host order) */
} http_task;

int initialize_http_tasks(char *useragent, char *from);
int do_http_task(http_task t, char **retdata, int *retlen);
int do_wingtask(http_task t, char **retdata, int *retlen);
int insert_host_into_url(http_task *t);

#endif










