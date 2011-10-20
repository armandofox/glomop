/***
** For gathering statistics on cache performance, and logic for figuring out
** if a request is uncachable, etc.
***/

#include "cache.h"
#include "logparse.h"

#ifndef HTTP_STATS_H
#define HTTP_STATS_H

typedef struct cache_perf_st {
  unsigned long int num_requests;
  unsigned long int num_misses;
  unsigned long int client_nocache_misses;
  unsigned long int server_nocache_misses;
  unsigned long int if_mod_since_misses;
  unsigned long int cache_add_evict;
  unsigned long int cache_add_refresh;
  unsigned long int trace_start_time;       /* in seconds since epoch */
  unsigned long int trace_latest_time;      /* in seconds since epoch */
  cache             the_cache;
} cache_perf;

/* Initialize the cache_perf and the_cache object.  Returns 1 on
   success, 0 on failure. */
int init_cache_el(unsigned long long cache_size,
		  unsigned long long num_entries,
		  cache_perf *init_me);

/* Process the log entry.  It will perform all the necessary magic
   and logic to test for pragma no_cache's etc, and update the cache
   statistics.  1 for success, 0 for failure. */
int process_log_entry(cache_perf *the_cache,
		      lf_entry *the_entry);

/* Dump out the cache state to the log file.  Presumably this will
   be done periodically by the callee.  Some standard output format
   (a number of fields per line) will be used.  I'll document the
   format once I've written it.  1 for success, 0 for failure. */
int dump_cache_stats(cache_perf *the_cache, FILE *output_file);

#endif
