/*
 * File:          configure.c
 * Purpose:       Configuration file loading/parsing
 * Author:        Steve Gribble
 * Creation Date: October 23rd, 1996
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "config_tr.h"
#include "optdb.h"

#include "clib.h"
#include "icp.h"
#include "utils.h"
#include "configure.h"

extern int errno;
ll config_list = NULL;  /* list of name-value pairs in config file */

char *name_tokens[] = { "cache.Partition", "cache.UDP_retries", 
			"cache.UDP_timeout_sec", "cache.UDP_timeout_usec",
                        "Monitor.listen",
			NULL };

/** 
**  config_parse:  this function accepts an Options structure as an
**  argument.
** 
**  The global linked list "config_list" is appended with cfg_nv structures,
**  representing the name/value pairs.  This implies that:
**   THIS FUNCTION IS NOT THREAD-SAFE.  IT SHOULD BE CALLED ONCE AND
**   ONLY ONCE AT THE BEGINNING OF THE EXECUTION OF SOME PROCESS USING
**   THIS LIBRARY, OR THE EXTERNAL CALLER IS RESPONSIBLE FOR THREAD
**   SAFIFYING THESE FUNCTIONS WITH AN EXTERNAL MUTEX.
**
**  The function returns CLIB_OK on success, and some other error value
**  in case of trouble.
**/

clib_response config_parse(Options opts)
{
  cfg_nv  *cur = NULL;
  int      i=0;

  if (config_list != NULL) {
    ll_destroy(&config_list, free);
    config_list = NULL;
  }

  while (name_tokens[i] != NULL) {
    if (cur == NULL) {
      cur = (cfg_nv *) calloc(1, sizeof(cfg_nv));
      if (cur == NULL) {
	fprintf(stderr, "out of memory in config_parse (1)\n");
	exit(1);
      }
    }

    cur->name = name_tokens[i];
    cur->value = (char *) Options_Find(opts, cur->name);

    if (cur->value != NULL) {
      if (ll_add_to_end(&config_list, (void *) cur) != 1) {
	free(cur);
	return CLIB_CONFIG_ERROR;
      }
      cur = NULL;
    }
    i++;
  }
  return CLIB_OK;
}

/** 
**  get_next_matching: given a string representing a name token, and a
**  pointer to an element "last_value" in the linked list, this
**  function will find and return a pointer to the first cfg_nv
**  structure in the list after the "last_value" element that has a name
**  that matches (case-insensitively) "name".  It returns a pointer to the
**  cfg_nv structure and a pointer to the linked list element containing
**  that structure on success, and NULL on failure.
**
**  This procedure IS THREAD UNSAFE if the linked list will be mucked
**  with, safe otherwise since it's only going to read from the list.
**/  

cfg_nv *get_next_matching(char *name, ll *last_value)
{
  ll      cur_val = *last_value;
  cfg_nv *data;

  while (cur_val != NULL) {
    data = (cfg_nv *) cur_val->data;
    if (strcasecmp(data->name, name) == 0) {
      *last_value = cur_val;
      return data;
    }
    cur_val = cur_val->next;
  }
  *last_value = NULL;
  return NULL;
}

int get_next_token(FILE *file, char **token)
{
  int nextc;
  int toklen = 0;
  char nextline[1024], *tmp;

  *token = (char *) malloc(sizeof(char));
  if (*token == NULL) {
    fprintf(stderr, "Out of memory in get_next_token.\n");
    exit(1);
  }
  *token = '\0';

  while(1) {
    /* Get next character, add it to the token */
    nextc = fgetc(file);
    if (nextc == EOF) {
      if (toklen == 0)
	return 0;
      return toklen;
    }

    switch(nextc) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      if (toklen == 0)
	continue;
      return toklen;
      break;
    case '#':
      /* read until end of line */
      while (1) {
	if (fgets(nextline, 1024, file) == NULL) {
	  return toklen;
	}
	if (nextline[strlen(nextline)-1] == '\n')
	  break;
      }
      break;
    default:
      /* we have a real character - append it to the token */
      tmp = realloc(*token, toklen+2);
      if (tmp == NULL) {
	fprintf(stderr, "Out of memory in get_next_token.\n");
	exit(1);
      }
      *token = tmp;
      *(tmp + toklen) = nextc;
      *(tmp + toklen + 1) = '\0';
      toklen++;
      break;
    }
  }
  return 0;
}

int is_name_tok(char *tok)
{
  int i=0;

  while(name_tokens[i] != NULL) {
    if (strcasecmp(name_tokens[i], tok) == 0)
      return 1;
    i++;
  }

  return 0;
}

