#ifndef __REQREP_H__
#define __REQREP_H__

#include "eventhan.h"
#include "database.h"


class RequestReplyTimer;
class RequestReplyEventSystem;

struct RequestReplyKey : public IndexKey {
  RequestReplyKey() : id(NoRequestReply) { };
  gm_Bool Equal(IndexKey *key) 
  { return (id==((RequestReplyKey*)key)->id) ? gm_True : gm_False; };
  UINT32 Hash() { return id; };

  UINT32 id;
};


class RequestReply : public EventHandler, public DatabaseRecord {
public:
  RequestReply(RequestReplyEventSystem *evs, long timeoutSec,long timeoutUsec);
  virtual ~RequestReply();

  virtual gm_Bool EvReplyReceived(RequestReplyEventSystem *evs, void *args)=0;
  virtual gm_Bool EvTimer(RequestReplyEventSystem *evs)=0;
  gm_Bool    Retry(long timeoutSec, long timeoutUsec);

  void    SetID(UINT32 id) { key.id = id; };
  UINT32  getID() { return key.id; };

  RequestReplyKey   key;
protected:
  gm_Bool   HandleEvent(EventSystem *evs, EventType eventType, void *args);

  RequestReplyTimer *timer;
  RequestReplyEventSystem *eventSystem;
};


class RequestReplyTimer : public gm_Timer {
public:
  RequestReplyTimer(EventSystem *evs, RequestReply *rr, long timeoutSec,
		    long timeoutUsec) 
    : gm_Timer(evs, timeoutSec, timeoutUsec, gm_False), requestReply(rr) { };

protected:
  gm_Bool EvTimer(EventSystem *evs);
  RequestReply *requestReply;
};



class RequestReplyEventSystem : public EventSystem {
public:
  RequestReplyEventSystem() : transactionCount(0), requestReplyMutex("*-rr"){};

  gm_Bool AddRequestReply   (RequestReply *requestReplyObject);
  void RemoveRequestReply(RequestReply *requestReplyObject);

  gm_Bool SendReplyReceivedEvent(UINT32 transactionID, void *args);
protected:
  class RequestReplyIndex : public Index {
  public:
    RequestReplyIndex(gm_Bool dupsAllowed=gm_False,
		      int numberOfBuckets=DefaultNumberOfBuckets)
      : Index(dupsAllowed, numberOfBuckets) { };
  protected:
    IndexKey *getIndexKey(DatabaseRecord *record)
    { return &((RequestReply*)record)->key; }
  };

  UINT32 transactionCount;
  RequestReplyIndex requestReplyIndex;
  Mutex requestReplyMutex;
};


#endif // __REQREP_H__

