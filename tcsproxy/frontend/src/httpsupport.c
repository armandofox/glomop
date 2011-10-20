/*
 *  httpsupport.c: support functions for HTTP parsing and other
 *  front-end work
 *
 *  INVARIANTS:
 *  DistillerBufferLength() always returns the number of characters
 *    EXCLUDING ANY POSSIBLE TERMINATING NULL.
 *  There is always a terminating NULL at the end of the headers, so it
 *    is safe to call strlen, strstr, etc.
 */

#include "frontend.h"
#include "utils.h"
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*
 *  Return the (string) value of the given MIME header, or NULL if it
 *  doesn't exist.
 *  ARGS:
 *    i: MIME headers verbatim from client
 *    i: header to look for
 *    o*: pointer to fill in: length of found value string, may be 0
 *       (if null, won't be filled in)
 *    o*: pointer to fill in: beginning of header line (if passed in as NULL,
 *      won't be filled in)
 *    o*: pointer to fill in: length of header line up to but not including
 *      first char of header value  (if NULL, won't be filled in)
 *  ASSUMPTIONS: none
 *  SIDE EFFECTS: none
 *  RETURNS: NULL (and sets length ptr to zero) if given header does not
 *  appear to be present; char pointer to header (and sets length
 *  pointer to length of value string) if found
 *  THREAD-SAFE: yes
 */

const char *
get_header_value(DistillerBuffer *buf, const char *which, int *len,
                 char **hdr, int *totallen)
{
  const char *tmp,*savetmp;
  const char *str;
  int hdrlen = DistillerBufferLength(buf);
  size_t ndx;

  if (hdrlen == 0)
    return NULL;

  str = DistillerBufferData(buf);

  if (len) {  *len = 0; }
  if (totallen) { *totallen = 0; }
  
  tmp = strcasestr(str, which);
  if (tmp == NULL)
    return NULL;

  savetmp=tmp;
  if (hdr) { *hdr = (char *)tmp; }

  tmp += strlen(which);

  /* assume we found the header.  Parse forward past a colon and
     possible whitespace.  If we hit a CRLF, we've gone too far.  */

  if (*tmp++ != ':')
    return NULL;

  ndx = strspn(tmp, " \t");
  if (ndx >= strlen(tmp))
    return NULL;

  /* set length to indicate actual string length */
  tmp += ndx;                   /* beginning of string */
  if (totallen) { *totallen = tmp-savetmp; }
  if (len) { *len = strcspn(tmp,"\r\n"); } /* length of string without trailing
                                              \r\n   */
  return tmp;
}


/*
 *  Delete a header.
 *  ARGS:
 *    i/o: headers
 *    i: header to delete
 *  RETURNS:
 *    nonzero iff delete succeeded
 *  SIDE EFFECTS:
 *    Modifies headers in place
 *  BUGS:
 *    BUG::empty headers (or all-whitespace values) are not deleted properly,
 *  yet this function still reports success on those cases.  
 */
int
delete_header(DistillerBuffer *buf, const char *str)
{
  char *hdrs = DistillerBufferData(buf);
  char *line;
  const char *dummy;
  int x;
  int len;
  int totallen;
  int finallen;
  
  dummy = get_header_value(buf, str, &x, &line, &len);
  if (dummy == NULL) {          /* not found */
    return 0;
  } else {
    /* close up space from other headers */
    /* BUG::this assumes lines end with \r\n (hence "+2" below).  i believe it
     *  may be legal for lines to end with just one or the other.
     */
    totallen = len+x+2;
    memmove(line, line+totallen, (hdrs + DistillerBufferLength(buf))
            - (line+totallen));
    finallen = DistillerBufferLength(buf) - totallen; /* finallen doesn't
							 include NULL */
    /*
     *  Always safe to add a null, since the headers have shrunk.
     */
    hdrs[finallen] = '\0';
    DistillerBufferSetLength(buf, finallen);
    return 1;
  }
}

/*
 *  Insert a new header.  If replace_p is nonzero, and this header already
 *  exists, delete the old one.
 *  ARGS:
 *    i/o: complete set of headers, verbatim
 *    i: header name to replace (string, case-insensitive)
 *    i: new value (string)
 *    i: if nonzero, delete existing header with same name first (to avoid
 *              duplicate headers)  
 *  RETURNS:
 *    Nonzero iff success.
 *  SIDE EFFECTS:
 *    Modifies the header structure passed in.
 *    If the headers had to be realloc'd to grow a header, the new buffer size
 *  is written over the 2nd argument.
 *  ASSUMPTIONS:
 *    If header is to be replaced, the header is unique (i.e. it can only
 *  appear once in the header list).
 */

int
insert_header(DistillerBuffer *buf, const char *str, const char *value,
              int replace_p)
{
  char *hdrs = NULL;
  int tmpstrlen;
  size_t hdrlen;
  size_t newhdrlen = strlen(str);
  size_t valuelen = strlen(value);
  int finallen;
  int grow;
  int size_needed;
  
  if (replace_p) {
    delete_header(buf, str);
  }

  /*
   *  BUG::assumes headers always end in "\r\n".   it may be legal for
   *  them to end in only one or the other; if so, the "+2"s below are wrong.
   */
  grow = newhdrlen + valuelen + 2 + 2; /* "Header: Value\r\n" */
  /* that's +2 for colon and space, +2 for \r\n */

  hdrlen = DistillerBufferLength(buf); /* point to char beyond end */
  /*
   *  Compute length of new headers.  SInce we like to always have a NULL on
   *  the end, add 2 to account for the NULL that isn't included in
   *   DistillerBufferLength and the NULL we want to include in the result.
   */

  size_needed = hdrlen+grow+2;

  if (hdrlen == 0) {
    /* create legal "dummy headers" first! */
    const char dummy_hdr[] = "HTTP/1.0 200 OK\r\n\r\n";
    hdrlen = strlen(dummy_hdr);
    size_needed += hdrlen + 1;
    assert(DistillerBufferRealloc(buf, size_needed) == gm_True);
    strcpy(DistillerBufferData(buf), dummy_hdr);
  } else if (size_needed >= buf->maxLength) { 
    /* grow enough for about 8 operations of this type (8 headers seems to
       be typical for server return).  */
    assert(DistillerBufferRealloc(buf, size_needed) == gm_True);
  }

  /*
   *  At this point, the headers end in "\r\n\0", and the buffer is guaranteed
   *  large enough to insert the new header + "\r\n".
   */
  hdrs = DistillerBufferData(buf);
    
  tmpstrlen = snprintf((hdrs + hdrlen) - 2, /* -2 for \r\n */
                       buf->maxLength - (DistillerBufferLength(buf)-3) - 1,
                       "%s: %s\r\n\r\n", str, value);
  finallen = hdrlen - 2 + tmpstrlen;  /* hdrlen doesn't include NULL term */
  hdrs[finallen] = '\0';      /* -1 because finallen includes NULL */
  DistillerBufferSetLength(buf, finallen);
  return 1;
}

/*
 *  Initialize and free an Request structure, respectively.
 */

void
init_Request(Request *h) {
  memset(h, 0, sizeof(Request));
  h->version = HTTP_VERSION_UNKNOWN;
}

void
free_Request(Request *h) {
  SAFEFREE(h->url);
  SAFEFREE(h->method);
  DistillerBufferFree(&h->cli_hdrs);
  DistillerBufferFree(&h->cli_data);
  DistillerBufferFree(&h->svr_hdrs);
  DistillerBufferFree(&h->svr_data);
  DistillerBufferFree(&h->pxy_hdrs);
  DistillerBufferFree(&h->pxy_data);
}

