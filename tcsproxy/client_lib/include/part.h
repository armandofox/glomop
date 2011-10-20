/*
 * File:          part.h
 * Purpose:       To manage the partitions of the Harvest subsystem.
 * Author:        Steve Gribble
 * Creation Date: September 25th, 1996
*/        

#include "clib.h"
#include "icp.h"
#include "utils.h"
#include "configure.h"

#ifndef CLIB_PART_H
#define CLIB_PART_H

typedef struct _part_element {
  struct sockaddr *addr;
  int              addrlen;
} part_element;

clib_response Part_Initialize(Options opts, Monitor *defMon);
clib_response Part_Add(struct sockaddr *addr, int addrlen);
clib_response Part_Delete(struct sockaddr *addr, int addrlen);
clib_response Part_GetTCPsock(char *url, int *fd);
clib_response Part_GetUDPsock(char *url, int *fd);

clib_response Part_HandleFailureS(struct sockaddr *addr, int addrlen);
clib_response Part_HandleFailure(int fd);

#endif


