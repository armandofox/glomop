#include "ptmdb.h"
#include "packet.h"
#include "ptm.h"
#include "distlaunch.h"


//#define __INSTRUMENT__
#ifdef __INSTRUMENT__
FILE *instFile=NULL;
#endif


DistillerRecord::DistillerRecord(PrivateConnection *privateConnection_,
				 RemoteID& _rid, DistillerType& _type,
				 DistillerSleepStatus status_,
				 gm_Bool mustRestartOnDeath_)
  : BasicDistiller(_rid, _type), privateConnection(privateConnection_), 
    sleepStatus(status_), history(NULL), historySize(0), currentPos(0),
    launchRecord(NULL), mustRestartOnDeath(mustRestartOnDeath_)
{
#ifdef __INSTRUMENT__
  if (instFile==NULL) {
    instFile = fopen("ptm/ptm.log", "w");
  }
#endif
  /*launchRecord = PTM::getInstance()->getDistillerLauncher()
    ->getLaunchRecord(&_type);*/
  launchRecord = PTM::getInstance()->getDistillerLauncher()
    ->getLaunchRecord(&_type, gm_True);
  if (launchRecord!=NULL) {
    if (sleepStatus==distWakeUp) launchRecord->EvRegisterDistiller(this);
    load.ignoreThreshold_ms = launchRecord->autoLaunchLimit * 2;
    historySize = launchRecord->loadHistorySize;
  }

  if (historySize==0) historySize = DefaultLoadHistorySize;
  history = new Load [historySize];
  if (history==NULL) VoidReturn(errOutOfMemory);
}


DistillerRecord::~DistillerRecord()
{
  if (launchRecord!=NULL && sleepStatus==distWakeUp) 
    launchRecord->EvDeregisterDistiller(this);
  if (history!=NULL) {
    delete [] history;
    history = NULL;
  }
}


void
DistillerRecord::EvUnnaturalDeath()
{
  if (mustRestartOnDeath==gm_True && launchRecord!=NULL) {
    launchRecord->Relaunch();
  }
}


void 
DistillerRecord::Update(Load &newLoad)
{
  Load prevLoad = load;
  loadOverHistory -= history[currentPos];
  loadOverHistory += newLoad;

  load.Average(loadOverHistory, historySize);
  history[currentPos] = newLoad;
  currentPos = (currentPos+1) % historySize;

#ifdef __INSTRUMENT__
  if (instFile!=NULL) {
    char buf[512], *bufPtr=buf;
    time_t now;
    tm *tm_;
    time(&now);
    tm_ = localtime(&now);
    sprintf(bufPtr, "%02d:%02d:%02d(%p): ", tm_->tm_hour, tm_->tm_min, 
	    tm_->tm_sec, this);
    bufPtr += strlen(bufPtr);
    for (int i=0; i<historySize; i++) {
      sprintf(bufPtr, "%lu, ",
	      history[(i+currentPos)%historySize].totalLatency_ms);
      bufPtr += strlen(bufPtr);
    }

    Load avg, totalLoad;
    int totalDist=0;
    if (launchRecord!=NULL && launchRecord->numberOfActiveDistillers > 0) {
      totalLoad = launchRecord->totalLoad;
      totalDist = launchRecord->numberOfActiveDistillers;
      avg.Average(totalLoad, totalDist);
    }

    sprintf(bufPtr, "Avg = %lu, Global avg (%lu/%d) = %lu\n", 
	    load.totalLatency_ms, totalLoad.totalLatency_ms, 
	    totalDist, avg.totalLatency_ms); 
    bufPtr += strlen(bufPtr);
    fwrite(buf, sizeof(char), bufPtr-buf, instFile);
  }
#endif
  if (launchRecord!=NULL) launchRecord->UpdateLoad(load, prevLoad);
}


gm_Bool
DistillerRecord::WakeUp()
{
  if (sleepStatus==distWakeUp) return gm_True;
  if (privateConnection==NULL) {
    // this shouldn't happen!
    Return(gm_False,errNULLPointer);
  }
  OStream stream;
  stream << Binary(gm_True) << UINT32(distWakeUp) << Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;
  gm_Packet packet(pktDistillerSleep, stream.getLength(), stream.getData());
  if (privateConnection->Write(&packet)==gm_False) {
    // could not write the packet
    privateConnection->EvConnectionBroken(PTM::getInstance()->
					  getEventSystem());
    Return(gm_False, errSocketWriteError);
  }
  else {
    sleepStatus = distWakeUp;
    if (launchRecord!=NULL) launchRecord->EvRegisterDistiller(this);
    return gm_True;
  }
}


gm_Bool
DistillerDatabase::FindMatchingDistillers(DistillerType *type, 
					  List<DatabaseRecord> *list)
{
  MatchingDistillerFinder finder(type, list);
  return ForEach(MatchEachDistiller, (void*) &finder);
}


gm_Bool
DistillerDatabase::MatchEachDistiller(Database */*db*/, DatabaseRecord *record,
				      void *args)
{
  MatchingDistillerFinder *finder = (MatchingDistillerFinder*) args;
  DistillerRecord *dist = (DistillerRecord*)record;
  if(dist->type.CanSatisfy(finder->distillerType)==gm_True 
     && dist->sleepStatus==distWakeUp){
    if (finder->distillerList->InsertAtHead(record)==gm_False) return gm_False;
  }

  return gm_True;
}


struct SleepingDistillerFinder {
  SleepingDistillerFinder(DistillerType *type_) : dist(NULL), type(type_) { };
  DistillerRecord *dist;
  DistillerType   *type;
};


DistillerRecord *
DistillerDatabase::WakeSleepingDistiller(DistillerType *type)
{
  SleepingDistillerFinder finder(type);

  while (gm_True) {
    ForEach(MatchSleepingDistiller, (void*) &finder);
    if (finder.dist==NULL) return NULL;
    if (finder.dist->WakeUp()==gm_False) {
      if (Error::getStatus()!=errSocketWriteError) return NULL;
      // could not wake up the distiller; try again...
      Error::SetStatus(success);
    }
    else return finder.dist;
  }
}


gm_Bool
DistillerDatabase::MatchSleepingDistiller(Database */*db*/, 
					  DatabaseRecord *record, void *args)
{
  SleepingDistillerFinder *finder = (SleepingDistillerFinder*) args;
  DistillerRecord *dist = (DistillerRecord*)record;
  if (dist->type.CanSatisfy(finder->type)==gm_True
      && dist->sleepStatus==distSleep) {
    finder->dist = dist;
    return gm_False; // we found something, so quit the loop
  }

  return gm_True;
}

