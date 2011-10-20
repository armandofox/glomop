/*
 *  winggo.c
 *  This file contains the request handling code that defines the
 *  TranSend distillation web proxy.  It corresponds to the TranSend Service
 *  layer in the layered SNS model.  This particular set of code handles
 *  the GLALFTP protocol to be used with Wingman compatible browsers.
 */

#define DEBUGGING
#include "frontend.h"
#include "debug.h"
#include "comm_http.h"
#include "userpref.h"
#include "url_magic.h"
#include "ARGS.h"
#include "utils.h"
#include "wingreq.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifdef LOGGING
/*
#include "md5.h"
static char magicKey[] = "aa883fd833ef";
*/
#endif /* LOGGING */

/* Metadata for uncachable content and the "is a database" flag */
static char uncache_metadata[] = 
  { 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };
static char contenttype_metadata[] = 
  { 0x00, 0x03, 0x00, 0x02, 0x00, 0x00 };

static userkey userkey_from_sock_ipaddr(int sock);
static void    wing_error_return(Request *h, 
				 task_t *t,
				 ArgumentList prefs,
				 int index,
				 gl_hdr *hdrs);
#if 0
static void wing_raw_return(int fd, UINT32 vers, UINT32 ID, UINT32 metalen,
    UINT32 datalen, UINT32 comp, unsigned char *metadata, unsigned char *data);
#endif

/* What client versions support what features? */
#define version_check(vers, needvers) \
    (((unsigned long)((vers)&0xffffff00UL)) >= \
     ((unsigned long)((needvers)&0xffffff00UL)))

#define CLIENT_VERSION_PLAIN 0x01000200
#define CLIENT_VERSION_AGGLIST 0x01000200
#define CLIENT_VERSION_LOADDB 0x01000300
#define CLIENT_VERSION_NEWAGG 0x01050300

#define AGGLISTURL "http://www.isaac.cs.berkeley.edu/pilot/wingman/nph-agg.cgi/agglist/"

#define DEFAULT_AGGLIST_URL "agg://agglist/"
#define DEFAULT_ABOUT_URL "http://www.isaac.cs.berkeley.edu/pilot/wingman/about-"

     /*#define CONTENT_TYPE_AGG        "application/x-wingman-agg"
       #define CONTENT_TYPE_AGGLIST    "application/x-wingman-agglist"
       #define CONTENT_TYPE_WINGDATA   "application/x-wingman"
       #define CONTENT_TYPE_PALMOSDB   "application/x-palmosdb"*/

#define CONTENT_TYPE_WINGPREFIX       "x-wingman/"
#define CONTENT_TYPE_AGG              "application/x-wingman-agg"
#define CONTENT_TYPE_WINGMAN_(type)   CONTENT_TYPE_WINGPREFIX ## #type
#define CONTENT_TYPE_WINGMAN(type)    CONTENT_TYPE_WINGMAN_(type)


/* See if the passed content type matches the given one, and the versions are
   compatible.  Note that the passed content_type is _not_ NUL-terminated. */
static int typecmp(char *matchtype, unsigned int matchvers, char *contenttype,
    unsigned int vers)
{
    int l;
    char c;

    if (!contenttype) return 1;
    if (!version_check(vers, matchvers)) return 1;
    l = strlen(matchtype);
    if (strncasecmp(contenttype, matchtype, l)) return 1;
    c = contenttype[l];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	(c >= '0' && c <= '9') || c == '-' || c == '_') {
	return 1;
    }
    return 0;
}


/*
 *  wing_read_client_request: reads in an entire client request from the 
 *  socket. Times out if the data cannot be read
 *
 *  ARGS:
 *  i/o: the wing_request structure; its http_req.cli_fd field contains 
 *      the file number of the socket on which the request is arriving.
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_read_client_request(wing_request *req)
{
  int res, timed_out;
  struct timeval to;
  unsigned char hdr_buf[sizeof_gl_hdr];

  /* Read the wingman protocol header */

  to.tv_sec = PERF_HTTP_REQ_TIMEOUT;
  to.tv_usec = 0;
  res = correct_read_to(req->http_req.cli_fd, (char *) hdr_buf, 
			sizeof_gl_hdr, to, &timed_out);
  if (timed_out || res != sizeof_gl_hdr) {
    /* We timed out or had a socket error */
    return gm_False;
  }


  /* We have our headers - let's extract out the fields */
  memcpy(&req->wing_hdr.version, hdr_buf, 4);
  req->wing_hdr.version = ntohl(req->wing_hdr.version);

  /* Sanity check on the version number */
  if ((req->wing_hdr.version & 0xffff0000) < 0x01000000 ||
      (req->wing_hdr.version & 0xffff0000) >= 0x02000000) {
    /* Failed the sanity check */
    return gm_False;
  }

  memcpy(&req->wing_hdr.request_id, hdr_buf+4, 4);
  req->wing_hdr.request_id = ntohl(req->wing_hdr.request_id);

  memcpy(&req->wing_hdr.metadatasize, hdr_buf+8, 4);
  req->wing_hdr.metadatasize = ntohl(req->wing_hdr.metadatasize);

  memcpy(&req->wing_hdr.datasize, hdr_buf+12, 4);
  req->wing_hdr.datasize = ntohl(req->wing_hdr.datasize);

  memcpy(&req->wing_hdr.comp, hdr_buf+16, 4);
  req->wing_hdr.comp = ntohl(req->wing_hdr.comp);

  proxy_debug_3(DBG_WING, "version %lu", req->wing_hdr.version);
  proxy_debug_4(DBG_WING, "reqid %lu metadatasize %lu", 
		req->wing_hdr.request_id, req->wing_hdr.metadatasize);
  proxy_debug_4(DBG_WING, "datasize %lu comp %lu", req->wing_hdr.datasize,
		req->wing_hdr.comp);
  if (req->wing_hdr.datasize == 0) {
    /* Some sort of bogosity - no control messages; we need data */
    return gm_False;
  }

  /* Now pull down the metadata */
  if (req->wing_hdr.metadatasize > 0) {
    to.tv_sec = PERF_HTTP_REQ_TIMEOUT;
    to.tv_usec = 0;
    assert(DistillerBufferAlloc(&req->wing_metadata, 
				req->wing_hdr.metadatasize+1) == gm_True);
    res = correct_read_to(req->http_req.cli_fd, 
			  (char *) DistillerBufferData(&req->wing_metadata),
			  req->wing_hdr.metadatasize, to, &timed_out);
    if (timed_out || res != req->wing_hdr.metadatasize) {
      /* Timed out or had socket error */
      return gm_False;
    }
    /* NULL terminate */
    *(((char *) DistillerBufferData(&req->wing_metadata)) + 
      req->wing_hdr.metadatasize) = '\0';
    proxy_debug_3(DBG_WING, "Metadata: %s",
		  DistillerBufferData(&req->wing_metadata));
  }

  /* Now pull down the data */
  if (req->wing_hdr.datasize > 0) {
    to.tv_sec = PERF_HTTP_REQ_TIMEOUT;
    to.tv_usec = 0;
    assert(DistillerBufferAlloc(&req->wing_data, req->wing_hdr.datasize+1)
	   == gm_True);
    res = correct_read_to(req->http_req.cli_fd,
			  (char *) DistillerBufferData(&req->wing_data),
			  req->wing_hdr.datasize, to, &timed_out);
    if (timed_out || res != req->wing_hdr.datasize) {
      /* Timed out or had socket error */
      return gm_False;
    }
    /* NULL terminate */
    *(((char *) DistillerBufferData(&req->wing_data)) + 
      req->wing_hdr.datasize) = '\0';
    proxy_debug_3(DBG_WING, "Data: %s", DistillerBufferData(&req->wing_data));
  }

  return gm_True;
}


/*
 *  wing_parse_metadata: parse out the wing_metadata field of the incoming
 *      request and set the appropriate flags in the req->flags structure
 *  ARGS:
 *  i/o: the wing_request structure; its wing_hdr field contains the wingman
 *      protocol header, the wing_data and wing_metadata fields contain the
 *      client request's data and metadata respectively
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_parse_metadata(wing_request *req)
{
  /* Parse incoming metadata */
  if (req->wing_hdr.metadatasize > 0) {
    unsigned char *metaptr, *endptr;
    
    metaptr = DistillerBufferData(&req->wing_metadata);
    endptr = metaptr + req->wing_hdr.metadatasize;

    /* Keep scanning until we run off the buffer */
    while ( (metaptr+4) <= endptr) {
      unsigned int m_type, m_len;

      m_type = ((*metaptr) << 8) + (*(metaptr+1));
      metaptr+=2;
      m_len = ((*metaptr) << 8) + (*(metaptr+1));
      metaptr+=2;

      switch(m_type) {
      case MetaCacheExpire:
      case MetaPageType:
	/* Wacked - this should only occur for server->client pragmas */
	break;

      case MetaNoCache:
	/* Aha - client has requested a nocache */
	req->flags.out_nocache = 1;
	break;

#ifdef BACKWARD_COMPATIBLE
      case MetaAggList:
	if (! version_check(req->wing_hdr.version, CLIENT_VERSION_NEWAGG)) {
	  /* Send an aggregator list? OK! */
	  req->flags.out_sendagglist = 1;
	  req->flags.out_nocache     = 1; /* Don't cache the agglist */
	}
	break;
#endif

      default:
	break;
      }
      metaptr += m_len;
    }
  }

  return gm_True;
}


/*
 *  wing_grab_userprefs: use the client's IP address to grab the user-prefs
 *      from the userpref database; Set the userid in the user-prefs to 
 *      "wingman". Also, add the FRONT_CLI_VERSION, FRONT_CLIENT_IP and
 *      FRONT_NOCACHE user prefs
 *  ARGS:
 *  i/o: the wing_request structure; its prefs field is filled with the
 *      client's user prefs.
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_grab_userprefs(wing_request *req)
{
  userkey k;
  int i;

  /* Grab the user's preferences.  Also add in the s11=wingman pref,
     and the client's IP address pref. */

  k = userkey_from_sock_ipaddr(req->http_req.cli_fd);
#ifdef LOGGING
  req->http_req.lo->ipaddr = k;
#endif /* LOGGING */
  (void)get_userprefs(k, &req->prefs);
  for (i=0; i<req->prefs.nargs; i++) {
    if (req->prefs.arg[i].id == FRONT_USERID) {
      SET_ARG_STRING(req->prefs.arg[i], "wingman");
      break;
    }
  }
  if (i == req->prefs.nargs) {
    assert(req->prefs.nargs != MAX_ARGS);
    SET_ARG_ID(req->prefs.arg[i], FRONT_USERID);
    SET_ARG_STRING(req->prefs.arg[i], "wingman");
    req->prefs.nargs++;
  }
  for (i=0; i<req->prefs.nargs; i++) {
    if (req->prefs.arg[i].id == FRONT_CLI_VERSION) {
      SET_ARG_INT(req->prefs.arg[i], req->wing_hdr.version);
      break;
    }
  }
  if (i == req->prefs.nargs) {
    assert(req->prefs.nargs != MAX_ARGS);
    SET_ARG_ID(req->prefs.arg[i], FRONT_CLI_VERSION);
    SET_ARG_INT(req->prefs.arg[i], req->wing_hdr.version);
    req->prefs.nargs++;
  }
  assert(req->prefs.nargs != MAX_ARGS);
  SET_ARG_INT(req->prefs.arg[req->prefs.nargs], (INT32) k);
  SET_ARG_ID(req->prefs.arg[req->prefs.nargs], FRONT_CLIENT_IP);
  req->prefs.nargs++;

  /* If we're sending a no-cache header to the server, we also need to set
     a no-cache argument so the distiller knows to send no-cache on inlines.
     */
  if (req->flags.out_nocache) {
    assert(req->prefs.nargs != MAX_ARGS);
    SET_ARG_INT(req->prefs.arg[req->prefs.nargs], (INT32) 1);
    SET_ARG_ID(req->prefs.arg[req->prefs.nargs], FRONT_NOCACHE);
    req->prefs.nargs++;
  }

  return gm_True;
}


/*
 *  wing_parse_url: parse out the client's requested URL from the wing_data
 *      field; replace the original replace URL with a new corresponding one,
 *      if necessary
 *  ARGS:
 *  i/o: the wing_request structure; its wing_data field contains a 
 *      NULL-terminated URL string
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_parse_url(wing_request *req)
{
  /* Copy in the URL - need to prepend http:/ / if it's not already
     in there, and we should demagify. XXXX - what about chained
     URLs, i.e. http://foo.bar/http://baz/bof/ ? */

  extern Options runtime_options;

  {
    char *magicURL;
    char *s,*d;
    int extrasize = 8;  /* The size of "http://" */
    const char *urlprefix = "";
    int urloffset = 0;

    /* Is this "view source"? */
    if (!strncmp(DistillerBufferData(&req->wing_data), "src:", 4) ||
	!strncmp(DistillerBufferData(&req->wing_data), "doc:", 4)) {
	int i, len;
	int isdoc = (!strncmp(DistillerBufferData(&req->wing_data), "doc:",4));

	len = DistillerBufferLength(&req->wing_data)-4;
	memmove(DistillerBufferData(&req->wing_data),
	    ((char *)DistillerBufferData(&req->wing_data))+4, len);
	DistillerBufferSetLength(&req->wing_data, len);

	for (i=0; i<req->prefs.nargs; i++) {
	    if (req->prefs.arg[i].id == FRONT_SRCFLAG) {
	      SET_ARG_INT(req->prefs.arg[i], isdoc ? 2 : 1);
	      break;
	    }
	}
	if (i == req->prefs.nargs) {
	    assert(req->prefs.nargs != MAX_ARGS);
	    SET_ARG_ID(req->prefs.arg[i], FRONT_SRCFLAG);
	    SET_ARG_INT(req->prefs.arg[i], isdoc ? 2 : 1);
	    req->prefs.nargs++;
	}
    }

    /* Check if this is an "about" URL */
    if (!strncmp(DistillerBufferData(&req->wing_data), "about:", 6)) {
      urlprefix = Options_Find(runtime_options, "wingman.about_url");
      if (urlprefix==NULL) {
	urlprefix = DEFAULT_ABOUT_URL;
      }
      extrasize = strlen(urlprefix) + 1;
      urloffset = 6;
    }

    magicURL = (char *) malloc(req->wing_hdr.datasize+extrasize);
    assert(magicURL);
    strcpy(magicURL, urlprefix);
    
    /* Allow http://, ftp://, gopher://, etc. to work */
    if (strncmp(DistillerBufferData(&req->wing_data), "http://", 7) &&
	strncmp(DistillerBufferData(&req->wing_data), "agg://", 6) &&
	strncmp(DistillerBufferData(&req->wing_data), "about:", 6) &&
	strncmp(DistillerBufferData(&req->wing_data), "ftp://", 6) &&
	strncmp(DistillerBufferData(&req->wing_data), "gopher://", 9)) {
      strcat(magicURL, "http://");
    }
    
    /* create the magic url; remove any special chars; replace %5E with ^ */
    strcat(magicURL, ((char *)DistillerBufferData(&req->wing_data))+urloffset);
    for(d=s=magicURL; *s; ++s, ++d) {
	if (*s < '!' || *s > 0x7e) {
	    /* Someone tried to sneak an illegal char in the URL */
	    *d = '+';
	} else if (*s == '%' && s[1] == '5' && (s[2] == 'E' || s[2] == 'e')) {
	    *d = '^';
	    s += 2;
	} else {
	    *d = *s;
	}
    }
    *d = '\0';
    req->http_req.url = (char *) malloc(req->wing_hdr.datasize+extrasize);
    assert(req->http_req.url);
    from_magic(magicURL, req->http_req.url, &req->prefs);
    free(magicURL);
    magicURL = NULL;
    proxy_debug_3(DBG_WING, "URL is %s", req->http_req.url);

#ifdef BACKWARD_COMPATIBLE
    if ((!version_check(req->wing_hdr.version, CLIENT_VERSION_NEWAGG)) && 
	req->flags.out_sendagglist) {
      const char *url;
      req->saveoldurl = req->http_req.url;
      url = Options_Find(runtime_options, "wingman.agglist_url");
      if (url==NULL) url = DEFAULT_AGGLIST_URL;
      req->http_req.url = strdup(url);
    }
#endif
  }
  
  /* Copy in the method - assume GET for now */
  {
    req->http_req.method = (char *) malloc(sizeof(char) * (strlen("GET")+1));
    assert(req->http_req.method);
    strcpy(req->http_req.method, "GET");
  }

  /* Just fake an HTTP/1.0 request, I guess */
  req->http_req.version = HTTP_VERSION_10_OR_LATER;

  return gm_True;
}


/*
 *  wing_create_client_http_headers: create HTTP client headers from
 *      the wingman request
 *  ARGS:
 *  i/o: the wing_request structure; 
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_create_client_http_headers(wing_request *req)
{
  char *hostpart = NULL;
  int hostlen = 0;
  
  /* Find the value for the Host: header */
  if (!strncmp(req->http_req.url, "http://", 7)) {
    char *hp;
    hp = hostpart = req->http_req.url + 7;
    for(hostlen = 0; *hp && *hp != ':' && *hp != '/'; ++hp) ++hostlen;
  } else  {
    hostpart = NULL;
    hostlen = 0;
  }
  
  /* Create and copy in client headers */
  {
    static char fake_nocache_hdr[] = "Pragma: no-cache\r\n";
    static char fake_headers[] = 
      "User-Agent: Top Gun Wingman (Pilot OS/2.0)\r\nAccept: text/*, "
      "image/gif, image/jpeg\r\n\r\n";

    /* Malloc space for GET <url> HTTP/1.0\r\n<fake_headers> */
    assert(DistillerBufferAlloc(&req->http_req.cli_hdrs, 
				strlen(fake_headers)+
				strlen(fake_nocache_hdr)+
				hostlen+8+1) == gm_True);
    sprintf(DistillerBufferData(&req->http_req.cli_hdrs),
	    "%s%s%.*s%s%s", 
	      req->flags.out_nocache ? fake_nocache_hdr : "", 
	      hostlen ? "Host: " : "", hostlen,
	      hostlen ? hostpart : "", hostlen ? "\r\n" : "",
	      fake_headers);
    proxy_debug_3(DBG_WING, "Client headers '%s'",
		  DistillerBufferData(&req->http_req.cli_hdrs));
  }

  return gm_True;
}


/*
 *  wing_dispatch_request: the main dispatch loop.
 *      The loop is executed as long as an HTTP-redirect response is
 *      received fromn the server/distiller (limited, of course, to some max)
 *      If the request URL is not an aggregator URL, then invoke 
 *      server_dispatch
 *      If the response is not an HTTP-redirect, invoke proxy dispatch
 *  ARGS:
 *  i/o: the wing_request structure; 
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_dispatch_request(wing_request *req)
{
  int index = TASK_THRINDEX(req->task), num_redirect=0, tot_num_redirect=0, 
    nfound;
  HTTP_Status result;
  DistillerStatus dist_result;
  DistillerBuffer *current_http_hdrs=NULL, *current_http_data=NULL;
  UINT32 current_http_status;

  /*
   * do
   *    if agg, construct fake response
   *    else do server_dispatch
   *    if response is not 300, reset ctr and do proxy_dispatch
   * while response is not 300 and ctr < max
   */

  while (num_redirect <= PERF_HTTP_MAX_REDIRECT &&
	 tot_num_redirect <= 3 * PERF_HTTP_MAX_REDIRECT) {
    
    if (!strncmp(req->http_req.url, "agg://", 6)) {
      /* this is an aggregator URL */
      static char agg_svr_reply[] = 
	"HTTP/1.0 200 OK\r\n"
	"Content-type: " CONTENT_TYPE_AGG "\r\n\r\n";
      int len;

      req->http_req.svr_http_status = 200;
      DistillerBufferSetStatic(&req->http_req.svr_hdrs, agg_svr_reply, 
			       strlen(agg_svr_reply));

      len = strlen(req->http_req.url);
      DistillerBufferAlloc(&req->http_req.svr_data, len);
      strncpy(DistillerBufferData(&req->http_req.svr_data), 
	      req->http_req.url, len);
    } else {
      if (wing_create_client_http_headers(req)==gm_False) return gm_False;

      /* Now, get the data from the server */
      req->http_req.errmsg[0] = '\0';
      result = server_dispatch(&req->prefs, req->task);
      /* If transport-level error, wrap error in HTML and continue as though
	 we got a good result from the cache. XXX - we need a wingman transport
	 level error reporting mechanism for this case, I believe. */
      if (result != HTTP_NO_ERR) {
	if (req->http_req.errmsg[0] == '\0')
	  sprintf(req->http_req.errmsg, 
		  "Internal proxy error: unknown server_dispatch "
		  "error %d.  Please retry, and report this bug if "
		  "it persists.", result);
	wing_error_return(&req->http_req, req->task, req->prefs, 
			  index, &req->wing_hdr);
	return gm_False;
      }
      proxy_debug_3(DBG_WING, "Server headers '%s'",
		    DistillerBufferData(&req->http_req.svr_hdrs));
      proxy_debug_3(DBG_WING, "Server datalen %d",
		    DistillerBufferLength(&req->http_req.svr_data));
    }

    /* XXX - this is where we should check for a redirect... */
    if (  !((req->http_req.svr_http_status >= 300) && 
	    (req->http_req.svr_http_status < 400))  ) {

      /*
       * reset the num_redirect counter
       */
      num_redirect = 0;

      /* 
       * Give data to distiller.
       */
      req->http_req.errmsg[0] = '\0';
      INST_set_thread_state(index, THR_DISTILLERSEND);
      dist_result = proxy_dispatch(&req->prefs, req->task);
      switch(dist_result) {
      case distOk:
	/* Fall through into writeback phase */
	break;
      case distDistillerNotFound:
      case distLaunchTimeout:
      case distBadInput:
      case distConnectionBroken:
      default:
	proxy_debug_3(DBG_WING, 
		      "Non-distOk result returned by proxy-dispatch (%d)",
		      dist_result);
	if (req->http_req.errmsg[0] == '\0')
	  sprintf(req->http_req.errmsg, 
		  "Distiller error - non-distOk returned by proxy-dispatch "
		  "(%d) - please report this internal error to the transend "
		  "team, along with the URL that generated it.",
		  dist_result);
	wing_error_return(&req->http_req, req->task, req->prefs, index, 
			  &req->wing_hdr);
	return gm_False;
      }

      current_http_hdrs = &req->http_req.pxy_hdrs;
      current_http_data = &req->http_req.pxy_data;

      nfound = sscanf(DistillerBufferData(current_http_hdrs), "%*s %lu",
		      &current_http_status);
      if (nfound < 1) {             /* no status found */
	current_http_status = 0;
      }
    }
    else {
      current_http_hdrs = &req->http_req.svr_hdrs;
      current_http_data = &req->http_req.svr_data;
      current_http_status = req->http_req.svr_http_status;
    }

#ifdef BACKWARD_COMPATIBLE
    if (! version_check(req->wing_hdr.version, CLIENT_VERSION_NEWAGG)) {
      const char *content_type;
      content_type = get_header_value(current_http_hdrs,
				      "content-type", NULL, NULL, NULL);
      if (!typecmp(CONTENT_TYPE_WINGMAN(PageTypeAgglist), 
		   CLIENT_VERSION_AGGLIST,
		   (char *)content_type, req->wing_hdr.version)) {
	req->agglistlen = DistillerBufferLength(current_http_data);
	if (req->agglistdata!=NULL) free(req->agglistdata);
	req->agglistdata = malloc(req->agglistlen);
	assert(req->agglistdata);
	memmove(req->agglistdata, DistillerBufferData(current_http_data),
		req->agglistlen);

	/* now redirect to the original URL */
	current_http_status = 300;
      }
    }
#endif

    if ((current_http_status >= 300) && 
	(current_http_status < 400)) {
      char *location, *locend; /* XXX , *content_type;*/
      
      /* See if we should be paying any attention to this content, anyway. */
      /* XXX content_type = (char *)get_header_value(current_http_hdrs,
                                              "content-type", NULL, NULL,
                                              NULL);*/

      /* Grab the "Location" header, and go there */
#ifdef BACKWARD_COMPATIBLE
      if (version_check(req->wing_hdr.version, CLIENT_VERSION_NEWAGG) ||
	  req->saveoldurl==0) {
#endif
	location = (char *) get_header_value(current_http_hdrs,
					     "location", 
					     NULL, NULL, NULL);
	if (location == NULL) {
	  proxy_debug_2(DBG_WING, "Couldn't find location in redirect.");
	  if (req->http_req.errmsg[0] == '\0')
	    sprintf(req->http_req.errmsg, 
		    "Couldn't find Location header in redirect - "
		    "this server is badly behaved.  Please report "
		    "this to the web page's administrator.");
	  wing_error_return(&req->http_req, req->task, req->prefs, index, 
			    &req->wing_hdr);
	  return gm_False;
	}
	  
	locend = strchr(location, '\r');
	if (locend == NULL) {
	  proxy_debug_2(DBG_WING, "Ill-formatted location in redirect.");
	  if (req->http_req.errmsg[0] == '\0')
	    sprintf(req->http_req.errmsg, 
		    "Ill-formatted location in redirect - "
		      "this server is badly behaved.  Please report this "
		    "to the web page's administrator.");
	  wing_error_return(&req->http_req, req->task, req->prefs, index, 
			    &req->wing_hdr);
	  return gm_False;
	}
	
	/* Let's copy in the location */
	if (req->http_req.url != NULL)
	  free(req->http_req.url);
	req->http_req.url = 
	  (char *) malloc(sizeof(char) * ((locend-location)+2));
	assert(req->http_req.url);
	memcpy(req->http_req.url, location, (locend-location));
	req->http_req.url[locend-location] = '\0';
#ifdef BACKWARD_COMPATIBLE
      } else {
	  if (req->http_req.url) free(req->http_req.url);
	  req->http_req.url = req->saveoldurl;
	  req->saveoldurl = NULL;
      }
#endif
      proxy_debug_3(DBG_WING, "Redirected URL is %s", req->http_req.url);

      /* Free up the client/server/proxy buffers from before */
      DistillerBufferFree(&req->http_req.cli_hdrs);
      DistillerBufferFree(&req->http_req.cli_data);

      DistillerBufferFree(&req->http_req.svr_hdrs);
      DistillerBufferFree(&req->http_req.svr_data);

      DistillerBufferFree(&req->http_req.pxy_hdrs);
      DistillerBufferFree(&req->http_req.pxy_data);

      /* All's well, keep trying */
      num_redirect++;
      tot_num_redirect++;
      continue;
    } else {

      /* 
       * we are done; we have done proxy dispatch, and the response is
       * not a HTTP redirect
       */

      break;
    }
  }

  if (num_redirect > PERF_HTTP_MAX_REDIRECT || 
      tot_num_redirect > 3 * PERF_HTTP_MAX_REDIRECT) {
    proxy_debug_2(DBG_WING, "Redirection loop detected.");
    if (req->http_req.errmsg[0] == '\0')
      sprintf(req->http_req.errmsg, "Redirection loop detected - the proxy "
	      "cannot resolve the URL, because the web server is badly "
	      "configured.");
    wing_error_return(&req->http_req, req->task, req->prefs, index, 
		      &req->wing_hdr);
    return gm_False;
  }

  /* Check to see if it is a non-200 return code */
  if (current_http_status != 200) {
    req->flags.in_nocache = 1;
    /* proxy_debug_3(DBG_WING, "Non-200 status code (%ld)", h.svr_http_status);
       if (h.errmsg[0] == '\0')
       sprintf(h.errmsg, "Non-200 status code (%ld) - the web server "
       "returned an error.", h.svr_http_status);
       wing_error_return(&h, t, prefs, index, hdrs);
       goto WINGGO_FINISH; */
  }

  return gm_True;
}


/*
 *  wing_writeback_reply: convert the HTTP server/distiller response to
 *      the wingman protocol and write it back to the client
 *  ARGS:
 *  i/o: the wing_request structure; the req->http_req.pxy_data/hdrs
 *      contains the HTTP response
 *  RETURNS: a gm_Bool; gm_True indicates that it's ok to proceed on to 
 *      the next step, gm_False indicates "abort the request"
 *  REENTRANT: yes
 *  
 */

gm_Bool
wing_writeback_reply(wing_request *req)
{
  const char *content_type;
  int content_type_len, content_type_int;

  /*
   * Check that the result is of a type the client can understand
   */
  {
    gm_Bool handled = gm_False;
    char *endptr;

    content_type = get_header_value(&req->http_req.pxy_hdrs,
				    "content-type", &content_type_len, 
				    NULL, NULL);
    if (content_type && !strncasecmp(content_type, CONTENT_TYPE_WINGPREFIX,
				     strlen(CONTENT_TYPE_WINGPREFIX))) {
      content_type_int = 
	strtoul(&content_type[strlen(CONTENT_TYPE_WINGPREFIX)], &endptr, 10);
      if ((endptr - content_type) == content_type_len) {
	/* 
	 * for now, we assume that distillers never return and parameters
	 * for x-wingman content-types 
	 */
	handled = gm_True;
      }
    }

    if (handled==gm_False) {
      proxy_debug_2(DBG_WING, "Content-type can't be handled");
      if (req->http_req.errmsg[0] == '\0') {
	char *endct;
	endct = content_type ? strchr(content_type, '\r') : NULL;
	if (endct) {
	  *endct = '\0';
	  snprintf(req->http_req.errmsg, HTTP_ERRMSG_MAX, 
		   "Content-type %s can't be handled yet by the proxy.", 
		   content_type);
	} else {
	  snprintf(req->http_req.errmsg, HTTP_ERRMSG_MAX, 
		   "Content-type can't be handled by the proxy.");
	}
      }
      wing_error_return(&req->http_req, req->task, req->prefs, 
			TASK_THRINDEX(req->task), &req->wing_hdr);
      return gm_False;
    }

  }

  /*
   * Check that the result is of a type the client can understand
   */
  {
    const char *pragma_value;
    pragma_value = get_header_value(&req->http_req.pxy_hdrs,
				    "pragma", NULL, NULL, NULL);
    if (pragma_value && !strncasecmp(pragma_value, "no-cache", 8)) {
      req->flags.in_nocache = 1;
    }
  }

#ifdef OLD
  {
    content_type = get_header_value(&req->http_req.pxy_hdrs,
				    "content-type", &content_type_len, 
				    NULL, NULL);

    if (!typecmp(CONTENT_TYPE_WINGMAN(PageTypeAgglist), CLIENT_VERSION_AGGLIST,
		 (char *)content_type, req->wing_hdr.version)) {
      req->flags.in_isdatabase = 0;
    }
    else if (!typecmp(CONTENT_TYPE_WINGMAN(PageTypeRegular), 
		      CLIENT_VERSION_PLAIN,
		      (char *)content_type, req->wing_hdr.version)) {
      req->flags.in_isdatabase = 0;
    } else if (!typecmp(CONTENT_TYPE_WINGMAN(PageTypeDatabase), 
			CLIENT_VERSION_LOADDB,
			(char *)content_type, req->wing_hdr.version)) {
      req->flags.in_isdatabase = 1;
    } else {
      proxy_debug_2(DBG_WING, "Content-type can't be handled");
      if (req->http_req.errmsg[0] == '\0') {
	char *endct;
	endct = content_type ? strchr(content_type, '\r') : NULL;
	if (endct) {
	  *endct = '\0';
	  snprintf(req->http_req.errmsg, HTTP_ERRMSG_MAX, 
		   "Content-type %s can't be handled yet by the proxy.", 
		   content_type);
	} else {
	  snprintf(req->http_req.errmsg, HTTP_ERRMSG_MAX, 
		   "Content-type can't be handled by the proxy.");
	}
      }
      wing_error_return(&req->http_req, req->task, req->prefs, 
			TASK_THRINDEX(req->task), &req->wing_hdr);
      return gm_False;
    }
  }
#endif

  /* 
   * Writeback the data from the distiller.
   */
  {
    char response_hdrbuf[sizeof_gl_hdr];
    gl_hdr response_hdr;

    INST_set_thread_state(TASK_THRINDEX(req->task), THR_WRITEBACK);
    response_hdr.version = htonl(req->wing_hdr.version);
    response_hdr.request_id = htonl(req->wing_hdr.request_id);

#ifdef OLD
    response_hdr.metadatasize = 
      (req->flags.in_nocache ? sizeof(uncache_metadata) : 0) +
      (req->flags.in_isdatabase ? sizeof(database_metadata) : 0) +
      (content_type_len + 5);
#endif
    response_hdr.metadatasize = 
      (req->flags.in_nocache ? sizeof(uncache_metadata) : 0) +
      (content_type_int ? sizeof(contenttype_metadata) : 0);

#ifdef BACKWARD_COMPATIBLE
    if (! version_check(req->wing_hdr.version, CLIENT_VERSION_NEWAGG)) {
      response_hdr.metadatasize += (req->agglistlen ? req->agglistlen + 4 : 0);
    }
#endif
    response_hdr.metadatasize = htonl(response_hdr.metadatasize);
    response_hdr.datasize = htonl(DistillerBufferLength(&req->
							http_req.pxy_data));
    response_hdr.comp = htonl(0);

    proxy_debug_4(DBG_WING,
		  "About to write headers %lu %lu",
		  ntohl(response_hdr.version), ntohl(response_hdr.request_id));
    proxy_debug_4(DBG_WING,
		  "  %d %d", ntohl(response_hdr.metadatasize), 
		  ntohl(response_hdr.datasize));
    proxy_debug_3(DBG_WING, "  %d", ntohl(response_hdr.comp));
    
    memcpy(response_hdrbuf+ 0, &response_hdr.version,     sizeof(UINT32));
    memcpy(response_hdrbuf+ 4, &response_hdr.request_id,  sizeof(UINT32));
    memcpy(response_hdrbuf+ 8, &response_hdr.metadatasize,sizeof(UINT32));
    memcpy(response_hdrbuf+12, &response_hdr.datasize,    sizeof(UINT32));
    memcpy(response_hdrbuf+16, &response_hdr.comp,        sizeof(UINT32));

    correct_write(req->http_req.cli_fd, response_hdrbuf, sizeof_gl_hdr);
    if (req->flags.in_nocache) 
      correct_write(req->http_req.cli_fd, uncache_metadata, 
		    sizeof(uncache_metadata));
    if (content_type_int) {
      char content_type_string[2];
      content_type_string[0] = (((unsigned int)content_type_int) >> 8) & 0xFF;
      content_type_string[1] =  ((unsigned int)content_type_int) & 0xFF;
      correct_write(req->http_req.cli_fd, contenttype_metadata, 
		    sizeof(contenttype_metadata) - 2);
      correct_write(req->http_req.cli_fd, content_type_string, 2);
    }

#ifdef OLD
    if (req->flags.in_isdatabase) 
      correct_write(req->http_req.cli_fd, database_metadata, 
		    sizeof(database_metadata));
#endif

#ifdef BACKWARD_COMPATIBLE
    if ((! version_check(req->wing_hdr.version, CLIENT_VERSION_NEWAGG)) &&
	req->agglistlen) {
      unsigned char hdr[4] = { 0, 4, 0, 0 };
      hdr[2] = (req->agglistlen >> 8) & 0xff;
      hdr[3] = req->agglistlen & 0xff;
      correct_write(req->http_req.cli_fd, (char *)hdr, 4);
      correct_write(req->http_req.cli_fd, req->agglistdata, req->agglistlen);
    }
#endif

#ifdef OLD
    {
      /* write the content type header */
      unsigned char hdr[4] = { 0, MetaContentType, 0, 0 };
      content_type_len++;
      hdr[2] = (content_type_len >> 8) & 0xff;
      hdr[3] = content_type_len & 0xff;
      correct_write(req->http_req.cli_fd, (char *)hdr, 4);
      correct_write(req->http_req.cli_fd, (char *)content_type, 
		    content_type_len-1);
      correct_write(req->http_req.cli_fd, "", 1);
    }
#endif

    correct_write(req->http_req.cli_fd, 
		  DistillerBufferData(&req->http_req.pxy_data),
		  DistillerBufferLength(&req->http_req.pxy_data));
  }
  
  return gm_True;
}



/*
 *  wing_go_proc: thread entry point to handle an incoming Wingman request 
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
wing_go_proc(task_t *t)
{
  wing_request req;
  int index = TASK_THRINDEX(t);

#ifdef LOGGING
  userkey k;
  struct loginfo lo;
  char logmsg[MAX_LOGMSG_LEN];
#endif /* LOGGING */

  /* Initialize all buffers */
  memset(&req, 0, sizeof(wing_request));

  /*
   *  New task data should be the request structure.
   */
  init_Request(&req.http_req);
  req.http_req.cli_fd = (int)TASK_DATA(t); /* socket FD of client */
  SET_TASK_DATA(t,&req.http_req);
  req.task = t;
  
  INST_begin_timestamp(index);
  INST_set_size(index,0);
  INST_set_thread_state(index, THR_ACCEPTED);
  INST_timestamp(index, m_arrival);
#ifdef LOGGING
  LOGGING_init_loginfo(&lo);
  req.http_req.lo = &lo;
#endif /* LOGGING */

  if (wing_read_client_request(&req)==gm_False) goto WINGGO_FINISH;

  INST_timestamp(index, m_headersdone);

  if (wing_parse_metadata(&req)==gm_False) goto WINGGO_FINISH;

  /* For the sake of logging, fill in the loginfo structure url field */
#ifdef LOGGING
  snprintf(req.http_req.lo->url, MAX_LOGMSG_LEN-1, "%s", 
	   DistillerBufferData(&req.wing_data));
  req.http_req.lo->url[MAX_LOGMSG_LEN-1] = '\0';
#endif /* LOGGING */

  if (wing_grab_userprefs(&req)==gm_False) goto WINGGO_FINISH;

  if (wing_parse_url(&req)==gm_False) goto WINGGO_FINISH;

  if (wing_dispatch_request(&req)==gm_False) goto WINGGO_FINISH;
  
  if (wing_writeback_reply(&req)==gm_False) goto WINGGO_FINISH;


  /* 
   * All done - clean up. 
   */


  /* all cases exit through this single exit point */
WINGGO_FINISH:
  free_Request(&req.http_req);
  DistillerBufferFree(&req.wing_data);
  DistillerBufferFree(&req.wing_metadata);
#ifdef BACKWARD_COMPATIBLE
  if (req.saveoldurl) free(req.saveoldurl);
  if (req.agglistdata) free(req.agglistdata);
#endif
  
  INST_timestamp(index, m_wbdone);
  INST_end_timestamp(index);

  if (TASK_PARENT(t) == 0 && TASK_CHILD_INDEX(t) == 0) {
    /* this is a "root task" */
    close(req.http_req.cli_fd);
  }

#ifdef LOGGING
  /* I compare the IP address to 127.0.0.1 so that I don't log connections
     from localhost, namely the fe_check script.  I also MD5 the IP address. */
  k = lo.ipaddr;
  if (((UINT32) k) != ((UINT32) htonl(0x7F000001))) {
    /* MD5_CTX   theHash;
       UINT32    res;

       MD5Init(&theHash);
       MD5Update(&theHash, magicKey, sizeof(magicKey));
       MD5Update(&theHash, &k, sizeof(UINT32));
       MD5Final(&theHash);
       
       memcpy(&res, theHash.digest, sizeof(UINT32));
       res = ntohl(res);
       
       snprintf(logmsg, MAX_LOGMSG_LEN-1,
       "(WING) %lu %lu \"%s\" %d %ld %ld\n",
       res, lo.date, lo.url,
       lo.http_response, lo.size_before, lo.size_after);*/
    
    if ((req.http_req.lo != NULL) && (lo.url != NULL)) {
      snprintf(logmsg, MAX_LOGMSG_LEN-1,
	       "(WING) %08x %08x %08x \"%s\" %d %ld %ld\n",
	       (UINT32) k, (UINT32) req.wing_hdr.version, lo.date, lo.url,
	       lo.http_response, lo.size_before, lo.size_after);
      gm_log(logmsg);
    }
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

static void    
wing_error_return(Request *h, 
		  task_t *t,
		  ArgumentList prefs,
		  int index,
		  gl_hdr *hdrs)
{
  DistillerStatus dist_result;
  char response_header[20];
  unsigned long ID, metalen, datalen, comp, vers;
  int content_length = 0;
  static char errhdr[] =
    "HTTP/1.0 200 Document follows\r\n\
Server: Wingman Proxy Server\r\n\
Content-type: text/html\r\n\
Content-length: ";
  static char errmsg1[] =
    "<html><head><TITLE>Wingman Error</TITLE></head>\n\
<body><h1>Error</h1><p>\n";
  static char errmsg2[] =
    "<BR><P>\
<img src=\"http://www.cs.berkeley.edu/~gribble/imagedir/homer.gif\">\n\
\n\
</body></html>\n\n";

  static unsigned char canned_msg[] =    /* A canned error message */
  { 
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x41, 0x00, 0x0b, 0x00, 0x01, 0x00, 0x0e, 
    0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20, 
    0x45, 0x72, 0x72, 0x6f, 0x72, 0x20, 0x00, 0x00, 
    0xff, 0xd2, 0x00, 0x26, 0x00, 0x00, 0x00, 0x0b, 
    0x00, 0x67, 0x00, 0x16, 0x00, 0x08, 0x00, 0x14, 
    0x50, 0x72, 0x6f, 0x78, 0x79, 0x20, 0x49, 0x6e, 
    0x74, 0x65, 0x72, 0x6e, 0x61, 0x6c, 0x20, 0x45, 
    0x72, 0x72, 0x6f, 0x72, 0x00, 0x00, 0xff, 0xcc, 
    0x00, 0x32, 0x00, 0x00, 0x00, 0x21, 0x00, 0xa0, 
    0x00, 0x0b, 0x00, 0x00, 0x00, 0x20, 0x54, 0x68, 
    0x65, 0x20, 0x70, 0x72, 0x6f, 0x78, 0x79, 0x20, 
    0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20, 
    0x6d, 0x61, 0x79, 0x20, 0x62, 0x65, 0x20, 0x64, 
    0x6f, 0x77, 0x6e, 0x20, 0x6f, 0x72, 0x00, 0x00, 
    0xff, 0xc0, 0x00, 0x36, 0x00, 0x00, 0x00, 0x2c, 
    0x00, 0x8f, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x24, 
    0x64, 0x6f, 0x65, 0x73, 0x20, 0x6e, 0x6f, 0x74, 
    0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 
    0x74, 0x68, 0x69, 0x73, 0x20, 0x70, 0x61, 0x67, 
    0x65, 0x20, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 
    0x6c, 0x79, 0x2e, 0x20, 0x00, 0x00, 0xff, 0xbc, 
    0x00, 0x22, 0x00, 0x00, 0x00, 0x37, 0x00, 0x54, 
    0x00, 0x0b, 0x00, 0x00, 0x00, 0x10, 0x54, 0x72, 
    0x61, 0x6e, 0x73, 0x65, 0x6e, 0x64, 0x2f, 0x57, 
    0x69, 0x6e, 0x67, 0x6d, 0x61, 0x6e, 0x00, 0x00, 
    0xff, 0xff };

  /* 
   * Package up nice, simple HTML for error response
   */
  DistillerBufferFree(&h->pxy_data);
  DistillerBufferFree(&h->pxy_hdrs);
  DistillerBufferFree(&h->svr_data);
  DistillerBufferFree(&h->svr_hdrs);
  content_length = strlen(errmsg1)+strlen(errmsg2)+strlen(h->errmsg);
  assert(DistillerBufferAlloc(&h->svr_hdrs, strlen(errhdr)+15) == gm_True);
  sprintf(DistillerBufferData(&h->svr_hdrs), "%s%d\r\n\r\n",
			      errhdr, content_length);
  DistillerBufferSetLength(&h->svr_hdrs,
			   strlen((char *) DistillerBufferData(&h->svr_hdrs)));
  assert(DistillerBufferAlloc(&h->svr_data, content_length+3) == gm_True);
  sprintf(DistillerBufferData(&h->svr_data), "%s%s%s",
	  errmsg1, h->errmsg, errmsg2);
  DistillerBufferSetLength(&h->svr_data,
			   strlen((char *) DistillerBufferData(&h->svr_data)));

  proxy_debug_3(DBG_WING, "Debug content: %s",
		DistillerBufferData(&h->svr_data));

  /* Prepare Wingman protocol headers */
  vers = htonl(hdrs->version);
  INST_set_thread_state(index, THR_WRITEBACK);
  ID = htonl(hdrs->request_id);
  metalen = htonl(sizeof(uncache_metadata));
  comp = htonl(0);
  *((UINT32 *) &(response_header[0])) = vers;
  *((UINT32 *) &(response_header[4])) = ID;
  *((UINT32 *) &(response_header[8])) = metalen;
  *((UINT32 *) &(response_header[16])) = comp;

  dist_result = proxy_dispatch(&prefs, t);
  switch(dist_result) {
  case distOk:
    /* Fall through into writeback phase */
    datalen = htonl(DistillerBufferLength(&h->pxy_data));
    *((UINT32 *) &(response_header[12])) = datalen;
    break;
  case distDistillerNotFound:
  case distLaunchTimeout:
  case distBadInput:
  case distConnectionBroken:
  default:
    /* XXX - Must send generic, prepackaged error response */
    datalen = htonl(sizeof(canned_msg));
    proxy_debug_3(DBG_WING, "Sizeof data is %ld\n", datalen);
    *((UINT32 *) &(response_header[12])) = datalen;
    proxy_debug_3(DBG_WING, "Debug distill failed (%d)", dist_result);
    correct_write(h->cli_fd, response_header, 20);
    correct_write(h->cli_fd, uncache_metadata, sizeof(uncache_metadata));
    correct_write(h->cli_fd, (char *) canned_msg,
		  sizeof(canned_msg));
    return;
  }

  /* 
   * Writeback the data from the distiller.
   */
  correct_write(h->cli_fd, response_header, 20);
  correct_write(h->cli_fd, uncache_metadata, sizeof(uncache_metadata));
  correct_write(h->cli_fd, DistillerBufferData(&h->pxy_data),
		DistillerBufferLength(&h->pxy_data));
  return;
}

#if 0
/* Send stuff that's already in the right form back to the client.  The
   UINT32 values are in _host_ order. */
static void wing_raw_return(int fd, UINT32 vers, UINT32 ID, UINT32 metalen,
    UINT32 datalen, UINT32 comp, unsigned char *metadata, unsigned char *data)
{
    unsigned char response_header[20];

    *((UINT32 *) &(response_header[0])) = htonl(vers);
    *((UINT32 *) &(response_header[4])) = htonl(ID);
    *((UINT32 *) &(response_header[8])) = htonl(metalen);
    *((UINT32 *) &(response_header[12])) = htonl(datalen);
    *((UINT32 *) &(response_header[16])) = htonl(comp);

    correct_write(fd, (char *)response_header, 20);
    if (metalen) {
	correct_write(fd, (char *)metadata, metalen);
    }
    if (datalen) {
	correct_write(fd, (char *)data, datalen);
    }
}
#endif
