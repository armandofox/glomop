/*
 *  Dispatch a request by determining its MIME type and contacting the
 *  PTM/distiller cache to send the request to a distiller.  The
 *  argument list is already provided: it is the result of the user
 *  prefs lookup.  Pretty cool, huh.
 */

#include "userpref.h"
#include "debug.h"
#include "utils.h"
#include "frontend.h"
#include "ARGS.h"
#include "comm_http.h"
#include "proxyinterface.h"     /* PTM stub interface */
#include <stdio.h>
#include <string.h>

static void set_distiller_type(DistillerBuffer *mime_headers, const char *url,
                               const char *user, C_DistillerType *dtype);
static void set_distiller_type_generic(DistillerBuffer *mime_headers,
                                       const char *url,
                                       const char *user, C_DistillerType *dtype);
static DistillerStatus  do_distillation(const char *url,
                                        DistillerInput *din,
                                        DistillerOutput *dout,
                                        C_DistillerType *dtype,
                                        ArgumentList *al);


gm_Bool is_server_url(const char *url)
{
  if (*url=='/' || strncasecmp(url, fe_agg_string, strlen(fe_agg_string))==0)
    return gm_True;
  else
    return gm_False;
}

/*
 *  proxy_dispatch: dispatch a request to the correct distiller, then
 *  return result to client. This is a long blocking operation.
 *
 *  ARGS:
 *  i: argument list for distiller (from user prefs)
 *  i: task data structure
 *
 *  REENTRANT: yes
 *  ASSUMPTIONS: PTM stub has been initialized
 *  RETURNS: HTTP_Status code corresponding to server's/cache's return
 *  status for the request
 */

HTTP_Status
server_dispatch(ArgumentList *al, task_t *tsk)
{
  int thrindex = TASK_THRINDEX(tsk);
  int agg = 0;
  int nfound;
  HTTP_Status retcode = HTTP_NO_ERR;
  Request *hp = (Request *)TASK_DATA(tsk);
#ifdef LOGGING
  struct loginfo *lo = hp->lo;
#endif

  INST_set_thread_state(thrindex, THR_CACHE);

  /*
   *  If we got here, we know the request is either GET or POST,
   *  or this is a URL in the frontend's namespace
   */

  tsk->task_age = 0;            /* no pipe stages done yet */

  /*if (*hp->url == '/' || 
      strncasecmp(hp->url, fe_agg_string, strlen(fe_agg_string)) == 0) {*/
  if (is_server_url(hp->url)==gm_True) {
    /* this is a URL in the frontend's namespace, or an aggregator URL */
    const char *workerName, *val;
    int val_len, len, to_copy, already_read;
    char *orig_hdrs, *eoh, *new_hdrs, *body;
    extern Options runtime_options;

    orig_hdrs = DistillerBufferData(&hp->cli_hdrs);
    eoh = strstr(orig_hdrs, "\r\n\r\n");
    if (eoh==NULL) {
      retcode = HTTP_ERR_MALFORMED_REQUEST;
      goto DISPATCH_RETURN;
    }
    eoh += 4;

    /* clone the headers */
    DistillerBufferAlloc(&hp->svr_hdrs, eoh-orig_hdrs+1);
    new_hdrs = DistillerBufferData(&hp->svr_hdrs);
    memcpy(new_hdrs, orig_hdrs, eoh-orig_hdrs);
    *(new_hdrs+(eoh-orig_hdrs)) = '\0';
    DistillerBufferSetLength(&hp->svr_hdrs, eoh - orig_hdrs);
    DistillerBufferFreeMe(&hp->svr_hdrs, gm_True);

    /* 
     * look for the content-length field; 
     * if found, read the rest of the data and create svr_data 
     */
    if ((val = get_header_value(&hp->cli_hdrs, "content-length", &val_len,
				NULL, NULL)) != NULL) {
      len = strtoul(val, NULL, 0);
      if (len < 0) {
	retcode = HTTP_ERR_MALFORMED_REQUEST;
	goto DISPATCH_RETURN;
      }

      already_read = DistillerBufferLength(&hp->cli_hdrs) - (eoh - orig_hdrs);
      if (len <= already_read) {
	to_copy = len;
      } else {
	to_copy = already_read;
      }

      DistillerBufferAlloc(&hp->svr_data, len+1);
      body = DistillerBufferData(&hp->svr_data);
      memcpy(body, eoh, to_copy);
      
      /* get the rest of the data from the socket */
      if (to_copy < len) {
	if (readline_or_timeout(hp, len - to_copy, body+to_copy)
	    != (len-to_copy)) {
	  retcode = HTTP_ERR_MALFORMED_REQUEST;
	  goto DISPATCH_RETURN;
	}
      }

      /* add a trailing '\0' */
      *(body+len) = '\0';
      DistillerBufferSetLength(&hp->svr_data, len);
    }
    
    workerName = Options_Find(runtime_options, "frontend.myurl.dispatch");
    if (workerName==NULL) workerName = "myurl/dispatch";

    insert_header(&hp->svr_hdrs, "X-Route", workerName, 1);
    hp->svr_http_status = 200; /* everything's ok with this HTTP request */
  }
  else {
    INST_timestamp(thrindex, m_cachestart);
    if (hp->method[0] == 'g' || hp->method[0] == 'G') {
      /* handle GET */

#ifdef OLD
      /*
       *  HACK HACK HACK - this should be replaced with the real interface that
       *  does a regexp match on URL to determine if any aggregator should get
       *  dibs before the original content is fetched.
       *
       *  Any references in this function to variable 'agg' represent a place
       *  where the hack was spliced in.
       */
      if (strncasecmp(hp->url, fe_agg_string, strlen(fe_agg_string)) != 0) {
	/* not an aggregator call */
#endif
	agg = 0;
	retcode = do_get(hp, al);
	if (retcode != HTTP_NO_ERR) {
	  goto DISPATCH_RETURN;
	}
#ifdef OLD
      } else {
	/* aggregator call */
	abort();
	agg = 1;
	/* don't clone the client headers - just point to them. */
	DistillerBufferSetStatic(&hp->svr_hdrs,
				 DistillerBufferData(&hp->cli_hdrs),
				 DistillerBufferLength(&hp->cli_hdrs));
	/* since we're pointing to a copy, don't try to free */
	DistillerBufferFreeMe(&hp->svr_hdrs, gm_False);
      }
#endif
    } else {
      /* handle POST */
      retcode = do_post(hp, al);
      if (retcode != HTTP_NO_ERR) {
	/* post failed! report error to client */
	goto DISPATCH_RETURN;
      }
    }

    INST_timestamp(thrindex, m_cachedone);

    /*
     *  if there was an HTTP-level error (response code not "200"), bypass
     *  the remainder straight to client.
     */
    
    nfound = sscanf(DistillerBufferData(&hp->svr_hdrs), "%*s %lu",
		    &hp->svr_http_status);
    if (nfound < 1) {             /* no status found */
      retcode = HTTP_ERR_GET_FAILED;
      hp->svr_http_status = 0;
    } else {
      retcode = HTTP_NO_ERR;
    }
  }

#ifdef LOGGING
  lo->http_response = hp->svr_http_status;;      /* log HTTP server response */
#endif /* LOGGING */

  /*
   *  Note that HTTP_NO_ERR means "transaction to server succeeded" (ie
   *  at the transport level).  it does NOT necessarily mean that the
   *  HTTP *operation* (eg GET) succeeded -- the HTTP status code tells that.
   */

DISPATCH_RETURN:
  return retcode;
}


DistillerStatus
proxy_dispatch(ArgumentList *al, task_t *t)
{
  DistillerStatus status;       /* result of this pipe stage distillation */
  DistillerInput din;
  DistillerOutput dout;
  Request *hp = (Request *)TASK_DATA(t);
  C_DistillerType dtype;
  int thrindex = TASK_THRINDEX(t);
  DistillerStatus retcode;
  int redispatch = 0;
  char *static_route = NULL;
  int static_route_initialized = 0;
#ifdef LOGGING
  struct loginfo *lo = hp->lo;
#endif

  /*
   *  Initialize for *first* pipe stage.
   */

  DistillerBufferClone(&din.data, &hp->svr_data);
  DistillerBufferFreeMe(&din.data, gm_True);

  /*
   *  Make a **copy** of the server headers, because they may get
   *  overwritten when preparing headers to send to a distiller.
   */
  DistillerBufferClone(&din.metadata, &hp->svr_hdrs);
  DistillerBufferFreeMe(&din.metadata, gm_True);
  
  do {
    const char *char_tmp;
    char content_type[MIME_TYPE_MAX+1];
    int content_type_len;
    Argument *arg;
    int num_tries_left;

    /*
     *  Initialize for next pipe stage.
     */
    status = distFatalError;

#ifdef LOGGING
    /*
     *   Log original content-length
     */
    char_tmp = get_header_value(&din.metadata,
                                "content-length",
                                &content_type_len, NULL, NULL);
    if (char_tmp != NULL)
      lo->size_before = strtoul(char_tmp, NULL, 0);
    else
      lo->size_before = -1;
#endif /* LOGGING */
    char_tmp = get_header_value(&din.metadata,
                                "content-type",
                                &content_type_len, NULL, NULL);
    if (char_tmp != NULL) {
      strncpy(content_type, char_tmp, MIN(content_type_len+1, MIME_TYPE_MAX));
      content_type[MIN(content_type_len,MIME_TYPE_MAX)] = '\0';
    } else {
      content_type[0] = '\0';
    }

    /* if there are attributes after the content-type, remove them. */
    if ((char_tmp = strchr((const char *)content_type, ';')) != NULL)
      *(char *)char_tmp = '\0';
    /* chop any trailing spaces. */
    if ((char_tmp = strchr((const char *)content_type, ' ')) != NULL)
      *(char *)char_tmp = '\0';

    /*
     *  Distillation is definitely needed, so go do it.  In case of
     *  distConnectionBroken error, (re)try at most N
     *  times, where N is the value of the FRONT_RETRY_DISTILL argument,
     *  or PERF_HTTP_RETRY_DISTILL by default.  In case of any other
     *  error, or if all retries fail, bypass the original content.  In
     *  case of distOk (success), return the distilled content.
     */

    arg = getArgumentFromIdInList(al, FRONT_RETRY_DISTILL);
    num_tries_left = (arg ? ARG_INT(*arg) : PERF_HTTP_RETRY_DISTILL);
    INST_timestamp(thrindex, m_distillstart);
    /*
     *  Add a "Location:" header so distillers have a way to get the URL of
     *  this document, if they want. 
     */
    if (get_header_value(&din.metadata,
                         "location", NULL, NULL, NULL)
        == NULL) {
      insert_header(&din.metadata, "Location", hp->url, 0);
    }

    do {
      INST_set_thread_state(thrindex, THR_DISTILLER);
      status = do_distillation(hp->url, &din, &dout, &dtype, al);
    } while (status == distConnectionBroken
             && num_tries_left-- > 0);

#ifdef LOGGING
    lo->size_after = -status;     /* pessimistically assume failure for log */
#endif /* LOGGING */

    switch(status) {
    case distOk:
    case distRedispatch:
      /*
       *  The rules for redispatch are as follows.
       *  - If the response headers contain a nonempty X-Static-Route header, it
       *    is the final authority. X-Static-Route is only saved the
       *    first time it's seen.
       *  - Otherwise, if the response code is distRedispatch, use the
       *    default rules to figure out who to go to next.
       *  - Otherwise, the response is distOk --> just finish.
       */

      if (! static_route_initialized
          && DistillerBufferLength(&dout.metadata) > 0) {
        /* look for X-Static-Route */
        int tmp_len;
        const char *static_route_hdr =
          get_header_value(&dout.metadata,
                           "x-static-route", &tmp_len, NULL, NULL);
        if (static_route_hdr != NULL && tmp_len > 0) {
          static_route = ALLOCA(tmp_len+1);
          strncpy(static_route, static_route_hdr, tmp_len);
          static_route[tmp_len] = '\0';
          static_route_initialized = 1;
          delete_header(&dout.metadata, "x-static-route");
        }
      }

      if (static_route ||
          (status == distRedispatch && !static_route_initialized)) {
        /*
         *  this is a redispatch.
         *  The redispatch strategy is as follows.  Since the
         * DistillerInput pointer starts out being  a *clone* of the
         * server headers and data, it's always safe to free it.  THen move the
         *  DistillerOutput pointer (result of previous pipestage) to the
         *  DistillerInput pointer of this pipestage.  
         */

        DistillerBufferFree(&din.metadata);
        DistillerBufferFree(&din.data);
        DistillerBufferClone(&din.metadata, &dout.metadata);
        DistillerBufferClone(&din.data, &dout.data);
        DistillerBufferFree(&dout.metadata);
        DistillerBufferFree(&dout.data);

        /*
         *  Fix the content-length and content-type, if nec.
         */
        if (get_header_value(&din.metadata, "content-type",NULL,NULL,NULL)
            == NULL)
          insert_header(&din.metadata, "Content-type", din.mimeType, 1);
        if (get_header_value(&din.metadata, "content-length", NULL,NULL,NULL)
            == NULL) {
          char __tmp[12];
          snprintf(__tmp, 11, "%lu", DistillerBufferLength(&din.data));
          insert_header(&din.metadata, "Content-length", __tmp, 1);
        }
        if (static_route) {
          status = distRedispatch;
          /* add the X-Route header to this request */
          insert_header(&din.metadata, "X-Route", static_route, 1);
          /* scan forward to the next component of the path */
          while (*static_route && (*static_route != ';')) {
            static_route++;
          }
          if (*static_route == '\0') {
            static_route = NULL;
          } else {
            /* skip semicolon and any spaces */
            while (*static_route && ((*static_route == ';')
                                     || (*static_route == ' '))) {
              static_route++;
            }
            if (*static_route == '\0')
              static_route = NULL;
          }
        }
        redispatch++;
        t->task_age++; /* "time to live" of this request */
        retcode = distRedispatch;
      } else {
        /*
         *  Here if any of the following were true:
         *  - status was distOk and no static route header overrides this
         *  - status was distRedispatch, but an existing static route header
         *     indicates that we should override and finish with 
         */
#ifdef LOGGING
        lo->size_after = DistillerBufferLength(&dout.data);
#endif /* LOGGING */
        status = retcode = distOk;
      }
      break;

    default:                    /* some other distiller/ptm error */
      /*
       *  BUG::we shouldn't make this visible to the user unless the
       * "guru" argument is set
       */
      snprintf(hp->errmsg, HTTP_ERRMSG_MAX,
               (char *)FE_getDistillerStatusString(status));
      /*      retcode = HTTP_ERR_DISTILLER; XXX - XXX - XXX - */
      retcode = status;
      break;
      
    } /* switch(status) */

    /* Note that the data size is set last.  So if the data size is not zero,
       it's a reasonable indication that this is a compelte set of valid
       measurements.  (should really have a separate valid bit)
       */
    INST_set_size(thrindex, DistillerBufferLength(&dout.data));

  }  while ((status == distRedispatch)
            &&  t->task_age <= PERF_REQUEST_TTL);  /* do */
  
  /*
   *  "Copy" final output buffer to hp->pxy_hdrs/hp->pxy_data.  (Really
   *  we just copy the pointers, since the pxy_hdrs/pxy_data buffers
   *  will be freed by the caller.)
   *
   *  First generate headers to return to client.  If last-stage
   *  distiller returned
   *  some headers, use them; otherwise, replace the "content-length"
   *  and "content-type" fields of the ORIGINAL server headers with
   *  new values deduced from the distiller data.
   */
  if (status == distOk) {
    if (DistillerBufferLength(&(dout.metadata)) > 0) {
      DistillerBufferClone(&hp->pxy_hdrs, &dout.metadata);
    } else {
      /*
       *  If the server headers begin with an HTTP/x.x response code, they are
       *  headers from an origin server; OK to clone them and just replace the
       *  content-type and content-length.  Otherwise, they are probably
       *  the result of a server-mode dispatch worker, which means they
       *  actually look like request headers (not response headers), so delete them.
       */
      if (DistillerBufferLength(&hp->svr_hdrs) > 0
          && strncasecmp(DistillerBufferData(&hp->svr_hdrs), "HTTP/", 5) == 0) {
        DistillerBufferClone(&hp->pxy_hdrs, &hp->svr_hdrs);
        delete_header(&hp->pxy_hdrs,  "content-type");
        delete_header(&hp->pxy_hdrs,  "content-length");
      } else {
        char *s = "HTTP/1.0 200 OK\r\n\r\n";
        int l = strlen(s);
        DistillerBufferAlloc(&hp->pxy_hdrs, 1+l);
        strcpy((char *)(hp->pxy_hdrs.data), s);
        DistillerBufferSetLength(&hp->pxy_hdrs,l);
      }
    }
  
    /*
   *  Replace the "Content-length" and "Content-type"
   *  headers with the correct info.  Leave other headers
   *  alone.
   */
    if (get_header_value(&hp->pxy_hdrs, "content-type", NULL,NULL,NULL) 
	== NULL) {
      insert_header(&hp->pxy_hdrs, "Content-type", dout.mimeType, 1);
    }
    
    if (get_header_value(&hp->pxy_hdrs,"content-length",NULL,NULL,NULL)
	== NULL) {
      char __tmp[12];
      snprintf(__tmp, 11, "%lu", DistillerBufferLength(&dout.data));
      insert_header(&hp->pxy_hdrs, "Content-length", __tmp, 1);
    }

    DistillerBufferClone(&hp->pxy_data, &dout.data);
  }

  if ((status == distOk) || (status == distRedispatch)) {
    DistillerBufferFree(&dout.data);
    DistillerBufferFree(&dout.metadata);
  }

  /* Always free this */
  DistillerBufferFree(&din.data);
  DistillerBufferFree(&din.metadata);
  
  /* BUG::must do Clib_Put here??? */
  return (retcode);
}

/*
 *  HTTP headers looked OK, and we were able to determine what type of
 *  distiller was needed, so go ahead and do the distillation, returning
 *  the results to the client.  If distillation fails, return the
 *  original content and headers to the client.
 *
 *  ARGS:
 *    i:  URL (BUG::shouldn't be needed - it's currently used to
 *      determine distiller type for "aggregator" virtual-site URLs )
 *    i: input to distiller (from previous "pipestage" or from server)
 *    o: output from distiller
 *    o: distiller type descriptor
 *    i: argument list for distiller
 *
 *  RETURNS:  Distiller status for the distillation
 *  REENTRANT: yes
 *  SIDE EFFECTS:
 *    If distillation succeeds, fills in new headers (for return to client)
 *      in DistillerOutput structure
 *  ASSUMTPIONS:
 *    None
 */
static DistillerStatus
do_distillation(const char *url,
                DistillerInput *dinp, DistillerOutput *doutp,
                C_DistillerType *dtype,
                ArgumentList *al)
{
  DistillerStatus result = distFatalError;
  Argument *arg;
  const char *whichuser = "transend";
  const char *mime;
  int mime_len;

  /*
   *  HTTP headers look OK.  Extract the MIME type, figure out what
   *  distiller to use, and dispatch to it.
   */

  if ((arg = getArgumentFromIdInList(al, FRONT_USERID))) {
    whichuser = ARG_STRING(*arg);
  }

  /*
   *  If this is the set-prefs page, be sure the user's args get munged into it
   * REMOVED by fox since it interferes with the "mini-httpd" worker (miniserver.pl):
   * this routing is now enforced by X-Route instead
   */
  if (0 && strcasecmp(url, fe_set_prefs_url) == 0) {
    SET_DISTILLER_TYPE(*dtype, "transend/text/html");
  } else {
    set_distiller_type(&dinp->metadata,
                       (const char *)url, 
                       whichuser, dtype);
  }
  
  if ((mime = get_header_value(&dinp->metadata,
                               "content-type",
                               &mime_len, NULL, NULL)) != NULL) {
    strncpy(dinp->mimeType, mime, MIN(mime_len,MAX_MIMETYPE-1));
    dinp->mimeType[MIN(mime_len,MAX_MIMETYPE-1)] = '\0';
  } else {
    strcpy(dinp->mimeType, "text/html");
  }
  result = Distill(dtype, al->arg, al->nargs, dinp, doutp);
  return result;
}
/*
 *  Set the distiller type based on the HTTP MIME headers,
 *  the generic user id, and/or the URL, as follows:
 *
 *  1. If headers contain X-Route: dist1;dist2;...;distN, (or X-Route-Strict),
 *     mutate the header by 
 *     popping off the head of that list, and use that as the distiller.
 *     EXCEPTION: the "prefs" page always gets the system distiller.
 *  2. Otherwise, grab the MIME type (from headers; if not found, assume
 *     text/html), and prepend either the user's userid or "transend" if the
 *     user has no registered userid.
 *
 *  If PTM reports distiller not found, also try using the transend-generic
 *     distiller for this MIME type, *unless* the routing header 
 *     indicated strict routing.
 *
 *  ASSUMPTIONS: none
 *  THREAD-SAFE: yes
 *  RETURNS: gm_True iff OK to retry distillation using system's generic
 *     distillers 
 *  SIDE EFFECTS: none
 */
  
static void
set_distiller_type(DistillerBuffer *mime_headers, const char *url,
                   const char *user, C_DistillerType *dtype)
{
  const char *route = NULL;
  int route_len;
  char *disttype;

  if ((route = get_header_value(mime_headers,
                                "x-route",&route_len,NULL,NULL))
      == NULL) {

    /* Aggregator? */

    if (strncasecmp(url, fe_agg_string, strlen(fe_agg_string)) == 0) {
      /* distiller "type" is everything up to but not incl next slash */
      char disttype_tmp[256];
      int i,j;
      for (i=0, j=strlen(fe_agg_string);
           i < strlen(url) && url[j] != '/';
           i++,j++)
        ;
      strncpy(disttype_tmp, url+strlen(fe_agg_string), i);
      disttype_tmp[i] = '\0';
      SET_DISTILLER_TYPE(*dtype, disttype_tmp);

    } else {
      /* generic transformation */
      set_distiller_type_generic(mime_headers, url, user, dtype);
    }
  } else {
    
    /* pop off next element, up to a semicolon */
    char *header_value = (char *)ALLOCA(route_len+1);
    char *semicolon;

    strncpy(header_value, route, route_len);
    header_value[route_len] = '\0';
    
    if ((semicolon = strchr(header_value, ';')) != NULL) {
      int i;
      for (i=0; i<route_len && route[i] != ';'; i++)
        ;
      disttype = (char*)ALLOCA(i+2);
      strncpy(disttype, route, i);
      disttype[i] = '\0';
      insert_header(mime_headers, "x-route", semicolon+1, 0);
    } else {
      /* no semicolon, this is the last guy in the list */
      disttype = (char *)ALLOCA(route_len+2);
      strncpy(disttype, route, route_len);
      disttype[route_len] = '\0';
    }
    delete_header(mime_headers, "x-route");
    SET_DISTILLER_TYPE(*dtype, disttype);
  }
}

static void
set_distiller_type_generic(DistillerBuffer *mime_headers, const char *url,
                           const char *user, C_DistillerType *dtype)
{
  char *mimetype;
  int mimetype_len;
  char disttype[MAX_ARG_STRING + MAX_MIMETYPE + 1];

  /*
   *  Here if no X-Route header: use MIME type and/or username. 
   */
  mimetype = (char *)get_header_value(mime_headers,
                                      "content-type",
                                      &mimetype_len, NULL, NULL);
  if (mimetype == NULL) {
    if (user == NULL || *user == '\0')
      sprintf(disttype, "text/html");
    else
      sprintf(disttype, "%s/text/html", user);
  } else {
    if (user && *user)
      sprintf(disttype, "%s/", user);
    else
      disttype[0] = '\0';
    strncat(disttype, mimetype, mimetype_len);
  }
  SET_DISTILLER_TYPE(*dtype, disttype);
}
    
