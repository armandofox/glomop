/*
 * Author: Steve Gribble
 *   Date: Feb 24th, 1997
 *   File: playback_sched.h
 */

#include "simclient.h"
#include <pthread.h>

#ifndef PLAYBACK_SCHED_H
#define PLAYBACK_SCHED_H

void set_dest_addr_and_port(char *host, char *port);
void set_max_delay(struct timeval maxdelay);
void set_speedup_factor(double sf);

/* You must call initialize_scheduler once for each client_id that you have
   obtained by adding files.  This will allow the scheduler to deduce the
   minimum starting time across all of the client files. */

void initialize_scheduler(unsigned long int client_id, simclient_list *clist);

/* Call this to actually begin scheduling events for a client. */

void inject_client(unsigned long int client_id, simclient_list *clist);
#endif
