/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: showtrace.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "logparse.h"

int main(int argc, char **argv)
{
  lf_entry lfntree;
  int      ret;

  while(1) {
    if ((ret = lf_get_next_entry(0, &lfntree, 0)) != 0) {
      if (ret == 1)  /* EOF */
          exit(0);
      fprintf(stderr, "Failed to get next entry.\n");
      exit(1);
    }
    lf_dump(stdout, &lfntree);
    free(lfntree.url);
  }
  exit(0);
}
