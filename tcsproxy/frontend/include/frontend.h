#ifndef _HTTPREQ_H
#define _HTTPREQ_H

#include "config_tr.h"
#include "thr_cntl.h"
#include "proxyinterface.h"
#include "debug.h"

#define FE_HISTORY_SIZE  60    /* number of minutes of frontend load history */

#define PERF_HTTP_MAX_REDIRECT  5      /* max # of consecutive redirects
					  (for loop avoidance) */
#define PERF_HTTP_MAX_HEADERS   30     /* max # headers from client */
#define PERF_HTTP_TOTAL_HEADERLEN  8192
#define PERF_HTTP_HEADERLEN     50   /* avg length of HTTP header */
#define PERF_HTTP_REQ_TIMEOUT    15   /* timeout in seconds for receiving HTTP
                                 req and headers */
#define PERF_HTTP_RETRY_DISTILL  3  /*  How many times to try
                                        distillation, in case it fails ; can be 
                                        overridden by FRONT_RETRY_DISTILL arg */
#define PERF_FRONT_MTU         1024 /* Default threshold for complete bypass */
#define PERF_REQUEST_TTL        10 /* max # "pipe stages" for single request */

#define HTTP_RESPONSE_BODY_MAX  1024
#define MIME_TYPE_MAX 80
#define HTTP_ERRMSG_MAX         512

#define MIN(x,y) ((x)<(y) ? (x): (y))

/*
 *  HTTP error conditions we return directly to client
 */

typedef enum {
  HTTP_NO_ERR = 0,
  HTTP_ERR_MALFORMED_REQUEST,
  HTTP_ERR_CACHE,
  HTTP_ERR_DISTILLER,
  HTTP_ERR_UNIMPLEMENTED_FEATURE,
  HTTP_ERR_BAD_TOKEN,
  HTTP_ERR_POST_NO_CONTENTLENGTH,
  HTTP_ERR_POST_READ_FAILED,
  HTTP_ERR_POST_FAILED,
  HTTP_ERR_GET_FAILED,
  HTTP_ERR_PUSH_FAILED,
  HTTP_ERR_CANT_HANDLE_DATATYPE,
  HTTP_ERR_ROUTING_ERROR,
  HTTP_ERR_AGGREGATOR_ERROR,
  HTTP_ERR_UNSPECIFIED
} HTTP_Status;

/*
 *  The following list must match the order of the enum HTTP_Status in
 *  frontend.h. 
 */

const static struct {
  const char *message;
  const char *longmessage;
} http_returns[HTTP_ERR_UNSPECIFIED+1] = {
  { "000 No error",
    "Should never get this status (%s)" },
  { "500 Malformed request or URL",
    "Request malformed: {%s}" },
  { "500 Cache error",
    "Cache error: %s" },
  { "500 Distiller/PTM error",
    "PTM error message: %s" },
  { "500 Unimplemented proxy feature",
    "Feature not yet implemented: %s" },
  { "500 Bad token",
    "Bad token in request: `%s'" },
  { "500 POST failed",
    "POST failed: couldn't parse Content-length" },
  { "500 POST failed",
    "POST failed: number of data bytes doesn't match content-length" },
  { "500 POST failed",
    "POST failed: client_lib failure" },
  { "500 GET failed",
    "GET failed: client_lib failure (warning: this should never happen)" },
  { "500 Push failed",
    "Push into cache failed for URL `%s'" },
  { "500 GET failed",
    "GET failed: wingman distiller can't handle datatype" },
  { "500 Routing/Redispatch error",
    "Request redispatched too many times (%s); there is probably an infinite"
    " loop in the route" },
  { "500 Internal server error",
    "Could not handle request for URL %s<br>"
    "Probably an internal server configuration error" },
  { "500 Miscellaneous Proxy Error",
    "Internal proxy error:<br> %s" },
};

#define http_strerror(e)  ((e)>=0 && (e)<= HTTP_ERR_UNSPECIFIED \
                           ? http_returns[(e)].message \
                           : http_returns[HTTP_ERR_UNSPECIFIED].message)

typedef enum {
  HTTP_VERSION_09_OR_EARLIER, HTTP_VERSION_10_OR_LATER, HTTP_VERSION_UNKNOWN
} HTTP_Version;

/*
 *  This data structure encapsulates all the metadata regarding an HTTP request. 
 */

typedef struct request {
  char *url;
  char *method;
  HTTP_Version version;
  int cli_fd;
  int cache_fd;
  DistillerBuffer cli_hdrs;
  DistillerBuffer cli_data;
  DistillerBuffer svr_hdrs;
  DistillerBuffer svr_data;
  DistillerBuffer pxy_hdrs;
  DistillerBuffer pxy_data;
  UINT32 svr_http_status;
  char errmsg[HTTP_ERRMSG_MAX+1];
  char zerobyte;                /* catch runaway strings */
#ifdef LOGGING
  struct loginfo *lo;
#endif
} Request;

void init_Request(Request *r);
void free_Request(Request *r);

/*
 *  Functions dealing with proxy startup/shutdown
 */
void proxy_shutdown(int sig);

/*
 *  Main dispatch function to service a request, once the request has
 *  been received from a client, parsed, etc. by the appropriate
 *  transport protocol module 
 */
gm_Bool is_server_url(const char *url);
HTTP_Status server_dispatch(ArgumentList *al, task_t *task);
DistillerStatus proxy_dispatch(ArgumentList *al, task_t *task);

/*
 *  Functions in httpsupport.c for manipulating http headers
 */
int insert_header(DistillerBuffer *dat, const char *str,
                  const char *value, int replace_p);
int delete_header(DistillerBuffer *dat, const char *str);
const char *get_header_value(DistillerBuffer *dat, const char *which, int *len,
                             char **hdr, int *totallen);
int has_nonempty_header(const char *str, const char *hdr);

/*
 *  Talking to the cache
 */
HTTP_Status  do_get(Request *hp, ArgumentList *al);
HTTP_Status do_post(Request *hp, ArgumentList *al);



/*
 *  Functions in httpreq.c
 */

gm_Bool       httpreq_init(int port, int boost_prio);

#endif /* ifndef _HTTPREQ_H */
