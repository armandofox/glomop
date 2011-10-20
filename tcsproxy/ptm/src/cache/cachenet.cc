#include "log.h"
#include "distdb.h"
#include "cache.h"
#include "cachenet.h"
#include "cacheman.h"
#include <stdlib.h>
#include <errno.h>


gm_Bool
MultipleUsersSocket::Write(gm_Packet *packet)
{
  gm_Bool returnValue;
  writeMutex.Lock();
  returnValue = TcpSocket::Write(packet);
  writeMutex.Unlock();
  return returnValue;
}


gm_Bool
MultipleUsersSocket::Read(gm_Packet *packet)
{
  gm_Bool returnValue;
  readMutex.Lock();
  returnValue = TcpSocket::Read(packet);
  readMutex.Unlock();
  return returnValue;
}


void
ThreadedSocket::ThreadMain()
{
  fd_set readfds, exceptfds;
  int returnValue, id;

  IncrReference();

  Mutex::Log_("started ThreadMain", "", (Mutex*)this);
  while (gm_True) {
    if (IsDeleted()==gm_True) break;

    id = getID();
    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    FD_SET(id, &readfds);
    FD_SET(id, &exceptfds);
    returnValue = select(id+1, &readfds, (fd_set*)NULL, NULL , NULL);
    Mutex::Log_("ThreadMain selected", "", (Mutex*)this);
    if (returnValue < 0) {
      if (errno==EINTR) continue;
      if (errno!=EBADF) {
	gm_Log("ThreadedSocket(" << name << ") failed on select. Errno was " 
	       << errno << " (" << strerror(errno) << ")\n");
	abort();
      }
      break;
    }

    if (EvSelect(evs)==gm_False) {
      gm_Log("ThreadedSocket(" << name << ") failed on EvSelect()\n");
      Error::Print();
      abort();
    }
  }

  gm_Log("ThreadedSocket(" << name << ") is exiting\n");
  Mutex::Log_("ThreadMain exiting", "", NULL);
  DecrReference();
}


gm_Bool
FE_PTMConnection::EvConnectionBroken(EventSystem */*evs*/)
{
  return CacheManager::getInstance()->ClosePTMConnection__();
}


/*
 * Format: <number-of-distillers>[per-distiller-info]|...\0
 */
gm_Bool
DistillerInfoHandler::Handle(IStream &stream, gm_Bool completeUpdate_)
{
  stream >> Binary(gm_True) >> numberOfDistillers >> Delimit('|');
  if (stream.IsGood()==gm_False) return gm_False;
  //XXXgm_Log("Got " << numberOfDistillers << " distillers **********\n");
  streamPtr = &stream;
  completeUpdate = completeUpdate_;
  return CacheManager::getInstance()->UpdateCache__(this);
}


gm_Bool
DistillerInfoHandler::getNext(BasicDistiller *record)
{
  if (idx >= numberOfDistillers) {
    // return gm_False *without* setting Error::status
    return gm_False;
  }

  idx++;
  (*streamPtr) >> (*record) >> Skip;
  if (streamPtr->IsGood()==gm_False) return gm_False;
  return gm_True;
}

  
class PTMBeaconHandler : public DistillerInfoHandler {
public:
  gm_Bool Handle(gm_Packet *packet);
};


/*
 * Format: <ptm-ip-address/port>|distiller-info
 */
gm_Bool
PTMBeaconHandler::Handle(gm_Packet *packet)
{
  RemoteID rid;
  UINT32 dummyRandomID;
  IStream stream(packet->getData(), packet->getLength());

  stream >> Delimit('|') >> rid >> Skip >> Binary(gm_True) >> dummyRandomID 
	 >> Binary(gm_False) >> Skip;
  if (stream.IsGood()==gm_False || rid.port==0) return gm_False;

  if (CacheManager::getInstance()->EvPTMLocationReceived__(rid)==gm_False) {
    // the only error that could have occurred is errOutOfMemory
    // all other errors are trapped inside CacheManager::PTMLocationReceived
    return gm_False;
  }

  return DistillerInfoHandler::Handle(stream, gm_True);
}


class AskForDistillerReplyHandler : public DistillerInfoHandler {
public:
  gm_Bool Handle(gm_Packet *packet);
};


gm_Bool
AskForDistillerReplyHandler::Handle(gm_Packet *packet)
{
  UINT32 replyStatus;
  IStream stream(packet->getData(), packet->getLength());

  stream >> Delimit('|') >> replyStatus >> Skip;

  if (replyStatus==ptmOk) {
    if (DistillerInfoHandler::Handle(stream, gm_False)==gm_False) 
      return gm_False;
  }

  return CacheManager::getInstance()->getEventSystem()->
    SendReplyReceivedEvent(packet->getRequestReplyID(), (void*)replyStatus);
}
  

gm_Bool
FE_PTMConnection::EvPacketReceived(EventSystem */*evs*/, gm_Packet *packet)
{
  switch (packet->getType()) {
  case pktAskForDistillerReply: 
    {
      AskForDistillerReplyHandler handler;
      return handler.Handle(packet);
    }

  case pktPTMBeacon: 
    {
      gm_Log("Received unicast beacon from PTM\n");
      PTMBeaconHandler handler;
      gm_Bool returnValue = handler.Handle(packet);
      if (returnValue==gm_False) {
	gm_Log("Error in pktPTMBeacon!!!\n");
	Error::Print();
      }
      return returnValue;
    }

  case pktFlushNCache:
    {
      gm_Log("Flushing negative cache at frontend\n");
      CacheManager::getInstance()->FlushNCache();
      return gm_True;
    }
  default:
    Return(gm_False, errInvalidPacketType);
  }  
}


gm_Bool
SharedBus::EvPacketReceived(EventSystem */*evs*/, gm_Packet *packet)
{
  switch (packet->getType()) {
  case pktPTMBeacon: 
    {
      PTMBeaconHandler handler;
      return handler.Handle(packet);
    }

  case pktWillStartPTM:
    {
      if (CacheManager::getInstance()->IsConnectedToPTM__()==gm_True)
	return gm_True;

      // someone else is going to start the PTM; I'm going to disable my timer
      // and wait for the PTM to start up
      return CacheManager::getInstance()->RestartPTMTimer__();
    }

  default:
    Return(gm_False, errInvalidPacketType);
  }  
}


DistillerConnection::DistillerConnection(CachedDistiller *distiller_)
  : ThreadedSocket(CacheManager::getInstance()->getEventSystem(),
		   "DistillerConnection"),
    gm_PacketHandler(),
    distiller(distiller_) 
{
  distiller->IncrReference();
}


DistillerConnection::~DistillerConnection()
{
  distiller->DecrReference();
}


/*void 
DistillerConnection::RemoveAllRequests()
{
  DistillerRequest *request;
  while (pendingRequests.IsEmpty()==gm_False) {
    request = pendingRequests.RemoveFromHead();
    request->EvDistillerConnectionBroken();
  }
}*/



gm_Bool
DistillerConnection::EvConnectionBroken(EventSystem */*evs*/)
{
  CachedDistiller *dist = distiller;
  // notify waiting requests
  dist->IncrReference();
  dist->EvConnectionBroken(); // dist->EvConnectionBroken results in 'this'
                              // being deleted
  dist->DecrReference();
  return gm_True;
}


gm_Bool
DistillerConnection::EvPacketReceived(EventSystem *evs, gm_Packet *packet)
{
  UINT32 statusUINT32;
  switch (packet->getType()) {
  case pktDistillerOutput:
    {
      DistillerReply reply;
      IStream stream(packet->getData(), packet->getLength());
      stream >> Binary(gm_True) >> statusUINT32 >> reply.output.data.length 
	     >> Binary(gm_False);
      if (stream.IsGood()==gm_False) {
	return gm_False;
      }

      reply.status = (DistillerStatus) statusUINT32;
      if (reply.output.data.length > 0) {
	reply.output.data.data = DistillerMalloc(1+reply.output.data.length);
	if (reply.output.data.data==NULL) {
	  Return(gm_False, errOutOfMemory);
	}
	reply.output.data.maxLength = 1+reply.output.data.length;
	
	stream.Extract(reply.output.data.data, reply.output.data.length);
	if (stream.IsGood()==gm_False) {
	  return gm_False;
	}
        ((char*)reply.output.data.data)[reply.output.data.length] = '\0';
      }
      else {
	reply.output.data.data = NULL;
      }

      stream >> Binary(gm_True) >> reply.output.metadata.length
	     >> Binary(gm_False);
      if (reply.output.metadata.length > 0) {
	reply.output.metadata.data = 
	  DistillerMalloc(1+reply.output.metadata.length);
	if (reply.output.metadata.data==NULL) {
	  Return(gm_False, errOutOfMemory);
	}
	reply.output.metadata.maxLength = 1+reply.output.metadata.length;

	stream.Extract(reply.output.metadata.data, 
		       reply.output.metadata.length);
	if (stream.IsGood()==gm_False) {
	  return gm_False;
	}
        ((char*)reply.output.metadata.data)[reply.output.metadata.length] = '\0';
      }
      else {
	reply.output.metadata.data = NULL;
      }


      stream >> reply.output.mimeType;
      if (stream.IsGood()==gm_False) {
	return gm_False;
      }

      char buf[80];
      sprintf(buf, "request-reply id = %lu", packet->getRequestReplyID());
      Mutex::Log_(buf, "", NULL);
      return ((RequestReplyEventSystem*)evs)->SendReplyReceivedEvent
	(packet->getRequestReplyID(), &reply);
    }

  default:
    Return(gm_False, errInvalidPacketType);
  }
}







DistillerRequest::DistillerRequest(CachedDistiller *dist_)
  : RequestReply(CacheManager::getInstance()->getEventSystem(),
		 SECONDS(DistillerRequestTimeout_ms),
		 USECONDS(DistillerRequestTimeout_ms)), 
    dist(dist_), reply(), stopWaiting(gm_False), mutex("*-distreq")
{
}


gm_Bool
DistillerRequest::Send(DistillerConnection *connection, Argument *args, 
		       int numberOfArgs, DistillerInput *input)
{
  OStream stream;
  int idx;


  SetThreadState(THR_DISTILLERSEND);
  // marshall the arguments

  stream << Binary(gm_True) << numberOfArgs;
  if (stream.IsGood()==gm_False) return gm_False;

  for (idx=0; idx < numberOfArgs; idx++) {
    stream << (UINT32) args->type << ARG_ID(*args);
    switch(args->type) {
    case typeInt:       stream << ARG_INT(*args); break;
    case typeString:    stream << ARG_STRING(*args) << '\0'; break;
    case typeDouble:
      {
	char   doubleString[MAX_ARG_STRING];
	double arg_double = ARG_DOUBLE(*args);
	sprintf(doubleString, "%f", arg_double);
	stream << doubleString << '\0'; 
	break;
      }
    }
    args++;

    if (stream.IsGood()==gm_False) return gm_False;
  }


  // marshall the data
  if (input->data.data==NULL) input->data.length = 0;

  stream << Binary(gm_True) << input->data.length << Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;

  if (input->data.length > 0) {
    stream.Append(input->data.data, input->data.length);
    if (stream.IsGood()==gm_False) return gm_False;
  }

  // marshall the metadata
  if (input->metadata.data==NULL) input->metadata.length = 0;

  stream << Binary(gm_True) << input->metadata.length << Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;

  if (input->metadata.length > 0) {
    stream.Append(input->metadata.data, input->metadata.length);
    if (stream.IsGood()==gm_False) return gm_False;
  }

  // finally, the return MIME type (really part of the metadata now...)
  stream << input->mimeType;
  if (stream.IsGood()==gm_False) return gm_False;
 
  gm_Packet packet(pktDistillerInput, stream.getLength(), getID(), 
		stream.getData());
  return connection->Write(&packet);
}

// Returns gm_True if we got a result, gm_False if tv expired (or error).
gm_Bool
DistillerRequest::Wait(struct timeval *tv)
{
  int waitres = 0;
  SetThreadState(THR_DISTILLERWAIT);
  mutex.Lock();
  while (stopWaiting == gm_False && waitres == 0) {
    // we haven't yet got a reply, so wait
    waitres = condition.TimedWait(&mutex, tv);
  }
  mutex.Unlock();
  return waitres ? gm_False : gm_True;
}

gm_Bool
DistillerRequest::SendAndWait(DistillerConnection *connection, Argument *args, 
			      int numberOfArgs, DistillerInput *input)
{
  if (Send(connection, args, numberOfArgs, input)==gm_False) return gm_False;
  char buf[80];
  sprintf(buf, "waiting for reply id = %lu", getID());
  Mutex::Log_(buf, "", NULL);
  SetThreadState(THR_DISTILLERWAIT);
  mutex.Lock();
  while (stopWaiting == gm_False) {
    // we haven't yet got a reply, so wait
    condition.Wait(&mutex);
  }
  mutex.Unlock();
  sprintf(buf, "got reply %d", reply.status);
  Mutex::Log_(buf, "", NULL);
  return gm_True;
}

void
DistillerRequest::Destroy(void)
{
    mutex.Lock();
    getOutput()->Free();
    if (!stopWaiting) {
	reply.status = distFatalError;
	stopWaiting = gm_True;
	dist->RemoveRequest_(this);
	condition.Signal();
    }
    dist->DecrPendingCount();
    dist->DecrReference();
    mutex.Unlock();
}


gm_Bool
DistillerRequest::EvReplyReceived(RequestReplyEventSystem */*evs*/, void *args)
{
  DistillerReply *distReply = (DistillerReply*) args;
  mutex.Lock();
  if (!stopWaiting) {
      reply = *distReply;
      distReply->output.DontFreeMe();
      stopWaiting = gm_True;
      dist->RemoveRequest_(this);
      condition.Signal();
  }
  mutex.Unlock();
  return gm_True;
}


gm_Bool
DistillerRequest::EvTimer(RequestReplyEventSystem */*evs*/)
{
  mutex.Lock();
  if (!stopWaiting) {
      reply.status = distReplyTimeout;
      stopWaiting = gm_True;
      dist->RemoveRequest_(this);
      condition.Signal();
  }
  mutex.Unlock();
  return gm_True;
}


void
DistillerRequest::EvDistillerConnectionBroken()
  // distMutex has already been locked; so don't lock it again
  // 'this' has already been removed from dist->pendingRequests, 
  // so don't remove it
{
  mutex.Lock();
  if (!stopWaiting) {
      reply.status = distConnectionBroken;
      stopWaiting = gm_True;
      condition.Signal();
  }
  mutex.Unlock();
}
