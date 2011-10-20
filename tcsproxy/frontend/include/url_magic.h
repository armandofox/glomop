#ifndef _URL_MAGIC_H
#define _URL_MAGIC_H

#include "distinterface.h"
#include "userpref.h"

#define MAX_URL_SIZE 1024

#define MAGIC_DELIM '^'
#define MAGIC_START '*'


gm_Bool is_magic(char * url);
gm_Bool to_magic(char * murl, char * url, ArgumentList *al);
gm_Bool from_magic(char * murl, char * url, ArgumentList *al);

#endif /* _URL_MAGIC_H */
