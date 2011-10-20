#ifndef __PTM_H__
#define __PTM_H__
#include <sys/types.h>
#include "communication.h"
#include "packet.h"
#include "ptmnet.h"
#include "reqrep.h"


class DistillerRecord;
class DistillerDatabase;
class DistillerLauncher;
class BeaconTimer;
class OptionsFileTimer;
class MonitorClient;
class OptionDatabase;


class PTM {
public:
  PTM(OptionDatabase *options);
  ~PTM();
  gm_Bool Run();
  gm_Bool OptionsUpdate(OptionDatabase *options);

  gm_Bool Register  (DistillerRecord *record);
  void Deregister(DistillerRecord *record);
  gm_Bool EvAskForDistiller(DistillerType *distillerType,
			 PrivateConnection *replyObject,
			 UINT32 replyID);

  void Beacon(PrivateConnection *privateConnection=NULL);

  RemoteID                &getAddress()           { return myAddress; };
  RemoteID                &getMulticast()         { return myMulticast; };
  RequestReplyEventSystem *getEventSystem()       { return evs; };
  DistillerDatabase       *getDistillerDB()       { return distillerDB; };
  DistillerLauncher       *getDistillerLauncher() { return distillerLauncher;};
  MonitorClient           *getMonitorClient()     { return monitorClient; };

  gm_Bool AddConnection(PrivateConnection *socket) {
    return listOfConnections.InsertAtTail(socket);
  }
  void RemoveConnection(PrivateConnection *socket) {
    listOfConnections.Remove(socket);
  }

  int CompareBeacon(RemoteID &otherAddr, UINT32 otherRandomID);
  static void CtlCPressed(int sig);
  static void Abort(char *string);
  static void Usage();
  static PTM* getInstance() { return instance; };
  static gm_Bool CreateInstance(OptionDatabase *options);
protected:
  static void OptionsUpdate_static(int sig);
  gm_Bool ExpandOptions(OptionDatabase *optDB);

  RemoteID                myAddress, myMulticast;
  UINT32                  myRandomID;

  RequestReplyEventSystem *evs;
  PrivateConnectionServer *server;
  SharedBus               *bus;

  BeaconTimer             *beaconTimer;
  OptionsFileTimer        *optionsTimer;

  DistillerDatabase       *distillerDB;
  DistillerLauncher       *distillerLauncher;
  MonitorClient           *monitorClient;

  List<PrivateConnection> listOfConnections;
  static PTM              *instance;
};


class BeaconTimer : public gm_Timer {
public:
  BeaconTimer(EventSystem *evs, UINT32 beaconingPeriod_ms) 
    : gm_Timer(evs, SECONDS(beaconingPeriod_ms), 
	       USECONDS(beaconingPeriod_ms)) {};

protected:
  virtual gm_Bool EvTimer(EventSystem */*evs*/) 
  { PTM::getInstance()->Beacon(); return gm_True; };
};

class OptionsFileTimer : public gm_Timer {
private:
   OptionDatabase *options;
   time_t lastModified;
public:
   OptionsFileTimer(EventSystem *evs, UINT32 beaconingPeriod_ms, 
                    OptionDatabase *opt);
protected:
   virtual gm_Bool EvTimer(EventSystem */*evs*/);
};





#endif // __PTM_H__
