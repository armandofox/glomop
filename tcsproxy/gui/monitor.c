/*
 *  C Library for talking to GUI monitor
 */

#include "libmon.h"

typedef void (*sigproc)(int);

#ifndef MALLOC
#  include <stdlib.h>
#  define MALLOC malloc
#  ifndef FREE
#    define FREE free
#  endif
#endif

#ifndef MSG
#  define MSG(x)
#endif

static Monitor *gmon_gasp = NULL;

Monitor *
MonitorClient_Create(const char *unitname, const char *ipaddr, int port,
                     int ttl)
{
  Monitor *m;
  struct utsname un;
  
  if ((m = (Monitor *)MALLOC(sizeof(Monitor))) == NULL) {
    MSG("client_create: malloc failed");
    return NULL;
  }
  if (strlen(unitname) > MON_MAXUNITNAME-1) {
    MSG("unit name too long");
    FREE(m);
    return NULL;
  }
  strcpy(m->unitname, unitname);

  (void)uname(&un);
  strncpy(m->myhostname, un.nodename, SYS_NMLN-1);
  if ((m->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    MSG("socket failed");
    FREE(m);
    return NULL;
  }

  m->pid = (int)getpid();
  
  memset((char *)&(m->sin), 0, sizeof(m->sin));
  m->sin.sin_family = AF_INET;
  m->sin.sin_addr.s_addr = inet_addr(ipaddr);
  if (port > 0 && port < 65535)
    m->sin.sin_port = htons((u_short)port);
  else
    m->sin.sin_port = (u_short)0;

  if (connect(m->sock, (struct sockaddr *)&(m->sin), sizeof(m->sin)) < 0) {
    MSG("connect failed");
    FREE(m);
    return NULL;
  }

  /*
   *  If multicast socket, set that option.
   */

  if (IN_CLASSD(ntohl(m->sin.sin_addr.s_addr))) {
    u_char t = (ttl > 255? 255 : ttl < 0? 0: ttl);
    if (setsockopt(m->sock, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&t,
                   sizeof(t)) < 0) {
      MSG("setsockopt TTL failed");
      FREE(m);
      return NULL;
    }
  }

  return (gmon_gasp = m);
}

int
MonitorClient_UnitSend(Monitor *m, const char *unit,
                       const char *field, const char *value,
                       const char *script)
{
  int len;
  int result;

  char *buf;

  if (m == NULL)
    return 0;

  if (m == MON_USE_STDERR) {
    fprintf(stderr, "%15s:%-20s  %s   | {%s}\n", unit, field, value, script);
    return 0;
  }
  buf = (char *)MALLOC(strlen(m->myhostname)
                       + 5        /* for pid+separator */
                       + strlen(unit)
                       + strlen(field)
                       + strlen(value)
                       + strlen(script)
                       + (5 * strlen(MON_SEPARATOR))
                       + 1);    /* for eos */

  if (buf == NULL) {
    result = -2;
    goto SEND_RETURN;
  }

  len = sprintf(buf, "%s%s%d%s%s%s%s%s%s%s%s",
                m->myhostname, MON_SEPARATOR,
                m->pid, MON_SEPARATOR,
                unit, MON_SEPARATOR,
                field, MON_SEPARATOR,
                value, MON_SEPARATOR,
                script);
  result = (sendto(m->sock, (const char *)buf, len, 0,
                   (const struct sockaddr *)&(m->sin), sizeof(m->sin)));
SEND_RETURN:
  FREE(buf);
  return(result);
}

void
MonitorClient_SetAsDefault(Monitor *m)
{
  gmon_gasp = m;
}

void
MonitorClient_Gasp(int sig)
{
  if (gmon_gasp == NULL) {
    /* nothing */
  } else if (gmon_gasp == MON_USE_STDERR) {
    fprintf(stderr, "MONITOR: Process %d gasps its last breath\n",
            (int)getpid());
  } else {
    char sigstring[10];
    sprintf(sigstring, "%d", sig);
    (void)MonitorClient_Send(gmon_gasp, "%DEATH%", sigstring, "");
  }
}


    
  
    
  
