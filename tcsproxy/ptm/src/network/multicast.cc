#include <iostream.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifndef INADDR_NONE
#define INADDR_NONE             ((u_long)0xffffffff)    /* -1 return */
#endif

#include "defines.h"
#include "communication.h"
#include "utils.h"


gm_Bool
MulticastSocket::CreateSender(const char *ipAddress, Port port, int ttl)
{
  struct sockaddr_in sin;

  socketID = socket(AF_INET, SOCK_DGRAM, 0);
  if (socketID < 0) {
    socketID = 0;
    Return(gm_False, errSocketCreationError);
  }

  memset((char*)&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port   = htons((u_short)port);

  // use IP addresses only for IP-Multicast
  if ((sin.sin_addr.s_addr = inet_addr(ipAddress))==INADDR_NONE) {
    Close();
    Return(gm_False, errMcastAddressError);
  }

  if (IN_CLASSD(ntohl(sin.sin_addr.s_addr))==0) {
    Close();
    Return(gm_False, errMcastAddressError);
  }
  
  if (connect(socketID, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
    Close();
    Return(gm_False, errSocketConnectError);
  }

  u_char t;

  t = (ttl > 255) ? 255 : (ttl < 0) ? 0 : ttl;
  if (setsockopt(socketID, IPPROTO_IP, IP_MULTICAST_TTL, (char*) &t, 
		 sizeof(t)) < 0) {
    Close();
    Return(gm_False,  errMcastTTLError);
  }

  return gm_True;
}


gm_Bool
MulticastSocket::CreateListener(const char *ipAddress, Port port)
{
  struct sockaddr_in sin;

  socketID = socket(AF_INET, SOCK_DGRAM, 0);
  if (socketID < 0) {
    socketID = 0;
    Return(gm_False, errSocketCreationError);
  }

  int on = 1;
  if (setsockopt(socketID,SOL_SOCKET,SO_REUSEADDR,(char*)&on,sizeof(on))<0) {
    Close();
    Return(gm_False, errSocketCreationError);
  }

  memset((char*)&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port   = htons((u_short)port);

  // use IP addresses only for IP-Multicast
  if ((sin.sin_addr.s_addr = inet_addr(ipAddress))==INADDR_NONE) {
    Close();
    Return(gm_False, errMcastAddressError);
  }

  if (IN_CLASSD(ntohl(sin.sin_addr.s_addr))==0) {
    Close();
    Return(gm_False, errMcastAddressError);
  }

  if (bind(socketID, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketID, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
      Close();
      Return(gm_False, errSocketBindError);
    }
  }


  // This multicast setup shouldn't really be required
  struct ip_mreq mr;
  mr.imr_multiaddr.s_addr = sin.sin_addr.s_addr;
  mr.imr_interface.s_addr = INADDR_ANY;
  if (setsockopt(socketID, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mr, 
		 sizeof(mr)) < 0) {
    Close();
    Return(gm_False, errMcastAddMembershipError);
  }

  return gm_True;
}

int
MulticastSocket::Read(char *data, int size, timeval *tv)
{
  int noOfBytesRead;


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
    while (gm_True) {
      returnValue = select(socketID+1, &readfds, (fd_set*)NULL, (fd_set*)NULL, 
			   &wait);
      if ( !(returnValue<0 && errno==EINTR) ) break;
    }
    gettimeofday(&endTime, NULL);
    diff = tv_timesub(endTime, startTime);
    *tv  = tv_timesub(*tv, diff);
    if (tv->tv_sec < 0 || tv->tv_usec < 0) tv->tv_sec = tv->tv_usec = 0;
    
    // select error
    if (returnValue < 0) {
      Return(gm_False, errSocketReadError);
    }
    // if select times out, then return
    if (returnValue==0 || (tv->tv_sec==0 && tv->tv_usec==0) ) {
      Return(0, errSocketTimeout);
    }
  }
  
  
  noOfBytesRead = read(socketID, data, size);
  if(noOfBytesRead==-1) {
    if(errno==EAGAIN)
      Return(0, errSocketReadNothingAvailable)
    else
      Return(-1, errSocketReadError);
  }
  else if(noOfBytesRead==0)
    Return(-1, errSocketEOF)
  else if(noOfBytesRead < size)
    Return(noOfBytesRead, errSocketReadNotEnoughData)
  else
    return noOfBytesRead;
}

gm_Bool
MulticastSocket::Write(const char *buf, int size)
{
  if (write(socketID, buf, size) < 0)
    Return(gm_False, errSocketWriteError)
  else
    return gm_True;
}


gm_CoordinationBus::gm_CoordinationBus(const char *ipAddress, Port port, 
				       int ttl, gm_Bool listener_,  
				       gm_Bool sender_)
  : sender(), listener(this)
{
  if (listener_ == gm_True) {
    if (listener.CreateListener(ipAddress, port)==gm_False) return;
  }

  if (sender_ == gm_True) {
    if (sender.CreateSender(ipAddress, port, ttl)==gm_False) return;
  }
}
