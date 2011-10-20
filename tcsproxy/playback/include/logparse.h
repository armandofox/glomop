/*
 *     Author: Steve Gribble
 *       Date: Nov. 23rd, 1996
 *       File: logparse.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config_tr.h"

#ifndef PB_LOGPARSE_H
#define PB_LOGPARSE_H

/* 
   EVERYTHING below is in NETWORK (yes, NETWORK) order.  If something
   is undefined in the traces, it is represented by FF...FF.  For
   example, an undefined client pragma is 0xFF, and an undefinfed
   server last response time is 0xFFFFFFFF.

   Version 1 trace records do not have pragmas, modified dates, or expiry
   dates.  Version 2 records have the PG_CLNT_NO_CACHE, PG_CLNT_KEEP_ALIVE
   pragmas, and no dates.  Version 3 records have all pragmas and dates.
*/

#define PB_CLNT_NO_CACHE       1
#define PB_CLNT_KEEP_ALIVE     2
#define PB_CLNT_CACHE_CNTRL    4
#define PB_CLNT_IF_MOD_SINCE   8
#define PB_CLNT_UNLESS         16

#define PB_SRVR_NO_CACHE       1
#define PB_SRVR_CACHE_CNTRL    2
#define PB_SRVR_EXPIRES        4
#define PB_SRVR_LAST_MODIFIED  8

typedef struct lf_entry_st {
  unsigned char  version; /* Trace record version */
  UINT32 crs;             /* client request seconds */
  UINT32 cru;             /* client request microseconds */
  UINT32 srs;             /* server first response byte seconds */
  UINT32 sru;             /* server first response byte microseconds */
  UINT32 sls;             /* server last response byte seconds */
  UINT32 slu;             /* server last response byte microseconds */
  UINT32 cip;             /* client IP address */
  UINT16 cpt;             /* client port */
  UINT32 sip;             /* server IP address */
  UINT16 spt;             /* server port */
  unsigned char  cprg;    /* client headers/pragmas */
  unsigned char  sprg;    /* server headers/pragmas */
  /* If a date is 0xFFFFFFFF (decimal 4294967295), it was missing or
     unreadable/unapplicable in trace */
  UINT32 cims;            /* client IF-MODIFIED-SINCE date, if applicable */
  UINT32 sexp;            /* server EXPIRES date, if applicable */
  UINT32 slmd;            /* server LAST-MODIFIED, if applicable */
  UINT32 rhl;             /* response HTTP header length */
  UINT32 rdl;             /* response data length, not including header */
  UINT16 urllen;          /* url length, not including NULL term */
  unsigned char *url;     /* request url, e.g. "GET / HTTP/1.0", + '\0' */
} lf_entry;

/** 
 ** lf_get_next_entry will suck the next record out of the logfile, and
 ** return a lf_entry record witih the information stuffed into it.  Note
 ** that memory WILL be allocated for the url field;  the caller is
 ** responsible for freeing the memory when done.  The logfile should have
 ** everything stored in network order, if all is well.
 **
 ** This function returns 0 on success, 1 for EOF, and something else
 ** otherwise.  On failure,
 ** no memory will have been allocated.
 **/

int  lf_get_next_entry(int logfile_fd, lf_entry *nextentry, int vers);

/** 
 ** This function will convert all of the entries in the record into/from
 ** host order.  This function is its own inverse.  This function cannot
 ** fail.
 **/

void lf_convert_order(lf_entry *convertme);

/**
 ** This function will write the entry pointed to by writeme back
 ** out to the file outf, in the canonical logfile binary format.
 ** It returns 0 on success, something else on failure.
 **/

int  lf_write(FILE *outf, lf_entry *writeme);

/** 
 ** This function will write the entry pointed to by writeme back out to
 ** the file descriptor fd, in the canonical logfile binary format.  It
 ** returns 0 on success, something else on failure.
 **/

int  lf_write_fd(int fd, lf_entry *writeme);

/** 
 ** This function will dump a human-readable output version of the record
 ** to the passed-in file pointer.  Assume that the record is in NETWORK
 ** order. Nothing can possibly go wrong. :)
 **/

void lf_dump(FILE *dumpf, lf_entry *dumpme);

/** 
 ** lf_ntoa takes a network IP address (in host order) and converts it
 ** to an ascii representation. addrbuf had better be 16 bytes or
 ** more...
 **/

void lf_ntoa(unsigned long addr, char *addrbuf);

#endif
