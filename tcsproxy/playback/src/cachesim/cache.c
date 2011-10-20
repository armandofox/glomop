/******
*** Simple cache simulator: Steve Gribble, Feb 20th, 1997
***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <strings.h>

#include "md5.h"
#include "cache.h"

#define C_ENTRY_HEAP_ALLOCSIZE 10000
cache_entry *heap_o_entries = NULL;

cache_entry *get_cache_entry(void);
void         free_cache_entry(cache_entry *freeme);
unsigned long int get_bytes(MD5_CTX *ctx);
unsigned long int get_bytes_from_hashval(cache_entry *head);
void         evict_lru(cache *tcache);

int initialize_cache(unsigned long long cache_size,
		     unsigned long long num_entries,
		     cache             *tcache)
{
  int i;

  if (tcache == NULL)
    return 0;

  /* Set up the cache fields */
  tcache->num_entries = num_entries;
  tcache->cur_num_entries = 0ULL;
  tcache->cache_size = cache_size;
  tcache->cur_cache_size = 0ULL;
  tcache->lru_head = tcache->lru_tail = NULL;
  
  /* Allocate an array of cache entry pointers */
  tcache->entries = (cache_entry_ptr *) calloc(num_entries,
					       sizeof(cache_entry_ptr));
  if (tcache->entries == NULL)
    return 0;

  /* Be a good citizen and allocate a heap of cache entries */
  if (heap_o_entries == NULL) {
    heap_o_entries = (cache_entry *) calloc(C_ENTRY_HEAP_ALLOCSIZE,
					    sizeof(cache_entry));
    if (heap_o_entries == NULL) {
      free(tcache->entries);
      tcache->entries = NULL;
      return 0;
    }
    for (i=0; i<C_ENTRY_HEAP_ALLOCSIZE-1; i++)
      heap_o_entries[i].lru_next = &(heap_o_entries[i+1]);
    heap_o_entries[C_ENTRY_HEAP_ALLOCSIZE-1].lru_next = NULL;
  }
  return 1;
}

int test_in_cache(char *url, cache *tcache)
{
  MD5_CTX the_md5;
  cache_entry *the_entry, *tmp;
  unsigned long int the_bytes;

  /* Get the offset into the array to check */
  MD5Init(&the_md5);
  MD5Update(&the_md5, url, strlen(url));
  MD5Final(&the_md5);
  the_bytes = get_bytes(&the_md5);
  
  /* Check in the array */
  the_entry = tcache->entries[the_bytes % tcache->num_entries];
  if (the_entry == NULL)
    return 0;

  while (the_entry != NULL) {
    if (memcmp(the_entry->hashval, the_md5.digest, NUM_HASH_BYTES) == 0) {

      /* Put this entry at the head of the LRU chain */
      if (the_entry->lru_prev != NULL)
	the_entry->lru_prev->lru_next = the_entry->lru_next;
      if (the_entry->lru_next != NULL)
	the_entry->lru_next->lru_prev = the_entry->lru_prev;

      if (tcache->lru_tail == the_entry)
	tcache->lru_tail = the_entry->lru_prev;
      if (tcache->lru_head == the_entry)
	tcache->lru_head = the_entry->lru_next;

      tmp = tcache->lru_head;
      tcache->lru_head = the_entry;
      the_entry->lru_next = tmp;
      the_entry->lru_prev = NULL;
      if (tmp != NULL)
	tmp->lru_prev = the_entry;
      return 1;
    }
    the_entry = the_entry->chain_next;
  }
  return 0;
}

int add_to_cache(char *url, unsigned long long size, cache *tcache)
{
  MD5_CTX the_md5;
  cache_entry *the_entry, *prev, *tmp;
  unsigned long int the_bytes;
  int retval = 0;

  /* See if we need to bump items from the cache */
  while (tcache->cur_cache_size + size > tcache->cache_size) {
    if (tcache->cache_size < size) {
      fprintf(stderr, "Trying to add size %llu, cache size %llu. Doomed! (url %s)\n",
	      size, tcache->cache_size, url);
      return 0;
      /* exit(1); */
    }
    evict_lru(tcache);
    retval &= 1;
  }

  /* Get the offset into the array to check */
  MD5Init(&the_md5);
  MD5Update(&the_md5, url, strlen(url));
  MD5Final(&the_md5);
  the_bytes = get_bytes(&the_md5);
  
  /* Check in the array */
  the_entry = tcache->entries[the_bytes % tcache->num_entries];
  if (the_entry == NULL) {
    /* This hash slot is empty.  Let's add to it. */
    the_entry = tcache->entries[the_bytes % tcache->num_entries] = 
      get_cache_entry();
    tcache->cur_num_entries++;
    tcache->cur_cache_size += size;
    the_entry->size = (unsigned long int) size;
    the_entry->byte_offset = the_bytes % tcache->num_entries;
    memcpy(the_entry->hashval, the_md5.digest, NUM_HASH_BYTES);

    /* Put this entry at head of LRU chain */
    tmp = tcache->lru_head;
    tcache->lru_head = the_entry;
    the_entry->lru_next = tmp;
    the_entry->lru_prev = NULL;
    if (tmp != NULL)
      tmp->lru_prev = the_entry;
    if (tcache->lru_tail == NULL)
      tcache->lru_tail = the_entry;

    /* Fix up chain structure */
      /* no need, as next and prev should be null from get_cache_entry(),
         and this is the only entry in the cache slot. */
  } else {
    /* Entries exist in this cache slot.  Check 'em all. */
    while (the_entry != NULL) {
      if (memcmp(the_entry->hashval, the_md5.digest, NUM_HASH_BYTES) == 0) {
	/* Put this entry at the head of the LRU chain, and fix up tail
	   if necessary.  We're overwriting the entry, so we don't have to
           fix up the chain linked list. */

	/* We're going to overwrite this entry, so deduct its size,
	 then add in new size */
	retval &= 2;
	tcache->cur_cache_size -= the_entry->size;
	the_entry->size = (unsigned long int) size;
	the_entry->byte_offset = the_bytes % tcache->num_entries;
	tcache->cur_cache_size += size;
	memcpy(the_entry->hashval, the_md5.digest, NUM_HASH_BYTES);

	/* Put entry at head of LRU chain */
	if (the_entry->lru_prev != NULL)
	  the_entry->lru_prev->lru_next = the_entry->lru_next;
	if (the_entry->lru_next != NULL)
	  the_entry->lru_next->lru_prev = the_entry->lru_prev;

	if (tcache->lru_tail == the_entry)
	  tcache->lru_tail = the_entry->lru_prev;
	if (tcache->lru_head == the_entry)
	  tcache->lru_head = the_entry->lru_next;
	tmp = tcache->lru_head;
	tcache->lru_head = the_entry;
	the_entry->lru_next = tmp;
	the_entry->lru_prev = NULL;
	if (tmp != NULL)
	  tmp->lru_prev = the_entry;
	break;
      }

      /* entry didn't match.  Get next from chain */
      prev = the_entry;
      the_entry = the_entry->chain_next;
    }

    if (the_entry == NULL) {

      /* None of the entries in the chain matched.  Let's add to the end. */
      the_entry = prev->chain_next = get_cache_entry();
      the_entry->size = (unsigned long int) size;
      tcache->cur_cache_size += size;
      the_entry->byte_offset = the_bytes % tcache->num_entries;
      memcpy(the_entry->hashval, the_md5.digest, NUM_HASH_BYTES);
      the_entry->chain_prev = prev;
      tcache->cur_num_entries++;

      /* Put this entry at the head of the LRU chain */
      if (the_entry->lru_prev != NULL)
	the_entry->lru_prev->lru_next = the_entry->lru_next;
      if (the_entry->lru_next != NULL)
	the_entry->lru_next->lru_prev = the_entry->lru_prev;

      if (tcache->lru_tail == the_entry)
	tcache->lru_tail = the_entry->lru_prev;
      if (tcache->lru_head == the_entry)
	tcache->lru_head = the_entry->lru_next;
      tmp = tcache->lru_head;
      tcache->lru_head = the_entry;
      the_entry->lru_next = tmp;
      the_entry->lru_prev = NULL;
      if (tmp != NULL)
	tmp->lru_prev = the_entry;
    }
  }
  return retval;
}

void dump_cache(cache *tcache, FILE *fileptr)
{
  unsigned long long int i;
  cache_entry            *foo;

  fprintf(stderr, "CACHE DUMP ARRAY:\n");
  for (i=0ULL; i<tcache->num_entries; i++) {
    if (tcache->entries[i] == NULL) {
      fprintf(stdout, "NULL: %llu\n", i);
    } else {
      fprintf(stdout, "Not NULL: %llu\n", i);
      foo = tcache->entries[i];
      while (foo != NULL) {
	fprintf(stdout, " %lu\n", foo->size);
	foo = foo->chain_next;
      }
    }
  }
  fprintf(stderr, "CACHE DUMP LRU:\n");
  foo = tcache->lru_head;
  while (foo != NULL) {
    fprintf(stdout, " %lu %lu %lu\n", foo->size,
	    (unsigned long int) foo->lru_prev,
	    (unsigned long int) foo->lru_next);
    foo = foo->lru_next;
  }
}

cache_entry *get_cache_entry(void)
{
  cache_entry *retme;
  int          i;

  if (heap_o_entries == NULL) {
    heap_o_entries = (cache_entry *) calloc(C_ENTRY_HEAP_ALLOCSIZE,
					    sizeof(cache_entry));
    if (heap_o_entries == NULL) {
      fprintf(stderr, "out of memory in get_cache_entry()\n");
      exit(1);  
      return NULL;
    }

    for (i=0; i<C_ENTRY_HEAP_ALLOCSIZE-1; i++)
      heap_o_entries[i].lru_next = &(heap_o_entries[i+1]);
    heap_o_entries[C_ENTRY_HEAP_ALLOCSIZE-1].lru_next = NULL;
  }

  retme = heap_o_entries;
  heap_o_entries = heap_o_entries->lru_next;
  retme->lru_next = retme->lru_prev = NULL;
  return retme;
}

void free_cache_entry(cache_entry *freeme)
{
  bzero(freeme, sizeof(cache_entry));
  freeme->lru_next = heap_o_entries;
  heap_o_entries = freeme;
}

unsigned long int get_bytes(MD5_CTX *ctx)
{
  unsigned long int res;

  memcpy((void *) &res, ctx->digest, sizeof(unsigned long int));
  return res;
}

unsigned long int get_bytes_from_hashval(cache_entry *head)
{
  unsigned long int res;

  memcpy((void *) &res, head->hashval, sizeof(unsigned long int));
  return res;
}

void evict_lru(cache *tcache)
{
  /* Get rid of the LRU entry in the cache */
  unsigned long int the_bytes;
  unsigned long long size;
  cache_entry *tail;

  if (tcache->lru_tail == NULL) {
    fprintf(stderr, "!!!!!!evict failed - nothing in cache!!!!!!!!!\n");
    tcache->cur_cache_size = 0;
    tcache->cur_num_entries = 0;
    return;
  }

  tail = tcache->lru_tail;
  if (tail == NULL) {
    fprintf(stderr, "nothing in chain...\n");
    return;
  }
  size = (unsigned long long) tcache->lru_tail->size;

  the_bytes = tail->byte_offset;
  tcache->lru_tail = tail->lru_prev;
  if (tcache->lru_tail != NULL)
    tcache->lru_tail->lru_next = NULL;
  if (tcache->lru_head == tail)
    tcache->lru_head = NULL;

  if (tail->chain_prev != NULL)
    tail->chain_prev->chain_next = tail->chain_next;
  if (tail->chain_next != NULL)
    tail->chain_next->chain_prev = tail->chain_prev;

  if (tail->chain_prev == NULL) {
    /* need to clean up array pointer */
    if (tail->chain_next == NULL) {
      /* NULL out array pointer */
      tcache->entries[the_bytes] = NULL;
    } else {
      tcache->entries[the_bytes] = tail->chain_next;
    }
  }
  tcache->cur_cache_size -= size;
  tcache->cur_num_entries -= 1;
  free_cache_entry(tail);  
}
