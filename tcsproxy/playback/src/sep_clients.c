/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: sepclients.c
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

#define CLIENT_BLOCK_SIZE 5000

int sanitycheck(lf_entry *entry);

int main(int argc, char **argv)
{
  lf_entry *lfn_array;
  int      ret, i, done=0, j;
  char     outfilename[1024];

  lfn_array = (lf_entry *) malloc((CLIENT_BLOCK_SIZE+1)*sizeof(lf_entry));
  if (lfn_array == NULL) {
    fprintf(stderr, "Out of memory allocating lfn_array.\n");
    exit(1);
  }
  
  if (argc != 1) {
    fprintf(stderr, "Usage: sep_clients < infile\n");
    exit(1);
  }

  while(!done) {
    for (i=0; i<CLIENT_BLOCK_SIZE; i++) {
      if ((ret = lf_get_next_entry(0, lfn_array+(unsigned long)i, 0)) != 0) {
	if (ret == 1)  /* EOF */
          break;
	fprintf(stderr, "Failed to get next entry.\n");
	exit(1);
      }
    }
    fprintf(stderr, "Got %d entries in block, processing.\n", i);
    if (i != CLIENT_BLOCK_SIZE)
      done = 1;

    /* OK - we've sucked in a block of i lfntrees.  Let's dump em out
       to the right client files. */
    for (j=0; j<i; j++) {
      lf_entry *t = lfn_array + (unsigned long) j;
      lf_entry *u;

      if (t->url == NULL) {
	/* just skip to next */
      } else if (!sanitycheck(t)) {
	fprintf(stderr, "url %s failed sanitycheck\n", t->url);
	free(t->url);
	t->url = NULL;
      } else {
	int k;
	FILE *outf;
	char  ebuf[128], *tmp, mkdirbuf[256];
	UINT32 cip;

/*	fprintf(stderr, "url %s passed sanitycheck\n", t->url);*/
	/* Let's open up the file for this sucker, and start spewing */
	lf_ntoa(ntohl(t->cip), ebuf);
	tmp = strtok(ebuf, ".");
	strcpy(outfilename, tmp);
	strcat(outfilename, ".");
	tmp = strtok(NULL, ".");
	strcat(outfilename, tmp);
	strcat(outfilename, ".");
	tmp = strtok(NULL, ".");
	strcat(outfilename, tmp);
	sprintf(mkdirbuf, "%s", outfilename);
	strcat(outfilename, "/");
	tmp = strtok(NULL, ".");
	strcat(outfilename, tmp);
	outf = fopen(outfilename, "a");
	cip = t->cip;
	if (outf == NULL) {
	  mkdir(mkdirbuf, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	  outf = fopen(outfilename, "a");
	  if (outf == NULL) {
	    fprintf(stderr, "Failed to open %s for append.\n", outfilename);
	    exit(1);
	  }
	}

	for (k=j; k<i; k++) {
	  u = lfn_array + (unsigned long) k;
	  if ((u->cip == cip) && (u->url != NULL)) {
	    lf_write(outf, u);
	    free(u->url);
	    u->url = NULL;
	  }
	}
	fclose(outf);
      }
    }
  }
  exit(0);
}

int sanitycheck(lf_entry *entry)
{
  if ( (strcmp((char *) entry->url, "-") == 0) ||
       (strlen((char *) entry->url) < 4) ||
       (entry->sip == 0xFFFFFFFF) ||
       (entry->spt == 0xFFFF) ||
       !((strncmp((char *) entry->url, "GET ", 4) == 0) ||
	 (strncmp((char *) entry->url, "POST ", 5) == 0) ||
	 (strncmp((char *) entry->url, "HEAD ", 5) == 0) ||
	 (strncmp((char *) entry->url, "PUT ", 4) == 0)) )
    return 0;
  return 1;
}
