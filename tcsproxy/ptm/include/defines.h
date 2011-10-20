#ifndef __DEFINES_H__
#define __DEFINES_H__

#include "config_tr.h"
#include "gmdebug.h"
#include <iostream.h>


#define Opt_PTMExecutable           "ptm.executable"
#define Opt_PTMMulticast            "ptm.multicast"
#define Opt_PTMMulticast_IP         "ptm.multicast.ip"
#define Opt_PTMMulticast_port       "ptm.multicast.port"
#define Opt_PTMMulticast_TTL        "ptm.multicast.ttl"

#define Opt_PTMUnicast_IP           "ptm.unicast.ip"
#define Opt_PTMUnicast_port         "ptm.unicast.port"

#define Opt_MonitorMulticast        "Monitor.listen"
#define Opt_MonitorMulticast_IP     "Monitor.listen.ip"
#define Opt_MonitorMulticast_port   "Monitor.listen.port"
#define Opt_MonitorMulticast_TTL    "Monitor.listen.ttl"
#define Opt_DistillerDB             "distiller.db"
#define Opt_Rsh                     "rexec.rsh"
#define Opt_RshArgs                 "rexec.args"
#define Opt_Hosts                   "rexec.hosts"
#define Opt_OptionsFile             "global.optfile"

#define Opt_PTMBeacon_ms            "ptm.beacon_ms"
#define Opt_PTMOptionsReRead_ms     "ptm.optionsrefresh"
#define Opt_LostBeacons             "ptm.lost_beacons"

#define DefaultPTMExecutable        "ptm"
#define DefaultMulticastAddress     "224.9.9.9"
#define DefaultMulticastPort        9999
#define DefaultTTL                  1
#define DefaultMonitorAddress       "228.8.8.8"
#define DefaultMonitorPort          8888
#define DefaultDistillerDB          "distillers.db"
#define DefaultRsh                  ""
#define DefaultRshArgs              ""
#define DefaultAutoLaunchLimit      5

#define NoLoadTimeout_sec             (5*60*60)
#define LaunchTimeout_ms              8000
#define MaxLaunchTimeouts             3

#define DefaultLostBeacons            5
#define DefaultBeaconingPeriod_ms     1000
#define DefaultNotificationTimeout_ms 1000

#define DefaultLoadHistoryPeriod_ms   10000
#define DefaultLoadHistorySize \
  (DefaultLoadHistoryPeriod_ms/DefaultNotificationTimeout_ms)

#define AutoLaunchHysteresisInterval_ms 7500
#define DistillerRequestTimeout_ms  180000  // 3 minutes
#define AskForDistillerTimeout_ms   25000
#define MaxNCSize                   32

//#define NoPTMWait_ms                4000
#define NoPTMRandom_ms              2000
#define ReportStats_ms              8000
#define DefaultDistillerMainTimeout_ms 60000

#define NoRequestReply 0


#define SECONDS(ms)  ((ms) / 1000)
#define USECONDS(ms) (((ms) % 1000) * 1000)
#define timeval_to_ms(tv) (((tv).tv_sec*1000) + ((tv).tv_usec/1000))
/*#define DEBUG(x) cout << x
//#define DEBUG(x) */

#define MAXIP   20
#define MAXPATH 1024
#define MAXARGS 1024
#define MAXLINE 256

#define NEW(objectPtr, constructor) \
{ \
  if ((objectPtr = new constructor)==NULL) VoidReturn(errOutOfMemory); \
  if (Error::getStatus()!=success) return; \
}

#define DELETE(objectPtr) \
{ \
    if (objectPtr!=0) { delete objectPtr; objectPtr = 0; } \
}


enum PTMPacketTypes {
  pktRegisterDistiller=100,
  pktDeregisterDistiller,
  pktReregisterDistiller,
  pktAskForDistiller,
  pktAskForDistillerReply,

  pktPTMBeacon,

  pktDistillerInput,
  pktDistillerOutput,
  pktDistillerLoad,

  pktWillStartPTM,
  pktAskForUnicastBeacon,
  pktDistillerSleep,
  pktFlushNCache
};


enum AskForDistillerStatus {
  ptmOk,
  ptmNoDistiller,
  ptmDistillerLaunchTimeout,
  ptmAskForDistillerTimeout,
  ptmNoPTM
};

enum DistillerSleepStatus {
  distSleep, distKill, distWakeUp
};



#include "error.h"

#endif
