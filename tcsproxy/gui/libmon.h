#ifndef _MON_H
#define _MON_H

#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define MON_MAXUNITNAME  100    /* max length of unitname */
#define MON_SEPARATOR "\001"    /* component separator in messages */

#ifndef MON_USE_STDERR
# define MON_USE_STDERR ((Monitor *)0xffffffff)
#endif

typedef struct monitor {
  struct sockaddr_in sin;
  char myhostname[SYS_NMLN]; /* in sys/utsname.h */
  int sock;
  char unitname[MON_MAXUNITNAME];
  int pid;
} Monitor;

/*
 *  Create instance of a monitor client for this unit
 */
Monitor  *MonitorClient_Create(const char *unitname, const char *ipaddr,
                             int port, int ttl);
/*
 *  Set this monitor client as the one that will be called for "dying gasp"
 */
void     MonitorClient_SetAsDefault(Monitor *m);
/*
 *  Call this function to tell the GUI Monitor you're about to die (call
 *  this before destroying the monitor client).  The only reason it
 *  takes an int argument is because it may also be called from inside
 *  this library as a signal handler.  You should call your cleanup
 *  function from inside this function since this function may be used
 *  to indicate (asynchronously) your impending doom.  DO NOT call
 *  MonitorClient_Destroy from inside here, though.
 */
void     MonitorClient_Gasp(int sig);
/*
 *  Destroy client
 */
int      MonitorClient_Destroy(Monitor *m);
/*
 *  Send a message overriding the unit name temporarily
 */
int     MonitorClient_UnitSend(Monitor *m, const char *unit, const char *field,
                               const char *value, const char *script);
/*
 *  Send a message giving field name, new value, optional Tcl script(s)
 */
#define  MonitorClient_Send(m,f,v,s)  \
      MonitorClient_UnitSend((m),((m)? (m)->unitname : "??"),(f),(v),(s))
     
/*
 *  Useful macro to cause Gasp function to be called when external
 *  signals received (eg SIGINT, SIGTERM).  Gasp function will also be
 *  called (asynchronously) when a "kill yourself" message is received
 *  from monitor.
 */
#define MonitorClient_GaspOn(sig) signal((sig), MonitorClient_Gasp)
/*
 *  Useful macros to get and set the unit name for an existing monitor client
 */
#define MonitorClient_GetUnitName(m) ((m)->unitname)
#define MonitorClient_SetUnitName(m,n) \
        (strncpy((m)->unitname, (n), MON_MAXUNITNAME))

/*
 *  Monitor behaviors
 */

#define MON_SIMPLE    ""
#define MON_LOG       "Log"
#define MON_TIMECHART "TimeChart"     
#define MON_ARRAY     "Array"

#endif /* ifndef _MON_H */
