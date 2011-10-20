/*
 *     Author: Steve Gribble
 *       Date: Dec. 8th, 1996
 *       File: simclient.h
 */

#include "utils.h"
#include "config_tr.h"
#include "utils.h"
#include "logparse.h"

#ifndef PB_SIMCLIENT_H
#define PB_SIMCLIENT_H

#define ENTRY_CACHE_SIZE 30

typedef struct simclient_entry_st {
  /* the path to the file */
  char              *client_path;

  /* the current offset into the file */
  int                fd;
  unsigned char      is_fd_valid;
  unsigned long int  curr_fd_offset;

  /* a cache of entries from the client file */
  lf_entry           entry_cache[ENTRY_CACHE_SIZE];
  unsigned int       entry_cache_offset;
  unsigned int       max_in_cache;
  pthread_mutex_t    entry_cache_mutex;
} simclient_entry;

#define NUM_FDS_USED 30
typedef struct fd_buf_struct {
  simclient_entry      *entry_with_fd;
  struct fd_buf_struct *next, *prev;
} fd_buf_el;

typedef struct simclient_list_st {
  unsigned long int    cur_num_clients;
  unsigned long int    max_num_clients;
  simclient_entry     *list_o_clients;

  fd_buf_el            fd_buf_ptrs[NUM_FDS_USED];
  fd_buf_el           *head, *tail;
  pthread_mutex_t      fd_buf_ptrs_mutex;
} simclient_list;

/* 
   Initialize the list o' clients structure.  0 for failure, 1 for success.
   THREAD UNSAFE.
*/

int init_list_o_clients(simclient_list *clist,
			unsigned long int start_num_clients);

/* 
   Add a client to the list of clients from which we will be sucking data.
   The following arguments are used:

   client_path:     a (relative or absolute) path to the (uncompressed) file
                    containing the client trace data.
   start_random:    if non-zero, start at a random offset in the trace
                    file.  Note that this will take a long time to initialize,
                    as we have to scan through the file to find the
                    appropriate entry.  (NOT CURRENTLY IMPLEMENTED - SG)

   Returns 0 for failure, a positive client_id for success.
   Will initialize client cache by opening file, reading data, etc.

   THREAD UNSAFE.
*/

unsigned long int add_client_to_list(char           *client_path,
		                     unsigned int    start_random,
		                     simclient_list *clist);

/*
   Get the next entry for the client.  Will wrap around the traces if
   we should hit the end of the file.  Once you've used a client entry,
   YOU are responsible for free'ing the url field of the lf_entry, as
   well as the lf_entry itself..

   THREAD SAFE.
*/

lf_entry *get_client_entry(simclient_list *clist,
			   unsigned long int client_id);

#endif
