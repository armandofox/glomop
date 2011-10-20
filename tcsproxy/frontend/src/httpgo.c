/*
 *  httpgo.c
 *  This file contains the HTTP request handling code that defines the
 *  TranSend distillation web proxy.  It corresponds to the TranSend Service
 *  layer in the layered SNS model.
 */

#include "frontend.h"
#include "debug.h"
#include "comm_http.h"
#include "userpref.h"
#include "url_magic.h"
#include "ARGS.h"
#include "utils.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifdef LOGGING
#include "md5.h"
static char magicKey[] = "aa883fd833ef";
#endif /* LOGGING */

static userkey userkey_from_sock_ipaddr(int sock);

#define TRANSEND_STATUS_HEADER  "X-Proxy-Status"
#define TRANSEND_ERROR_HEADER   "X-Proxy-Error"

/*
 *  Error codes are roughly:
 *  100-199  Informational
 *  200-299  OK
 *  300-399  Additional user action required
 *  400-499  Error on this transaction
 *  500-599  Fatal error (rare)
 */

/*
 *  http_go_proc: thread entry point to handle an incoming HTTP request 
 *  The request will be parsed, validated, and handed off to a remote
 *  distiller (or else rejected).  This is a long blocking operation.
 *
 *  ARGS:
 *  i/o: the task_t structure; its task_data field contains the file
 *      number of the socket on which the request is arriving.
 *  RETURNS: a void* which is ignored; errors are reported directly on
 *      socket using HTTP error protocol (500 Error, etc)
 *  REENTRANT: yes
 *  
 */

void *
http_go_proc(task_t *t)
{
  int index = TASK_THRINDEX(t);
  Request h;
  ArgumentList prefs;
  userkey k;
  HTTP_Status result;
  DistillerStatus dist_result;
  const char *content_type;
  Argument *arg;
  int thresh;
  gm_Bool no_distill = gm_False;
  gm_Bool is_text_html = gm_False;
#ifdef LOGGING
  struct loginfo lo;
  char logmsg[MAX_LOGMSG_LEN];
#endif /* LOGGING */
  
  /*
   *  New task data should be the request structure.
   */
  init_Request(&h);
  h.cli_fd = (int)TASK_DATA(t); /* socket FD of client */
  SET_TASK_DATA(t,&h);
  
  INST_begin_timestamp(index);
  INST_set_size(index,0);
  INST_set_thread_state(index, THR_ACCEPTED);
  INST_timestamp(index, m_arrival);
#ifdef LOGGING
  LOGGING_init_loginfo(&lo);
  h.lo = &lo;
#endif /* LOGGING */
  /*
   *  this should read all the headers.
   */
  if (TASK_METADATA(t) == NULL) {
    int result;
    
    assert( DistillerBufferAlloc(&h.cli_hdrs, PERF_HTTP_TOTAL_HEADERLEN)
            == gm_True);

    result = readline_or_timeout(&h, READ_ALL_HEADERS, NULL);

    INST_timestamp(index, m_headersdone);
    if (result == -1) { /* client request timed out, or error reading hdrs */
      /* TC::1 client timeout, or request doesn't sanity check */
      goto HTTPGO_FINISH;
      /* NOTREACHED */
    }
  } else {
#ifdef NEWFE
    /*
     *  This task is a child of a previous 
     */
    strncpy(h.cli_hdrs.mime_headers, TASK_METADATA(t),
            PERF_HTTP_TOTAL_HEADERLEN-1);
    FREE(TASK_METADATA(t));
#endif /* NEWFE */
  }

  /* parse the headers and request line, filling them in to the loginfo */
  result = parse_status_and_headers(&h);
  if (result != HTTP_NO_ERR) {
    /* BUG:: last arg of http_error_return should be a substitution arg */
    /* TC::2 parse_status_and_headers returns an error */
    http_error_return(&h, result);
    printf("Error occured here!\n");
    goto HTTPGO_FINISH;
  }
  
  /*
   *  We have a reasonable looking request.  Get prefs for this user.
   */

  k = userkey_from_sock_ipaddr(h.cli_fd);
#ifdef LOGGING
  lo.ipaddr = k;
#endif /* LOGGING */
  (void)get_userprefs(k, &prefs);
  /*
   *  Extract any arguments embedded in the URL itself, and add them
   *  to the arg list.
   *  TC::3 url is magic vs nonmagic
   */
  if (is_magic ((char *) h.url)) {
    /* demagifying a url will never lengthen it so this is a good 
     * upper bounds on the length of the non magical url
     */
    char *demagicURL = ALLOCA(strlen(h.url)+1);
    assert(demagicURL);
    strcpy(demagicURL, h.url);
    from_magic((char *) demagicURL, h.url, &prefs);
  }

  /* determine the threshold for bypassing */
  arg  = getArgumentFromIdInList(&prefs, FRONT_MTU);
  thresh = (arg == NULL ? PERF_FRONT_MTU : ARG_INT(*arg));

  /* determine if distillation is turned off for this
   * request. EXCEPTION: Force distillation for Prefs html form.
   */
  arg = getArgumentFromIdInList(&prefs, FRONT_NO_DISTILL);
  if (arg != NULL
      && ARG_INT(*arg)
      && strcasecmp(fe_get_prefs_url, h.url) != 0) {
    /* TC::4 no_distill is set */
    no_distill = gm_True;
  }  else {
    no_distill = gm_False;
  }
  
  /* Short-circuit the following special  URL's:
   *  - "set my prefs as follows" (e.g. form submission)
   */
  if (is_setpref_url(h.url)) {
    /* 
     * handle what send user prefs form sends back.
     * TC::5 is_setpref_url returns true
     */
    result = parse_and_change_prefs(h.url, k, h.errmsg);
    if (result == HTTP_NO_ERR) {
      /* TC::5.1 setpref url succeeds in setting prefs */
      correct_write(h.cli_fd, "HTTP/1.0 200\r\nContent-type: text/html\r\n\r\n",
                    -1);
      correct_write(h.cli_fd,
                    "<html><head><title>Preferences Set</title></head>"
                    "<body><center><h1>Preferences set</h1>"
                    "Your new preferences have been set.  Press the back "
                    "button twice to resume browsing."
                    "<p></center></body></html>", -1);
    } else {
      /* TC::5.2 setpref url fails in setting prefs */
      http_error_return(&h, result);
    }
  } else if (is_getpref_url(h.url)) {
    /* TC::6 gimme my prefs*/
    send_prefs(&prefs, h.cli_fd);
  } else if (is_server_url(h.url)==gm_False   && 
	     strcasecmp(h.method, "get") != 0 && 
	     strcasecmp(h.method, "post") != 0) {
    /*
     *  Doesn't appear to be an HTTP GET or POST request; so act as a
     *  "dumb tunnel" for passing the request to the server (actually, via
     *  the cache) and relaying the result to the client.
     */
    /* TC::7 tunnel */
    proxy_debug_3(DBG_HTTP, "Tunneling '%s'", 
		  DistillerBufferData(&h.cli_hdrs));
    INST_set_thread_state(index, THR_DISTILLERSEND);
    tunnel(&h);
  } else {
    /*
     *  It's not a special URL, and the request appears to be a GET/POST.
     * Add in the client's IP address as an INT32 argument so that
     *  the distiller driver can get at it.
     */
    SET_ARG_INT(prefs.arg[prefs.nargs], (INT32) k);
    SET_ARG_ID(prefs.arg[prefs.nargs], FRONT_CLIENT_IP);
    prefs.nargs++;
    result = server_dispatch(&prefs, t);
    /*
     *  If we get a transport-level error (i.e. fetch from cache failed
     *  due to an internal cache error), wrap the error in HTML (if
     *  needed) and return error to user.
     *  Otherwise, if the transaction succeeded but the server return
     *  code indicates failure (i.e. != 200),  **OR** if the server data
     *  is smaller than a threshold size, bypass the server data
     *  directly to the client.
     *  Otherwise, attempt to distill.
     */
    if (result != HTTP_NO_ERR) {
      /* transport level error: wrap in HTML for delivery to user */
      /* TC::8 server_dispatch returns transport level error */
      http_error_return(&h, result);
      goto HTTPGO_FINISH;
    }

    content_type = get_header_value(&h.svr_hdrs,
                                    "content-type", NULL, NULL, NULL);
    if (content_type == NULL) {
      /* TC::9 content-type can't be deduced */
      content_type = "application/octet-stream";
    }
    is_text_html = (  ((strncasecmp(content_type, "text/html", 9) == 0) ||
                       (strncasecmp(content_type, "text/plain", 10) == 0))
                    ? gm_True : gm_False);
    
    /*if ( (*h.url != '/' && strncasecmp(h.url, fe_agg_string, 
				       strlen(fe_agg_string)) != 0) &&*/

    /* bypass ONLY if it is not a server-type URL */
    /* TC::10.1 bypass because non-200s status */
    /* TC::10.2 bypass because not text/html and too small to distill */
    /* TC::4 bypass because no_distill is set */
    /* bypass server data directly to user */

    if ( is_server_url(h.url)==gm_False) {
      char *bypass_reason = NULL;
      if (h.svr_http_status != 200) {
        bypass_reason = "201 Non-OK server status";
      } else if (is_text_html == gm_False
                 && DistillerBufferLength(&h.svr_data) <= thresh) {
        bypass_reason = "202 content-type not text/html and content-length too short";
      } else if (no_distill == gm_True) {
        bypass_reason = "203 distillation not indicated";
      }
      if (bypass_reason) {
        INST_set_thread_state(index, THR_WRITEBACK);
        /*
         *  Set return headers to indicate why the bypass occurred.
         */
        insert_header(&h.svr_hdrs, TRANSEND_STATUS_HEADER, bypass_reason, 0);
        complete_bypass(&h);
        goto HTTPGO_FINISH;
      }
    }
    
    /* all is well: continue by dispatching to a worker for distillation */
    dist_result = proxy_dispatch(&prefs, t);
    switch(dist_result) {
    case distOk:
      /* TC::11 distillation succeeded */
      INST_timestamp(index, m_wbstart);
      insert_header(&h.pxy_hdrs, TRANSEND_STATUS_HEADER, "200 distillation OK", 0);
      correct_write(h.cli_fd, (char *)DistillerBufferData(&h.pxy_hdrs),
                    (int)DistillerBufferLength(&h.pxy_hdrs));
      /* -1 to avoid NULL term gunk */
      correct_write(h.cli_fd, (char *)DistillerBufferData(&h.pxy_data),
                    (int)DistillerBufferLength(&h.pxy_data));
      INST_timestamp(thrindex, m_wbdone);
      break;

    case distDistillerNotFound:
    case distLaunchTimeout:
    case distBadInput:
    case distConnectionBroken:

      /* forward original if distiller for this type not found, connection
         repeatedly broken, or couldn't be launched */
      /* TC::12 bypass because distillation failed */

      insert_header(&h.svr_hdrs,
                      (dist_result == distBadInput ? TRANSEND_STATUS_HEADER
                       : TRANSEND_ERROR_HEADER),
                      FE_getDistillerStatusString(dist_result), 0);
      if ((arg = getArgumentFromIdInList(&prefs, FRONT_DEVELOPER))
          && ARG_INT(*arg)) {
        /* return explicit error message */
        int tmp_len =
          snprintf(h.errmsg, HTTP_ERRMSG_MAX,
                   "<i>[set arg <tt>i%d</tt> to 0 to suppress "
                   "this diagnostic]</i><br>",
                   FRONT_DEVELOPER);
        strncat(h.errmsg, FE_getDistillerStatusString(dist_result),
                HTTP_ERRMSG_MAX - tmp_len - 1);
        http_error_return(&h, HTTP_ERR_UNSPECIFIED);
      } else {

        /*if (*h.url == '/' || 
	    strncasecmp(h.url, fe_agg_string, strlen(fe_agg_string)) == 0) {*/

        if (is_server_url(h.url)==gm_True) {
          /* this is a URL in the frontend's namespace, or an aggregator URL */

          strcpy(h.errmsg, h.url);
          http_error_return(&h, HTTP_ERR_AGGREGATOR_ERROR);
        } else {
          complete_bypass(&h);
        }
      }
    break;
        
    case distRedispatch:
      /*
       *  Redispatch count expired: too many redispatches (probably indicates
       * infinite loop in redispatch route.)
       */
      snprintf(h.errmsg, HTTP_ERRMSG_MAX, "%d", PERF_REQUEST_TTL);
      http_error_return(&h, HTTP_ERR_ROUTING_ERROR);
      break;
      
    default:
        
      /* TC::13 some other distillation error */
      insert_header(&h.svr_hdrs, TRANSEND_STATUS_HEADER,
                    FE_getDistillerStatusString(dist_result), 0);

      http_error_return(&h, HTTP_ERR_UNSPECIFIED);
      break;
    } /* switch(dist_result) */
  } /* if...else...else...endif */

  /* all cases exit through this single exit point */
HTTPGO_FINISH:
  free_Request(&h);
  INST_timestamp(index, m_wbdone);
  INST_end_timestamp(index);

  if (TASK_PARENT(t) == 0 && TASK_CHILD_INDEX(t) == 0) {
    /* this is a "root task" */
    close(h.cli_fd);
  }

#ifdef LOGGING
  /* log the request info */
  /* BUG::relies on formatting of the userkey */

  /* I compare the IP address to 127.0.0.1 so that I don't log connections
     from localhost, namely the fe_check script.  I also MD5 the IP address. */
  k = lo.ipaddr;
  if (((UINT32) k) != ((UINT32) htonl(0x7F000001))) {
/*    MD5_CTX   theHash;
    UINT32    res;

    MD5Init(&theHash);
    MD5Update(&theHash, magicKey, sizeof(magicKey));
    MD5Update(&theHash, &k, sizeof(UINT32));
    MD5Final(&theHash);

    memcpy(&res, theHash.digest, sizeof(UINT32));
    res = ntohl(res);

    snprintf(logmsg, MAX_LOGMSG_LEN-1,
             "(HTTP) %lu %lu \"%s\" %d %ld %ld\n",
             res, lo.date, lo.url,
             lo.http_response, lo.size_before, lo.size_after);*/
    snprintf(logmsg, MAX_LOGMSG_LEN-1,
             "(HTTP) %08x %08x \"%s\" %d %ld %ld\n",
             (UINT32) k, lo.date, lo.url,
             lo.http_response, lo.size_before, lo.size_after);
    gm_log(logmsg);
  }
#endif /* LOGGING */
  return (void *)0;
}


/*
 *  userkey_from_sock_ipaddr: make incoming IP address into a user prefs
 *  key, so we can look up user in prefs database
 *
 *  ARGS: socket (filehandle)
 *  REENTRANT: as safe as getpeername()
 *  RETURNS: userkey; the "null key" if conversion fails
 *
 */

static userkey
userkey_from_sock_ipaddr(int s)
{
  struct sockaddr_in a;
  int alen = sizeof(a);

  memset(&a, 0, alen);
  if (getpeername(s, (struct sockaddr *)&a, &alen) != 0) {
    proxy_errlog_2("userkey_from_sock_ipaddr: %s", strerror(errno));
    return USERKEY_NULL;
  }
  return ((userkey)(a.sin_addr.s_addr));
}

