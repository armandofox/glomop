/*
 *  parse_upref.c:  Receive a "set preferences" request from client,
 *  parse and validate the preferences values, and call the userprefs
 *  interface to actually change the preferences in the database.  
 *
 */

#include "config_tr.h"
#include "utils.h"
#include "debug.h"
#include "frontend.h"
#include "userpref.h"
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern Monitor *gMon;

static gm_Bool remove_from_arglist(UINT32 id, ArgumentList *al);


/*
 *  Parse the URL from the "GET" command and use the info to change the prefs.
 *
 *  URL format is something like:
 *  .../?name=val&name=val&name=val&...&name=val/
 *
 *  ARGS:
 *  input:  http headers struct of the incoming request, including the
 *      pre-parsed URL (but not the HTTP version)
 *  input:  socket to collect more data from or write data to
 *  RETURNS:  true/false (success/failure)
 *  SIDE EFFECTS: calls http_error_return to send the status message back to
 *              the user
 *  REENTRANT: yes, but in practice only called from a non-reentrant single
 *      thread 
 */

HTTP_Status
parse_and_change_prefs(const char *url, userkey ukey, char *err_str)
{
  int changed = 0;
  char *token, *val;
  char *tmp;
  ts_strtok_state *ts_st;
  ArgumentList arglist;
  Argument *al = arglist.arg;
  int argct = 0;
  char *error_string;
  int i;
  char type;
  UINT32 arg_id;
  UINT32 tmp_u;
  double tmp_f;
#ifdef DEBUG_USER_PREF
  char str[HTTP_RESPONSE_BODY_MAX];
#endif /* DEBUG_USER_PREF */

  /* get the userkey from the socket connecting ip address*/

  if (ukey == 0) {
    snprintf(err_str, HTTP_ERRMSG_MAX, "Can't determine user");
    return HTTP_ERR_UNSPECIFIED;
  }

  get_userprefs_with_write_intent(ukey, &arglist);

#ifdef DEBUG_USER_PREF
  sprintf(str, "<p>You are connecting from %lu.%lu.%lu.%lu<p>",
	   (ukey >> 24),
	   (ukey >> 16) % 256,
	   (ukey >> 8) % 256,
	   (ukey) % 256);
  correct_write(sock, str, -1);  

  sprintf(str,"You have %i args before setting <br>", arglist.nargs);
  correct_write(sock, str, -1);  

  for (i=0; i < arglist.nargs;i++) {
    switch (al[i].type) {
    case typeInt:
      sprintf(str, "<tt>[%i] Id = %lu, type = int, value = %lu</tt><br>", i, 
	      ARG_ID(al[i]), ARG_INT(al[i]));
      correct_write(sock, str, -1);
      break;
    case typeDouble:
      sprintf(str, "<tt>[%i] Id = %lu, type = dbl, value = %f</tt><br>", i,
	      ARG_ID(al[i]), ARG_DOUBLE(al[i]));
      correct_write(sock, str, -1);      
      break;
    case typeString:
      sprintf(str, "<tt>[%i] Id = %lu, type = str, value = %s</tt><br>", i, 
	      ARG_ID(al[i]), ARG_STRING(al[i]));
      correct_write(sock, str, -1);      
    }
  }
#endif /* DEBUG_USER_PREF */
  /* look for the initial question mark. */

  if ((tmp = strchr(url, '?')) == NULL) {
    snprintf(err_str, HTTP_ERRMSG_MAX,"User preferences");
    return HTTP_ERR_MALFORMED_REQUEST;
  }
  tmp++;
  ts_st = ts_strtok_init(tmp);

  changed = 0;

  while ((token = ts_strtok("&", ts_st)) != NULL) {

    error_string = NULL;
    
    /* token should have the form "name=val" where val may be null. */
    if ((tmp = strchr((const char *)token, '=')) == NULL) {
      error_string = token;
      break;
    }
    /* name should be of the form "xnnnn" where x is a type specifier letter
     *  ('i' for int, 'f' for float, 's' for string), and nnnn is 
     *  the argument id.
     */
    type = token[0];
    if (sscanf((const char *)token+1, "%lu", &arg_id) != 1) {
      /* couldn't scan the argument id number */
      error_string = token+1;
      break;
    }
    /*
     *  Value can be anything, including the null string; but if it's an 'i' or
     *  'f' argument, the value should be parsable as an int or float
     *  respectively.
     */
    if ((val = strchr((const char *)token, '=')) == NULL) {
      error_string = token;
      break;
    }
    /* now find out if the argument is in the current set of arguments 
       and if so, just change that value.
    */
    argct = -1;
    for (i = 0; i < arglist.nargs; i++) {
      if (ARG_ID(al[i]) == arg_id) {
	argct = i;
      } 
    }
    /* if we don't find the id add it to the arg list if we can*/
    if (argct == -1) {
      /* make sure we don't go over the max number of args, if we would */
      if (arglist.nargs >= MAX_ARGS) {
	continue;
      }
      arglist.nargs++;
      argct = arglist.nargs-1;
      SET_ARG_ID(al[argct], arg_id);
    }
    val++;
    /* BUG::what if incrementing 'val' overflowed the string? */
    switch(type) {
    case 'i':
      if (*val == '\0') {
	remove_from_arglist(arg_id, &arglist);
        break;
      }
      if (sscanf(val, "%lu", &tmp_u) == 1) {
        SET_ARG_INT(al[argct], tmp_u);
      } else {
        error_string = val;
      }
      break;
    case 'f':
      if (*val == '\0') {
	remove_from_arglist(arg_id, &arglist);
	break;
      }
      if (sscanf(val, "%lf", &tmp_f) == 1) {
        SET_ARG_DOUBLE(al[argct], tmp_f);
      } else {
        error_string = val;
      } break;
    case 's':
      if (*val == '\0') {
	remove_from_arglist(arg_id, &arglist);
	break;      
      }
      SET_ARG_STRING(al[argct], val);
      break;
    default:
      error_string = token;
      break;
    }
    if (error_string)
      break;
  }

  /*
   *  Loop finishes when we run out of args in the URL, when we have filled the
   *  allowed arg list size, or when we encounter a parsing error.
   */
  ts_strtok_finish(ts_st);
  if (error_string) {
    snprintf(err_str, HTTP_ERRMSG_MAX, error_string);
    changed = 0;
    return HTTP_ERR_BAD_TOKEN;
  } 
#ifdef DEBUG_USER_PREF
  sprintf(str,"<p>Your have %i args after setting<br>", arglist.nargs);
  correct_write(sock, str, -1);  
    
  for (i=0; i < arglist.nargs;i++) {
    switch (al[i].type) {
    case typeInt:
      sprintf(str, "<tt>[%i] Id = %lu, type = int, value = %lu</tt><br>", i, 
	      ARG_ID(al[i]), ARG_INT(al[i]));
      correct_write(sock, str, -1);
      break;
    case typeDouble:
      sprintf(str, "<tt>[%i] Id = %lu, type = dbl, value = %f</tt><br>", i, 
	      ARG_ID(al[i]), ARG_DOUBLE(al[i]));
      correct_write(sock, str, -1);      
      break;
    case typeString:
      sprintf(str, "<tt>[%i] Id = %lu, type = str, value = %s</tt><br>", i, 
	      ARG_ID(al[i]), ARG_STRING(al[i]));
      correct_write(sock, str, -1);      
    }
    correct_write(sock, "</tt>", -1);      
  }
#endif /*DEBUG_USER_PREF*/

  set_userprefs(ukey, &arglist);

  return HTTP_NO_ERR;
}    

gm_Bool
remove_from_arglist(UINT32 id, ArgumentList *al) {
  int i;
  int found = 0;

  for (i=0; i<al->nargs; i++) {
    if (ARG_ID(al->arg[i]) == id) {
      found = 1;      
      i++;      
      break;
    } 
  }
  if (found) {
    for (;i<al->nargs; i++) {
      memcpy(&(al->arg[i-1]), &(al->arg[i]), sizeof(Argument));
    }
    al->nargs--;
    return gm_True;
  } else {
    return gm_False;
  }
}
