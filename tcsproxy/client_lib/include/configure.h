/*
 * File:          configure.h
 * Purpose:       To load in the partition configuration file
 * Author:        Steve Gribble
 * Creation Date: October 23rd, 1996
*/        

#include "config_tr.h"
#include "clib.h"
#include "icp.h"
#include "utils.h"
#include "optdb.h"

#ifndef CLIB_CONFIG_H
#define CLIB_CONFIG_H

typedef struct _config_nv_pair {
  char *name;
  char *value;
} cfg_nv;

clib_response config_parse(Options opts);
cfg_nv *get_next_matching(char *name, ll *last_value);

#endif


