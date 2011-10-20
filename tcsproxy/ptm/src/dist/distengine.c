#include "config_tr.h"
#include "distinterface.h"
#include "proxyinterface.h"
#include "monitor.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define Message(x) MonitorClientSend(mon,"Distiller Errors",(x),"Log")

MonitorClientID mon;

DistillerStatus
DistillerInit(C_DistillerType distType, int argc, const char * const * argv)
{
  mon = DistillerGetMonitorClientID();
  
  Message("distengine started\n");

  InitializeDistillerCache(Options_DefaultDb(),
			   MonitorClientUnitID(DistillerGetMonitorClientID()));
  return distOk;
}


void DistillerCleanup()
{
}


DistillerStatus
ComputeDistillationCost(Argument *args, int numberOfArgs,
			DistillerInput *input, DistillationCost *cost)
{
  cost->estimatedTime_ms = 0;
  return distOk;
}


DistillerStatus
DistillerMain(Argument *args, int  numberOfArgs,
	      DistillerInput *input, DistillerOutput *output)
{
  char msg[200];
  int t = 0;
  DistillerStatus status;
  C_DistillerType type;
  struct {
    DistillerRequestType dr;
    DistillerOutput output;
  } as[5];
  int i, left;
  sprintf(type.string, "foo/%s", MimeType(input));
  left = 0;
  for(i=0;i<5;++i) {
      status = DistillAsync(&type, args, numberOfArgs, input, &as[i].dr);
      if (status != distOk) {
	DistillDestroy(&as[i].dr);
	fprintf(stderr, "number %d destroyed\n", i);
	return status;
      } else {
	left++;
      }
  }
  i = 0;
  do {
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 500000;
      ++t;
      ++i; if (i == 5) i = 0;
      if (!as[i].dr) continue;
      status = DistillRendezvous(&as[i].dr, i ? &as[i].output : output, &tv);
      if (status != distRendezvousTimeout) --left;
      fprintf(stderr, "try %d left = %d status = %d\n", t, left, status);
      sprintf(msg, "%p %p %p %p %p\n",
	as[0].dr, as[1].dr, as[2].dr, as[3].dr, as[4].dr);
      Message(msg);
  } while (left);
  return status;
}
