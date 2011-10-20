#include <iostream.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "sockets.h"
#ifdef SUNOS
#include <strings.h>
#endif


TcpServer::TcpServer(Port port, int qLength)
{
  Initialize(port, qLength);
}


int
TcpServer::ConnectToClient()
{
  int newSocketID, alen;
  struct sockaddr_in fsin;

  alen = sizeof(fsin);
  newSocketID = accept(socketID, (struct sockaddr*) &fsin, &alen);
  return newSocketID;
}


gm_Bool
TcpServer::HandleEvent(EventSystem *evs, EventType eventType, void *args)
{
  switch (eventType) {
  case evSelect:
    return EvSelect(evs);

  case evConnectionReceived:
    return EvConnectionReceived(evs, (int)args);
 
  default:
    return TcpSocket::HandleEvent(evs, eventType, args);
  }
}


gm_Bool
TcpServer::EvSelect(EventSystem *evs)
{
  int newSocketID = ConnectToClient();
  return evs->PostEvent(evConnectionReceived, this, (void*)newSocketID);
}


gm_Bool
TcpServer::EvConnectionReceived(EventSystem *evs, int socketID)
{
  return gm_True;
}


gm_Bool
TcpServer::Initialize(Port port, int qLength)
{
  struct servent *pse;	// pointer to service info entry
  struct protoent *ppe;	// pointer to protocol information entry
  struct sockaddr_in sin;	// an Internet endpoint address
  int    type;

  bzero ((char*) &sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;

  sin.sin_port = htons((u_short) port);

  if ((ppe = getprotobyname("tcp")) == NULL)
    gm_BoolErrorReturn(errTcpProtocolNameError);

  type = SOCK_STREAM;		// for TCP PROTOCOL
  socketID = socket(PF_INET, type, ppe->p_proto);
  if (socketID < 0)
    gm_BoolErrorReturn(errTcpSocketCreationError);

  if (bind (socketID, (struct sockaddr*)&sin, sizeof(sin)) < 0)
    gm_BoolErrorReturn(errTcpBindError);
  if (listen(socketID, qLength) < 0)
    gm_BoolErrorReturn(errTcpListenError);
    
  gm_BoolErrorReturn(success);
}

