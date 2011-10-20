/*
 *     Author: Steve Gribble
 *       Date: Mar 3/97
 *       File: netsend.c
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
#include "utils.h"

int  num_targets = 0;
int  num_clients = 0;
int  target_fds[2000];

void parse_targets(void);
int filter_clients(lf_entry *next);

int main(int argc, char **argv)
{
  lf_entry lfntree;
  int      ret, i, j=0;

  if (argc != 2) {
    fprintf(stderr, "usage: netsend numclients\n");
    exit(1);
  }
  if (sscanf(argv[1], "%d", &num_clients) != 1) {
    fprintf(stderr, "usage: netsend numclients\n");
    exit(1);
  }
  fprintf(stderr, "using %d clients\n", num_clients);
  parse_targets();

  while(1) {
    if ((ret = lf_get_next_entry(0, &lfntree, 0)) != 0) {
      if (ret == 1)  /* EOF */
          exit(0);
      fprintf(stderr, "Failed to get next entry.\n");
      exit(1);
    }

    if (j++ == 5000) {
      j = 0;
      fprintf(stderr, "At %lu\n", htonl(lfntree.crs));
    }

    if (filter_clients(&lfntree)) {
      for (i=0; i<num_targets; i++) {
	lf_write_fd(target_fds[i], &lfntree);
      }
    }
    free(lfntree.url);
  }
  exit(0);
}

void parse_targets(void)
{
  FILE *tf;
  char  nextline[1500], *tmp, *tmp2;

  tf = fopen("targets.txt", "r");
  if (tf == NULL) {
    fprintf(stderr, "opening targets.txt failed\n");
    exit(1);
  }

  while (fgets(nextline, 1499, tf) != NULL) {
    if (nextline[strlen(nextline)-1] == '\n')
      nextline[strlen(nextline)-1] = '\0';

    tmp = strtok(nextline, " ");
    tmp2 = strtok(NULL, " ");
    if ((tmp == NULL) || (tmp2 == NULL))
      continue;
    target_fds[num_targets] = sconnect(tmp, tmp2);
    if (target_fds[num_targets] != -1) {
      num_targets++;
      fprintf(stderr, "opening %s %s succeeded\n", tmp, tmp2);
    } else
      fprintf(stderr, "opening %s %s failed\n", tmp, tmp2);
  }
}

int filter_clients(lf_entry *next)
{
  static UINT32 cip;
  unsigned char a, b, c, d;

  if ((ntohl(next->rdl) > 100000000) ||
      (ntohl(next->rhl) > 100000000))
    return 0;

  cip = ntohl(next->cip);
  a = (cip >> 24);
  b = (cip >> 16) % 256;
  c = (cip >> 8) % 256;
  d = (cip % 256);

  if (num_clients == 1)
    if ((a == 136) && (b == 152) && (c == 12) && (d == 8)) {
      return 1;
    } else
      return 0;

  if (num_clients == 2)
    if ((a == 136) && (b == 152) && (c == 12) &&
	((d >= 8) && (d <= 9)) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 4)
    if ((a == 136) && (b == 152) && (c == 12) &&
        ((d >= 8) && (d <= 34)) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 8)
    if ((a == 136) && (b == 152) && (c == 12) &&
        ((d >= 8) && (d <= 50)) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 16)
    if ((a == 136) && (b == 152) && (c == 12) &&
        ((d >= 8) && (d <= 110)) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 32)
    if ((a == 136) && (b == 152) && (c == 12) &&
        ((d >= 8) && (d <= 164)) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 64)
    if (((a == 136) && (b == 152) && (c == 12)) ||   
        ((a == 136) && (b == 152) && (c == 13) &&
	 ((d >= 11) && (d <= 50))) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 128)
    if (((a == 136) && (b == 152) && (c == 12)) ||   
        ((a == 136) && (b == 152) && (c == 13)) ||
	((a == 136) && (b == 152) && (c == 14))) {
      return 1;
    } else
      return 0;

  if (num_clients == 256)
    if (((a == 136) && (b == 152) && (c == 12)) ||   
        ((a == 136) && (b == 152) && (c == 13)) ||
	((a == 136) && (b == 152) && (c == 14)) ||
	((a == 136) && (b == 152) && (c == 15)) ||
	((a == 136) && (b == 152) && (c == 16)) ||
	((a == 136) && (b == 152) && (c == 17)) ||
	((a == 136) && (b == 152) && (c == 19)) ) {
      return 1;
    } else
      return 0;

  if (num_clients == 512)
    if ( (a == 136) && (b == 152) && 
         ((c >= 12) && (c <= 24)) ) {
      return 1;
    } else
      return 0;
  
  if (num_clients == 1024)
    if ((a == 136) && (b == 152) && 
	((c >= 10) && (c <= 29))) {
      return 1;
    } else
      return 0;

  if (num_clients == 2048)
    if ((a == 136) && (b == 152) && 
	((c >= 10) && (c <= 41))) {
      return 1;
    } else
      return 0;

  if (num_clients == 4096)
    if ((a == 136) && (b == 152) && 
	((c >= 10) && (c <= 78))) {
      return 1;
    } else
      return 0;

  if (num_clients == 8192)
    if ((a == 136) && (b == 152)) {
      return 1;
    } else
      return 0;

  fprintf(stderr, "what the heck?  I can't do %d clients\n", num_clients);
  exit(1);
  return 0;
}
