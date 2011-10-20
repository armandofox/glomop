#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#include "clib.h"
#include "icp.h"
#include "utils.h"
#include "utils.h"
#include "part.h"
#include "libmon.h"

#include "../../config_tr.h"
#include "optdb.h"

extern struct sockaddr *localaddr;
Monitor *gMon = NULL;

char defaulturl[] = "http://www.cs.berkeley.edu/";

int do_proper_read(int tcp_fd, char *buff, size_t size);

int main(int argc, char **argv)
{
  clib_data           returned_data, put_data;
  char                *url = defaulturl;
  char                srv1[2048], srv2[2048];
  char                url1[2048], url2[2048], urlfin[4096];
  clib_response       clresp, clresp1;

  Options             myOpt;

  if (argc == 2) {
    url = argv[1];
  }

  if (Options_New(&myOpt, "gm_options.testclient") != success) {
    fprintf(stderr, "Options_New failed.\n");
    exit(1);
  }
  Clib_Initialize(myOpt, NULL);

  clresp = Clib_Async_Fetch(url, NULL);

  exit(0);

/*  clresp = Clib_Post("http://www.matisse.net/cgi-bin/test",
		     NULL, "test", 4, &returned_data);
  printf("(%lu):%s\n(%lu):%s\n", returned_data.mime_size,
	 returned_data.mime_headers, returned_data.data_size,
	 returned_data.data);
  sleep(2);
  clresp = Clib_Query(url);
  Clib_Perror(clresp, "Query");
  clresp = Clib_Fetch(url, NULL, &returned_data);
  Clib_Perror(clresp, "Fetch");
  printf("(%lu):%s\n(%lu):%s\n", returned_data.mime_size,
	 returned_data.mime_headers, returned_data.data_size,
	 returned_data.data);*/

  /* Go out and fetch an URL/IP */
  printf("Enter hostname of real server: ");
  scanf("%s", srv1);
  printf("Enter URL of real data: ");
  scanf("%s", url1);
  printf("Enter hostname of duped server: ");
  scanf("%s", srv2);
  printf("Enter URL of duped data: ");
  scanf("%s", url2);

  sprintf(urlfin, "http://%s%s", srv1, url1);
  clresp1 = Clib_Fetch(urlfin, NULL, &returned_data);
  if (clresp1 != CLIB_OK) {
    fprintf(stderr, "Fetch of real data failed.\n");
    Clib_Perror(clresp1, "Fetch real");
    exit(1);
  }
  
  sprintf(urlfin, "http://%s%s", srv2, url2);
  put_data.mime_headers = returned_data.mime_headers;
  put_data.mime_size = returned_data.mime_size;  /* Isn't looked at on PUT */
  put_data.data = returned_data.data;
  put_data.data_size = returned_data.data_size;
  clresp = Clib_Push(urlfin, put_data);
  Clib_Perror(clresp, "Push");
  free(returned_data.mime_headers);
  free(returned_data.data);

  exit(0);
}

/****
**  This function will perform a blocking read of exactly size bytes from the
**  stream-oriented file descriptor, or will fail.  It returns the number of bytes
**  read, 0 for end-of-file, or -1 for error.
****/

int do_proper_read(int tcp_fd, char *buff, size_t size)
{
  int result, num_so_far = 0;

  while (num_so_far != size) {
    result = read(tcp_fd, buff + (unsigned) num_so_far, size-num_so_far);
    if (result < 0) {
       perror("do_proper_read: ");
    }
    if (result < 0) 
      return result;
    num_so_far += result;
    if (result == 0)
      return num_so_far;
  }

#if 0
  {
    int i, j;

    for (i=0; i<num_so_far/5; i++) {
      for (j=0; j<5; j++) {
	printf("%.02x ", *((unsigned char *) (buff + (unsigned) (5*i+j))));
      }
      printf("\n");
    }
    if ((num_so_far/5)*5 != num_so_far) {
      for (j=0; j<num_so_far % 5; j++)
	printf("%.02x ", *((unsigned char *) (buff + (unsigned) (5 * (num_so_far/5) + j))));
      printf("\n");
    }
  }
#endif
  return num_so_far;
}

