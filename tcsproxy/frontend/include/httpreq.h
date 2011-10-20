#ifndef _HTTPREQ_H
#define _HTTPREQ_H

#define FE_HISTORY_SIZE  60    /* number of minutes of frontend load history */

#define PERF_HTTP_MAX_HEADERS   30     /* max # headers from client */
#define PERF_HTTP_TOTAL_HEADERLEN  8192
#define PERF_HTTP_HEADERLEN     50   /* avg length of HTTP header */
#define PERF_HTTP_REQ_TIMEOUT    30   /* timeout in seconds for receiving HTTP
                                 req and headers */
#define PERF_HTTP_RETRY_DISTILL  3  /*  How many times to try
                                        distillation, in case it fails ; can be 
                                        overridden by FRONT_RETRY_DISTILL arg */
#define PERF_FRONT_MTU         1024 /* Default threshold for complete bypass */
#define PERF_REQUEST_TTL        10 /* max # "pipe stages" for single request */

#define HTTP_RESPONSE_BODY_MAX  1024
#define MIME_TYPE_MAX 80

#include "clib.h"
#include "userpref.h"
#include "utils.h"
#include <sys/types.h>

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
#ifdef FORBID
  HTTP_ERR_ACCESS_DENIED,
#endif /* FORBID */
  HTTP_ERR_PUSH_FAILED,
  HTTP_ERR_UNSPECIFIED
} HTTP_Status;

/*
 *  The following list must match the order of the enum HTTP_Status in
 *  httpreq.h. 
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
#ifdef FORBID
  { "500 Forbidden by proxy",
    "Site `%s' is restricted to local users" },
#endif /* FORBID */
  { "500 Push failed",
    "Push into cache failed for URL `%s'" },
  { "500 Miscellaneous",
    "Unspecified internal proxy error on {%s}" },
};

typedef enum {
  HTTP_VERSION_09_OR_EARLIER, HTTP_VERSION_10_OR_LATER, HTTP_VERSION_UNKNOWN
} HTTP_Version;

typedef char HeaderData[PERF_HTTP_TOTAL_HEADERLEN];

struct http_headers {
  const char *url;              /* pointer into HeaderData */
  const char *header;
  UINT32 len;                   /* total length of HeaderData field */
  HeaderData header_data;
  HTTP_Version version;
  const char *method;
};

/*
 *  Functions in httpsupport.c for dealing with client connection
 */

#define READ_ALL_HEADERS  ((int)(-1))

int readline_or_timeout(int sock, char *buf, size_t *buflen, int nbytes);
void send_content(const char *content, int len, const char *mimetype, int sock);
userkey userkey_from_sock_ipaddr(int sock);
void http_error_return(int sock, HTTP_Version version, HTTP_Status
                       status, char *subst);
HTTP_Status do_post(struct http_headers *hp, ArgumentList *args, int sock,
                    task_t *task, clib_data *dat_p);

/*
 *  Functions in httpsupport.c for manipulating http headers
 */
int insert_header(char *hdrs, const char *str,
                      const char *value, int replace_p);
int delete_header(char *hdrs, const char *str);
const char *get_header_value(const char *str, const char *which, int *len,
                                    char **hdr, int *totallen);

/*
 *  Functions in dispatch.c
 */

void proxy_dispatch(struct http_headers *hp, ArgumentList *args, 
                    struct loginfo *lo, task_t *tsk);

/*
 *  Functions in httpreq.c
 */

gm_Bool       httpreq_init(int port, int boost_prio);

#endif /* ifndef _HTTPREQ_H */
