#include "log.h"
#include "ptm.h"
#include "distlaunch.h"
#include <stdlib.h>
#include <netinet/in.h>


gm_Bool
PrivateConnectionServer::EvConnectionReceived(EventSystem *evs, 
					      int newSocketID)
{
  PrivateConnection *socket;
  if ((socket = new PrivateConnection(newSocketID))==NULL) 
    Return(gm_False, errOutOfMemory);
  if (Error::getStatus()!=success) return gm_False;
  
  gm_Log("New connection received (socket-id " << newSocketID << ")\n");
  return evs->AddCommunicationObject(socket);
}


/*gm_Bool
PrivateConnection::EvSelect(EventSystem *evs)
{
  gm_Packet packet;
  if (packet.ReceiveAndHandle(evs, (gm_PacketHandler*)
			      PTM::getInstance()->getPacketHandler(), 
			      this) == gm_False) {
    packet.HandleIgnorableErrors(Log);

    switch(Error::getStatus()) {
    case success:
      return gm_True;
      
    case errSocketReadEOF:
      Error::SetStatus(success);
      return EvConnectionBroken(evs);
      
    default:
      return gm_False;
    }
  }
  else {
    return gm_True;
  }
}*/



PrivateConnection::PrivateConnection(int socketID)
  : TcpSocket(socketID), gm_PacketHandler(), record(NULL) 
{
  if (PTM::getInstance()->AddConnection(this)==gm_False) return;
}


PrivateConnection::~PrivateConnection() 
{
  PTM::getInstance()->RemoveConnection(this);
}


gm_Bool
PrivateConnection::EvConnectionBroken(EventSystem *evs)
{
  gm_Log("Connection broken on socket-id " << getID() << '\n');

  if (IsDistillerConnection()==gm_True) {
    // this is a distiller; deregister it

    DistillerRecord *distillerRecord = getDistiller();
    PTM::getInstance()->Deregister(distillerRecord);
    distillerRecord->EvUnnaturalDeath();
    delete distillerRecord;
    SetDistiller(NULL);
  }
  else {
    // this is a proxy socket; check if it has any pending distiller launches 
    // associated with it

    PTM::getInstance()->getDistillerLauncher()->RemoveBrokenConnection(this);
  }

  evs->RemoveCommunicationObject(this);
  delete this;
  return gm_True;
}


SharedBus::SharedBus(RemoteID &multicast, int ttl)
  : gm_CoordinationBus(multicast.ipAddress, multicast.port, ttl, gm_True, 
		       gm_True)
{
  /*if (CreateSender(multicast.ipAddress, multicast.port, ttl)==gm_False)
    return;*/
}


/* 
 * Packet format:
 *   "IP-Address|Port-No|Distiller-type\0"
 */
class RegisterDistillerHandler {
public:
  gm_Bool Handle(gm_Packet *packet, PrivateConnection *connection);
protected:
};


gm_Bool
RegisterDistillerHandler::Handle(gm_Packet *packet, 
				 PrivateConnection *connection)
{
  /*char ipAddress[MAXIP], distillerTypeString[MAXDISTILLER_TYPE_STRING];
  Port port;*/
  RemoteID distAddress;
  DistillerType distillerType;
  UINT32 sleepStatusUINT32, restartFlagUINT32;
  
  DistillerRecord *distillerRecord;

  if (connection->IsDistillerConnection()==gm_True) {
    // someone's already registered on this connection!
    Return(gm_False, errOutOfContextPacket);
  }

  IStream stream(packet->getData(), packet->getLength());

  stream >> Delimit('|') >> distAddress >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;
  if (distAddress.port==0) Return(gm_False, errFormattingError);

  stream >> Delimit('|') >> distillerType >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;

  stream >> Delimit('|') >> sleepStatusUINT32 >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;

  stream >> Delimit('\0') >> restartFlagUINT32 >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;

  if (stream.IsDone()==gm_False) Return(gm_False, errFormattingError);

  if ((distillerRecord = new DistillerRecord(connection, distAddress, 
					     distillerType,
					     (DistillerSleepStatus)
					     sleepStatusUINT32,
					     (gm_Bool)restartFlagUINT32
					     ))==NULL)
    Return(gm_False, errOutOfMemory);

  if (PTM::getInstance()->Register(distillerRecord)==gm_False) {
    if (Error::getStatus()==errDuplicateIndexEntry) {
      delete distillerRecord;

      // remove the connection as well!
      connection->EvConnectionBroken(PTM::getInstance()->getEventSystem());
      Error::SetStatus(success);
      return gm_True;
    }
    else return gm_False;
  }

  if (PTM::getInstance()->getEventSystem()->SendReplyReceivedEvent
      (packet->getRequestReplyID(), distillerRecord)==gm_False) {
    return gm_False;
  }

  connection->SetDistiller(distillerRecord);
  return gm_True;
}


/* 
 * Packet format:
 *   ""
 */
class DeregisterDistillerHandler {
public:
  gm_Bool Handle(gm_Packet *packet, PrivateConnection *connection);
protected:
};


gm_Bool 
DeregisterDistillerHandler::Handle(gm_Packet *packet, 
				   PrivateConnection *connection)
{
  if (connection->IsDistillerConnection()==gm_False) {
    // there is no distiller to deregister!
    Return(gm_False, errOutOfContextPacket);
  }
  
  // parse the packet
  if (packet->getLength() > 0) Return(gm_False, errFormattingError);

  // deregister the distiller
  PTM::getInstance()->Deregister(connection->getDistiller());
  delete connection->getDistiller();
  connection->SetDistiller(NULL);
  return gm_True;
}


/* 
 * Packet format:
 *   "Distiller-type\0"
 */
class ReregisterDistillerHandler {
public:
  gm_Bool Handle(gm_Packet *packet, PrivateConnection *connection);
protected:
};


gm_Bool
ReregisterDistillerHandler::Handle(gm_Packet *packet, 
				   PrivateConnection *connection)
{
  DistillerType distillerType;
  DistillerRecord *distillerRecord;

  if (connection->IsDistillerConnection()==gm_False) {
    // there is no distiller to reregister!
    Return(gm_False, errOutOfContextPacket);
  }
  
  // parse the packet
  IStream stream(packet->getData(), packet->getLength());

  stream >> Delimit('\0') >> distillerType >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;

  if (stream.IsDone()==gm_False) Return(gm_False, errFormattingError);

  // reregister the distiller
  distillerRecord = connection->getDistiller();
  PTM::getInstance()->Deregister(distillerRecord);
  distillerRecord->type = distillerType;
  if (PTM::getInstance()->Register(distillerRecord)==gm_False) {
    if (Error::getStatus()==errDuplicateIndexEntry) {
      delete distillerRecord;

      // remove the connection as well!
      connection->EvConnectionBroken(PTM::getInstance()->getEventSystem());
      Error::SetStatus(success);
      return gm_True;
    }
    else return gm_False;
  }

  connection->SetDistiller(distillerRecord);
  return gm_True;
}

/* 
 * Packet format:
 *   "Distiller-type\0"
 */
class AskForDistillerHandler {
public:
  gm_Bool Handle(gm_Packet *packet, PrivateConnection *connection);
protected:
};


gm_Bool
AskForDistillerHandler::Handle(gm_Packet *packet, 
			       PrivateConnection *connection)
{
  char type[MAXDISTILLER_TYPE_STRING];

  if (connection->IsDistillerConnection()==gm_True) {
    // a distiller can't ask for another distiller
    Return(gm_False, errOutOfContextPacket);
  }

  // parse the packet
  if (packet->getLength() <= 0) Return(gm_False, errFormattingError);

  IStream stream(packet->getData(), packet->getLength());
  stream >> Delimit('\0') >> SetW(MAXDISTILLER_TYPE_STRING) >> type >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;
  if (stream.IsDone()==gm_False) Return(gm_False, errFormattingError);

  DistillerType distillerType(type);
  return PTM::getInstance()->EvAskForDistiller(&distillerType, connection,
					       packet->getRequestReplyID());
}


class AskForUnicastBeaconHandler {
public:
  gm_Bool Handle(gm_Packet *packet, PrivateConnection *connection);
protected:
};


gm_Bool
AskForUnicastBeaconHandler::Handle(gm_Packet */*packet*/, 
				   PrivateConnection *connection)
{
  PTM::getInstance()->Beacon(connection); // ignore return value
  return gm_True;
}


/* 
 * Packet format:
 *   "<Load>\0"
 */
class DistillerLoadHandler {
public:
  gm_Bool Handle(gm_Packet *packet, PrivateConnection *connection);
protected:
};


gm_Bool
DistillerLoadHandler::Handle(gm_Packet *packet, PrivateConnection *connection)
{
  Load load;
  //MonitorClient *monitorClient;
  IStream stream(packet->getData(), packet->getLength());

  if (connection->IsDistillerConnection()==gm_False) {
    // only a distiller can report load information
    Return(gm_False, errOutOfContextPacket);
  }

  stream >> Delimit('\0') >> load >> Skip;
  if (stream.IsGood()==gm_False) return gm_False;
  if (stream.IsDone()==gm_False) Return(gm_False, errFormattingError);

  connection->getDistiller()->Update(load);

  /*monitorClient = PTM::getInstance()->getMonitorClient();
  if (monitorClient!=NULL) {
    char value[MAXLINE];
    sprintf(value, "%s (%s/%d)=%lu,blue", 
	    (char*) connection->getDistiller()->type,
	    connection->getDistiller()->rid.ipAddress,
	    connection->getDistiller()->rid.port, load.totalLatency);
    monitorClient->Send("Distiller Load", value, "TimeChart");
  }*/

  return gm_True;
}


#define HANDLE(HandlerObjectType) \
{ \
  HandlerObjectType handler; \
  return handler.Handle(packet, this); \
}


gm_Bool
PrivateConnection::EvPacketReceived(EventSystem */*evs*/, gm_Packet *packet)
{
  switch (packet->getType()) {
  case pktRegisterDistiller:   HANDLE(RegisterDistillerHandler);
  case pktDeregisterDistiller: HANDLE(DeregisterDistillerHandler);
  case pktReregisterDistiller: HANDLE(ReregisterDistillerHandler);
  case pktAskForDistiller:     HANDLE(AskForDistillerHandler);
  case pktAskForUnicastBeacon: HANDLE(AskForUnicastBeaconHandler);
  case pktDistillerLoad:       HANDLE(DistillerLoadHandler);
  default:
    Return(gm_False, errInvalidPacketType);
  }
}


static gm_Bool
ConstructDistillerList(OStream &stream, List<DatabaseRecord> *list)
{
  UINT32  distillersPos; 
  UINT32 numberOfDistillers=0;
  ListIndex idx;

  distillersPos = stream.getLength() - 1; // length includes trailing '\0'

  /*if (list!=NULL) {
    numberOfDistillers = list->getSize();
  }*/

  stream << Binary(gm_True) << htonl(numberOfDistillers) << Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;

  if (list!=NULL) {
    idx = list->BeginTraversal();
    for (; list->IsDone(idx)==gm_False; idx = list->getNext(idx)) {
      if (((DistillerRecord*)(list->getData(idx)))->sleepStatus!=distWakeUp) {
	continue;
      }

      stream << *((DistillerRecord*)list->getData(idx)) << '|';
      if (stream.IsGood()==gm_False) return gm_False;
      numberOfDistillers++;
    }
    list->EndTraversal();
  }

  char *data = stream.getData();
  numberOfDistillers = htonl(numberOfDistillers);
  memcpy(data + distillersPos, (char*) &numberOfDistillers, sizeof(UINT32));
  return gm_True;
}


gm_Bool
PrivateConnection::AskForDistillerReply(UINT32 replyID, 
					AskForDistillerStatus status,
					List<DatabaseRecord> *list)
{
  OStream stream;

  stream << (UINT32) status << '|';
  if (stream.IsGood()==gm_False) return gm_False;
  if (ConstructDistillerList(stream, list)==gm_False) return gm_False;
  gm_Packet packet(pktAskForDistillerReply, stream.getLength(), replyID, 
		   stream.getData());
  Write(&packet); // could not write to the socket; ignore it for now!
  return gm_True;
}


static gm_Bool
SendBeacon_(CommunicationObject *object, RemoteID &ptmAddress, 
	    UINT32 ptmRandomID, List<DatabaseRecord> *list)
{
  OStream stream;

  stream << ptmAddress << '|'  << Binary(gm_True) << ptmRandomID 
	 << (Binary(gm_False)) << '|';
  if (stream.IsGood()==gm_False) return gm_False;

  if (ConstructDistillerList(stream, list)==gm_False) 
    return gm_False;

  gm_Packet packet(pktPTMBeacon, stream.getLength(), stream.getData());
  if (object->Write(&packet)==gm_False) return gm_False;

  return gm_True;
}



gm_Bool
PrivateConnection::Beacon(RemoteID &ptmAddress, 
			  UINT32 ptmRandomID,
			  List<DatabaseRecord> *list)
{
  return SendBeacon_(this, ptmAddress, ptmRandomID, list);
}


gm_Bool
SharedBus::Beacon(RemoteID &ptmAddress, 
		  UINT32 ptmRandomID,
		  List<DatabaseRecord> *list)
{
  /*OStream stream;

  stream << ptmAddress << '|'  << Binary(gm_True) << ptmRandomID 
	 << (Binary(gm_False)) << '|';
  if (stream.IsGood()==gm_False) return gm_False;

  if (ConstructDistillerList(stream, list)==gm_False) 
    return gm_False;

  gm_Packet packet(pktPTMBeacon, stream.getLength(), stream.getData());
  if (getSender()->Write(&packet)==gm_False) return gm_False;

  return gm_True;*/
  return SendBeacon_(getSender(), ptmAddress, ptmRandomID, list);
}



class PTMBeaconHandler {
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
  UINT32 randomID;

  IStream stream(packet->getData(), packet->getLength());

  stream >> Delimit('|') >> rid >> Skip >> Binary(gm_True) >> randomID 
	 >> Binary(gm_False);
  if (stream.IsGood()==gm_False || rid.port==0) return gm_False;

  // ignore the rest of the stream

  switch (PTM::getInstance()->CompareBeacon(rid, randomID)) {
  case -1:
    // I'm the inferior PTM, so I must die
    gm_Log("Found another PTM at " << rid 
	   << "; so I'm going to commit suicide\n");
    PTM::getInstance()->getEventSystem()->PostEvent(evQuit, 0, 0);
    return gm_True;

  default:
    // I'm the superior beacon (or it's my beacon), so ignore it
    return gm_True;

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
  default:
    Return(gm_False, errInvalidPacketType);
  }  
}


