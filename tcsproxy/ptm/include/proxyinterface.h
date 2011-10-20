#ifndef __PROXYINTERFACE_H__
#define __PROXYINTERFACE_H__

#include <sys/time.h>
#include "distinterface.h"
#include "optdb.h"

#ifdef __cplusplus
extern "C" {
#endif


gm_Bool InitializeDistillerCache(Options options, const char *monitorUnitID);
/* 
 * call this function when the proxy front end starts up.
 * If the PTM IP address and port number are known, then pass them as
 * arguments, otherwise pass them as "" and 0 respectively. The Distiller 
 * Cache Manager will figure out the address through beaconing packets from
 * PTM which are sent every couple of seconds
 *
 * optionsFile indicates the pathname of the file containing the various 
 * options for the system. Have a look at tcsproxy/ptm/.gm_options.sample for
 * an example of the options file
 *
 * proxyPort is used simply to identify this frontend to the monitor
 */


DistillerStatus
Distill(C_DistillerType *type,  Argument *args, int numberOfArgs,
	DistillerInput  *input, DistillerOutput *output);
/*gm_Bool *freeOutputBuffer);*/

/*void FreeOutputBuffer(DistillerOutput *output);*/

typedef void *DistillerRequestType;

DistillerStatus
DistillAsync(C_DistillerType *type,  Argument *args, int numberOfArgs,
	DistillerInput  *input, DistillerRequestType *request);

DistillerStatus
DistillRendezvous(DistillerRequestType *request, DistillerOutput *output,
    struct timeval *tv);

void DistillDestroy(DistillerRequestType *request);

const char *
FE_getDistillerStatusString(DistillerStatus status);
const char *
FE_GetMonitorClientUnitID();


void
WaitForPTM();


#ifdef __cplusplus
}
#endif



#endif /* __PROXYINTERFACE_H__ */
