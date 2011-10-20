#ifndef __CACHE_H__
#define __CACHE_H__

#include "distdb.h"
#include "cachenet.h"
#include "garbage.h"


class NegativeCache {
public:
  NegativeCache() : touchCounter(0), mutex("*-nc") { };
  ~NegativeCache();
  gm_Bool Add(DistillerType *type);
  gm_Bool IsThere(DistillerType *type);
  void Flush();

protected:
  struct NegativeCacheEntry : public DatabaseRecord {
    NegativeCacheEntry(DistillerType *type_) 
      : type(*type_), lastTouched(0) { };
    DistillerType type;
    UINT32 lastTouched;
  };

  class NCIndex : public Index {
  public:

  protected:
    IndexKey *getIndexKey(DatabaseRecord *record) 
    { return &((NegativeCacheEntry*)record)->type; }
  };

  void Touch_(NegativeCacheEntry *record) {
    if (record->lastTouched==touchCounter && touchCounter!=0) 
      return; // don't bother!
    touchCounter++;
    record->lastTouched = touchCounter;
  }
  void Evict_();

  LockingList<NegativeCacheEntry> list;
  UINT32 touchCounter;
  NCIndex index;
  Mutex mutex;
};




struct CachedDistiller;


class DistillerTypeIndex : public Index {
public:
  DistillerTypeIndex(gm_Bool dupsAllowed=gm_True,
		     int numberOfBuckets=DefaultNumberOfBuckets)
    : Index(dupsAllowed, numberOfBuckets) { };
protected:
  IndexKey *getIndexKey(DatabaseRecord *record) 
  { return &((BasicDistiller*)record)->type; }
};


class DistillerCache : public BasicDistillerDatabase {
public:
  DistillerCache();
  ~DistillerCache();

  CachedDistiller *getDistiller(DistillerType *type, DistillerStatus &status);
  gm_Bool Update(DistillerInfoHandler *handler);
  void    Remove__(CachedDistiller *dist) {
    cacheMutex.Lock();
    Remove_(dist);
    cacheMutex.Unlock();
  }
  void FlushNCache() { negativeCache.Flush(); }

private:
  // all private functions assume that you've already grabbed the mutex
  CachedDistiller *Find_(DistillerType *type) { return Find_Lottery_(type); }
  CachedDistiller *Find_Lottery_(DistillerType *type);
  CachedDistiller *Find_Random_ (DistillerType *type);
  CachedDistiller *Find_(RemoteID *rid);
  gm_Bool         Add_(CachedDistiller *dist);
  void            Remove_(CachedDistiller *dist);
  gm_Bool         Update_(BasicDistiller *newRecord);
  void            Sweep_();
  void            MarkAsValid_(gm_Bool flag=gm_True) 
  { ForEach(Validate_, &flag); };
  
  static gm_Bool Validate_(Database* db,  DatabaseRecord* record, void* flag);
  static gm_Bool SweepFunction_(Database*,DatabaseRecord* record, void* dummy);

  // these definition are in place only to make sure no one is calling
  // the underlying Database::Add/Remove
  gm_Bool Add(CachedDistiller *dist);
  void    Remove(CachedDistiller *dist);

  Mutex cacheMutex;
  DistillerTypeIndex *typeIndex;

  NegativeCache negativeCache;
};


#define LOCK1(call) \
  distMutex.Lock(); \
  call; \
  distMutex.Unlock();

#define LOCK2(return_type, call) \
  return_type returnValue; \
  distMutex.Lock(); \
  returnValue = call; \
  distMutex.Unlock(); \
  return returnValue;


//#define __INSTRUMENT__
#ifdef __INSTRUMENT__
extern FILE *instFile;
#endif

struct CachedDistiller : public BasicDistiller, GarbageCollectedObject {
public:
  CachedDistiller(RemoteID& rid, DistillerType& type)
    : BasicDistiller(rid, type), connection(NULL), validFlag(gm_True),
      distMutex(type.c_struct.string), pendingCount(0) { };
  CachedDistiller() 
    : BasicDistiller(), connection(NULL), validFlag(gm_True), 
      distMutex("unknown-dist"), pendingCount(0) { };
  ~CachedDistiller() {
    Disconnect_();
  }
  DistillerStatus Distill(Argument *args, int numberOfArgs, 
			  DistillerInput *input, DistillerOutput *output);
  DistillerStatus DistillAsync(Argument *args, int numberOfArgs, 
			  DistillerInput *input,
			  DistillerRequest *& newrequest);
	
  gm_Bool IsConnected__() {
    gm_Bool returnValue;
    distMutex.Lock();
    returnValue = IsConnected_();
    distMutex.Unlock();
    return returnValue;
  }
  /*gm_Bool Connect__() {
    gm_Bool returnValue;
    distMutex.Lock();
    returnValue = Connect_();
    distMutex.Unlock();
    return returnValue;
  }*/
  void Disconnect__() {
    distMutex.Lock();
    Disconnect_();
    distMutex.Unlock();
  }

  void EvConnectionBroken() {
    distMutex.Lock();
    //RemoveAllRequests_();
    Disconnect_();
    distMutex.Unlock();
  }

  void IncrPendingCount() {
    distMutex.Lock();
    pendingCount++;
    distMutex.Unlock();
  }
  void DecrPendingCount() {
    distMutex.Lock();
    pendingCount--;
    distMutex.Unlock();
  }

  UINT32 getTickets() { 
    UINT32 ql = load.totalLatency_ms;
#ifdef __INSTRUMENT__
    UINT32 qlOld=ql;
#endif
    int correction = pendingCount;
    ql = (( (int(ql)) < -correction) ? (0) : (ql+correction));
#ifdef __INSTRUMENT__
    if (instFile!=NULL) {
      time_t now;
      tm *tm_;
      time(&now);
      tm_ = localtime(&now);
      fprintf(instFile, "%02d:%02d:%02d(%p): %lu %d %lu\n", tm_->tm_hour, 
	      tm_->tm_min, tm_->tm_sec, this, qlOld, correction, ql);
    }
#endif
    return (100/((ql>>2)+1)) + 1; 
  };
  gm_Bool ShouldIgnore() { 
    /*return (totalLatency_ms >= ignoreThreshold_ms && ignoreThreshold_ms!=0) ?
      gm_True : gm_False; */
    // don't ever ignore!
    return gm_False;
  };

  void SetName() { distMutex.SetName(type.c_struct.string); };

private:
  // these functions assume that the mutex has already been locked before 
  // they are called
  gm_Bool IsConnected_() { return (connection==NULL) ? gm_False:gm_True; };
  gm_Bool Connect_();
  void    Disconnect_();

  void    Update(BasicDistiller *newRecord) { 
    LOCK1(load = newRecord->load; pendingCount = 0); 
  };
  void    MarkAsValid(gm_Bool flag=gm_True) { LOCK1(validFlag = flag); };
  gm_Bool IsValid() { LOCK2(gm_Bool, validFlag); };

  gm_Bool AddRequest_(DistillerRequest *request) {
    return pendingRequests.InsertAtHead(request);
  }
  void RemoveRequest_(DistillerRequest *request) {
    pendingRequests.Remove(request);
  }
  void RemoveAllRequests_();

  /*Mutex *getMutex() { return &distMutex; };
    gm_Bool Send_(Packet *packet) { 
    gm_Bool returnValue;
    DistillerConnection *conn = connection;
    // use 'conn' instead of connection, since we don't want to use an 
    // instance variable while the mutex in unlocked!
    if (conn==NULL) 
      Return(gm_False, errGenericError); // this shouldn't happen
    
    // release the mutex before sending
    conn->IncrReference();
    distMutex.Unlock(); // unlock the mutex before sending
    
    Mutex::Log_("sending data", "", NULL);
    returnValue = conn->Write(packet);
    Mutex::Log_("done sending data", "", NULL);
    
    distMutex.Lock();
    conn->DecrReference();
    return returnValue;
  }*/
  DistillerConnection *getConnection_() { return connection; };

  DistillerConnection *connection;
  gm_Bool validFlag;
  Mutex   distMutex;
  LockingList<DistillerRequest> pendingRequests;
  int     pendingCount;

  /*friend DistillerStatus DistillerCache::Distill(DistillerType *type, 
         Argument *args, int numberOfArgs, DistillerInput *input, 
         DistillerOutput *output, gm_Bool *freeOutputBuffer);*/
  friend class DistillerCache;
  friend class DistillerRequest;
};


#undef LOCK1
#undef LOCK2

#endif // __CACHE_H__
