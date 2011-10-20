/*
 *  url_magic.c: magic urls that have arguments as part of the url
 *  a magic url looks like http://machine/rest*^string^string^string^
 *  where each string is of the form CId=Val where C is one of s i or f
 *  (string argument, int argument, double argument) Id is the id of
 *  the argument and Val is the value.
 *  In order to handle forms and imagemaps the magic can occur before
 *  a group of ? arguments.  So you could have 
 *  http://machine/where*^mstring1^mstring2^?formarg1?formarg2 for instance.
 */

#include "config_tr.h"
#include "userpref.h"
#include "ARGS.h"
#include "utils.h"
#include "debug.h"
#include "frontend.h"
#include "distinterface.h"
#include "url_magic.h"
#include <string.h>

/*
 * returns true if the url is one with magic
 * arguments and false otherwise
 */
gm_Bool is_magic(char * url) {
  char * magic_start = NULL;
  char * form_start = NULL;
  int i;

  /* 
   * find the start of the forms part of the url, if non existant set to
   * the end of the url 
   */
  form_start = strchr(url, '?');
  if (form_start == NULL) {
    form_start = url + strlen(url);
  }
  

  /* find the first MAGIC_START char that lies before a ?*/
  for (i = 0; i < form_start - url; i++) {
    if (url[i] == MAGIC_START) {
      magic_start = url + i;
    }
  }

  if (magic_start) {
    magic_start++;
  } else {
    return gm_False;
  }
  
  /* now does this actually look like a magic url? */
  if ((*magic_start == MAGIC_DELIM) && 
      (url[form_start-url-1] == MAGIC_DELIM) && 
      (magic_start != form_start-1)) {
    return gm_True;
  } else {
    return gm_False;
  }
}

/*
 * Takes a normal URL url and and argumentlist.  It puts the new
 * magicified url in murl with all of the arguments of the argumentlist
 * being used to make the magic url
 */

gm_Bool to_magic(char * murl, char * url, ArgumentList *al) {
  int i;
  char * murl_pos;
  char * form_start;
  int error = 0; 
  if (!al->nargs) {
    strcpy(murl, url);
    return gm_False;
  }

  form_start = strchr(url, '?');
  if (form_start == NULL) {
    form_start = url + strlen(url);
  }
  
  strncpy(murl, url, form_start - url);
  
  murl_pos = murl + (form_start - url);

  if (!is_magic(url)) {
    *murl_pos = MAGIC_START;
    murl_pos++;
    *murl_pos = MAGIC_DELIM;
    murl_pos++;
  }
    
  for (i = 0; i < al->nargs; i++) {
    switch (al->arg[i].type) {
    case typeString:
      murl_pos += sprintf(murl_pos, "s%lu=%s^", 
			  ARG_ID(al->arg[i]), ARG_STRING(al->arg[i]));
      break;
    case typeInt:
      murl_pos += sprintf(murl_pos, "i%lu=%lu^", 
			  ARG_ID(al->arg[i]), ARG_INT(al->arg[i]));
      break;
    case typeDouble:
      murl_pos += sprintf(murl_pos, "f%lu=%f^", 
			  ARG_ID(al->arg[i]), ARG_DOUBLE(al->arg[i]));
      break;
    default:
      error = 1;
    }
  }
  
  if (form_start != url+strlen(url)) {
    strcpy(murl_pos, form_start);
  }

  if (error) return gm_False;
  else return gm_True;
}

/*
 * takes a magic url (murl) and adds its arguemnts to the arguemntlist given
 * (including overwriting ones that are already in the argumentlist)
 * in addition puts the non magic url in the buffer being pointed to
 * by url.  Ugly scanner, should be fixed.
 */
gm_Bool from_magic(char * murl, char * url, ArgumentList *al) {
  int i;
  char * murl_pos;
  char * url_pos;
  char * murl_magicend; 
  char * form_start;
  int error = 0;
  UINT32 id;
  UINT32 intval;
  double dblval;
  char strval[MAX_ARG_STRING];
  
  if (!is_magic (murl)) {
    strcpy(url, murl);
    return gm_False;
  }  

  /* 
   * find the start of the forms part of the url, if non existant set to
   * the end of the url 
   */
  form_start = strchr(murl, '?');
  if (form_start == NULL) {
    form_start = murl + strlen(murl);
  }


  murl_magicend = form_start;
  
  murl_pos = strrchr(murl, MAGIC_START);
  strncpy(url, murl, murl_pos-murl);
  url_pos = murl_pos-murl + url;
  murl_pos+= 2;

  do {
    switch (*murl_pos) {
    case 's':
      sscanf(murl_pos+1, "%lu=%[^^]", &id, strval);
      break;
    case 'i':
      sscanf(murl_pos+1, "%lu=%lu", &id, &intval);
      break;
    case 'f':
      sscanf(murl_pos+1, "%lu=%lf", &id, &dblval);
      break;
    default:
      error = 1;
      continue;
    }
    for (i = 0; i < al->nargs; i++) {
      if (ARG_ID(al->arg[i]) == id) {
	break;
      }
    } 
    if (i != MAX_ARGS) {
      if (i == al->nargs) al->nargs++;
      switch (*murl_pos) {
      case 's':
	SET_ARG_STRING(al->arg[i], strval);
	SET_ARG_ID(al->arg[i], id);
	break;	
      case 'i':
	SET_ARG_INT(al->arg[i], intval);
	SET_ARG_ID(al->arg[i], id);
	break;
      case 'f':
	SET_ARG_DOUBLE(al->arg[i], dblval);
	SET_ARG_ID(al->arg[i], id);
	break;
      }
    } else {
      error = 1;
    }
  } while ((murl_pos = strchr(murl_pos, MAGIC_DELIM)+1) 
	   && (murl_pos < murl_magicend));
  strcpy(url_pos, murl_pos);
  if (error) return gm_False;
  else return gm_True;
}
  


