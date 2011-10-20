/*
 * Author:  Steve Gribble
 * Date:    Feb. 19th/1997
 *
 * File:    cache.h
 * Purpose: interface to cache simulator
 */

#ifndef CACHE_H
#define CACHE_H

#define NUM_HASH_BYTES 6
typedef struct cache_entry_st {
  unsigned char   hashval[NUM_HASH_BYTES];    /* the hashed URL */
  struct cache_entry_st *lru_prev, *lru_next; /* for LRU purposes */
  struct cache_entry_st *chain_next;          /* for chaining purposes */
  struct cache_entry_st *chain_prev;          /* for chaining purposes */
  unsigned long int size;                     /* element size */
  unsigned long int byte_offset;              /* offset in array */
} cache_entry, *cache_entry_ptr;

typedef struct cache_st {
  cache_entry_ptr   *entries;             /* array of item pointers */
  unsigned long long num_entries;         /* max number of items in cache */
  unsigned long long cur_num_entries;     /* current number of entries */
  unsigned long long cache_size;          /* max cache size, in bytes */
  unsigned long long cur_cache_size;      /* current cache size, in bytes */

  cache_entry *lru_head;                  /* the most recently used page */
  cache_entry *lru_tail;                  /* the least recently used page */
} cache;

/* initialize a cache structure.  cache_size will limit the total size of
   data stored in the cache to cache_size bytes.  num_entries is a hint to
   the cache routines, for how many entries the cache is likely to hold at
   any given time.  The cache routines will grow the cache as necessary.
   (I hope).  A good ballpark average from the web is about 3KB per cache
   entry.  This function returns 1 on success, 0 on failure. */

int initialize_cache(unsigned long long cache_size,
		     unsigned long long num_entries,
		     cache             *tcache);

/* test_in_cache takes an 'url', and tests whether or not it is in the
   the cache 'tcache'.  If the URL is in the cache, return 1, if not, return
   0.  This test is considered to be a cache access, so the LRU list will
   be munged as appropriate. */

int test_in_cache(char *url, cache *tcache);

/* add an url to the cache.  It will evict the least recently used item
   from the cache if the cache would overflow with this entry.  This
   function returns 0 if the URL was added and nothing was evicted, 
   1 if the URL was added and something was evicted, and 2 if the URL
   was refreshed (i.e. it already existed). */

int add_to_cache(char *url, unsigned long long size, cache *tcache);

/* Dumps out the cache state */
void dump_cache(cache *tcache, FILE *fileptr);

#endif
