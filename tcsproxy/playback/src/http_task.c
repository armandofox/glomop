/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: http_task.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

#include "http_task.h"
#include "utils.h"
#include "logparse.h"

char               *common_header = NULL;
int                 common_header_len = 0;
struct sockaddr    *local_addr;    /* Local inet address */
int                 local_addrlen;
int                 tcp_protonum;

int numconnecting = 0;
pthread_mutex_t numc_mutex;
extern int errno;
unsigned long reqids = 0L;

pthread_mutex_t     time_mutex;

/**
 ** initialize_http_tasks will initialize a common header buffer that
 ** will be used on every outgoing request.  This will save us time and
 ** effort later on.  This function returns 0 on success, something else
 ** on failure.
 **/

int initialize_http_tasks(char *useragent, char *from)
{
  char scratchbuf[4096], *tmp;
  int  res;

  signal(SIGPIPE, SIG_IGN);

  if (common_header != NULL)
    return 1;

  if ((res = pthread_mutex_init(&time_mutex, NULL)) != 0) {
    strerror(res);
    exit(1);
  }

  if (pthread_mutex_init(&numc_mutex, NULL) != 0)
    exit(1);

  /** Add the "User-Agent: " component **/
  if (gethostname(scratchbuf, 4095) != 0)
    return 1;
  common_header = (char *) malloc(sizeof(char)*
				  (strlen(scratchbuf)+12+3+3+
				   strlen(useragent)));
  if (common_header == NULL) {
    fprintf(stderr, "Out of memory in initialize_http_tasks(1)\n");
    exit(1);
  }
  sprintf(common_header, "User-Agent: %s (%s)\r\n", useragent, scratchbuf);

  /** Add the "From: " component **/
  tmp = realloc(common_header, strlen(common_header)+1+8+strlen(from));
  if (tmp == NULL) {
    fprintf(stderr, "Out of memory in initialize_http_tasks(2)\n");
    exit(1);
  }
  common_header = tmp;
  strcat(common_header, "From: ");
  strcat(common_header, from);
  strcat(common_header, "\r\n");

  /** Add the "Accept: " component **/
  tmp = realloc(common_header, strlen(common_header)+1+13);
  if (tmp == NULL) {
    fprintf(stderr, "Out of memory in initialize_http_tasks(2)\n");
    exit(1);
  }
  common_header = tmp;
  strcat(common_header, "Accept: */*\r\n");

  common_header_len = strlen(common_header);

  /* Finally, initialize the local inet address */
  local_addr = (struct sockaddr *) malloc(sizeof(struct sockaddr_in));
  local_addrlen = sizeof(struct sockaddr_in);
  if (local_addr == NULL) {
    fprintf(stderr, "Out of memory in initialize_http_tasks(3)\n");
    exit(1);
  }
  make_inetaddr(NULL, NULL, (struct sockaddr_in *) local_addr);

  tcp_protonum = protonumber("tcp");
  if (tcp_protonum < 0) {
     fprintf(stderr, "protonumber(\"tcp\") failed in initialize_http_tasks\n");
     exit(1);
  }
  return 0;
}

/** 
 ** do_http_task will accept a task structure, which includes an HTTP
 ** command, a host/port, and some various pragmas to be used while
 ** constructing headers.  This procedure will open up a socket to the
 ** destination server/port, give the command, and suck down the
 ** response.  A buffer will be allocated for the response, and stored
 ** in the *retdata argument.  We'll be nice and NULL-terminate it for
 ** the caller, but won't include the NULL in the data length count.
 ** This function returns 0 on success.  No memory is allocated on
 ** failure.
 **/

#define HTTP_TASK_READ_CHUNK 4096

int do_http_task(http_task t, char **retdata, int *retlen)
{
  struct sockaddr_in  dest;
  int                 protonum, s;

  *retdata = NULL;
  *retlen = 0;
  /* Create and connect the socket to the destination host */
  if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
  numconnecting++;
  if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = t.dst_host;
  dest.sin_port = t.dst_port;
  protonum = tcp_protonum;
  if ((s = socket(PF_INET, SOCK_STREAM, protonum)) < 0) {
    fprintf(stderr, "socket failed in do_http_task.\n");
    if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
    numconnecting--;
    if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
    return 1;
  }
  if (bind(s, local_addr, local_addrlen) < 0) {
    perror("bind failed in do_http_task: ");
    close(s);
    if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
    numconnecting--;
    if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
    return 1;
  }

  if (connect(s, (struct sockaddr *) &dest, sizeof(struct sockaddr_in)) < 0) {
    perror("connect failed in do_http_task: ");
    close(s);
    if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
    numconnecting--;
    if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
    return 1;
  }
  if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
  numconnecting--;
  if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);

  /* Excellent!  We have a connection to the remote machine.  Now let's
     write off our request and headers, and get back our response */

  if (correct_write(s, t.url, t.urllen) != t.urllen) {
    close(s);
    return 1;
  }
  if (correct_write(s, "\r\n", 2) != 2) {
    close(s);
    return 1;
  }
  if (correct_write(s, common_header, common_header_len) != common_header_len) {
    close(s);
    return 1;
  }

  /* Now let's test and write out pragmas */
  if (t.client_pragmas & PG_CLNT_NO_CACHE) {
    if (correct_write(s, "Pragma: no-cache\r\n", 18) != 18) {
      close(s);
      return 1;
    }
  }
  if (t.client_pragmas & PG_CLNT_KEEP_ALIVE) {
    /* We don't do keep-alive, for the sake of simplicity */
  }
  if (t.client_pragmas & PG_CLNT_CACHE_CNTRL) {
    /* We don't do cache control, for sake of simplicity */
  }
  if (t.client_pragmas & PG_CLNT_IF_MOD_SINCE) {
    struct tm mod_time, *tmptime;
    char      modbuf[512];
    int       flen;
    
    /* Yuck.  gmtime is not threadsafe, so need to wrap in mutex. */
    if (pthread_mutex_lock(&time_mutex) < 0) {
      fprintf(stderr, "mutex lock failed in PG_CLNT_IF_MOD_SINCE\n");
      exit(1);
    }
    tmptime = gmtime((const time_t *) &t.client_if_modified_date);
    memcpy(&mod_time, tmptime, sizeof(struct tm));
    if (pthread_mutex_unlock(&time_mutex) < 0) {
      fprintf(stderr, "mutex unlock failed in PG_CLNT_IF_MOD_SINCE\n");
      exit(1);
    }

    if ( (flen = 
	  strftime(modbuf, 511, 
		   "If-Modified-Since: %a, %d %b %Y %H:%M:%S GMT\r\n",
		   (const struct tm *) &mod_time)) 
	 <= 0 ) {
      fprintf(stderr, "strftime failed in PG_CLNT_IF_MOD_SINCE\n");
      close(s);
      return 1;
    }
    if (correct_write(s, modbuf, flen) != flen) {
      close(s);
      return 1;
    }
  }
  if (t.client_pragmas & PG_CLNT_UNLESS) {
    /* We don't do UNLESS, since it's insane */
  }

  /* Write out the final \r\n to kick start the server */
  if (correct_write(s, "\r\n", 2) != 2) {
    close(s);
    return 1;
  }
  
  /* Now, let's suck back the data */
  {
    int      retval;
    unsigned cursize = 0, curbuff = 0;
    char     *bup, *ldata = NULL, *ptr = NULL;

    *retdata = NULL;
    while (1) {
      if (cursize % HTTP_TASK_READ_CHUNK == 0) {
	/* maxxed out curbuff, add some more to it */
	bup = realloc(ldata, curbuff + HTTP_TASK_READ_CHUNK + 1);
	if (bup == NULL) {
	  fprintf(stderr, "Out of memory in do_http_task\n");
	  exit(1);
	}
	ldata = bup;
	curbuff += HTTP_TASK_READ_CHUNK;
	if (ptr == NULL)
	  ptr = ldata;
	else
	  ptr = ldata + cursize;
      }
      
      /* Attempt to maxx curbuff with read */
      retval = read(s, ptr, curbuff-cursize);
      if (retval <= 0) {
	/* EOF */
	*(ldata + cursize) = '\0';
	*retlen = cursize;
	*retdata = ldata;
	close(s);
	return retval;
      }
      ptr += retval;
      cursize += retval;
    }
  }

  close(s);
  return 0;
}

/** 
 ** do_http_task will accept a task structure, which includes an HTTP
 ** command, a host/port, and some various pragmas to be used while
 ** constructing headers.  This procedure will open up a socket to the
 ** destination server/port, give the command, and suck down the
 ** response.  A buffer will be allocated for the response, and stored
 ** in the *retdata argument.  We'll be nice and NULL-terminate it for
 ** the caller, but won't include the NULL in the data length count.
 ** This function returns 0 on success.  No memory is allocated on
 ** failure.
 **/

#define WING_TASK_READ_CHUNK 4096

int do_wingtask(http_task t, char **retdata, int *retlen)
{
  struct sockaddr_in  dest;
  int                 protonum, s;
  ts_strtok_state     *strstate;
  char                *tokstr;
  char                 header_buffer[20];
  unsigned long        vers, reqid, metasize, datasize, comp;

  *retdata = NULL;
  *retlen = 0;
  /* Create and connect the socket to the destination host */
  if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
  numconnecting++;
  if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = t.dst_host;
  dest.sin_port = t.dst_port;
  protonum = tcp_protonum;
  if ((s = socket(PF_INET, SOCK_STREAM, protonum)) < 0) {
    fprintf(stderr, "socket failed in do_http_task.\n");
    if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
    numconnecting--;
    if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
    return 1;
  }
  if (bind(s, local_addr, local_addrlen) < 0) {
    perror("bind failed in do_http_task: ");
    close(s);
    if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
    numconnecting--;
    if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
    return 1;
  }

  if (connect(s, (struct sockaddr *) &dest, sizeof(struct sockaddr_in)) < 0) {
    perror("connect failed in do_http_task: ");
    close(s);
    if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
    numconnecting--;
    if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);
    return 1;
  }
  if (pthread_mutex_lock(&numc_mutex) < 0)
      exit(1);
  numconnecting--;
  if (pthread_mutex_unlock(&numc_mutex) < 0)
      exit(1);

  /* Excellent!  We have a connection to the remote machine.  Now let's
     write off our request and headers, and get back our response.  We
     know the request is a GET of an HTTP/1.0 or later, so we can do the
     strtoks on space characters in order to get the "Real" url to
     write. */
  strstate = ts_strtok_init(t.url);
  if (strstate == NULL) {
    close(s);
    return 1;
  }
  tokstr = ts_strtok(" ", strstate);
  if (tokstr == NULL) {
    close(s);
    ts_strtok_finish(strstate);
    return 1;
  }
  if (strcmp(tokstr, "GET") != 0) {
    close(s);
    ts_strtok_finish(strstate);
    return 1;
  }
  tokstr = ts_strtok(" ", strstate);
  if (tokstr == NULL) {
    close(s);
    ts_strtok_finish(strstate);
    return 1;
  }
  vers = 1;
  reqid = reqids++;
  metasize = 0;
  datasize = strlen(tokstr);
  comp = 0;

  *((unsigned long *) header_buffer) = htonl(vers);
  *((unsigned long *) (header_buffer+4)) = htonl(reqid);
  *((unsigned long *) (header_buffer+8)) = htonl(metasize);
  *((unsigned long *) (header_buffer+12)) = htonl(datasize);
  *((unsigned long *) (header_buffer+16)) = htonl(comp);

  if (correct_write(s, header_buffer, 20) != 20) {
    close(s);
    ts_strtok_finish(strstate);
    return 1;
  }
  fprintf(stderr, "Writing url (%ld) %s\n", datasize, tokstr);
  if (correct_write(s, tokstr, datasize) != datasize) {
    close(s);
    ts_strtok_finish(strstate);
    return 1;
  }
  ts_strtok_finish(strstate);

  /* Now, let's suck back the data */
  {
    int      retval;
    unsigned cursize = 0, curbuff = 0;
    char     *bup, *ldata = NULL, *ptr = NULL;

    *retdata = NULL;
    while (1) {
      if (cursize % WING_TASK_READ_CHUNK == 0) {
	/* maxxed out curbuff, add some more to it */
	bup = realloc(ldata, curbuff + WING_TASK_READ_CHUNK + 1);
	if (bup == NULL) {
	  fprintf(stderr, "Out of memory in do_http_task\n");
	  exit(1);
	}
	ldata = bup;
	curbuff += WING_TASK_READ_CHUNK;
	if (ptr == NULL)
	  ptr = ldata;
	else
	  ptr = ldata + cursize;
      }
      
      /* Attempt to maxx curbuff with read */
      retval = read(s, ptr, curbuff-cursize);
      if (retval <= 0) {
	/* EOF */
	*(ldata + cursize) = '\0';
	*retlen = cursize;
	*retdata = ldata;
	close(s);
	return retval;
      }
      ptr += retval;
      cursize += retval;
    }
  }

  close(s);
  return 0;
}

/**
 ** insert_host_into_url accepts an http_task structure;  it will take the
 ** url in the task, and if the url isn't fully qualified (i.e. no mention
 ** is made of the hostname, instead it looks like "GET / HTTP/1.0"), it
 ** will insert the hostname so it looks like "GET http://1.2.3.4/ HTTP/1.0".
 ** It does this by realloc'ing the url field in the task structure.
 **
 ** This function returns 1 if it changed the field, 0 otherwise.
 **/
int insert_host_into_url(http_task *t)
{
  char *firstspace, *new;
  char hoststuff[128];
  int  firstlen;
  
  firstspace = strchr(t->url, ' ');
  if (firstspace == NULL)
    return 0;

  firstspace++;
  firstlen = firstspace-(t->url);
  if (strncmp(firstspace, "http://", 7) == 0)
    return 0;

  /* we've found our spot! */
  lf_ntoa(ntohl(t->dst_host), hoststuff);
  new = (char *) malloc(t->urllen + 1 + 7 + strlen(hoststuff));
  if (new == NULL) {
    fprintf(stderr, "Out of memory in insert_host_into_url\n");
    exit(1);
  }
  memcpy(new, t->url, firstlen);
  *(new + firstlen) = '\0';
  strcat(new, "http://");
  strcat(new, hoststuff);
  strcat(new, firstspace);
  free(t->url);
  t->url = new;
  t->urllen = strlen(t->url);
  return 1;
}

