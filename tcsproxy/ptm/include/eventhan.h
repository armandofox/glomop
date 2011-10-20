#ifndef __EVENTHAN_H__
#define __EVENTHAN_H__

#include "defines.h"
#include "linkedlist.h"
#include "gm_mutex.h"
#include <time.h>


#define EventType UINT32


class LogMutex : public Mutex {
public:
  LogMutex(const char *name_) : Mutex(name_) { }
  
  void Lock();
  void Unlock();
};


enum __EventType {
  evSelect=1,
  evConnectionReceived,
  evTimer,
  evReplyReceived,
  evQuit,

  evUserDefinedEvent
};


class EventSystem;
class CommunicationObject;


class EventHandler {
public:
  virtual gm_Bool HandleEvent(EventSystem *evs, EventType evType, void *args)=0;
};


struct Event {
  Event() : type(EventType(0)), handler(0), args(0) {};
  Event(EventType t, EventHandler *h, void *a) : type(t),handler(h),args(a) {};
  EventType    type;
  EventHandler *handler;
  void         *args;
};



class gm_Timer : public EventHandler {
public:
  gm_Timer(EventSystem *evSys, long seconds, long useconds, 
	gm_Bool repeatMode=gm_True);
  virtual ~gm_Timer();
  void IncrementTimeout();
  void ComputeAheadOfGMT(timeval *ahead);
  gm_Bool ShouldIRepeat() { return repeatMode; }

  static int  Compare(gm_Timer *t1, gm_Timer *t2);
  static void GetCurrentTime(timeval *gmt);
  
protected:
  virtual gm_Bool HandleEvent(EventSystem *evs, EventType evType, void *args);
  virtual gm_Bool EvTimer(EventSystem */*evs*/) { return gm_True; };

  struct timeval timeoutInterval, nextTimeout;
  gm_Bool   repeatMode;
  EventSystem *eventSystem;
};



class EventSystem {
public:
  EventSystem();
  virtual ~EventSystem();

  gm_Bool Run();
  gm_Bool AddCommunicationObject   (CommunicationObject*);
  void RemoveCommunicationObject(CommunicationObject*);
  gm_Bool AddTimer(gm_Timer*);
  void RemoveTimer(gm_Timer*);

  gm_Bool PostEventNoMutex(EventType eventType, EventHandler *object, 
			   void *args);
  gm_Bool PostEvent(EventType eventType, EventHandler *object, void *args);
  gm_Bool SendEvent(EventType eventType, EventHandler *object, void *args) 
  { 
    if (object!=NULL) 
      return object->HandleEvent(this, eventType, args); 
    else Return(gm_False, errEvSysSendError);
  }

  /*gm_Bool CheckCommunicationObjects() {
    gm_Bool returnValue;
    mutex.Lock();
    returnValue = ci.Check(this);
    mutex.Unlock();
    return returnValue;
  }*/

protected:
  gm_Bool GetEvent(Event *event);
  void Notify();

  struct CommunicationInfo {
    CommunicationInfo();
    gm_Bool Add   (CommunicationObject *comm);
    void    Remove(CommunicationObject *comm);
    //gm_Bool Check (EventSystem *evs);

    List<CommunicationObject> objectList;
    fd_set descrs;
    int    maxDescr;
  };
  

  struct TimerInfo {
  public:
    gm_Bool  Add   (gm_Timer *timer);
    void     Remove(gm_Timer *timer) { queue.Remove(timer); };
    gm_Timer*   RemoveFromHead() { return queue.RemoveFromHead();     };
    gm_Timer*   TimerAtHead()    { return queue.PeekAtHead(); };
  protected:
    List<gm_Timer> queue;
  };


  List<Event>        eventQueue;
  Mutex              mutex;
  Condition          notifyCond;
  int                pipeWriteFD, pipeReadFD; 
  gm_Bool            blockedOnSelect;
  // using a pipe as a condition variable for the queue, since the event system
  // thread can only block while doing a select!

  CommunicationInfo ci;
  TimerInfo         ti;
};



#endif // __EVENTHAN_H__
