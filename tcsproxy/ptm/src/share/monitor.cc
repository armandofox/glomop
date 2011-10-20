#include <string.h>
#include <iostream.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "defines.h"
#include "log.h"
#include "monitor.h"


Log Log::log_;

/*static int CurrentMonitorSock = 0;
static char *CurrentUnitId = NULL;*/

MonitorClient *MonitorClient::defaultClient = NULL;
char MonitorClient::Separator = '\001';

extern "C" void MonitorClientSigHandler(int sig);

MonitorClient::MonitorClient(const char *unitID_, const char *ipAddr, 
			     Port port, int ttl)
    : socket(NULL)
{
  sprintf(address, "%s/%d/%d", ipAddr, port, ttl);
  
  if (IN_CLASSD(ntohl(inet_addr(ipAddr)))==0) {
    // this is a UDP address
    UdpSocket *udp = new UdpSocket;
    if (udp->Connect(ipAddr, port, gm_False)==gm_False) {
      delete udp;
      return;
    }

    socket = udp;
  } 
  else {
    // this is a multicast address
    MulticastSocket *mcast = new MulticastSocket;
    if (mcast->CreateSender(ipAddr, port, ttl)==gm_False) {
      delete mcast;
      return;
    }

    socket = mcast;
  }

  if (strlen(unitID_) < MAXLINE) strcpy(unitID, unitID_);
  else { 
    strncpy(unitID, unitID_, MAXLINE-1); 
    unitID[MAXLINE]='\0'; 
  }

  CommunicationObject::GetHostName(hostName);
  pid = getpid();
}


gm_Bool
MonitorClient::Send(const char *unitID_, const char *fieldID, 
		    const char *value,   const char *script)
{
  if (socket==NULL) {
    Return(gm_False, errGenericError);
  }

  char *buffer;
  gm_Bool returnValue;
  int  len = strlen(hostName) + 1 + 25 + strlen(unitID_) + 1 + strlen(fieldID) 
    + 1 + strlen(value) + 1 + strlen(script) + 1;

  buffer = new char [len];
  if (buffer==NULL) Return(gm_False, errOutOfMemory);
  sprintf(buffer, "%s%c%ld%c%s%c%s%c%s%c%s", hostName, Separator, (long) pid, 
	  Separator, unitID_, Separator, fieldID, Separator, value, 
	  Separator, script);
  returnValue = socket->Write(buffer, strlen(buffer)+1);
  delete [] buffer;
  return returnValue;
}


void
MonitorClient::GaspOnSignals(int nsigs, int sigs[])
{
  int i;

  defaultClient = this;
  for (i=0; i<nsigs; i++)
    (void)signal(sigs[i], MonitorClient::SigHandler);
}


void
MonitorClient::GaspOnStdSignals()
{
  int sigs[] = {
    SIGTERM,
    SIGKILL,
    SIGILL,
    SIGABRT
  };

  GaspOnSignals(sizeof(sigs)/sizeof(int), sigs);
}



void
MonitorClient::Gasp(int sig)
{
  static char sigString[50];
  sprintf(sigString, "%d", sig);
  Send("%DEATH%", sigString, "");
}


void
MonitorClient::SigHandler(int sig)
{
  if (defaultClient) defaultClient->Gasp(sig);
  exit(-1);
}
    

MonitorClientID
MonitorClientCreate(const char *unitID, const char *multicastAddr, 
		    Port port, int ttl)
{
  return (MonitorClientID) new MonitorClient(unitID, multicastAddr, port, ttl);
}


void
MonitorClientDestroy(MonitorClientID client)
{
  if (client!=NULL) delete ((MonitorClient*)client);
}


const char *
MonitorClientUnitID(MonitorClientID client)
{
  return ((MonitorClient*)client)->getUnitID();
}


gm_Bool
MonitorClientSend(MonitorClientID client, const char *fieldID, 
		  const char *value, const char *script)
{
  if (client==NULL) return gm_False;
  return ((MonitorClient*)client)->Send(fieldID, value, script);
}


gm_Bool
MonitorClientSend_With_Unit(MonitorClientID client, const char *unitID, 
			    const char *fieldID, const char *value, 
			    const char *script)
{
  if (client==NULL) return gm_False;
  return ((MonitorClient*)client)->Send(unitID, fieldID, value, script);
}



Log&
Log::operator << (const char *string)
{
  if (client!=0) {
    client->Send("PTM Log", string, "Log");
  }
  else if (logFD!=0) {
    fprintf(logFD, string);
  }
  else cerr << prefix << string;
  return *this;
}

