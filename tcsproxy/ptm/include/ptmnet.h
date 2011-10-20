#ifndef __PTMNET_H__
#define __PTMNET_H__

#include "communication.h"
#include "ptmdb.h"
#include "log.h"



class PrivateConnection : public TcpSocket, public gm_PacketHandler {
public:
  PrivateConnection(int socketID);
  ~PrivateConnection();
  void SetDistiller(DistillerRecord *r) { record = r;    }
  DistillerRecord *getDistiller()       { return record; }
  gm_Bool IsDistillerConnection() { return (record==NULL) ? gm_False:gm_True;}

  gm_Bool EvConnectionBroken(EventSystem *evs);
  gm_Bool AskForDistillerReply(UINT32 replyID, AskForDistillerStatus status,
			       List<DatabaseRecord> *list);
  gm_Bool Beacon(RemoteID &ptmAddress, UINT32 ptmRandomID, 
		 List<DatabaseRecord> *list);
protected:
  gm_Bool EvSelect(EventSystem *evs) 
  { return gm_PacketHandler::EvSelect(evs); };
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { return Read(packet); };
  void ErrorOutput(const char *string) { gm_Log(string); };

  DistillerRecord *record;	// will be NULL for non-distiller sockets
};


class PrivateConnectionServer : public TcpServer {
public:
  PrivateConnectionServer() : TcpServer(0) { };
  // let the system pick a port number
protected:
  gm_Bool EvConnectionReceived(EventSystem *evs, int socketID);
};


class SharedBus : public gm_CoordinationBus, public gm_PacketHandler { 
//MulticastSocket {
public:
  SharedBus(RemoteID &rid, int ttl=DefaultTTL);
  gm_Bool Beacon(RemoteID &ptmAddress, UINT32 ptmRandomID, 
		 List<DatabaseRecord> *list);
protected:
  gm_Bool EvSelect(EventSystem *evs) {return gm_PacketHandler::EvSelect(evs);};
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { return getListener()->Read(packet);};
  void ErrorOutput(const char *string) { gm_Log(string); };

};


#endif // __PTMNET_H__
