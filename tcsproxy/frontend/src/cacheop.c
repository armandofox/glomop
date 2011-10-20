#include "config_tr.h"
#include "clib.h"
#include "frontend.h"
#include "comm_http.h"
#include "ARGS.h"
#include <string.h>
#include <stdlib.h>

HTTP_Status
do_get(Request *hp, ArgumentList *al)
{
  clib_response cl;
  clib_data dat;            /* data from cache or previous pipe stage */
  HTTP_Status retcode;
  Argument *arg;
  
  dat.mime_headers = dat.data = NULL;
  if ((arg =getArgumentFromIdInList(al, FRONT_ANONYMIZE))
      && (ARG_INT(*arg) > 0)) {
    cl = Clib_Fetch((char *)(hp->url), "", &dat);
  } else {
    cl = Clib_Fetch((char *)(hp->url), DistillerBufferData(&hp->cli_hdrs), &dat);
  }
  if (cl == CLIB_OK) {
    DistillerBufferSetString(&hp->svr_hdrs, (char*)dat.mime_headers,
                             dat.mime_size);
    DistillerBufferFreeMe(&hp->svr_hdrs, gm_True);
    DistillerBufferSetStatic(&hp->svr_data, dat.data, dat.data_size);
    DistillerBufferFreeMe(&hp->svr_data, gm_True);
    hp->cache_fd = dat.fd;
    retcode = HTTP_NO_ERR;
  } else {
    /* cache error */
    snprintf(hp->errmsg, HTTP_ERRMSG_MAX, "Client library error %d: %s",
             (int)cl, clib_strerror(cl));
    retcode = HTTP_ERR_CACHE;
  }
  return retcode;
}
  
/*
 *  do_post: handle a POST request from client.
 *
 *  ARGS:
 *    i: HTTP headers of request
 *  RETURNS:
 *    HTTP_Status code of POST operation.  If success, caller should read and
 *  forward the server response; otherwise,caller should report the error
 *  (probably via http_error_return).  POST can fail if content-length cannot
 *  be parsed, or if number of data bytes successfully read does not match
 *  content-length.  (BUG::should the latter case just be a warning?)
 *  
 *  SIDE EFFECTS:
 *    Causes POST request to go (through cache) to server.
 */
HTTP_Status
do_post(Request *hp, ArgumentList *al)
{
  INT32 len, buffersize;
  int len_read, to_read;
  const char *val;
  int val_len;
  char *buffer;
  clib_response cl;
  clib_data returned;
  HTTP_Status result;
  char *already_read;
  size_t chars_already_read;
  char *hdrs = DistillerBufferData(&hp->cli_hdrs);

  if ((val = get_header_value(&hp->cli_hdrs, "content-length", &val_len,
                              NULL, NULL))
      == NULL) {
    /* no content-length header: barfulation */
    result = HTTP_ERR_POST_NO_CONTENTLENGTH;
    goto POST_RETURN;
  }

  len = strtoul(val, NULL, 0);
  if (len < 0) {
    result = HTTP_ERR_POST_NO_CONTENTLENGTH;
    goto POST_RETURN;
  }

  /*
   *  Now determine how many bytes to read.  Since the initial read of the
   *  headers may have gotten some of the data bytes too, we have to subtract
   *  out those bytes that have already been read.  Do this by taking the
   *  length of the headers and subtracting the number of character *following*
   *  "\r\n\r\n" (end of headers).
   */

  already_read = strstr(hdrs, "\r\n\r\n"); /* look for EOH */
  if (already_read == NULL) {   /* can't find EOH -- badness 10000 */
    result = HTTP_ERR_MALFORMED_REQUEST;
    goto POST_RETURN;
  }
  already_read += 4;            /* skip \r\n\r\n at end of hdrs */
  chars_already_read = (hdrs + (strlen(hdrs))) - already_read;
  to_read = len - chars_already_read; /* how much data still to be read */
  buffersize = len + 100;       /* a few bytes of insurance */
  buffer = (char *)ALLOCA(buffersize); 
  strncpy(buffer, already_read, chars_already_read);
  *already_read = '\0';     /* NOTE! this modifies hp->header_data so it
                               contains NULL-terminated headers only */
  if (to_read > 0) {
    len_read = readline_or_timeout(hp, to_read, buffer);
#if 0  /* for now, data count mismatch will be ignored.  */
    if (len_read != to_read) {
      /* data length != content-length */
      result = HTTP_ERR_POST_READ_FAILED;
      goto POST_RETURN;
    }
#endif
  } 
  /* forward POST to server */
  
  cl = Clib_Post(hp->url, DistillerBufferData(&hp->cli_hdrs),
                 buffer, len, &returned);
  if (cl != CLIB_OK) {
    result = HTTP_ERR_POST_FAILED;
  } else {
    result = HTTP_NO_ERR;
    DistillerBufferSetStatic(&hp->svr_hdrs, returned.mime_headers,
                             returned.mime_size + 1 ); /* +1 for NULL term */
    DistillerBufferFreeMe(&hp->svr_hdrs, gm_True);
    DistillerBufferSetStatic(&hp->svr_data, returned.data,
                             returned.data_size);
    DistillerBufferFreeMe(&hp->svr_data, gm_True);
  }
POST_RETURN:
  return result;
}

