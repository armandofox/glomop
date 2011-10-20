#ifndef __OPTDB_H__
#define __OPTDB_H__


#include "error.h"


typedef void *Options;


#ifdef __cplusplus
extern "C" {
#endif

  /*
   * Options_New: Create a new options database
   *              e.g. Options options;
   *                   Options_New(&options, "gm_options.yatin");
   *
   * Options_Add: Add a char string option to the database
   * Options_Add_UINT32: Add an integer value to the database
   *
   * The above functions return "success" on success, and an integer error 
   * code on failure. Check ptm/include/error.h (enum Errors) for 
   * definitions of the various kinds of errors.
   * 
   * Options_Find: Returns the value corresponding to the input key.
   *               If no value exists, returns NULL
   * Options_Find_UINT32: Returns the integer value corresponding to the 
   *                      input key. If no value exists, returns 0
   */


int     Options_New(Options *options, char *filename);
void    Options_Delete(Options options);
int     Options_Add(Options options, const char *key, const char *value);
int     Options_Add_UINT32(Options options, const char *key, UINT32 value);
const   char *Options_Find(Options options, const char *key);
UINT32  Options_Find_UINT32(Options options, const char *key);
void    Options_Remove(Options options, const char *key);
Options Options_DefaultDb();

#ifdef __cplusplus
}
#endif




#endif /* __OPTDB_H__ */
