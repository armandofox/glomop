/*
 *     Author: Steve Gribble
 *       Date: Dec. 10th, 1996
 *       File: tracestats.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <math.h>

#include "config_tr.h"
#include "logparse.h"
#include "utils.h"

ll       mts = NULL;
long int mtc = 0L, mti=0L;

void mimetypes_init(void);
void mimetypes_process(lf_entry *nxt);
void mimetypes_dumpstats(FILE *dumptome, FILE *szdump, FILE *drdump);

void size_init(long int *hdr_buckets, long int *dta_buckets, long int *tot_buckets);
void size_process(lf_entry *nxt, long int *hdr_buckets, long int
		  *dta_buckets, long int *tot_buckets);
void size_dumpstats(FILE *dumptome, long int *hdr_buckets, long int
		    *dta_buckets, long int *tot_buckets, char *tag);

void dur_init(long int *durcs_buckets, long int *durss_buckets, long int *durt_buckets);
void dur_process(lf_entry *nxt, long int *durcs_buckets, long int
		 *durss_buckets, long int *durt_buckets);
void dur_dumpstats(FILE *dumptome, long int *durcs_buckets, long int
		 *durss_buckets, long int *durt_buckets, char *tag);

int  test_for_cgibin(char *url);
int  test_for_qm(char *url);
void get_extension(char *url, char *tag, int max_tagsize);
int  bucknum(UINT32 hdr);
int  bucknumll(unsigned long long int hdr);

int main(int argc, char **argv)
{
  lf_entry  lfntree;
  int       ret;
  FILE     *mt_file, *sz_file, *dur_file;

  /* Let's open the output files _first_ */
  if (argc != 4) {
    fprintf(stderr, "Usage: tracestats [mimetypes.stats] [size.stats] [dur.stats]\n");
    exit(1);
  }
  mt_file = fopen(*(argv+1), "w");
  sz_file = fopen(*(argv+2), "w");
  dur_file = fopen(*(argv+3), "w");
  if (mt_file == NULL) {
    fprintf(stderr, "Couldn't open %s for writing.\n", *(argv+1));
    exit(1);
  }
  if (sz_file == NULL) {
    fprintf(stderr, "Couldn't open %s for writing.\n", *(argv+2));
    exit(1);
  }
  if (dur_file == NULL) {
    fprintf(stderr, "Couldn't open %s for writing.\n", *(argv+3));
    exit(1);
  }

  /* Initialize the stats gathering code */
  mimetypes_init();

  /* Loop through all entries being given to us in stdin */
  while(1) {
    /* Get the next entry */
    if ((ret = lf_get_next_entry(0, &lfntree, 0)) != 0) {
      if (ret == 1)  /* EOF */
	break;
      fprintf(stderr, "Failed to get next entry.\n");
      break;
    }

    /* Process this next entry */
    mimetypes_process(&lfntree);

    /* Free extra space malloc'ed for entry's URL */
    free(lfntree.url);
  }

  /* We've looped through all entries, now save the stats out */
  mimetypes_dumpstats(mt_file, sz_file, dur_file);
  fprintf(stdout, "processed: %ld  ignored: %ld\n", mtc, mti);
  exit(0);
}




/******* The following analyze the entries for mime type *******/

/** Urls generically have the following format:
       http://foo.bar.net/blarg/blorg.foo?baz=1&bam=2
    or
       http://foo.bar.net/blarg/blorg?a=1&b=2

          In either case, we will look for the tag that finishes off the
          url ("foo" in the first case, "" in the second), whether or not
          the url has a ? or ; in it (true in both cases), and whether or
          not the url has either cgi-bin or cgibin in it (false in both
          cases).  This data will be added to the appropriate list entry.
 **/

#define MAX_TAG_SIZE 128
#define NUM_BUCS 2048
#define DUR_BUCS NUM_BUCS
typedef struct _mt_st {
  char tag[MAX_TAG_SIZE];
  unsigned long times_seen;           /* how many times this tag is seen */
  unsigned long times_seen_with_qm;   /* is ? or ; in URL? */ 
  unsigned long times_seen_with_cgi;  /* is cgi-bin or cgibin in URL? */
  unsigned long get;
  unsigned long post;
  unsigned long head;
  unsigned long put;
  unsigned long other;
  unsigned long clnt_nocache;
  unsigned long clnt_keepalive;
  unsigned long clnt_cachecntl;
  unsigned long clnt_ifmodsince;
  unsigned long clnt_unless;
  unsigned long srvr_nocache;
  unsigned long srvr_cachecntl;
  unsigned long srvr_expires;
  unsigned long srvr_lastmod;
  long int hdr_buckets[NUM_BUCS];
  long int dta_buckets[NUM_BUCS];
  long int tot_buckets[NUM_BUCS];
  long int durcs_buckets[NUM_BUCS];
  long int durss_buckets[NUM_BUCS];
  long int durt_buckets[NUM_BUCS];
} mt_st;

int mt_compare(void *d1, void *d2)
{
  mt_st *p1 = (mt_st *) d1, *p2 = (mt_st *) d2;
  return strcmp(p1->tag, p2->tag);
}

void mimetypes_init(void)
{
  mts = NULL;
  mtc = 0;
}

void mimetypes_process(lf_entry *nxt)
{
  mt_st dummy, *ptr;
  int   cgibin, qm;
  ll    entry;
  unsigned char clp, srp;
  int   i,j;

  cgibin = test_for_cgibin((char *) nxt->url);
  qm = test_for_qm((char *) nxt->url);
  get_extension((char *) nxt->url, dummy.tag, MAX_TAG_SIZE);

  mti++;
  j = strlen(dummy.tag);
  if (j > 18)
    return;
  for (i=0; i<j; i++) {
    if (dummy.tag[i] == ' ')
      return;
  }
  mti--;

  /* See if we have an entry corresponding to this */
  entry = ll_find(&mts, (void *) &dummy, mt_compare);
  if (entry != NULL) {
    ptr = (mt_st *) entry->data;
  } else {
    ptr = (mt_st *) calloc(sizeof(mt_st), 1);
    if (ptr == NULL) {
      fprintf(stderr, "Out of memory in mimetypes_process\n");
      exit(1);
    }
    strcpy(ptr->tag, dummy.tag);
    ll_add_to_end(&mts, (void *) ptr);
  }
  mtc++;
  ptr->times_seen++;
  if (cgibin)
    ptr->times_seen_with_cgi++;
  if (qm)
    ptr->times_seen_with_qm++;

  if (strncasecmp((char *) nxt->url, "GET", 3) == 0)
    ptr->get++;
  else if (strncasecmp((char *) nxt->url, "POST", 4) == 0)
    ptr->post++;
  else if (strncasecmp((char *) nxt->url, "HEAD", 4) == 0)
    ptr->head++;
  else if (strncasecmp((char *) nxt->url, "PUT", 3) == 0)
    ptr->put++;
  else
    ptr->other++;

  clp = nxt->cprg;
  srp = nxt->sprg;

  if (clp & PB_CLNT_NO_CACHE)
    ptr->clnt_nocache++;
  if (clp & PB_CLNT_KEEP_ALIVE)
    ptr->clnt_keepalive++;
  if (clp & PB_CLNT_CACHE_CNTRL)
    ptr->clnt_cachecntl++;
  if (clp & PB_CLNT_IF_MOD_SINCE)
    ptr->clnt_ifmodsince++;
  if (clp & PB_CLNT_UNLESS)
    ptr->clnt_unless++;
  if (srp & PB_SRVR_NO_CACHE)
    ptr->srvr_nocache++;
  if (srp & PB_SRVR_CACHE_CNTRL)
    ptr->srvr_cachecntl++;
  if (srp & PB_SRVR_EXPIRES)
    ptr->srvr_expires++;
  if (srp & PB_SRVR_LAST_MODIFIED)
    ptr->srvr_lastmod++;

  size_process(nxt, ptr->hdr_buckets, ptr->dta_buckets, ptr->tot_buckets);
  dur_process(nxt, ptr->durcs_buckets, ptr->durss_buckets, ptr->durt_buckets);
  if ((mtc/20000)*20000 == mtc) {
    fprintf(stdout, "Processed %ld, ignored %ld\n", mtc, mti);
  }
}
void mimetypes_dumpstats(FILE *dumptome, FILE *szdump, FILE *drdump)
{
  mt_st *ptr;
  ll    tmp = mts;

  while (tmp != NULL) {
    ptr = (mt_st *) tmp->data;
    fprintf(dumptome, "'%s' %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
	    ptr->tag, ptr->times_seen, ptr->times_seen_with_qm,
	    ptr->times_seen_with_cgi,
	    ptr->get, ptr->post, ptr->head, ptr->put, ptr->other,
	    ptr->clnt_nocache, ptr->clnt_keepalive, ptr->clnt_cachecntl,
	    ptr->clnt_ifmodsince, ptr->clnt_unless,
	    ptr->srvr_nocache, ptr->srvr_cachecntl,
	    ptr->srvr_expires, ptr->srvr_lastmod);
    tmp = tmp->next;
    size_dumpstats(szdump, ptr->hdr_buckets, ptr->dta_buckets, ptr->tot_buckets, ptr->tag);
    dur_dumpstats(drdump, ptr->durcs_buckets, ptr->durss_buckets, ptr->durt_buckets, ptr->tag);
  }
  return;
}


/******* The following analyze the entries for size info *******/

void size_init(long int *hdr_buckets, long int *dta_buckets, long int *tot_buckets)
{
  int i;

  for (i=0; i<NUM_BUCS; i++) {
    hdr_buckets[i] = dta_buckets[i] = tot_buckets[i] = 0L;
  }
}

void size_process(lf_entry *nxt, long int *hdr_buckets, long int
		  *dta_buckets, long int *tot_buckets)
{
  UINT32 hdr, dta, tot;

  hdr = nxt->rhl;  dta = nxt->rdl;  tot = hdr+dta;
  hdr_buckets[bucknum(hdr)]++;
  dta_buckets[bucknum(dta)]++;
  tot_buckets[bucknum(tot)]++;
}

void size_dumpstats(FILE *dumptome, long int *hdr_buckets, long int
		    *dta_buckets, long int *tot_buckets, char *tag)
{
  int i;

  fprintf(dumptome, "'%s' ", tag);
  for (i=0; i<NUM_BUCS-1; i++)
    fprintf(dumptome, "%ld ", hdr_buckets[i]);
  fprintf(dumptome, "%ld\n", hdr_buckets[NUM_BUCS-1]);

  fprintf(dumptome, "'%s' ", tag);
  for (i=0; i<NUM_BUCS-1; i++)
    fprintf(dumptome, "%ld ", dta_buckets[i]);
  fprintf(dumptome, "%ld\n", dta_buckets[NUM_BUCS-1]);

  fprintf(dumptome, "'%s' ", tag);
  for (i=0; i<NUM_BUCS-1; i++)
    fprintf(dumptome, "%ld ", tot_buckets[i]);
  fprintf(dumptome, "%ld\n", tot_buckets[NUM_BUCS-1]);
}

/******* The following analyze the entries for duration info *******/

void dur_init(long int *durcs_buckets, long int *durss_buckets, long int *durt_buckets)
{
  int i;

  for (i=0; i<DUR_BUCS; i++) {
    durcs_buckets[i] = durss_buckets[i] = durt_buckets[i] = 0L;
  }
}

void dur_process(lf_entry *nxt, long int *durcs_buckets, long int
		 *durss_buckets, long int *durt_buckets)
{
  unsigned long long int starte;
  unsigned long long int firste;
  unsigned long long int laste;
  unsigned long long int cs, ss, t;

  if ((nxt->crs == 0xFFFFFFFF) ||
      (nxt->srs == 0xFFFFFFFF) ||
      (nxt->sls == 0xFFFFFFFF))
    return;

  starte = (unsigned long long int) nxt->crs;
  firste = (unsigned long long int) nxt->srs;
  laste =  (unsigned long long int) nxt->sls;
  starte *= (unsigned long long int) 1000000U;
  firste *= (unsigned long long int) 1000000U;
  laste *=  (unsigned long long int) 1000000U;
  starte += (unsigned long long int) nxt->cru;
  firste += (unsigned long long int) nxt->sru;
  laste +=  (unsigned long long int) nxt->slu;

  cs = firste-starte;
  ss = laste-firste;
  t = laste-starte;

  durcs_buckets[bucknumll(cs)]++;
  durss_buckets[bucknumll(ss)]++;
  durt_buckets[bucknumll(t)]++;
}

void dur_dumpstats(FILE *dumptome, long int *durcs_buckets, long int
		 *durss_buckets, long int *durt_buckets, char *tag)
{
  int i;

  fprintf(dumptome, "'%s' ", tag);
  for (i=0; i<DUR_BUCS-1; i++)
    fprintf(dumptome, "%ld ", durcs_buckets[i]);
  fprintf(dumptome, "%ld\n", durcs_buckets[DUR_BUCS-1]);

  fprintf(dumptome, "'%s' ", tag);
  for (i=0; i<DUR_BUCS-1; i++)
    fprintf(dumptome, "%ld ", durss_buckets[i]);
  fprintf(dumptome, "%ld\n", durss_buckets[DUR_BUCS-1]);

  fprintf(dumptome, "'%s' ", tag);
  for (i=0; i<DUR_BUCS-1; i++)
    fprintf(dumptome, "%ld ", durt_buckets[i]);
  fprintf(dumptome, "%ld\n", durt_buckets[DUR_BUCS-1]);
}

/********** Some spiffy utility functions *************/
int test_for_cgibin(char *url)
{
  if (strstr(url, "cgi-bin") != NULL)
    return 1;
  if (strstr(url, "cgibin") != NULL)
    return 1;
  return 0;
}

int test_for_qm(char *url)
{
  if (strchr(url, '?') != NULL)
    return 1;
  if (strchr(url, ';') != NULL)
    return 1;
  return 0;
}

void get_extension(char *url, char *tag, int max_tagsize)
{
  /* If there is a question mark, look for the tag right before
     the question mark.  If not, look for the tag at the end of the
     string */

  char *endpt, *fndPt;

  /* Look for ? or ; to mark end of url-proper */
  endpt = strchr(url, '?');
  if (endpt == NULL)
    endpt = strchr(url, ';');

  if (endpt == NULL) {
    char *httppt;

    httppt = strstr(url, " HTTP/1.0");
    if (httppt == NULL)
      endpt = url + strlen(url) - 1;
    else
      endpt = httppt - 1;
  }
  else
    endpt -= 1;

  /* OK - lets work our way back, looking for a dot in the good case,
     and either a "/", ":", or the start of string in bad case */
  for (fndPt = endpt; fndPt > url; fndPt--) {
    if (*fndPt == '.') {
      /* We have our good case */
      int diff, i;

      diff = endpt - fndPt;
      if (diff >= max_tagsize)
	diff = max_tagsize-1;
      memcpy(tag, fndPt+1, diff);
      *(tag+diff) = '\0';
      for (i=0; i<diff; i++)
	*(tag+i) = tolower(*(tag+i));
      return;
    } else if ((*fndPt == ':') || (*fndPt == '/'))
      break;  /* bad case */
  }

  /* bad case */
  *tag = '\0';
  return;
}

int  bucknum(UINT32 hdr)
{
  double logmax, loghdr, n;
  int i;

  /* We're going to partition hdr into NUM_BUCS logarithmically spaced partitions. */
  if (hdr == (UINT32) 0)
    return 0;

  logmax = log10((double) ULONG_MAX);
  loghdr = log10((double) hdr);
  n = ((double) NUM_BUCS) * loghdr;
  n /= logmax;

  i = (int) n;
  return i;
}

#ifndef ULLONG_MAX
  #define ULLONG_MAX 18446744073709551615ULL
#endif
int  bucknumll(unsigned long long int hdr)
{
  double logmax, loghdr, n;
  int i;

  if (hdr == (unsigned long long int) 0)
    return 0;

  logmax = log10((double) ULLONG_MAX);
  loghdr = log10((double) hdr);
  n = ((double) NUM_BUCS) * loghdr;
  n /= logmax;

  i = (int) n;
  return i;
}
