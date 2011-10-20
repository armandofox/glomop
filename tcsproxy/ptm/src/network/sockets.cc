#include <iostream.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "defines.h"
#include "communication.h"
#include "log.h"
#include "utils.h"


extern "C" {
struct hostent  *gethostbyname_r
        (const char *,           struct hostent *, char *, int, int *h_errnop);
struct protoent *getprotobyname_r
        (const char *, struct protoent *, char *, int);
}


#if !defined(INADDR_NONE)
#define INADDR_NONE             ((u_long)0xffffffff)    /* -1 return */
#endif


CommunicationObject::CommunicationObject()
{
}


CommunicationObject::~CommunicationObject()
{
}


gm_Bool
CommunicationObject::GetIPAddress(const char* host, char* ipAddr)
{
  struct in_addr addr;
  struct hostent he;
  char buffer[1024];
  int errorStatus;

  if ( (gethostbyname_r(host, &he, buffer, sizeof(buffer), &errorStatus))!=0 )
    memcpy ((char*)&addr, he.h_addr, he.h_length);
  else 
    Return(gm_False, errIPAddressError);

  strcpy (ipAddr, inet_ntoa(addr));
  return gm_True;
}


gm_Bool 
CommunicationObject::GetHostIPAddress(char* ipAddr)
{
  struct utsname hostName;

  if (uname(&hostName) < 0) Return(gm_False, errIPAddressError);
  return GetIPAddress(hostName.nodename, ipAddr);
}


gm_Bool 
CommunicationObject::GetHostName(char* hostNameString)
{
  struct utsname hostName;
  if (uname(&hostName) < 0) Return(gm_False, errIPAddressError);
  strcpy(hostNameString, hostName.nodename);
  return gm_True;
}


Socket::~Socket()
{
  if (destructorClosesSocket==gm_True) Close();
}


void
Socket::Close()
{
  if (socketID!=0) {
    close(socketID);
    socketID = 0;
  }
}


/*int
Socket::getObjectType()
{
  int socketType, len=sizeof(int);

  if (socketID==0) return -1;
  if (getsockopt(socketID, SOL_SOCKET, SO_TYPE, (char*)&socketType, &len) < 0) 
    return -1;
  if (socketType!=SOCK_STREAM && socketType!=SOCK_DGRAM) return -1;
  return socketType;
}*/


Port
Socket::getLocalPort()
{
  if (socketID==0) return 0;

  struct sockaddr_in sin;
  int len=sizeof(sin);
  if (getsockname(socketID, (sockaddr*) &sin, &len) < 0) return 0;
  return htons(sin.sin_port);
}


/*int 
Socket::Read(char* buf, int size)
{
  int noOfBytesRead = 0;
  int nextBytes;
  
  while (noOfBytesRead < size) {
    nextBytes = read(socketID, buf+noOfBytesRead, size-noOfBytesRead);

    if(nextBytes < 0) {
      if(errno==EAGAIN) {
	Return(0, errSocketReadNothingAvailable);
      } else if (errno==ECONNRESET || errno==EPIPE || errno==ETIMEDOUT) {
	Return(-1, errSocketEOF);
      }
      else {
        Return(-1, errSocketReadError);
      }
    } else if(nextBytes==0) {
      Return(-1, errSocketEOF);
    }
    // here if nextBytes > 0
    noOfBytesRead += nextBytes;
  }

  return noOfBytesRead;
}*/


int
Socket::Read(char *buf, int size, timeval *tv)
  /*
   * at the end of this function, *tv is updated to reflect the time left
   */
{
  int noOfBytesRead = 0;
  int nextBytes;
  
  while (noOfBytesRead < size) {
    if (tv!=NULL) {
      fd_set readfds, exceptfds;
      timeval wait, startTime, endTime, diff;
      int returnValue;

      FD_ZERO(&readfds);
      FD_ZERO(&exceptfds);
      FD_SET(socketID, &readfds);
      FD_SET(socketID, &exceptfds);

      gettimeofday(&startTime, NULL);
      wait = *tv;
      returnValue = select(socketID+1, &readfds, (fd_set*)NULL, &exceptfds,
			   &wait);
      gettimeofday(&endTime, NULL);
      diff = tv_timesub(endTime, startTime);
      *tv  = tv_timesub(*tv, diff);
      if (tv->tv_sec < 0 || tv->tv_usec < 0) tv->tv_sec = tv->tv_usec = 0;

      // select error
      if (returnValue < 0) {
	if (errno==EINTR) continue;
	Return(gm_False, errSocketReadError);
      }
      // if select times out, then return
      if (returnValue==0 || (tv->tv_sec==0 && tv->tv_usec==0) ) {
	Return(0, errSocketTimeout);
      }
    }

    nextBytes = read(socketID, buf+noOfBytesRead, size-noOfBytesRead);
    if(nextBytes < 0) {
      if(errno==EAGAIN) {
	Return(0, errSocketReadNothingAvailable);
      } else if (errno==ECONNRESET || errno==EPIPE || errno==ETIMEDOUT) {
	Return(-1, errSocketEOF);
      }
      else {
        Return(-1, errSocketReadError);
      }
    } else if(nextBytes==0) {
      Return(-1, errSocketEOF);
    }
    // here if nextBytes > 0
    noOfBytesRead += nextBytes;
  }

  return noOfBytesRead;
}


gm_Bool
Socket::Write(const char* buf, int size)
{
  int writtenSoFar = 0, wrote;

  while (writtenSoFar < size) {
    if ((wrote = write(socketID, buf+writtenSoFar, size-writtenSoFar)) < 0) {
      if (errno==EPIPE || errno==ETIMEDOUT) {
	Return(gm_False, errSocketEOF);
      }
      Return(gm_False, errSocketWriteError);
    }
    writtenSoFar += wrote;
  }
  return gm_True;
}


gm_Bool
Socket::HandleEvent(EventSystem *evs, EventType eventType, void */*args*/)
{
  switch (eventType) {
  case evSelect:
    return EvSelect(evs);

  default:
    Return(gm_False, errEvSysInvalidEvent);
  }
}


gm_Bool
Socket::EvSelect(EventSystem */*evs*/)
{
  return gm_True;
}


gm_Bool
UdpSocket::Connect(const char* host, Port port, gm_Bool useIPAddrOnly)
{
  struct hostent he;		// name of host to which connection is desired
  struct sockaddr_in sin;	// an internet endpoint address
  int    errorStatus;
  char   buffer[1024];

  memset((char*)&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;

  sin.sin_port = htons((u_short) port);

  // Map host name to IP address
  if (useIPAddrOnly==gm_True) {
    if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
      Return(gm_False, errUdpAddressError);
  }
  else {
    if (gethostbyname_r(host, &he, buffer, sizeof(buffer), &errorStatus)!=0)
      memcpy((char*)&sin.sin_addr, he.h_addr, he.h_length);
    else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
      Return(gm_False, errUdpAddressError);
  }

  // Allocate a socket
  socketID = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP); 
  if (socketID < 0) {
    socketID = 0;
    Return(gm_False, errSocketCreationError);
  }

  if (connect(socketID, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
    Close();
    Return(gm_False, errSocketConnectError);
  }

  return gm_True;
}


gm_Bool
TcpSocket::Connect(const char* host, Port port, gm_Bool useIPAddrOnly)
{
  struct hostent he;		// name of host to which connection is desired
  struct sockaddr_in sin;	// an internet endpoint address
  int    type, errorStatus;
  char   buffer[1024];

  memset((char*)&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;

  sin.sin_port = htons((u_short) port);

  // Map host name to IP address
  if (useIPAddrOnly==gm_True) {
    if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
      Return(gm_False, errTcpAddressError);
  }
  else {
    if (gethostbyname_r(host, &he, buffer, sizeof(buffer), &errorStatus)!=0)
      memcpy((char*)&sin.sin_addr, he.h_addr, he.h_length);
    else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
      Return(gm_False, errTcpAddressError);
  }

  // Map protocol name to protocol number
  /*if (getprotobyname_r("tcp", &pe, buffer, sizeof(buffer))==0)
    Return(gm_False, errTcpAddressError);*/

  type = SOCK_STREAM;		// for TCP PROTOCOL

  // Allocate a socket
  socketID = socket(PF_INET, type, IPPROTO_TCP); //pe.p_proto);
  if (socketID < 0) {
    socketID = 0;
    Return(gm_False, errSocketCreationError);
  }

  if (connect(socketID, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
    Close();
    Return(gm_False, errSocketConnectError);
  }

  return gm_True;
}


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
TcpServer::EvConnectionReceived(EventSystem */*evs*/, int /*socketID*/)
{
  return gm_True;
}


gm_Bool
TcpServer::Initialize(Port port, int qLength)
{
  //struct protoent *ppe;	// pointer to protocol information entry
  struct sockaddr_in sin;	// an Internet endpoint address
  int    type;

  memset((char*) &sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;

  sin.sin_port = htons((u_short) port);

  /*if ((ppe = getprotobyname("tcp")) == NULL)
    Return(gm_False, errTcpProtocolNameError);*/

  type = SOCK_STREAM;		// for TCP PROTOCOL
  socketID = socket(PF_INET, type, IPPROTO_TCP);
  if (socketID < 0)
    Return(gm_False, errSocketCreationError);

  if (bind (socketID, (struct sockaddr*)&sin, sizeof(sin)) < 0) 
    Return(gm_False, errSocketBindError);

  int on=1;
  if (setsockopt(socketID,SOL_SOCKET,SO_REUSEADDR,(char*)&on,sizeof(on))<0) {
    Return(gm_False, errSocketCreationError);
  }

  if (listen(socketID, qLength) < 0)
    Return(gm_False, errSocketListenError);
    
  return gm_True;
}

