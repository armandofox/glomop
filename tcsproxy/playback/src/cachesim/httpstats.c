/***
** For gathering statistics on cache performance, and logic for figuring out
** if a request is uncachable, etc.
**
** Steve Gribble, Feb 21st/97
***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "http_task.h"
#include "httpstats.h"
#include "cache.h"
#include "logparse.h"

int sanitycheck(lf_entry *entry);

int init_cache_el(unsigned long long cache_size,
		  unsigned long long num_entries,
		  cache_perf *init_me)
{
  bzero(init_me, sizeof(cache_perf));
  if (!initialize_cache(cache_size, num_entries, &(init_me->the_cache)))
    return 0;

  return 1;
}

int process_log_entry(cache_perf *the_cache,
		      lf_entry *the_entry)
{
  int miss = 0;
  http_task footask;

/*  dump_cache(&(the_cache->the_cache), stderr);*/

  /* Make sure isn't garbage entry */
  if (!sanitycheck(the_entry)) {
/*    fprintf(stderr, "\n\n!!!!!Failed sanity check..!!!!\n\n");
    lf_dump(stderr, the_entry);*/
    return 0;
  }

  footask.url = the_entry->url;
  footask.urllen = the_entry->urllen;
  footask.dst_host = the_entry->sip;
  insert_host_into_url(&footask);
  the_entry->url = footask.url;
  the_entry->urllen = footask.urllen;
  lf_convert_order(the_entry);

  if (the_cache->trace_start_time == 0) {
    fprintf(stderr, "setting start time.\n");
    the_cache->trace_start_time = the_entry->crs;
  }
  the_cache->trace_latest_time = the_entry->crs;  /* more or less right */

  the_cache->num_requests++;

  /* Test to see if we're forced to miss because of pragmas */
  if (the_entry->cprg & PB_CLNT_NO_CACHE) {
    the_cache->client_nocache_misses++;
    miss = 1;
  }
  if (the_entry->sprg & PB_SRVR_NO_CACHE) {
    the_cache->server_nocache_misses++;
    miss = 1;
  }
  if (the_entry->cprg & PB_CLNT_IF_MOD_SINCE) {
    if (the_entry->rhl > 5) {  /* sanity check got decent response */
      if (the_entry->rdl > 5) {
	the_cache->if_mod_since_misses++;
	miss = 1;
      }
    }
  }

  if (miss == 1) {
    the_cache->num_misses++;
  } else {
    int res;

    if (!test_in_cache((char *) the_entry->url, &(the_cache->the_cache))) {
      the_cache->num_misses++;
      res = add_to_cache((char *) the_entry->url, 
		   (unsigned long long) (the_entry->rhl + the_entry->rdl),
		   &(the_cache->the_cache));

      if (res & 1)
	the_cache->cache_add_evict++;
      if (res & 2)
	the_cache->cache_add_refresh++;
    }
  }
  lf_convert_order(the_entry);
  return 1;
}

int dump_cache_stats(cache_perf *the_cache, FILE *output_file)
{
  fprintf(output_file,
	  "%lu %lu %lu %lu %lu %lu %lu %lu %lu %llu %llu\n",
	  the_cache->num_requests,
	  the_cache->num_misses,
	  the_cache->client_nocache_misses,
	  the_cache->server_nocache_misses,
	  the_cache->if_mod_since_misses,
	  the_cache->cache_add_evict,
	  the_cache->cache_add_refresh,
	  the_cache->trace_start_time,
	  the_cache->trace_latest_time,
	  the_cache->the_cache.cur_num_entries,
	  the_cache->the_cache.cur_cache_size);
  return 1;
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
         (strncmp((char *) entry->url, "PUT ", 4) == 0)) ||
       (entry->rhl + entry->rdl == 0)
    ) {
    return 0;
  }
  return 1;
}
