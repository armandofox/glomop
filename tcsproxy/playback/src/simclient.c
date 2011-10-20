/*
 *     Author: Steve Gribble
 *       Date: December 8th, 1996
 *       File: simclient.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "simclient.h"

int  get_fd_for_client(unsigned long int client_id, simclient_list *clist);
void suck_in_buffer_data(unsigned long int client_id, simclient_list *clist);

int init_list_o_clients(simclient_list *clist,
			unsigned long int start_num_clients)
{
  int i, res;

  fprintf(stderr, "%d\n", sizeof(simclient_entry));
  /* Initialize the list of clients structure */
  clist->cur_num_clients = 0;
  clist->max_num_clients = start_num_clients;
  clist->list_o_clients = (simclient_entry *) 
    calloc(start_num_clients, sizeof(simclient_entry));
  if (clist->list_o_clients == NULL) {
    fprintf(stderr, "Out of memory in init_list_o_clients\n");
    return 0;
  }
  for (i=0; i<start_num_clients; i++) {
    if ((res = pthread_mutex_init(
      &((clist->list_o_clients[i]).entry_cache_mutex), NULL)) != 0) {
      strerror(res);
      exit(1);
    }
  }

  /* The doubly linked list of open file descriptors */
  for (i=0; i<NUM_FDS_USED; i++) {
    (clist->fd_buf_ptrs[i]).entry_with_fd = NULL;
    if (i != (NUM_FDS_USED-1))
      (clist->fd_buf_ptrs[i]).next = &(clist->fd_buf_ptrs[i+1]);
    else
      (clist->fd_buf_ptrs[i]).next = NULL;
    if (i != 0)
      (clist->fd_buf_ptrs[i]).prev = &(clist->fd_buf_ptrs[i-1]);
    else
      (clist->fd_buf_ptrs[i]).prev = NULL;
  }
  clist->head = &(clist->fd_buf_ptrs[0]);
  clist->tail = &(clist->fd_buf_ptrs[NUM_FDS_USED-1]);
  if ((res = pthread_mutex_init(&(clist->fd_buf_ptrs_mutex), NULL)) != 0) {
    strerror(res);
    exit(1);
  }
  return 1;
}

unsigned long int add_client_to_list(char           *client_path,
		                     unsigned int    start_random,
		                     simclient_list *clist)
{
  unsigned long int  ret_id = 0UL;
  simclient_entry   *tmp = NULL;

  if (pthread_mutex_lock(&(clist->fd_buf_ptrs_mutex)) < 0)
    exit(1);

  /* Make sure we have space for this new client */
  clist->cur_num_clients++;
  if (clist->cur_num_clients == clist->max_num_clients) {
    tmp = (simclient_entry *) realloc(clist->list_o_clients,
		  2*clist->max_num_clients*sizeof(simclient_entry));
    if (tmp == NULL) {
      fprintf(stderr, "Out of memory in add_client_to_list\n");
      if (pthread_mutex_unlock(&(clist->fd_buf_ptrs_mutex)) < 0)
	exit(1);
      return 0UL;
    }
    bzero(&(tmp[clist->max_num_clients]),
	  (clist->max_num_clients)*sizeof(simclient_entry));
    clist->max_num_clients *= 2;
    clist->list_o_clients = tmp;
  }
  ret_id = clist->cur_num_clients;

  /* Let's prep the client */
  tmp = &((clist->list_o_clients)[ret_id]);
  tmp->client_path = (char *) malloc(sizeof(char) * (1 + strlen(client_path)));
  if (tmp->client_path == NULL) {
    fprintf(stderr, "Out of memory in add_client_to_list (2)\n");
    if (pthread_mutex_unlock(&(clist->fd_buf_ptrs_mutex)) < 0)
      exit(1);
    return 0UL;
  }
  strcpy(tmp->client_path, client_path);

  /* Open up the file */
  if (pthread_mutex_unlock(&(clist->fd_buf_ptrs_mutex)) < 0)
    exit(1);

  /* Suck in the buffer data */
  suck_in_buffer_data(ret_id, clist);
  return ret_id;
}

lf_entry *get_client_entry(simclient_list *clist, 
			   unsigned long int client_id)
{
  simclient_entry *tmp;
  lf_entry  *retval;

  /* Job - pull entry out of cache.  If cache runs out, suck in some
     more. */

  if (pthread_mutex_lock(&(clist->fd_buf_ptrs_mutex)) < 0)
    exit(1);
  tmp = &(clist->list_o_clients[client_id]);
  if (pthread_mutex_unlock(&(clist->fd_buf_ptrs_mutex)) < 0)
    exit(1);
  while ((tmp->entry_cache_offset == tmp->max_in_cache) ||
	 (tmp->max_in_cache == 0UL))
    suck_in_buffer_data(client_id, clist);

  retval = (lf_entry *) malloc(sizeof(lf_entry));
  if (retval == NULL) {
    fprintf(stderr, "Out of memory in get_client_entry()\n");
    exit(1);
  }
  if (pthread_mutex_lock(&(tmp->entry_cache_mutex)) < 0)
    exit(1);
  *retval = tmp->entry_cache[tmp->entry_cache_offset];
  tmp->entry_cache_offset++;
  if (pthread_mutex_unlock(&(tmp->entry_cache_mutex)) < 0)
    exit(1);

  return retval;
}

int get_fd_for_client(unsigned long int client_id, simclient_list *clist)
{
  simclient_entry *tmp, *tmp_e;
  fd_buf_el       *fd_tmp;

  /* Check to see if we already have one */
  tmp = &(clist->list_o_clients[client_id]);
  if (tmp->is_fd_valid)
    return tmp->fd;
  
  /* Do the LRU thing - move the end to the front */
  fd_tmp = clist->tail;
  (fd_tmp->prev)->next = NULL;
  fd_tmp->next = clist->head;
  (clist->head)->prev = fd_tmp;
  clist->head = fd_tmp;

  if (fd_tmp->entry_with_fd != NULL) {
    /* Now close what's in it */
    tmp_e = fd_tmp->entry_with_fd;
    if (tmp_e->is_fd_valid) {
      tmp_e->curr_fd_offset = (unsigned long int) lseek(tmp_e->fd,
							SEEK_CUR, 0);
      tmp_e->is_fd_valid = 0;
      close(tmp_e->fd);
      tmp_e->fd = 0;
    }
  }

  /* Set up so it now points to the right one */
  fd_tmp->entry_with_fd = tmp;
  tmp->fd = open(tmp->client_path, O_RDONLY);

  if (tmp->fd == -1) {
    fprintf(stderr, "Failed to open file %s in get_fd_for_client.\n",
	    tmp->client_path);
    perror("Reason ");
    exit(1);
  }
  if (lseek(tmp->fd, SEEK_SET, (int) tmp->curr_fd_offset) == -1) {
    fprintf(stderr, "Seek to %d failed in get_fd_for_client.\n",
	    (int) tmp->curr_fd_offset);
    exit(1);
  }
  tmp->is_fd_valid = 1;
  return tmp->fd;
}

void suck_in_buffer_data(unsigned long int client_id, simclient_list *clist)
{
  /* Here we will suck in data for the client, and purge data from the
     previous cache.  I assume that URLs had been free'd as they are consumed
     by the user of this interface. */

  int i, read_fd, res;
  simclient_entry *tmp;

  if (pthread_mutex_lock(&(clist->fd_buf_ptrs_mutex)) < 0)
    exit(1);
  tmp = &(clist->list_o_clients[client_id]);
  if (pthread_mutex_lock(&(tmp->entry_cache_mutex)) < 0)
    exit(1);

  read_fd = get_fd_for_client(client_id, clist);

  if (pthread_mutex_unlock(&(tmp->entry_cache_mutex)) < 0)
    exit(1);

  for (i=0; i<ENTRY_CACHE_SIZE; i++) {
    res = lf_get_next_entry(read_fd, &(tmp->entry_cache[i]), 3);
    if (res != 0) {
      /* Hit end of file - set up for restart */
      tmp->max_in_cache = i;
      tmp->is_fd_valid = 0;
      close(tmp->fd);
      tmp->curr_fd_offset = 0UL;
      break;
    }
    tmp->max_in_cache = i+1;
  }
  if (pthread_mutex_unlock(&(clist->fd_buf_ptrs_mutex)) < 0)
    exit(1);
  tmp->entry_cache_offset = 0;
}


