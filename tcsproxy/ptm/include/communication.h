#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#include "eventhan.h"


#define DefaultServerQLength 32
typedef int Port;
class gm_Packet;


class CommunicationObject : public EventHandler {
public:
  CommunicationObject();
  virtual ~CommunicationObject();

  virtual int getID()=0;
  //virtual int getObjectType()=0; // SOCK_STREAM, SOCK_DGRAM, -1
  virtual gm_Bool Write(const char *data, int size)=0;
  virtual int Read(char *data, int size, timeval *tv)=0;

  virtual gm_Bool Write(gm_Packet *packet)=0;
  virtual gm_Bool Read (gm_Packet *packet, timeval *tv=NULL)=0;

  static  gm_Bool GetIPAddress(const char* host, char* ipAddr);
  static  gm_Bool GetHostIPAddress(char* ipAddr);
  static  gm_Bool GetHostName(char* hostName);

protected:
  gm_Bool HandleEvent(EventSystem *evs, EventType eventType, void *args)=0;
};


class Socket : public CommunicationObject {
public:
  Socket(int sID=0, gm_Bool closeSocketFlag=gm_True) : socketID(sID), 
    destructorClosesSocket(closeSocketFlag), CommunicationObject() { };

  Socket(Socket& socket) : socketID(socket.socketID), 
    destructorClosesSocket(gm_False) { };

  virtual ~Socket();

  virtual void Close();
  virtual gm_Bool Write(const char *data, int size);
  virtual int     Read (char *data, int size, timeval *tv=NULL);

  virtual gm_Bool Write(gm_Packet *packet);
  virtual gm_Bool Read (gm_Packet *packet, timeval *tv=NULL);

  virtual int  getID() { return socketID; };
  //virtual int  getObjectType(); // SOCK_STREAM, SOCK_DGRAM, -1
  virtual Port getLocalPort();
  /*virtual gm_Bool getLocalIP(char *ipAddress);
    virtual gm_Bool getLocalAddress(char *ipAddress, Port &port);*/

protected:
  virtual gm_Bool HandleEvent(EventSystem *evs, EventType event, void *args);
  virtual gm_Bool EvSelect(EventSystem *evs);

  int  socketID;
  gm_Bool destructorClosesSocket;
};


class UdpSocket : public Socket {
public:
  UdpSocket(int _socketID=0, gm_Bool destructorClosesSocket=gm_True)
    : Socket(_socketID, destructorClosesSocket) { };
  
  gm_Bool Connect(const char* host, Port port, gm_Bool useIPAddrOnly=gm_True);
};



class TcpSocket : public Socket {
public:
  TcpSocket(TcpSocket& socket) : Socket(socket) { };
  TcpSocket(int _socketID=0, gm_Bool destructorClosesSocket=gm_True)
    : Socket(_socketID, destructorClosesSocket) { };
  
  gm_Bool Connect(const char* host, Port port, gm_Bool useIPAddrOnly=gm_True);
};


class TcpServer : public TcpSocket {
public:
  TcpServer(Port port, int qLength=DefaultServerQLength);

protected:
  virtual gm_Bool HandleEvent(EventSystem *evs, EventType event, void *args);
  virtual gm_Bool EvSelect(EventSystem *evs);
  virtual gm_Bool EvConnectionReceived(EventSystem *evs, int socketID);

  gm_Bool Initialize(Port port, int qLength);
  int  ConnectToClient();
};


class MulticastSocket : public Socket {
public:
  MulticastSocket(int _socketID=0, gm_Bool destructorClosesSocket=gm_True)
    : Socket(_socketID, destructorClosesSocket) { };
  
  gm_Bool CreateSender  (const char *ipAddress, Port port, int ttl);
  gm_Bool CreateListener(const char *ipAddress, Port port);
  virtual int Read(char *data, int size, timeval *tv=NULL);
  virtual gm_Bool Write(const char *data, int size);
  virtual gm_Bool Write(gm_Packet *packet);
  virtual gm_Bool Read (gm_Packet *packet, timeval *tv=NULL);
};


class gm_CoordinationBus {
public:
  gm_CoordinationBus(const char *ipAddress, Port port, int ttl, 
		  gm_Bool listener_=gm_True, gm_Bool sender_=gm_True);
  virtual ~gm_CoordinationBus() { };

  virtual gm_Bool Write(const char *data, int size) 
  { return sender.Write(data,size); };
  virtual int Read(char *data, int size, timeval *tv=NULL) { 
    return listener.Read(data, size, tv);
  };

  MulticastSocket *getListener() { return &listener; };
  MulticastSocket *getSender()   { return &sender;   };

protected:
  virtual gm_Bool EvSelect(EventSystem */*evs*/) { return gm_True; };
  class gm_CoordinationListener : public MulticastSocket {
  public:
    gm_CoordinationListener(gm_CoordinationBus *b) :MulticastSocket(),bus(b){};

  protected:
    virtual gm_Bool EvSelect(EventSystem *evs) { return bus->EvSelect(evs); }

  private:
    gm_CoordinationBus *bus;
  };

private:
  MulticastSocket sender;
  gm_CoordinationListener listener;
};




#endif // __COMMUNICATION_H__
