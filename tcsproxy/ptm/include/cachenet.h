#ifndef __CACHENET_H__
#define __CACHENET_H__


#include "communication.h"
#include "packet.h"
#include "reqrep.h"
#include "garbage.h"
#include "threads.h"


class CachedDistiller;


class MultipleUsersSocket : public TcpSocket {
public:
  MultipleUsersSocket(int sID=0, gm_Bool closeSocketFlag=gm_True) 
    : TcpSocket(sID, closeSocketFlag), readMutex("*-mu-rsck"),
    writeMutex("*-mu-wsck") { };
  ~MultipleUsersSocket() {
    /* // grab the mutex so that you can ensure no one else is using the
    // socket
    writeMutex.Lock();
    readMutex.Lock();*/
  }

  virtual gm_Bool Write(gm_Packet *packet);
  virtual gm_Bool Read (gm_Packet *packet);

private:
  Mutex readMutex;
  Mutex writeMutex;
};


/*class ThreadedSocket : public MultipleUsersSocket, public GarbageCollectedObject {
public:
  ThreadedSocket(EventSystem *evs_) : evs(evs_)  { };
  ~ThreadedSocket() { };

  gm_Bool Connect(const char* host, Port port, gm_Bool useIPAddrOnly=gm_True){
    if (MultipleUsersSocket::Connect(host, port, useIPAddrOnly)==gm_False)
      return gm_False;
    return ForkThread();
  }
protected:
  gm_Bool ForkThread() { return thread.Fork(ForkHelper, (void*)this); }

  void ThreadMain();
  static void *ForkHelper(void *this_) {
    ((ThreadedSocket*)this_)->ThreadMain();
    return 0;
  }
private:
  Thread thread;
  EventSystem *evs;
};*/


class ThreadedSocket : public GarbageCollectedObject {
public:
  ThreadedSocket(EventSystem *evs_, char *name_) : evs(evs_), name(name_) { };
  ~ThreadedSocket() { };

  gm_Bool ForkThread() { return thread.Fork(ForkHelper, (void*)this); }

protected:
  void ThreadMain();
  static void *ForkHelper(void *this_) {
    ((ThreadedSocket*)this_)->ThreadMain();
    return 0;
  }

  virtual int getID()=0;
  virtual gm_Bool EvSelect(EventSystem *evs)=0;

private:
  Thread thread;
  EventSystem *evs;
  const char *name;
};


class FE_PTMConnection : public ThreadedSocket, public MultipleUsersSocket, 
		      public gm_PacketHandler {
public:
  FE_PTMConnection(EventSystem *evs)
    : ThreadedSocket(evs, "FE_PTMConnection") { 
      Mutex::Log_("created new FE_PTMConnection", "", (Mutex*)this);
  };
  gm_Bool EvConnectionBroken(EventSystem *evs);

  gm_Bool Connect(const char* host, Port port, gm_Bool useIPAddrOnly=gm_True){
    if (MultipleUsersSocket::Connect(host, port, useIPAddrOnly)==gm_False)
      return gm_False;
    return ForkThread();
  }
protected:
  int getID() { return MultipleUsersSocket::getID(); };
  gm_Bool EvSelect(EventSystem *evs) {return gm_PacketHandler::EvSelect(evs);};
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { return Read(packet); };
  void ErrorOutput(const char *string) { gm_Log(string); };
};


class SharedBus : public ThreadedSocket, public gm_CoordinationBus, 
		  public gm_PacketHandler {
public:
  SharedBus(EventSystem *evs, const char *ipAddress, Port port, int ttl) 
    : ThreadedSocket(evs, "SharedBus"), 
      gm_CoordinationBus(ipAddress, port, ttl, gm_True, gm_True), 
    gm_PacketHandler() { 
      ForkThread();
  };

protected:
  int getID() { return getListener()->getID(); };
  gm_Bool EvSelect(EventSystem *evs) {return gm_PacketHandler::EvSelect(evs);};
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { return getListener()->Read(packet);};
  void ErrorOutput(const char *string) { gm_Log(string); };
};


class DistillerRequest;

class DistillerConnection : public ThreadedSocket, public MultipleUsersSocket,
			    public gm_PacketHandler {
public:
  DistillerConnection(CachedDistiller *distiller_);
  ~DistillerConnection();

  gm_Bool Connect(const char* host, Port port, gm_Bool useIPAddrOnly=gm_True){
    if (MultipleUsersSocket::Connect(host, port, useIPAddrOnly)==gm_False)
      return gm_False;
    return ForkThread();
  }
  /*gm_Bool AddRequest(DistillerRequest *request)
  {
    gm_Bool returnValue;
    //mutex.Lock();
    returnValue = pendingRequests.InsertAtHead(request);
    //mutex.Unlock();
    return returnValue;
  }
  void RemoveRequest(DistillerRequest *request)
  {
    //mutex.Lock();
    pendingRequests.Remove(request);
    //mutex.Unlock();
  }
  void RemoveAllRequests();*/

protected:
  int getID() { return MultipleUsersSocket::getID(); };
  gm_Bool EvSelect(EventSystem *evs) {return gm_PacketHandler::EvSelect(evs);};
  gm_Bool EvConnectionBroken(EventSystem *evs);
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { 
    gm_Bool returnValue;
    IncrReference();
    returnValue = Read(packet);
    DecrReference();
    return returnValue;
  };
  void ErrorOutput(const char *string) { gm_Log(string); };  

private:
  CachedDistiller *distiller;
  //LockingList<DistillerRequest> pendingRequests;
};


struct DistillerReply {
  DistillerReply() : output(), status(distOk) { }

  DistillerOutput output;
  DistillerStatus status;
};


class DistillerRequest : public RequestReply {
public:
  DistillerRequest(CachedDistiller *dist);
  // the Send method is invoked after the distMutex is *UNLOCKED*
  gm_Bool Send(DistillerConnection *connection, Argument *args, 
		      int numberOfArgs, DistillerInput *input);
  gm_Bool Wait(struct timeval *tv);
  gm_Bool SendAndWait(DistillerConnection *connection, Argument *args, 
		      int numberOfArgs, DistillerInput *input);
  void Destroy(void);
  void EvDistillerConnectionBroken();

  DistillerOutput *getOutput() { return &reply.output; };
  DistillerStatus getStatus()  { return reply.status;  };
protected:
  gm_Bool EvReplyReceived(RequestReplyEventSystem *evs, void *args);
  gm_Bool EvTimer(RequestReplyEventSystem *evs);

private:
  CachedDistiller *dist; // this distiller already has its ref count set
  DistillerReply  reply;
  gm_Bool         stopWaiting;
  Mutex           mutex;
  Condition       condition;
};


class DistillerInfoHandler {
public:
  DistillerInfoHandler() : streamPtr(0), idx(0), numberOfDistillers(0),
    completeUpdate(gm_False) { };
  gm_Bool Handle(IStream &istream, gm_Bool completeUpdate);
  gm_Bool getNext(BasicDistiller *record);
  gm_Bool IsCompleteUpdate() { return completeUpdate; };
private:
  IStream *streamPtr;
  UINT32  idx, numberOfDistillers;
  gm_Bool completeUpdate;
};



#endif // __CACHENET_H__

