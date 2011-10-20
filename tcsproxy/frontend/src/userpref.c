/*
 *  userpref.c: support for maintaining user preferences in-memory hash
 *  backed up by gdbm.
 */

#include "config_tr.h"
#include "libmon.h"
#include <string.h>
#include <gdbm.h>
#include <stdio.h>
#include "utils.h"
#include "userpref.h"
#include "debug.h"
#include "ARGS.h"

int getFromUcache(userkey key, ArgumentList *al);
int inUcache(userkey ukey, ArgumentList *al);
void putInUcache(userkey ukey, ArgumentList * al);
void getFromDBM(userkey ukey, ArgumentList *al);
void writeToDBM(userkey ukey, ArgumentList *al);
UINT32 hash (userkey key);

UserPrefsCache userCache[USERCACHE_SIZE];
GDBM_FILE prefs;
pthread_mutex_t pref_mutex;

/*
 *  NOTE: Because of the ANSI C rules for initializing static unions, the
 *  initializer value must be type-compatible with the *first component* of the
 *  union.  Looking at distinterface.h, you'll see that this means we can only
 *  staitically-initialize typeInt arguments.  Others can be set in the
 *  init_prefs function, i guess.
 */
static Argument default_args[] = {
  { typeInt, FRONT_EXPERT, {0} },
  { typeInt, FRONT_QUALITY, {3} },
  { typeInt, FRONT_SHOW_ICON, {1} },
  { typeInt, FRONT_NO_DISTILL, {0} }
};

static ArgumentList default_prefs;

gm_Bool
init_prefs(const char *filename)
{
  int i;

  /* Set up default arguments for a new user */
  default_prefs.nargs = sizeof(default_args)/sizeof(Argument);

  for (i=0; i<default_prefs.nargs; i++) {
    default_prefs.arg[i] = default_args[i];
  }

  for (i = 0; i<USERCACHE_SIZE; i++) {
    userCache[i].key = 0;    
  }
  pthread_mutex_init(&pref_mutex, NULL);
  if ((prefs = gdbm_open((char *) filename, 0, GDBM_WRCREAT, 
			USERCACHE_MODE, NULL)) != NULL) {	
    MON_SEND_2(MON_LOGGING,"Initialized user-prefs database: %s\n", filename);
    return gm_True;
  } else {
    char extFilename[1024];
    int  idx=1;
    while (idx < 256) { /* 256 is some random upper bound */
      sprintf(extFilename, "%s.%d", filename, idx++);
      if ((prefs = gdbm_open((char *) extFilename, 0, GDBM_WRCREAT, 
			     USERCACHE_MODE, NULL)) != NULL) {
	MON_SEND_2(MON_LOGGING,"Initialized user-prefs database: %s\n", extFilename);
	return gm_True;
      }
    }
    return gm_False;
  }
}

/*
 *  Return gm_True iff the given URL is unambiguously known to be
 *  the "send me a prefs change form" URL.
 */

gm_Bool
is_changepref_url(const char *url)
{
  if (strncasecmp(fe_prefs_change_url_string, url, strlen(fe_prefs_change_url_string)) == 0) {
    return gm_True;
  } else {
    return gm_False;
  }
}

/*
 *  Return gm_True iff the given URL is unambiguously known to be
 *  the "set prefs" URL.
 */

gm_Bool
is_setpref_url(const char *url)
{
  if (strncasecmp(fe_prefs_set_url_string, url, strlen(fe_prefs_set_url_string)) == 0) {
    return gm_True;
  } else {
    return gm_False;
  }
}

void
end_prefs()
{
  gdbm_close(prefs);
  pthread_mutex_destroy(&pref_mutex);
}

/*
 * takes a userkey and and a ArgumentList pointer
 * checks the local cache to see if the data exists there
 * and if it doesn't it find it get it from the server 
 * set by setPrefServer.
 * returns the length of ArgumentList and sets ArguementList
 * to a copy of the info.
 */
int
get_userprefs(userkey ukey, ArgumentList *p) {
  ArgumentList al;

  getFromUcache(ukey, &al);
  memcpy(p, &al, sizeof (ArgumentList));
  return p->nargs;
}

/*
 * takes a userkey and and a ArgumentList pointer
 * and gets the data off of the database file.
 * this data will always be correct, but it takes
 * a lock operation and so is slower.
 * returns the length of ArgumentList and sets ArguementList
 * to a copy of the info.
 */
int
get_userprefs_with_write_intent(userkey ukey, ArgumentList *p) {
  getFromDBM(ukey, p);
  return p->nargs;
}

/*
 * takes a user key and an argumentlist.
 * sets the cache for that key to that argumentlist.
 * writes to the gdbm file the the argument list for the key
 * multithread safety is good
 */
void
set_userprefs(userkey ukey, ArgumentList *p) {
  putInUcache(ukey, p);
  writeToDBM(ukey, p);
}

/*
 * returns a 1if it can set it
 * 0 if the new argumentslist would be too long
 */
int
set_oneuserpref(userkey ukey, Argument *a) {
  int i;

  ArgumentList al;
  get_userprefs_with_write_intent(ukey, &al);
  for (i = 0; i < al.nargs; i++) {
    if (al.arg[i].id == a->id) {
      memcpy(&al.arg[i], a, sizeof(Argument));
      set_userprefs(ukey, &al); 
      return 1;
    }
  }
  if (i == al.nargs) {
    if (i < MAX_ARGS) {
      al.nargs++;
      memcpy(&al.arg[i], a, sizeof(Argument));
      set_userprefs(ukey, &al); 
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}

int
getFromUcache(userkey key, ArgumentList *al) {
  if (inUcache(key, al)) {
    return 1;
  } else {
    getFromDBM(key, al);
    putInUcache(key, al);
    return 0;
  }
}

/*
 * make this thread safe!
 * if the key ukey is in the cache set al to a copy of the 
 * information in the cache and return 1, else return 0.
 * if al is set to NULL don't try the copy.
 */
int
inUcache(userkey ukey, ArgumentList *al) {
  int hashBucket;

  hashBucket = (int) (hash(ukey));

  if (userCache[hashBucket].key == ukey) {
    if (al != NULL) { 
      memcpy(al, &userCache[hashBucket].al, sizeof(ArgumentList));
    }
    return 1;
  } else 
    return 0;
}

/*
 * make this thread safe!
 */
void 
putInUcache(userkey ukey, ArgumentList * al) {
  int hashBucket;
  
  hashBucket = (int) (hash(ukey));
  userCache[hashBucket].key = ukey;
  memcpy(&userCache[hashBucket].al, al, sizeof(ArgumentList));
}

void
getFromDBM(userkey ukey, ArgumentList *al) {
  datum dkey;
  datum data;

  dkey.dptr = (char *) (&ukey);
  dkey.dsize = sizeof(ukey);
  
  pthread_mutex_lock(&pref_mutex);
  data = gdbm_fetch(prefs, dkey);
  pthread_mutex_unlock(&pref_mutex);
  if (data.dptr) {
    memcpy(al, data.dptr, data.dsize);
    free(data.dptr);
  } else {
    memcpy(al, &(default_prefs), sizeof(default_prefs));
  }
}

void
writeToDBM(userkey ukey, ArgumentList *al) {
  datum dkey;
  datum data;

  dkey.dptr = (char *) (&ukey);
  dkey.dsize = sizeof(ukey);

  data.dsize = sizeof(ArgumentList);
  data.dptr = (char *) (al);

  pthread_mutex_lock(&pref_mutex);
  gdbm_store(prefs, dkey, data, GDBM_REPLACE);
  pthread_mutex_unlock(&pref_mutex);
}

UINT32
hash (userkey key) {

  return((UINT32) key % USERCACHE_SIZE);
}

gm_Bool
is_getpref_url(const char *url) {
    if (strncmp(fe_get_prefs_url, url, strlen(fe_get_prefs_url)) == 0) {
    return gm_True;
  } else {
    return gm_False;
  }
}

void
send_prefs(ArgumentList *prefs, int sock) {
  int disabled = 0;
  int quality = 3;
  char str[256];
  int i;

  correct_write(sock, "HTTP/1.0 200 OK\n", -1);
  correct_write(sock, "Content-Type: text/plain\n\n", -1);

  for (i = 0; i < prefs->nargs; i++) {
    if (ARG_ID(prefs->arg[i]) == FRONT_NO_DISTILL) {
      disabled = ARG_INT(prefs->arg[i]);
    } else if (ARG_ID(prefs->arg[i]) == FRONT_QUALITY) {
      quality = ARG_INT(prefs->arg[i]);
    }
  }
  
  sprintf (str, "%s\n%d\n", disabled ? "false" : "true", quality);
  correct_write(sock, str, -1);
}
