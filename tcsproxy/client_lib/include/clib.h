/*
 * File:          clib.h
 * Purpose:       To define the structures and public functions available
 *                to clients for communicating with the Harvest partitioned
 *                cache.
 * Author:        Steve Gribble
 * Creation Date: September 25th, 1996
*/        

#ifndef CLIB_H
#define CLIB_H

/** XXX - The following really should be taken from autoconf.h **/
typedef unsigned long  clib_u32;
typedef long           clib_s32;

typedef int clib_response;

  /***** Generic success message - operation succeeded *****/
#define  CLIB_OK        0

  /***** Messages pertinent only to Clib_Query() *****/
#define  CLIB_CACHE_HIT  1         /* URL is in the cache */
#define  CLIB_CACHE_MISS 2        /* URL is not in the cache */

  /***** Messages pertinent only to Clib_Initialize() *****/
#define  CLIB_NO_CONFIGFILE 3     /* Couldn't find configuration file */
#define  CLIB_CONFIG_ERROR 4      /* Error occured while reading config file */
#define  CLIB_NO_SERVERS 5        /* Couldn't find any cache servers */

  /***** Generic error messages, could be returned anytime *****/
#define  CLIB_BAD_URL 6           /* URL is ill-formed */
#define  CLIB_BAD_VERSION 7       /* Harvest server is running old/new version */
#define  CLIB_TIMEOUT 8           /* Cache server returned "timeout" error */
#define  CLIB_ACCESS_DENIED 9     /* Denied access to operation */
#define  CLIB_SERVICE_UNAVAIL 10 /* Cache server doesn't support the operation */
#define  CLIB_SERVER_INTERNAL 11   /* Cache server suffered internal error */
#define  CLIB_SERVER_DOWN 12       /* Couldn't connect to the cache server */
#define  CLIB_SERVER_TIMEOUT 13    /* Cache server response itself timed out */
#define  CLIB_NO_FD 14             /* Ran out of file descriptors */
#define  CLIB_NO_SUCH_PART 15      /* No such partition (part_delete) */ 
#define  CLIB_PTHREAD_FAIL 16      /* Generic pthread failure */
#define  CLIB_SOCKETOP_FAIL 17     /* Socket operation (read,write,connect,...) fail */

#ifdef __GNUC__
static const char * clib_error_str[] __attribute__ ((unused)) = {
#else  /* not gcc */
static const char * clib_error_str[] = {
#endif
  "Everything is just fine",
  "Cache hit",
  "Cache miss",
  "Can't find configuration file",
  "Configuration file error",
  "No cache servers available",
  "Bad URL",
  "Cache server says: wrong version",
  "Cache server says: timeout error",
  "Access denied",
  "Cache server says: service unavailable",
  "Cache server says: internal error",
  "Couldn't connect to cache server",
  "Cache server response timed out",
  "Ran out of file descriptors",
  "Part_delete says no such partition!",
  "Generic pthreads failure",
  "Socket operation failed"
};

#define clib_strerror(x) \
        ((x)>=0 && (x)<(int)(sizeof(clib_error_str)/sizeof(const char *)) \
         ? clib_error_str[(x)] : "(Error code out of range)")

#define CLIB_CUTTHROUGH_THRESH ((1<<20)+(1<<19)) /* 1.5 MByte limit */
#define CLIB_CUTTHROUGH 18         /* Cutthrough operation in effect */

typedef struct {
  char          *mime_headers; /* NULL-termed headers associated with data */
  clib_u32       mime_size;    /* size of returned mime headers */
  char          *data;         /* the data itself - may be binary */
  clib_u32       data_size;    /* size of returned data */
  int            fd;           /* cutthrough filedescriptor */
} clib_data;

/*
 * Initialize this client library - read in the config file, attempt to
 * ping all of the the cache servers to ensure that they are alive.
*/

#include "config_tr.h"
#include "libmon.h"
#include "optdb.h"

clib_response Clib_Initialize(Options opts, Monitor *defMon);

/*
 * Query the cache substrate to see if a certain URL is in the cache.
*/

clib_response Clib_Query(char *url);

/*
 * Ask the cache substrate to return a piece of data.  If the data is
 * not in the cache, it will go out and attempt to retrieve it.  Caller
 * is responsible for free'ing the "mime_headers" and "data" fields of
 * the returned_data structure.
 *
 * "url" should be the URL of the requested data, for example:
 *     "http://www.cs.berkeley.edu/~gribble"
 *
 *  A GET request will be issued to retrieve the data.
 *
 * The mime_headers field you pass in should have the double \r\n
 * terminating it.  If mime_headers is NULL, default headers are passed
 * on by the client-side library.
*/

clib_response Clib_Fetch(char *url, char *mime_headers,
			 clib_data *returned_data);

/*
 * Like Clib_Fetch, but follow redirects up to follow_redirects times (if > 0)
 */
clib_response Clib_Redir_Fetch(char *url, char *mime_headers,
			 clib_data *returned_data, int follow_redirects);

/*
 * Hint to the cache substrate that you will soon be asking it to return a 
 * piece of data.  This will cause the cache to make sure that the data is
 * in the cache by fetching it.
 *
 * This takes advantage of the fact that caches will continue to load data
 * from a server even when the client closes it's connection.
 *
 * "url" should be the URL of the requested data, for example:
 *     "http://www.cs.berkeley.edu/~gribble"
 *
 *  A GET request will be issued to retrieve the data.
 *
 * The mime_headers field you pass in should have the double \r\n
 * terminating it.  If mime_headers is NULL, default headers are passed
 * on by the client-side library.
 *
*/

clib_response Clib_Async_Fetch(char *url, char *mime_headers);

/*
 * Ask the cache substrate to post and return a piece of data. 
 * Caller is responsible for free'ing the "mime_headers" and "data" fields 
 * of the returned_data structure.
 *
 * "url" should be the URL of the requested data, for example:
 *     "http://www.cs.berkeley.edu/~gribble"
 *
 *  A POST request will be issued to retrieve the data.
 *
 * The mime_headers field you pass in should have the double \r\n
 * terminating it.  If mime_headers is NULL, default headers are passed on
 * by the client-side library, including a "content-length" header and a
 * content-type header that is constructed for the data.  If mime_headers
 * is not null, make sure the headers have content-length and content-type
 * in them.  The "data" and "data_len" arguments are the data to pass in,
 * and the length of data, respectively.  */

clib_response Clib_Post(char *url, char *mime_headers,
                        char *data, unsigned data_len,
			clib_data *returned_data);

/*
 *  Free the buffers associated with a previous request, if they appear to have
 *  been malloc'd by this library.
 */

void Clib_Free(clib_data *dat);

/*
 * This function is a lower-level version of the Clib_Fetch routine that
 * allows callers to provide all of the data that is to be sent to the
 * harvest partitioned cache, including the mime headers of the request.
 * A block of data is returned, which is simply all of the returned data
 * from the cache (or server if cache miss).
 *
 * The data to be sent will be parsed and an URL extracted, so that the
 * partitioning works properly.
*/

clib_response Clib_Lowlevel_Op(char *indata, unsigned inlen,
			       char **outdata, unsigned *outlen,
			       int *fd);
/* 
 * Given an HTTP/1.x response from a server (presumably returned by
 * Clib_Lowlevel_Op), parse out the headers and content from the
 * response.  Memory will be allocated for the headers and content
 * fields if this routine is successful.  The response headers will
 * be canonicalized - right now this means lines will be forced to
 * terminate with \r\n, not just \n as NCSA/1.4 does.
 *
 * If an error occurs (mainly because of ill-formed data from the server),
 * both *mime_headers and *content_size will be NULL, otherwise
 * *mime_headers will be the newly allocated buffer for mime headers and
 * *mime_size will be the size of the headers (including the \r\n\r\n),
 * and similarly for *content and *content_size.
 *
 * You should pass in "data" and "dlen".  Note that clib_u32 is just
 * unsigned long.
 */

void   get_header_content(char *data, int dlen,
                     char **mime_headers,
                     clib_u32 *mime_size,
                     char **content,
                     clib_u32 *content_size);

/*
 * Force some data into the cache as a particular URL and mime type.
*/

clib_response Clib_Push(char *url, clib_data data);

/*
 * Print out a (helpful?) error message for a given response, prepended
 * with err_str, to stderr.
*/

void Clib_Perror(clib_response response, char *err_str);

#endif
