#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "clib.h"
#include "part.h"
#include "utils.h"
#include "icp.h"
#include "libmon.h"
#include "optdb.h"

#define CLIB_UDP_TIMEOUT_SEC  0
#define CLIB_UDP_TIMEOUT_USEC 100000
#define CLIB_UDP_RETRIES      5

extern struct sockaddr *localaddr;
extern int              localaddrlen;
extern ll               config_list;
extern int              num_partitions;

struct timeval query_tv;
int            udp_retries;

Options part_opts;

char  *get_url_from_data(char *indata, int inlen);
char  *canonicalize_headers(char *header_data, int header_len);

clib_response Clib_Initialize(Options opts, Monitor *defMon)
{
  clib_response  presp;
  cfg_nv        *cfg_el;
  ll             tmp;

  part_opts = opts;
  if ((presp = Part_Initialize(opts, defMon)) != CLIB_OK)
    return presp;
  /* Grab retry info out of config file */
  tmp = config_list;
  cfg_el = get_next_matching("cache.udp_retries", &tmp);
  if (tmp == NULL)
    udp_retries = CLIB_UDP_RETRIES;
  else {
    if (sscanf(cfg_el->value, "%d", &udp_retries) != 1)
      udp_retries = CLIB_UDP_RETRIES;
  }

  tmp = config_list;
  cfg_el = get_next_matching("cache.udp_timeout_sec", &tmp);
  if (tmp == NULL)
    query_tv.tv_sec = CLIB_UDP_TIMEOUT_SEC;
  else {
    if (sscanf(cfg_el->value, "%u", &(query_tv.tv_sec)) != 1)
      query_tv.tv_sec = CLIB_UDP_TIMEOUT_SEC;
  }

  tmp = config_list;
  cfg_el = get_next_matching("cache.udp_timeout_usec", &tmp);
  if (tmp == NULL)
    query_tv.tv_usec = CLIB_UDP_TIMEOUT_USEC;
  else {
    if (sscanf(cfg_el->value, "%u", &(query_tv.tv_usec)) != 1)
      query_tv.tv_usec = CLIB_UDP_TIMEOUT_USEC;
  }

  return CLIB_OK;
}

clib_response Clib_Query(char *url)
{
  clib_response  resp = CLIB_OK, presp;
  int            tries, num_ready = 0, fd, iresp;
  fd_set         fdset;
  icp_message_t  message_obj;

  if ((presp = Part_GetUDPsock(url, &fd)) != CLIB_OK)
    return presp;

  FD_ZERO(&fdset);
  for (tries=0; tries < udp_retries; tries++) {
    if (do_icp_query(fd, url, *((struct in_addr *) &localaddr), 1) <= 0) {
      Part_HandleFailure(fd);
      close(fd);
      if (num_partitions == 0)
	return CLIB_SOCKETOP_FAIL;
      else
	return Clib_Query(url);
    }
    FD_SET(fd, &fdset);
    num_ready = select(fd+1, &fdset, NULL, NULL, &query_tv);
    if (num_ready < 0) {
      Part_HandleFailure(fd);
      close(fd);
      if (num_partitions == 0)
	return CLIB_SOCKETOP_FAIL;
      else
	return Clib_Query(url);
    }
    if (num_ready > 0)
      break;
  }
  if (num_ready <= 0) {
    Part_HandleFailure(fd);
    close(fd);
    if (num_partitions == 0)
      return CLIB_SERVER_TIMEOUT;
    return Clib_Query(url);
  }

  iresp = do_icp_udp_receive(fd, &message_obj);
  close(fd);
  if (message_obj.header.opcode == ICP_OP_HIT) {
    free(message_obj.op.hit.h_url);
    return CLIB_CACHE_HIT;
  } else if (message_obj.header.opcode == ICP_OP_MISS) {
    free(message_obj.op.miss.m_url);
    return CLIB_CACHE_MISS;
  } else if (message_obj.header.opcode == ICP_OP_INVALID) {
    return CLIB_BAD_URL;
  } else {
    return CLIB_SERVER_INTERNAL;
  }
  return resp;
}

clib_response Clib_Fetch(char *url, char *mime_headers,
			 clib_data *returned_data)
{
  int            iresp, fd;
  unsigned int   dlen;
  clib_response  presp;
  char          *data = NULL;

  returned_data->fd = -1;

  if ((presp = Part_GetTCPsock(url, &fd)) != CLIB_OK)
    return presp;

  if ((iresp = do_http_send(fd, url, mime_headers, 
			    NULL, 0, 1, ICP_GET)) != 1) {
    Part_HandleFailure(fd);
    close(fd);
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Fetch(url, mime_headers, returned_data);
  }

  if ((iresp = do_http_receive(fd, &dlen, &data)) != 0) {

    if (iresp == -2) {
      /* Cutthrough! */
      returned_data->mime_headers = NULL;
      returned_data->mime_size = 0;
      returned_data->data = data;
      returned_data->data_size = dlen;
      returned_data->fd = fd;
      return CLIB_CUTTHROUGH;
    }

    Part_HandleFailure(fd);
    close(fd);
    if (data != NULL) {
      free(data);
      data = NULL;
    }
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Fetch(url, mime_headers, returned_data);
  }

  /* Parse out the header and content from the return data */
  get_header_content(data, dlen,
		     &(returned_data->mime_headers),
		     &(returned_data->mime_size),
		     &(returned_data->data),
		     &(returned_data->data_size));
  if ((returned_data->data == NULL) || (returned_data->mime_headers == NULL)) {
    close(fd);
    if (data)
      free(data);
    return CLIB_SERVER_DOWN;
  }
  close(fd);
  if (data)
   free(data);
  return CLIB_OK;
}

clib_response Clib_Redir_Fetch(char *url, char *mime_headers,
			    clib_data *returned_data, int follow_redirects)
{
    char *theurl = NULL;
    int done = 0;
    clib_response ret = CLIB_SERVER_INTERNAL;

    if (follow_redirects <= 0) {
	return Clib_Fetch(url, mime_headers, returned_data);
    }
    theurl = strdup(url);
    assert(theurl);
    returned_data->mime_headers = NULL;
    returned_data->data = NULL;
    while(!done && follow_redirects--) {
	char *hp;
	clib_u32 hl;

	/* Fetch the page */
	free(returned_data->mime_headers);
	free(returned_data->data);
	ret = Clib_Fetch(theurl, mime_headers, returned_data);
	if (ret != CLIB_OK) {
	    return ret;
	}

	/* Check returned_data->mime_headers for a redirect code */
	hp = returned_data->mime_headers;
	while(hp && *hp && *hp != ' ' && *hp != '\n') ++hp;
	if (!hp || *hp != ' ' || *(hp+1) != '3') {
	    /* It wasn't a redirect */
	    return ret;
	}

	/* OK, we got a redirect.  To where? */
	done = 1;
	hp = returned_data->mime_headers;
	hl = returned_data->mime_size;
	while(*hp && hl) {
	    if (*hp == '\n' && hl >= 10 &&
		!strncasecmp(hp+1, "location: ", 10)) {
		char *locstart;
		hp += 10;
		hl -= 10;
		locstart = hp;
		while(*hp && hl) {
		    if (*hp == '\r' || *hp == '\n') break;
		    ++hp;
		    --hl;
		}
		free(theurl);
		theurl = malloc(hp-locstart+1);
		assert(theurl);
		memmove(theurl, locstart, hp-locstart);
		theurl[hp-locstart] = '\0';
		done = 0;
		break;
	    }
	    ++hp;
	    --hl;
	}
    }
    return ret;
}

clib_response Clib_Async_Fetch(char *url, char *mime_headers)
{
  int            iresp, fd;
  clib_response  presp;

  if ((presp = Part_GetTCPsock(url, &fd)) != CLIB_OK)
    return presp;

  if ((iresp = do_http_send(fd, url, mime_headers, 
			    NULL, 0, 1, ICP_GET)) != 1) {
    Part_HandleFailure(fd);
    close(fd);
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Async_Fetch(url, mime_headers);
  }
  close(fd);
  return CLIB_OK;
}

clib_response Clib_Post(char *url, char *mime_headers,
                        char *data, unsigned data_len,
			clib_data *returned_data)
{
  int            iresp, fd;
  unsigned int   dlen;
  clib_response  presp;
  char          *cpdata = NULL;

  returned_data->fd = -1;
  if ((presp = Part_GetTCPsock(url, &fd)) != CLIB_OK)
    return presp;
  if ((iresp = do_http_send(fd, url, mime_headers, 
			    data, data_len, 1, ICP_POST)) != 1) {
    Part_HandleFailure(fd);
    close(fd);
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Post(url, mime_headers, data, data_len, returned_data);
  }

  if ((iresp = do_http_receive(fd, &dlen, &cpdata)) != 0) {

    if (iresp == -2) {
      /* Cutthrough! */
      returned_data->mime_headers = NULL;
      returned_data->mime_size = 0;
      returned_data->data = cpdata;
      returned_data->data_size = dlen;
      returned_data->fd = fd;
      return CLIB_CUTTHROUGH;
    }

    Part_HandleFailure(fd);
    close(fd);
    if (cpdata != NULL) {
      free(cpdata);
      cpdata = NULL;
    }
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Post(url, mime_headers, data, data_len, returned_data);
  }

  /* Parse out the header and content from the return data */
  get_header_content(cpdata, dlen,
		     &(returned_data->mime_headers),
		     &(returned_data->mime_size),
		     &(returned_data->data),
		     &(returned_data->data_size));
  if ((returned_data->data == NULL) || (returned_data->mime_headers == NULL)) {
    close(fd);
    if (cpdata)
      free(cpdata);
    return CLIB_SERVER_DOWN;
  }
  close(fd);
  if (cpdata)
   free(cpdata);
  return CLIB_OK;
}

clib_response Clib_Lowlevel_Op(char *indata, unsigned inlen,
			       char **outdata, unsigned *outlen,
			       int *fd)
{
  int            iresp, fud, rv;
  clib_response  presp;
  char *url;

  *outdata = NULL;
  *outlen = 0;

  url = get_url_from_data(indata, inlen);
  if (url == NULL) {
    *outdata = NULL;
    return CLIB_SERVER_DOWN;
  }

  if ((presp = Part_GetTCPsock(url, &fud)) != CLIB_OK) {
    free(url);
    return presp;
  }
  free(url);

  *outdata = NULL;
  if ((rv = correct_write(fud, indata, inlen)) != inlen) {
    Part_HandleFailure(fud);
    close(fud);
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Lowlevel_Op(indata, inlen, outdata, outlen, fd);
  }

  *outdata = NULL;
  *outlen = 0;
  if ((iresp = do_http_receive(fud, (unsigned *) outlen, outdata)) != 0) {

    if (iresp == -2) {
      /* Cutthrough! */
      *fd = fud;
      return CLIB_CUTTHROUGH;
    }

    Part_HandleFailure(fud);
    close(fud);
    if (*outdata != NULL) {
      free(*outdata);
      *outlen = 0;
      *outdata = NULL;
    }
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else 
      return Clib_Lowlevel_Op(indata, inlen, outdata, outlen, fd);
  }

  close(fud);
  return CLIB_OK;
}

clib_response Clib_Push(char *url, clib_data data)
{
  clib_response presp;
  int           putresp, fd;

  if ((presp = Part_GetTCPsock(url, &fd)) != CLIB_OK)
    return presp;

  putresp = do_http_put(fd, url, data.mime_headers, data.mime_size, data.data, data.data_size);
  if (putresp <= 0) {
    Part_HandleFailure(fd);
    close(fd);
    if (num_partitions == 0)
      return CLIB_SERVER_DOWN;
    else
      return Clib_Push(url, data);
  }

  close(fd);
  return CLIB_OK;
}

void Clib_Perror(clib_response response, char *err_str)
{
  switch(response) {
  case CLIB_OK:
    fprintf(stderr, "%s: everything is just fine.\n", err_str);
    break;

  case CLIB_CACHE_HIT:
    fprintf(stderr, "%s: cache hit\n", err_str);
    break;
  case CLIB_CACHE_MISS:
    fprintf(stderr, "%s: cache miss\n", err_str);
    break;         /* URL is not in the cache */

  case CLIB_NO_CONFIGFILE:
    fprintf(stderr, "%s: couldn't find configuration file\n", err_str);
    break;
  case CLIB_CONFIG_ERROR:
    fprintf(stderr, "%s: error while reading configuration file\n", err_str);
    break; 
  case CLIB_NO_SERVERS:
    fprintf(stderr, "%s: couldn't find any cache partitions\n", err_str);
    break;

  case CLIB_BAD_URL:
    fprintf(stderr, "%s: ill-formed URL\n", err_str);
    break;
  case CLIB_BAD_VERSION:
    fprintf(stderr, "%s: harvest partition version not same as clib\n",
	    err_str);
    break;
  case CLIB_TIMEOUT:
    fprintf(stderr,
	    "%s: harvest partition timed out talking to a web server\n",
	    err_str);
    break;
  case CLIB_ACCESS_DENIED:
    fprintf(stderr, "%s: harvest partition denied us access\n", err_str);
    break;
  case CLIB_SERVICE_UNAVAIL:
    fprintf(stderr, "%s: harvest partition doesn't support the operation\n",
	    err_str);
    break;
  case CLIB_SERVER_INTERNAL:
    fprintf(stderr, "%s: harvest partition suffered internal error\n",
	    err_str);
    break;
  case CLIB_SERVER_DOWN:
    fprintf(stderr, "%s: couldn't connect to any cache server\n",
	    err_str);
    break;
  case CLIB_SERVER_TIMEOUT:
    fprintf(stderr, "%s: we timed out talking to the harvest partition\n",
	    err_str);
    break;
  case CLIB_NO_FD:
    fprintf(stderr, "%s: no more file descriptors\n", err_str);
    break;
  case CLIB_NO_SUCH_PART:
    fprintf(stderr, "%s: no such partition (part_delete)\n", err_str);
    break;
  case CLIB_PTHREAD_FAIL:
    fprintf(stderr, "%s: pthread failure\n", err_str);
    break;
  case CLIB_SOCKETOP_FAIL:
    fprintf(stderr, "%s: a socket operation failed\n", err_str);
    break;
  default:
    fprintf(stderr, "%s: (unrecognized error)\n", err_str);
    break;
  }
}

void   get_header_content(char *data, int dlen,
		     char **mime_headers,
		     clib_u32 *mime_size,
		     char **content,
		     clib_u32 *content_size)
{
  /* Our task is simple - run through the data, looking for CRLFCRLF,
     CRCR, or LFLF.  We know the request was HTTP/1.0 or greater, so
     this pair will definitely happen before the data proper */
  char *cr, *lf, *crsplit = NULL, *lfsplit = NULL, *split = NULL;
  int   charsleft;

  crsplit = data;
  while ((crsplit-data) < dlen) {
    /* Get ready to try CRCR and CRLFCRLF */
    cr = (char *) memchr((void *) crsplit, 0x0D, dlen - (crsplit-data));
    if (cr  == NULL)
      break;

    charsleft = dlen - ((int) (cr-data)) - 1;
    crsplit = cr+1;

    /* Try for crcr */
    if (charsleft > 0) {
      if (*(cr+1) == 0x0D) {
	split = cr + 2;
	break;
      }
    } else {
      break;
    }

    /* Try for crlfcrlf */
    if ((split == NULL) && (charsleft >= 3)) {
      if ( (*(cr+1) == 0x0A) &&
	   (*(cr+2) == 0x0D) &&
	   (*(cr+3) == 0x0A)) {
	split = cr + 4;
	break;
      }
    } else {
      break;
    }
  }
  crsplit = split;

  lfsplit = data;
  split = NULL;
  while ((lfsplit-data) < dlen) {
    /* Last ditch attempt for LFLF */
    lf = (char *) memchr((void *) lfsplit, 0x0A, dlen - (lfsplit-data));
    if (lf == NULL)
      break;

    charsleft = dlen - ((int) (lf-data)) - 1;
    lfsplit = lf+1;

    if (charsleft > 0) {
      if ( *(lf+1) == 0x0A ) {
	split = lf + 2;
	break;
      }
    } else
      break;
  }
  lfsplit = split;

  if ((lfsplit == NULL) && (crsplit == NULL)) {
    *mime_headers = *content = NULL;
    *mime_size = *content_size = 0;
    return;
  }
  if (lfsplit > crsplit) {
    if (crsplit == NULL)
      split = lfsplit;
    else
      split = crsplit;
  }
  if (crsplit >= lfsplit) {
    if (lfsplit == NULL)
      split = crsplit;
    else
      split = lfsplit;
  }

  /* OK - We'd better have a sensible split now */
  if ((split - data) > dlen) {
    *mime_headers = *content = NULL;
    *mime_size = *content_size = 0;
    return;
  }

  /* Good. We have a split point.  Allocate space for header and content,
     and copy it in. */
  *mime_size = (clib_u32) (split - data);
  *content_size = (clib_u32) dlen - *mime_size;
  *content = (char *) malloc(*content_size + 1);
  *mime_headers = canonicalize_headers(data, *mime_size);
  *mime_size = strlen(*mime_headers);

  if ((*mime_headers == NULL) || (*content == NULL)) {
    if (*mime_headers != NULL) {
      free(*mime_headers);
      *mime_headers = NULL;
    }
    if (*content != NULL) {
      *content = NULL;
      free(*content);
    }
    *mime_size = *content_size = 0;
    return;
  }
  memcpy((void *) *content, split, *content_size);
  *(*content + *content_size) = '\0';
}

char  *get_url_from_data(char *indata, int inlen)
{
  char *start, *end, *buf;
  int   buflen;

  if ((start = strchr(indata, ' ')) == NULL)
    return NULL;

  start++;
  if ((end = strchr(start, ' ')) == NULL)
    return NULL;

  buflen = (int) (end - start) + 1;
  buf = (char *) malloc(sizeof(char) * buflen);
  if (buf == NULL) {
    fprintf(stderr, "Out of memory in get_url_from_data\n");
    exit(1);
  }
  memcpy(buf, start, buflen-1);
  *(buf + (buflen-1)) = '\0';
  return buf;
}

char  *canonicalize_headers(char *header_data, int header_len)
{
  /* Memory is (relatively) cheap, so we'll be nasty and just allocate
     twice of what we'd normally need, to ensure the canonicalization
     works.  This is not so bad, I think. */

  char *ret_hdr, *curpos, *bufpos;

  ret_hdr = (char *) malloc((sizeof(char)*2*header_len)+1);
  if (ret_hdr == NULL)
    return NULL;

  /* Now swing through the headers, line by line, and canonicalize that
     puppy.  Strategy:  scan through looking for either \r or \n;  copy
     everything up to that, then replace as many \r or \n 's that appear
     with a single \r\n.  Then, append one more to very end to get
     header termination. */

  curpos = header_data;
  bufpos = ret_hdr;
  while ( (curpos - header_data) < header_len) {
    if ((*curpos == '\r') || (*curpos == '\n')) {
      /* Aha - let's ignore all of this crazy stuff... */
      while ( (curpos - header_data < header_len) &&
	      ((*curpos == '\r') || (*curpos == '\n')) )
	curpos++;
      *bufpos = '\r';  bufpos++;
      *bufpos = '\n';  bufpos++;
    } else {
      /* Regular old character - just copy in. */
      *bufpos = *curpos;
      curpos++;
      bufpos++;
    }
  }
  *bufpos = '\r'; bufpos++;
  *bufpos = '\n'; bufpos++;
  *bufpos = '\0';
  return ret_hdr;
}

void
Clib_Free(clib_data *dat)
{
  if ((dat->mime_headers) && (dat->mime_size != 0)) {
    FREE(dat->mime_headers);
    dat->mime_headers = NULL;
  }
  if ((dat->data) && (dat->data_size != 0)) {
    FREE(dat->data);
    dat->data = NULL;
  }
}
