/*
 *  Debugging flags and definitions, and logging flags and definitions
 */

#ifndef _DEBUG_HEADER_H
#define _DEBUG_HEADER_H

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include "libmon.h"
#include "userpref.h"

/**** Logging definitions and structures ****/

#ifdef LOGGING

#define MAX_LOGMSG_LEN 512
struct loginfo {
  char url[MAX_LOGMSG_LEN];
  time_t date;
  INT32 size_before;
  INT32 size_after;            /* 0 = no distillation, <0 = dist failed */
  userkey ipaddr;
  INT32 http_response;
};

  /* This one is used to initialize a loginfo structure */
#define LOGGING_init_loginfo(lp)  \
         memset((void *)lp, 0, sizeof(struct loginfo)), \
         (lp)->date = time(NULL);

  /* And these are used to do the actual logging */
char *init_logging(void);
void gm_log(char *msg);
void log_close(void);

#else  /* !LOGGING */

#define init_logging() "true"
#define gm_log()
#define log_close()
#define LOGGING_init_loginfo(lp)

#endif /* LOGGING */



/**** Debugging primitives and structures ****/

#define DBG_THREAD      0x0001
#define DBG_HTTP        0x0002
#define DBG_PTM         0x0004
#define DBG_ADMIN       0x0008
#define DBG_CACHE       0x0010
#define DBG_MIME        0x0020
#define DBG_WING        0x0040
#define DBG_ALL         0xffff

#define FE_ERRMSGLEN    256

void _proxy_debug(const char *, const int, int, char *, ...);
void _proxy_errlog(const char *, const int, char *, ...);
void set_debug_mask(int);

#ifdef DEBUGGING
#define proxy_debug_1(x) _proxy_debug(__FILE__,__LINE__,(x))
#define proxy_debug_2(x,y) _proxy_debug(__FILE__,__LINE__,(x),(y))
#define proxy_debug_3(x,y,z) _proxy_debug(__FILE__,__LINE__,(x),(y),(z))
#define proxy_debug_4(x,y,z,w) _proxy_debug(__FILE__,__LINE__,(x),(y),(z),(w))
#define proxy_errlog_1(x) _proxy_errlog(__FILE__,__LINE__,(x))
#define proxy_errlog_2(x,y) _proxy_errlog(__FILE__,__LINE__,(x),(y))
#define proxy_errlog_3(x,y,z) _proxy_errlog(__FILE__,__LINE__,(x),(y),(z))
#define proxy_errlog_4(x,y,z,w) _proxy_errlog(__FILE__,__LINE__,(x),(y),(z),(w))
#else
#define proxy_debug_1(x)
#define proxy_debug_2(x,y)
#define proxy_debug_3(x,y,z)
#define proxy_debug_4(x,y,z,w)
#define proxy_errlog_1(x)
#define proxy_errlog_2(x,y)
#define proxy_errlog_3(x,y,z)
#define proxy_errlog_4(x,y,z,w)
#endif

extern Monitor *gMon;

#define MON_ERR        "Errors"
#define MON_LOGGING    "Messages"

#ifdef _TESTING_
#define MON_SEND(what,x) fprintf(stderr, (x))
#define MON_SEND_2(what,x,y) fprintf(stderr,(x),(y))
#define MON_SEND_3(what,x,y,z) fprintf(stderr,(x),(y),(z))
#define MON_SEND_4(what,x,y,z,w) fprintf(stderr,(x),(y),(z),(w))
#else  /* not _TESTING_ */
#define MON_SEND(what,x) \
    MonitorClient_Send(gMon,what,(x),MON_LOG)
#define MON_SEND_2(what,x,y) \
    { char __msg[FE_ERRMSGLEN]; \
      snprintf(__msg,FE_ERRMSGLEN-1,(x),(y)); \
      MonitorClient_Send(gMon,what,__msg,MON_LOG); \
    }
#define MON_SEND_3(what,x,y,z) \
    { char __msg[FE_ERRMSGLEN]; \
      snprintf(__msg,FE_ERRMSGLEN-1,(x),(y),(z)); \
      MonitorClient_Send(gMon,what,__msg,MON_LOG); \
    }
#define MON_SEND_4(what,x,y,z,w) \
    { char __msg[FE_ERRMSGLEN]; \
      snprintf(__msg,FE_ERRMSGLEN-1,(x),(y),(z),(w)); \
      MonitorClient_Send(gMon,what,__msg,MON_LOG); \
    }
#endif /* not _TESTING_ */

#define MON_STATS(f,v) \
    { char __msg[FE_ERRMSGLEN]; \
      snprintf(__msg,FE_ERRMSGLEN-1, "%d", (int)(v)); \
      MonitorClient_Send(gMon, (f), __msg, MON_SIMPLE); \
    }    
#define MON_LOAD(msg,f,v) \
    { sprintf((msg),"diff=%d,red",(int)(v)); \
      MonitorClient_Send(gMon, (f), (msg), MON_TIMECHART); \
    }                                                           
#define MON_INDICATORS(msg,f,n,v) \
    { int i,ndx=0; \
      for (i=0; i<(n); i++) { \
        ndx+=sprintf((msg)+ndx,"%d,",*(((int*)v)+i)); \
      } \
      msg[strlen(msg)-1] = 0; \
      MonitorClient_Send(gMon, (f), (msg), MON_ARRAY); \
    }

#endif /* _DEBUG_HEADER_H */
