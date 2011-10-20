#ifndef _USERPREF_H
#define _USERPREF_H

#include "proxyinterface.h"

typedef unsigned long userkey;
#define USERKEY_NULL ((userkey)NULL)
#define USERCACHE_SIZE 401
#define USERCACHE_MODE 0644

/*
 *  BUG::most of what's below does NOT need to be exported
 */
typedef struct userprefscache {
  userkey key;
  ArgumentList al;
} UserPrefsCache;

gm_Bool   init_prefs(const char *dbfile);
void  end_prefs();

int   get_userprefs(userkey ukey, ArgumentList *p);
int   get_userprefs_with_write_intent(userkey ukey, ArgumentList *p);
void  set_userprefs(userkey ukey, ArgumentList *p);
int   set_oneuserpref(userkey ukey, Argument *a);

gm_Bool  parse_and_change_prefs(const char *url, userkey ukey, char *errstr);
gm_Bool   parse_prefs_request(int sock);
gm_Bool is_changepref_url(const char *url);
gm_Bool is_getpref_url(const char *url);
gm_Bool is_setpref_url(const char *url);

void send_prefs(ArgumentList *prefs, int sock);
void send_change_prefs_page(userkey k, ArgumentList *current_prefs, int sock);

#endif /* ifndef _USERPREF_H */
