/*
 *  comm_http.c:  HTTP-specific I/O and parsing routines
 */

/*
 *  readline_or_timeout: read client request (ie all HTTP header lines)
 *  from socket (file descriptor), or timeout.
 *
 *  ARGS:
 *  input: socket filehandle   
 *  input/output: ptr to string buffer
 *  value-result: length of string buffer
 *  input: number of bytes to read;  if negative, just read until blank line
 *  seen 
 *
 *  REENTRANT: yes, if output and value-result args are per-thread
 *  RETURNS: if line was collected, returns its length, with
 *  null-terminated string (including \n) in buffer.  If timeout,
 *  returns -1.  If end-of-file on socket, returns 0.
 *  ASSUMPTIONS: O_NONBLOCK is set for socket.
 */

#include "config_tr.h"
#include "debug.h"
#include "clib.h"
#include "thr_cntl.h"
#include "frontend.h"
#include "utils.h"
#include "comm_http.h"
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*
 *  readline_or_timeout:  Read an HTTP client request, including all
 *  headers (and possibly the first few bytes of POST data), from an
 *  open file descriptor.  Use the cli_hdrs buffers in the request
 *  structure to store result, reading at most the number of bytes
 *  specified by the buffer length there.
 *
 *  Returns the number of bytes actually read, or -1 on failure.  IN
 *  case of failure, an error message is also
 *  copied to the request structure's errmsg field.
 *
 *  In case of success, sets the length field to the actual number of
 *  bytes read (may be less than the maxLength field, which is
 *  the total buffer size available).
 *
 * ARGS:
 *  i/o: headers structure.  The cli_hdrs.maxLength field is used as an
 *  upper bound on the buffer size, and cli_hdrs.data
 *  is filled in with the request headers read.
 *  The socket file descriptor is in cli_fd.
 *
 *  i: number of bytes to read; READ_ALL_HEADERS means read until the end of the
 *  headers is detected (double blank line for HTTP 1.0 or later).
 *
 *  o: optional buffer; if given, it must be guaranteed large enough to
 *  hold nbytes bytes; if NULL, the buffer in the Request struct is used.
 */

int
readline_or_timeout(Request *hp, int nbytes, char *opt_buffer)
{
  int time_so_far = 0;
  size_t len = 0;
  int count;
  int result = 0;
  fd_set readfds,exceptfds;
  struct timeval to;
  int sock = hp->cli_fd;
  char *sbuf;
  size_t buflen;
  /* char tmpbuf[PERF_HTTP_HEADER_BLOCKSIZE]; */

  /*
   *  BUG::should use tmpbuf to read from socket in blocks, and allocate
   *  cli_hdrs data buffer as we go.
   */
  assert(sock != 0);            /* sock must be valid FD */

  if (opt_buffer) {
    buflen = nbytes;
    sbuf = opt_buffer;
  } else {
    buflen = DistillerBufferLength(&hp->cli_hdrs);
    sbuf = DistillerBufferData(&hp->cli_hdrs);
  }

  assert(sbuf != NULL);
  assert(buflen != 0);

  /*
   *  repeatedly check for bytes on nonblocking socket.  If a
   *  terminating "\n" is seen, return success; otherwise select on
   *  socket.  Continue until total timeout is exhausted.
   */

  to.tv_sec = PERF_HTTP_REQ_TIMEOUT;
  to.tv_usec = 0;
  while (time_so_far <= to.tv_sec) {
    /*
     *  BUG::time_so_far is never incremented so timeout timer is really
     *  just an inactivity timer for select().
     */
    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    FD_SET(sock,&readfds);
    FD_SET(sock,&exceptfds);
    to.tv_sec = PERF_HTTP_REQ_TIMEOUT;
    to.tv_usec = 0;
    count = select(sock+1, &readfds, (fd_set *)NULL, &exceptfds, &to);
    if (count == -1) {
      if (errno != EINTR) {
        MON_SEND_2(MON_ERR,"Unexpected select() failure: %s",strerror(errno));
	result = -1;
	goto RETURN;
      } else {
	continue;
      }
    }
    if (count == 0) {
      /* timeout occurred */
      MON_SEND(MON_ERR,"Client socket timeout\n");
      result = -1;
      goto RETURN;
    }
    count = read(sock, sbuf+len, buflen-len);
    if (count == 0) {         /* end of file! */
      MON_SEND(MON_ERR,"Premature EOF on client socket\n");
      result = -1;
      goto RETURN;
    } else if (count == -1 && (errno != EAGAIN && errno != EWOULDBLOCK &&
                               errno != EINTR)) {
      /* I/O error on socket */
      result = -1;
      MON_SEND_2(MON_ERR,"Unexpected read() failure: %s",strerror(errno));
      goto RETURN;
    } else if (count > 0) {
      /* valid header data 
       *  If we're just reading till end-of-headers, and this is detected
       *  (blank line), return.
       *  NOTE: if this is a POST request, the \r\n\r\n will NOT be the lats
       *  thing we see (since POST data follows).  This leads to ugliness in the
       *  POST logic, since some of the POST data has already been read and some
       *  hasn't. 
       */
      len += count;
      if (nbytes < 0
          && len >= 4
          && dumb_strnstr(sbuf, "\r\n\r\n", len)) {
        result = len;
        goto RETURN;
      }
      /*
       *  If we're reading fixed number of bytes, and we're done, return.
       */
      if (nbytes >= 0
          && len >= nbytes) {
        result = len;
        goto RETURN;
      }
    }
    /* here if errno==EAGAIN, or count>0 but still more bytes to read:
     just continue loop.  */
  }
RETURN:
  DistillerBufferSetLength(&hp->cli_hdrs, len);
  sbuf[len] = '\0';
  return(result);
}

/*
 *  bypass: act as a "dumb conduit" for data for a particular request.
 *
 *  ARGS:
 *   i: http headers struct containing the client request, with
 *      properly filled in version
 *   i: socket filehandle to the client
 *
 *  REENTRANT: yes
 *  BUGS: The return data from Harvest should be pipelined.
 */

void
tunnel(Request *h)
{
  clib_response cl;
  char *outptr = NULL;
  unsigned outlen;
  size_t blocklen;
  int wrote;
  int fd;
  char *headers = DistillerBufferData(&h->cli_hdrs);
  int len = DistillerBufferLength(&h->cli_hdrs);
  int sock = h->cli_fd;
  
  if (len == -1)
    len = strlen(headers);

  cl = Clib_Lowlevel_Op(headers, len, &outptr, &outlen, &fd);

  switch(cl) {

  case CLIB_OK:
    if ((wrote = correct_write(sock, outptr, (int)outlen)) != (int)outlen) {
      MON_SEND_4(MON_ERR,"Wrote %d instead of %u for '%s'",
                 wrote, outlen, headers);
    }
    break;

  case CLIB_CUTTHROUGH:
    blocklen = outlen;
    while (blocklen > 0) {
      if ((wrote = correct_write(sock, outptr, blocklen)) != blocklen) {
        if (wrote != -1) {
          MON_SEND_4(MON_ERR,"Wrote %d instead of %u for '%s'",
                     wrote, blocklen, headers);
        }
        blocklen = 0;
      } else {
        blocklen = read(fd, outptr, outlen);
      }
    }
    close(fd);
    break;

  default:
    /* error in harvest fetch */
    MON_SEND_3(MON_ERR,"Harvest error %d on '%s'",(int)cl,headers);
  }

  /* in any case, free the allocated data structure */
  if (outptr != NULL)
    FREE((void *)outptr);
}

/*
 *  Do a complete bypass of data returned from the client lib (i.e. from
 *  the cache).  This may be done because distillation failed, because
 *  no such distiller exists, or because the content was deemed too small
 *  to be worth distilling....or perhaps some other reason.
 */

void
complete_bypass(Request *hp)
{
  int sock = hp->cli_fd;
  (void)correct_write(sock, DistillerBufferData(&hp->svr_hdrs),
                      DistillerBufferLength(&hp->svr_hdrs));
  (void)correct_write(sock, DistillerBufferData(&hp->svr_data),
                      DistillerBufferLength(&hp->svr_data));
}

/*
 *  Given an Request structure returned as a result of reading a
 *  client request, parse out the status line and other interesting
 *  information from the request and perform minimal validation.
 *  If successful, sets the hp->url, hp->method, and hp->version fields.
 *
 *  ARGS:
 *    i/o: Request structure; various fields get filled in
 *  RETURNS:
 *    An HTTP_Status code; HTTP_NO_ERR means OK, others are various
 *    parsing errors.  
 *  ASSUMPTIONS:
 *    Request's cli_hdrs field contains complete verbatim client request,
 *  including HTTP status line, all headers, and separator blank line
 *  (two CRLF's in a row) at end.
 */
HTTP_Status
parse_status_and_headers(Request *hp)
{
  char *vers;
  int result = -1;
  char *tmpptr;
  ts_strtok_state *ts_st;
#ifdef LOGGING
  struct loginfo *l = hp->lo;
  int bytesremaining = sizeof(l->url) - 2;
#endif /* LOGGING */

  /*
   *  Sanity check request line only, for HTTP/0.9 or HTTP/1.x
   *  compliance.  Collect version number, method, and URL.
   */

  tmpptr = DistillerBufferData(&hp->cli_hdrs);
  strncpy(hp->errmsg, tmpptr, HTTP_ERRMSG_MAX);
  ts_st = ts_strtok_init(tmpptr);
  if ((tmpptr = ts_strtok(" ", ts_st)) == NULL) {
    /* zero tokens on line!    */
    result = HTTP_ERR_MALFORMED_REQUEST;
    goto PARSE_DONE;
  }
  hp->method = (char *)MALLOC(strlen(tmpptr)+1);
  assert(hp->method);
  strcpy(hp->method, tmpptr);

#ifdef LOGGING
  if (bytesremaining > 2) {
    strncat(l->url, hp->method, bytesremaining-1);
    bytesremaining = sizeof(l->url) - strlen(l->url);
    if (bytesremaining > 0) {
      strcat(l->url, " ");
      bytesremaining--;
    }
  }
#endif /* LOGGING */

  /*
   *  Get URL
   */

  if ((tmpptr = ts_strtok(" ", ts_st)) == NULL) {
    /* only one token on line!    */
    result = HTTP_ERR_MALFORMED_REQUEST;
    goto PARSE_DONE;
  }

  if (*tmpptr == '/') {
    /*
     *  If URL begins with a slash, discard the slash (only if this is a
     *  proxied URL).  This is to allow mechanisms like
     *  "http://foo.bar.com/http://real.page.com/" to work (for
     *  proxy chaining, etc.)
     */

    /* check whether this is a proxied URL */
    char *slash, *colon;
    slash = strchr(tmpptr+1, '/');
    colon = strstr(tmpptr+1, "://");
    if (slash!=NULL && colon!=NULL && colon < slash) {
      /* this is a proxied URL */
      while (*tmpptr == '/') tmpptr++;
    }
  }

  if (*tmpptr == '\0') {        /* empty URL */
    result = HTTP_ERR_MALFORMED_REQUEST;
    goto PARSE_DONE;
  }
  hp->url = (char *)MALLOC(strlen(tmpptr)+1);
  assert(hp->url);
  strcpy(hp->url, tmpptr);

#ifdef LOGGING
  if (bytesremaining > 2) {
    strncat(l->url, hp->url, bytesremaining-1);
    bytesremaining = sizeof(l->url) - strlen(l->url);
    if (bytesremaining > 0) {
      strcat(l->url, " ");
      bytesremaining--;
    }
  }
#endif /* LOGGING */
  
  if ((vers = ts_strtok(" \r\n", ts_st)) == NULL) {
    /* must be HTTP 0.9 or earlier */
    hp->version = HTTP_VERSION_09_OR_EARLIER;
  } else if (strncasecmp(vers, "HTTP/", 5) != 0) {
    /* this token looks bogus... */
    result = HTTP_ERR_MALFORMED_REQUEST;
    goto PARSE_DONE;
  } else {
    /* got a token, and it begins with "HTTP/".  Should really parse out
     * the version, but for now... what the hey.
     * BUG::fails to distinguish http version except to know it's >=1.0.
     */
    hp->version = HTTP_VERSION_10_OR_LATER;
  }

#ifdef LOGGING
  if (bytesremaining > 2) {
    strncat(l->url, vers, bytesremaining-1);
  } else {
    l->url[strlen(l->url)-1] = '\0';
  }
#endif /* LOGGING */
  
  result = HTTP_NO_ERR;
PARSE_DONE:
  /* looks OK. */
  ts_strtok_finish(ts_st);
  return(result);
}

/*
 *  http_error_return: return an HTTP error directly to client socket
 *
 *  ARGS:
 *    i: request descriptor (errmsg field contains string to substitute
 *              into HTTP error msg template)
 *    i: HTTP status
 */

void
http_error_return(Request *h, HTTP_Status status)
{
  char *str, *tmpstr;
  size_t tmpstrlen;
  int sock = h->cli_fd;
  HTTP_Version vers = h->version;
  static const char http_error_template[] = {
    "<HTML><HEAD><TITLE>%s</TITLE></HEAD><BODY><H1>%s</H1>%s</BODY></HTML>\r\n"
  };

  tmpstrlen = strlen(h->errmsg);
  tmpstr = (char *)ALLOCA(strlen(http_returns[status].longmessage)
                          + tmpstrlen
                          + 1);
  tmpstrlen = sprintf(tmpstr, http_returns[status].longmessage, h->errmsg);

  str = (char *)ALLOCA(sizeof(http_error_template)
                       + (strlen(http_returns[status].message) << 1)
                       + tmpstrlen
                       + 2);

  sprintf(str, http_error_template,
          http_returns[status].message, http_returns[status].message, tmpstr);
  
  /* BUG::HTTP version detection needs to be correct here */
  if (1 || vers != HTTP_VERSION_09_OR_EARLIER) {
    char *str2  = (char *)ALLOCA(strlen("HTTP/1.0 \r\n")
                                 + strlen(http_returns[status].message)
                                 + 80);
    /* note, +80 gets minimum size needed for minimal headers. */
    sprintf(str2, "HTTP/1.0 %s\r\n", http_returns[status].message);
    if (correct_write(sock, str2, -1) < 0)
      return;
    sprintf(str2, "Content-type: text/html\r\nContent-length: %d\r\n\r\n",
            strlen(str));
    if (correct_write(sock, str2, -1) < 0)
      return;
  }
  proxy_debug_2(DBG_HTTP, str);
  (void)correct_write(sock, str, -1);
}

