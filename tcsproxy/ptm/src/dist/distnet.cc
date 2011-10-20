#include "defines.h"
#include "log.h"
#include "distiller.h"
#include "packet.h"


gm_Bool
PTMConnection::EvConnectionBroken(EventSystem */*evs*/)
{
  gm_Log("PTM connection lost; trying to listen for new PTM address\n");
  MyDistiller::getInstance()->ClosePTMConnection();
  return MyDistiller::getInstance()->ListenOnSharedBus();
}


gm_Bool
ProxyServer::EvConnectionReceived(EventSystem *evs, int newSocketID)
{
  ProxyConnection *socket;
  if ((socket = new ProxyConnection(newSocketID))==NULL) 
    Return(gm_False, errOutOfMemory);
  if (Error::getStatus()!=success) return gm_False;
  
  gm_Log("Received connection from front end\n");
  return evs->AddCommunicationObject(socket);  
}


/* 
 * Packet format:
 *   "PTM-IP-Address|PTM-Port-Number|.....\0"
 */
class PTMBeaconHandler {
public:
  gm_Bool Handle(gm_Packet *packet, CommunicationObject *object);
protected:
};


gm_Bool
PTMBeaconHandler::Handle(gm_Packet *packet, CommunicationObject */*object*/)
{
  RemoteID ptmAddress;
  UINT32 dummyRandomID;
  IStream stream(packet->getData(), packet->getLength());

  stream >> Delimit('|')  >> ptmAddress >> Skip >> Binary(gm_True) 
	 >> dummyRandomID >> Binary(gm_False) >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;
  if (ptmAddress.port==0) Return(gm_False, errFormattingError);

  return MyDistiller::getInstance()->PTMLocationReceived(ptmAddress);
}


/* 
 * Packet format:
 *   "DistillerSleepStatus\0"
 */
class DistillerSleepHandler {
public:
  gm_Bool Handle(gm_Packet *packet, CommunicationObject *object);
protected:
};


gm_Bool
DistillerSleepHandler::Handle(gm_Packet *packet, 
			      CommunicationObject */*object*/)
{
  UINT32 sleepStatusUINT32;
  IStream stream(packet->getData(), packet->getLength());
  stream >> Binary(gm_True) >> sleepStatusUINT32 >> Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;
  return MyDistiller::getInstance()->Sleep_WakeUp((DistillerSleepStatus)
						  sleepStatusUINT32);
}



#define HANDLE(HandlerObjectType) \
{ \
  HandlerObjectType handler; \
  return handler.Handle(packet, this); \
}


gm_Bool
Listener::EvPacketReceived(EventSystem */*evs*/, gm_Packet *packet)
{
  switch (packet->getType()) {
  case pktPTMBeacon: HANDLE(PTMBeaconHandler);
  default:
    Return(gm_False, errInvalidPacketType);
  }
}


gm_Bool
PTMConnection::EvPacketReceived(EventSystem */*evs*/, gm_Packet *packet)
{
  switch (packet->getType()) {
  case pktDistillerSleep: HANDLE(DistillerSleepHandler);
  default:
    Return(gm_False, errInvalidPacketType);
  }  
}


gm_Bool
ProxyConnection::EvPacketReceived(EventSystem */*evs*/, gm_Packet *packet)
{
  switch (packet->getType()) {
  case pktDistillerInput:
    {
      Work *work;
      if ((work = new Work(this))==NULL) Return(gm_False, errOutOfMemory);
      
      work->replyID = packet->getRequestReplyID();
      if (UnmarshallDistillerInput(work, packet)==gm_False) {
	delete work;
	return gm_False;
      }
      
      if (MyDistiller::getInstance()->WorkReceived(work)==gm_False) {
	delete work;
	return gm_False;
      }
      return gm_True;
    }

  default:
    Return(gm_False, errInvalidPacketType);
  }
}


gm_Bool
ProxyConnection::EvConnectionBroken(EventSystem *evs)
{
  gm_Log("Front end connection lost\n");
  evs->RemoveCommunicationObject(this);

  MarkForDeletion(); // this object will be garbage-collected later on
  return gm_True;
}


gm_Bool
ProxyConnection::UnmarshallDistillerInput(Work *work, gm_Packet *packet)
{
  UINT32   argTypeUINT32;
  Argument *argument;

  IStream stream(packet->getData(), packet->getLength());

  stream >> Binary(gm_True) >> work->numberOfArgs;
  if (stream.IsGood()==gm_False) return gm_False;

  if (work->numberOfArgs > 0) {
    work->args = new Argument [work->numberOfArgs];
    if (work->args==NULL) Return(gm_False, errOutOfMemory);
  }

  for (UINT32 idx=0; idx < work->numberOfArgs; idx++) {
    argument = &work->args[idx];

    stream >> argTypeUINT32 >> argument->id;
    if (stream.IsGood()==gm_False) { 
      delete [] work->args; 
      work->args = NULL; 
      return gm_False; 
    }
    argument->type = (ArgumentType) argTypeUINT32;

    switch(argument->type) {
    case typeInt: 
      {
	stream >> _ARG_INT(*argument);
	if (stream.IsGood()==gm_False) return gm_False;
	break;
      }
    case typeString:
      {
	stream >> SetW(MAX_ARG_STRING)   >> Delimit('\0') 
	       >> _ARG_STRING(*argument) >> Skip;
	if (stream.IsGood()==gm_False) return gm_False;
	break;
      }
    case typeDouble:
      {
	char   doubleString[MAX_ARG_STRING];
	float  arg_double;
	stream >> SetW(MAX_ARG_STRING) >> Delimit('\0') >> doubleString >>Skip;
	if (stream.IsGood()==gm_False) return gm_False;
	sscanf(doubleString, "%f", &arg_double);
	SET_ARG_DOUBLE(*argument, (double)arg_double);
	break;
      }
    default:
      Return(gm_False, errFormattingError);
    }
  }

  /*
   *  Read data block
   */
  stream >> Binary(gm_True) >> work->input.data.length >> Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;

  if (work->input.data.length > 0) {
    work->input.data.data = DistillerMalloc(work->input.data.length);
    if (work->input.data.data==NULL) Return(gm_False, errOutOfMemory);
    work->input.data.maxLength = work->input.data.length;
    stream.Extract(work->input.data.data, work->input.data.length);
    if (stream.IsGood()==gm_False) return gm_False;
  }
  else {
    // no data
    work->input.data.data = NULL;
  }
  /*
   *  Read metadata block
   */
  stream >> Binary(gm_True) >> work->input.metadata.length >> Binary(gm_False);
  if (stream.IsGood() == gm_False) return gm_False;

  if (work->input.metadata.length > 0) {
    work->input.metadata.data = DistillerMalloc(work->input.metadata.length);
    if (work->input.metadata.data == NULL) Return(gm_False, errOutOfMemory);
    work->input.metadata.maxLength = work->input.metadata.length;
    stream.Extract(work->input.metadata.data, work->input.metadata.length);
    if (stream.IsGood() == gm_False) return gm_False;
  } else {
    // no metadata
    work->input.metadata.data = NULL;
  }

  stream >> work->input.mimeType;
  if (stream.IsGood()==gm_False) return gm_False;

  return gm_True;
}


gm_Bool
ProxyConnection::SendReply(DistillerStatus status, DistillerOutput *output,
			   UINT32 replyID)
{
  OStream stream;

#ifdef _REPORT_STATS
  if (status!=distOk) {
    Statistics *stats = MyDistiller::getInstance()->getStats();
    if (stats!=NULL) stats->IncrementBadRequests();
  }
#endif

  if (output->data.data==NULL) output->data.length = 0;

  stream << Binary(gm_True) << (UINT32)status << output->data.length 
	 << Binary(gm_False);
  if (stream.IsGood()==gm_False) {
    return gm_False;
  }

  if (output->data.length > 0) {
    stream.Append(output->data.data, output->data.length);
    if (stream.IsGood()==gm_False) return gm_False;
  }

  // write metadata length followed by metadata
  if (output->metadata.data==NULL) output->metadata.length = 0;

  stream << Binary(gm_True) << output->metadata.length << Binary(gm_False);
  if (output->metadata.length > 0) {
    stream.Append(output->metadata.data, output->metadata.length);
    if (stream.IsGood()==gm_False) return gm_False;
  }

  stream << output->mimeType;
  if (stream.IsGood()==gm_False) return gm_False;
  
  gm_Packet packet(pktDistillerOutput, stream.getLength(), replyID,
		stream.getData());

  if (Write(&packet)==gm_False) {
    // couldn't send over the socket
    // probably means that the socket connection is broken
    Error::SetStatus(success);
    EvConnectionBroken(MyDistiller::getInstance()->getEventSystem());
    return gm_True;
  }

  return gm_True;
}
