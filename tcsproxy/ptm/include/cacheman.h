#ifndef __CACHEMAN_H__
#define __CACHEMAN_H__

#include "reqrep.h"
#include "options.h"
#include "rexec.h"
#include "cache.h"

class Thread;
#define evAskForDistiller (evUserDefinedEvent+1)



class StartPTMTimer {
public:
  StartPTMTimer() : timeout_ms(DefaultLostBeacons*DefaultBeaconingPeriod_ms),
    timer(NULL), mutex("*-start-ptm") { };
  void Initialize(UINT32 timeout_ms_) { timeout_ms = timeout_ms_; };

  void Disable() { 
    mutex.Lock();
    timer = NULL; 
    mutex.Unlock();
  };
  gm_Bool IsDisabled() { 
    mutex.Lock();
    gm_Bool returnValue = (timer==NULL) ? gm_True : gm_False; 
    mutex.Unlock();
    return returnValue;
  };

  gm_Bool NewTimer(EventSystem *evs) { 
    mutex.Lock();
    timer = new ActualTimer(this, evs, RandomDelay());
    if (timer==NULL) {
      mutex.Unlock();
      Return(gm_False, errOutOfMemory);
    }
    mutex.Unlock();
    return gm_True;
  }

protected:
  gm_Timer *getTimer() {
    mutex.Lock();
    gm_Timer *t = timer;
    mutex.Unlock();
    return t;
  }
  class ActualTimer : public gm_Timer {
  public:
    ActualTimer(StartPTMTimer *parent_, EventSystem *evs, UINT32 delay_ms)
      : gm_Timer(evs, SECONDS(delay_ms), USECONDS(delay_ms), gm_False), 
	parent(parent_) { 
      gm_Log("Creating a timer for starting the PTM (" << (UINT32)this 
	     << ")\n");
    }
    
  protected:
    gm_Bool EvTimer(EventSystem *evs) {
      if (parent->getTimer()==this) {
	gm_Log("StartPTMTimer (" << (UINT32)this 
	       << ") expired; invoking EvTimer()\n");
	// no one deleted the timer yet, so it is a real timeout
	if (parent->EvTimer(evs)==gm_False) return gm_False;
      } else {
	gm_Log("StartPTMTimer (" << (UINT32) this 
	       << ") expired, but this timer is obsolete\n");
      }
      delete this;
      return gm_True;
    };
    StartPTMTimer *parent;
  };


  gm_Bool EvTimer(EventSystem *evs);
  UINT32 RandomDelay();

  UINT32 timeout_ms;
  ActualTimer *timer;
  Mutex mutex;
  friend class ActualTimer;
};




class CacheManager {
public:
  CacheManager(OptionDatabase *options, const char *monitorUnitID);
  virtual ~CacheManager();
  
  /* called by the distiller threads */
  DistillerStatus Distill(DistillerType *type, Argument *args,int numberOfArgs,
			  DistillerInput *input, DistillerOutput *output);
  DistillerStatus DistillAsync(DistillerType *type, Argument *args,
  			  int numberOfArgs, DistillerInput *input,
			  DistillerRequest *& newrequest);
  DistillerStatus DistillRendezvous(DistillerRequest *& newrequest,
			  DistillerOutput *output, struct timeval *tv);
  void DistillDestroy(DistillerRequest *& newrequest);
  RequestReplyEventSystem *getEventSystem() { return evs; }
  MonitorClient *getMonitorClient() { return monitorClient; }

  /* Invoked by socket threads */
  gm_Bool EvPTMLocationReceived__(RemoteID &newPtmAddress);
  gm_Bool UpdateCache__(DistillerInfoHandler *handler) { 
    return cache->Update(handler); 
  }
  void FlushNCache() { cache->FlushNCache(); }

  gm_Bool RestartPTMTimer__();
  gm_Bool ClosePTMConnection__();
  gm_Bool StartPTM__();
  gm_Bool IsConnectedToPTM__() { 
    gm_Bool returnValue;
    mgrMutex.Lock();
    returnValue = IsConnectedToPTM_();
    mgrMutex.Unlock();
    return returnValue;
  };
  gm_Bool IsConnectedToPTM_() {
    return (ptmConnection_==NULL) ? gm_False:gm_True; 
  }
  void WaitForPTM__();
  gm_Bool AskPTMForDistiller__(DistillerType *type, 
			       AskForDistillerStatus &status);
  gm_Bool AskPTMForUnicastBeacon() {
    FE_PTMConnection *conn = ptmConnection_;
    if (conn==NULL) return gm_True;
    conn->IncrReference();
    gm_Packet packet(pktAskForUnicastBeacon, 0, 0);
    if (conn->Write(&packet)==gm_False) {
      return gm_False;
    }
    conn->DecrReference();
    return gm_True;
  }

  
  static CacheManager* getInstance() { return instance; };
  static void Abort(const char *string);
  static void CtlCPressed(int sig);
  static gm_Bool CreateInstance(OptionDatabase *options, 
				const char *monitorUnitID);


private:
  gm_Bool ConnectToPTM_(RemoteID &ptmAddress_);
  gm_Bool EvPTMConnectionLost_();
  gm_Bool EvPTMConnectionLost__() {
    mgrMutex.Lock();
    gm_Bool returnValue = EvPTMConnectionLost_();
    mgrMutex.Unlock();
    return returnValue;
  }

  class NoBeaconTimer : public gm_Timer {
  public:
    NoBeaconTimer(EventSystem *evs, UINT32 timeout_ms) 
      : gm_Timer(evs, SECONDS(timeout_ms),
	      USECONDS(timeout_ms), gm_True),
	beaconRecvd(gm_False), askedForUnicastBeacon(gm_False) { };
    
    gm_Bool EvTimer(EventSystem *evs);
    gm_Bool EvBeaconReceived(EventSystem */*evs*/) { 
      beaconRecvd = gm_True; 
      askedForUnicastBeacon = gm_False;
      return gm_True;
    };
    
  private:
    gm_Bool beaconRecvd;
    gm_Bool askedForUnicastBeacon;
  };
  
  static void *ForkHelper(void *);
  void Main();
  gm_Bool ExpandOptions(OptionDatabase *options_);

  RequestReplyEventSystem *evs;
  FE_PTMConnection *ptmConnection_;
  SharedBus        *bus;
  Thread           *managerThread;
  DistillerCache   *cache;
  MonitorClient    *monitorClient;
  StartPTMTimer    startPTMTimer;
  NoBeaconTimer    *noBeaconTimer;
  OptionDatabase   *options;
  RExec            rexec;

  Mutex            mgrMutex;
  Mutex            ptmFoundMutex;
  Condition        ptmFoundCond;
  
  static CacheManager *instance;
  


  struct AskForDistiller : public RequestReply {
    AskForDistiller(RequestReplyEventSystem *evs) 
      : RequestReply(evs, SECONDS(AskForDistillerTimeout_ms),
		     USECONDS(AskForDistillerTimeout_ms)), mutex("ask-ptm"){ };
    gm_Bool SendAndWait(FE_PTMConnection *connection, DistillerType *type);
    AskForDistillerStatus Status() { return status; };
    
  protected:
    gm_Bool EvReplyReceived(RequestReplyEventSystem *evs, void *args);
    gm_Bool EvTimer(RequestReplyEventSystem *evs);
    
    AskForDistillerStatus status;
    Mutex     mutex;
    Condition condition;
  };
};


extern "C" {
#include "thr_cntl.h"
}
inline void SetThreadState(thread_state_t state) {
  extern pthread_key_t index_key;
  INST_set_thread_state((int)pthread_getspecific(index_key), state);
}




#endif // __CACHEMAN_H__
