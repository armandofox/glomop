#include "config_tr.h"
#include "debug.h"
#include "clib.h"
#include "thr_cntl.h"
#include "frontend.h"
#include "utils.h"
#include "comm_http.h"

#include <stdio.h>
#include <string.h>

Monitor *gMon = NULL;

clib_response
Clib_Lowlevel_Op(char *indata, unsigned inlen,
                 char **outdata, unsigned *outlen,
                 int *fd)
{
  return CLIB_OK;
}

int
main(int ac,char *argv[]) {
  Request hp;
  int sock;
  int len;
  int quit = 0;
  int newconn;
  
  sock = slisten(argv[1]);

  while (! quit) {
    newconn = saccept(sock);
    if (newconn == -1) {
      fprintf(stderr, "Conn failed: %s\n", strerror(errno));
      continue;
    }
    init_Request(&hp);
    hp.cli_hdrs.mime_headers = (char *)MALLOC(PERF_HTTP_TOTAL_HEADERLEN);
    assert(hp.cli_hdrs.mime_headers);
    hp.cli_hdrs.mime_size = PERF_HTTP_TOTAL_HEADERLEN;
    hp.cli_hdrs.fd = newconn;
    len = readline_or_timeout(&hp, -1, NULL);
    if (len == -1) {
      fprintf(stderr, "Error %d\n", len);
      continue;
    }
    fprintf(stderr, "Headers = '%s'\n", hp.cli_hdrs.mime_headers);
    len = parse_status_and_headers(&hp);
    if (len != HTTP_NO_ERR) {
      fprintf(stderr, "Error %s\n", http_strerror(len));
    } else {
      fprintf(stderr, "URL = '%s'\nMethod = '%s'\nHeaders='%s'\n\n\n",
              hp.url, hp.method, hp.cli_hdrs.mime_headers);
      if (strcmp(hp.method, "quit") == 0) {
        quit = 1;
      }
    }
  }
  shutdown(sock,2);
  close(sock);
  return 0;
}

  
