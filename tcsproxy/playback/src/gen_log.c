/*
 *     Author: Steve Gribble
 *       Date: Mar. 5, 1997
 *       File: gen_log.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "logparse.h"
#include "utils.h"

unsigned long lf_aton(char *addr);

int main(int argc, char **argv)
{
  lf_entry lfntry, lfarray[100];
  unsigned char url0[1024], url1[1024], url2[1024], url3[1024],
    url4[1024], url5[1024], url6[1024], url7[1024], url8[1024];
  long int i, num;
  
  if ( (argc != 2) ||
       ((sscanf(argv[1], "%ld", &num)) != 1)) {
    fprintf(stderr, "Usage: gen_log size > outfile\n");
    exit(1);
  }

  lfntry.version = 3;
  lfntry.crs = lfntry.cru = lfntry.srs = lfntry.sru = htonl(0);
  lfntry.sls = lfntry.slu = lfntry.cip = htonl(0);
  lfntry.cpt = htons(0);

  lfntry.sip = htonl(lf_aton("128.32.35.31"));
  lfntry.spt = htons(80);

  lfntry.cprg = lfntry.sprg = 0;
  lfntry.cims = lfntry.sexp = lfntry.slmd = htonl(0);
  lfntry.rhl = lfntry.rdl = htonl(0);

  lfarray[0] = lfarray[1] = lfarray[2] = lfarray[3] = 
    lfarray[4] = lfarray[5] = lfarray[6] = lfarray[7] = lfarray[8] = lfntry;

  lfarray[0].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/cobra.gif HTTP/1.0"));
  sprintf((char *) url0, 
	  "GET /~gribble/imagedir/testpics/cobra.gif HTTP/1.0");
  lfarray[0].url = url0;

  lfarray[1].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/jupiter5.gif HTTP/1.0"));
  sprintf((char *) url1, 
	  "GET /~gribble/imagedir/testpics/jupiter5.gif HTTP/1.0");
  lfarray[1].url = url1;

  lfarray[2].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/kitty.gif HTTP/1.0"));
  sprintf((char *) url2, 
	  "GET /~gribble/imagedir/testpics/kitty.gif HTTP/1.0");
  lfarray[2].url = url2;

  lfarray[3].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/vader.gif HTTP/1.0"));
  sprintf((char *) url3,
	  "GET /~gribble/imagedir/testpics/vader.gif HTTP/1.0");
  lfarray[3].url = url3;

/*  lfarray[4].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/6.jpg HTTP/1.0"));
  sprintf((char *) url4,
	  "GET /~gribble/imagedir/testpics/6.jpg HTTP/1.0");
  lfarray[4].url = url4;

  lfarray[5].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/4.jpg HTTP/1.0"));
  sprintf((char *) url5,
	  "GET /~gribble/imagedir/testpics/4.jpg HTTP/1.0");
  lfarray[5].url = url5;

  lfarray[6].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/26.jpg HTTP/1.0"));
  sprintf((char *) url6,
	  "GET /~gribble/imagedir/testpics/26.jpg HTTP/1.0");
  lfarray[6].url = url6;

  lfarray[7].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/17.jpg HTTP/1.0"));
  sprintf((char *) url7,
	  "GET /~gribble/imagedir/testpics/17.jpg HTTP/1.0");
  lfarray[7].url = url7;

  lfarray[8].urllen = 
    htons(strlen("GET /~gribble/imagedir/testpics/12.jpg HTTP/1.0"));
  sprintf((char *) url8,
	  "GET /~gribble/imagedir/testpics/12.jpg HTTP/1.0");
  lfarray[8].url = url8;*/

  for (i=0; i<num; i++) {
    if (lf_write(stdout, &(lfarray[i%4])) != 0) {
      fprintf(stderr, "lf_write failed\n");
      exit(1);
    }
  }

  exit(0);
}

unsigned long lf_aton(char *addr)
{
  unsigned int ap, bp, cp, dp;
  unsigned char a, b, c, d;
  unsigned long ret;

  if (sscanf(addr, "%u.%u.%u.%u", &ap, &bp, &cp, &dp) != 4) {
    fprintf(stderr, "lf_aton failed.\n");
    exit(1);
  }

  a = ap; b = bp; c = cp; d = dp;
  ret = (a << 24) | (b << 16) | (c << 8) | d;
  return ret;
}
