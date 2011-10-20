#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <strings.h>

#include "cache.h"
#include "httpstats.h"
#include "utils.h"

void usage(void);

int main(int argc, char **argv)
{
  cache_perf tc;
  unsigned long long cache_size, num_entries;
  unsigned long int report_interval;
  int   accept_socket, dsocket, cntr = 0;
  lf_entry   next_entry;

  if (argc != 5)
    usage();

  if (sscanf(argv[2], "%llu", &cache_size) != 1)
    usage();

  if (sscanf(argv[3], "%llu", &num_entries) != 1)
    usage();

  if (sscanf(argv[4], "%lu", &report_interval) != 1)
    usage();

  accept_socket = slisten(argv[1]);
  if (accept_socket == -1)
    usage();

  fprintf(stderr, "Starting up cache sim server, params:\n");
  fprintf(stderr, "  port %s, cachesize %llu, num_entries %llu, ri %lu\n",
	  argv[1], cache_size, num_entries, report_interval);

  if (!init_cache_el(cache_size, num_entries, &tc)) {
    fprintf(stderr, "initialize_cache failed.\n");
    usage();
  }

  if ( (dsocket = saccept(accept_socket)) < 0) {
    fprintf(stderr, "accept failed.\n");
    usage();
  }

  while(1) {
    int gne_res;

    gne_res = lf_get_next_entry(dsocket, &next_entry, 3);
    if ( (gne_res != 0) && (gne_res != 1)) {
      fprintf(stderr, "get_next_entry failed.  All done!\n");
      exit(0);
    }

    process_log_entry(&tc, &next_entry);
    cntr++;
    if (gne_res == 1) {
      fprintf(stderr, "All done!\n");
      exit(0);
    }

    if ( (cntr % report_interval) == 0)
      dump_cache_stats(&tc, stdout);

    free(next_entry.url);
  }

  exit(0);
}

void usage(void)
{
    fprintf(stderr, "Usage: cachesim port cache_size num_entries report_interval\n");
    exit(1);
}
