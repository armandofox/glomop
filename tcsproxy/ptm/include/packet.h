#ifndef __PACKET_H__
#define __PACKET_H__

#include "defines.h"
#include "eventhan.h"
#define  MAGIC_KEY             0x12345678
#define  evPacketReceived      (evUserDefinedEvent+1)
#define  MAXDGRAMSIZE          1024

class CommunicationObject;
class EventSystem;
class gm_PacketHandler;

struct gm_PacketHeader {
  gm_PacketHeader() 
    : magic(MAGIC_KEY), type(0), length(0), requestReplyID(NoRequestReply) { };
  gm_PacketHeader(UINT32 t, UINT32 l, UINT32 id) 
    : magic(MAGIC_KEY), type(t), length(l), requestReplyID(id) { };
  void HostToNetwork();
  void NetworkToHost();

  UINT32 AddToPacket(char *pktStream);
  UINT32 ExtractFromPacket(const char *pktStream);
  void   Print();

  static UINT32 Size() { return 4*sizeof(UINT32); }

  UINT32 magic;
  UINT32 type;
  UINT32 length;
  UINT32 requestReplyID;
};


class gm_Packet {
public:
  gm_Packet(UINT32 type, UINT32 length, char *_data,gm_Bool isDynamic=gm_False)
    : header(type, length, NoRequestReply), 
    data(_data), 
    isDynamicallyAllocated(isDynamic) { };
  gm_Packet(UINT32 type, UINT32 length, UINT32 requestReplyID, char *_data, 
	    gm_Bool isDynamic=gm_False)
    : header(type, length, requestReplyID), 
    data(_data), 
    isDynamicallyAllocated(isDynamic) { };
  
  gm_Packet() : header(), data(0), isDynamicallyAllocated(gm_False) { };
  ~gm_Packet();

  void Delete();
  void    Print(UINT32 start=0, UINT32 len=0);
  static  void PrintDump(const char* msg, const char *buffer, UINT32 len);

  char*  FreezeData() { // use UnfreezeData() once you are done with the data!
    if(isDynamicallyAllocated==gm_False) return NULL;
    isDynamicallyAllocated = gm_False;
    return data;
  }
  static void UnfreezeData(char *data) {
    delete [] data;
  }

  char*  getData()          { return data; };
  UINT32 getType()          { return header.type; };
  UINT32 getLength()        { return header.length; };
  UINT32 getRequestReplyID(){ return header.requestReplyID; };

protected:
  static void CreateChecksum(char *checksumLocation, const char *buffer,
			     UINT32 bufferLength);
  static gm_Bool VerifyChecksum(const char *checksumLocation, 
				const char *buffer, UINT32 bufferLength);

private:
  gm_PacketHeader header;
  char         *data;
  gm_Bool isDynamicallyAllocated;
  friend class Socket;
  friend class MulticastSocket;
};


class gm_PacketHandler {
protected:
  gm_Bool EvSelect(EventSystem *evs); 
  // EvSelect is not automatically called by anyone - you must explicitly 
  // call it from the EvSelect function of the inherited Socket object

  virtual gm_Bool ReadPacket(gm_Packet *packet)=0;
  virtual gm_Bool EvConnectionBroken(EventSystem */*evs*/) 
  { Return(gm_False, errSocketEOF); };
  virtual gm_Bool EvPacketReceived(EventSystem */*evs*/, gm_Packet */*packet*/)
  { return gm_True; };

  virtual gm_Bool HandleSelectError(EventSystem *evs, gm_Packet *packet);
  virtual void ErrorOutput(const char *string)=0;
};



class Stream;
typedef void (Stream::*ManipFunc)(void *args);

struct Manip {
  Manip(ManipFunc f, void *a) : func(f), args(a) { };
  ManipFunc func;
  void      *args;
};


class Stream
{
public:
  Stream() : isGood(gm_True), binaryMode(gm_False) { };
  Stream& operator << (const Manip manip) 
  { ManipFunc func=manip.func; func(manip.args); return *this; };
  Stream& operator >> (const Manip manip) 
  { ManipFunc func=manip.func; func(manip.args); return *this; };
  
  virtual void Delimit_(void* /*ch*/) { };
  virtual void SetW_(void* /*width*/) { };
  virtual void Skip_(void* /*howMany*/) { };
  void Binary_(void *flag) { binaryMode = (gm_Bool)flag; };

  gm_Bool IsBinary() { return binaryMode; };

  gm_Bool IsGood() { return isGood; };
  void SetGoodFlag(gm_Bool flag) { isGood = flag; };

private:
  gm_Bool   isGood;
  gm_Bool   binaryMode;
};


class IStream : public Stream {

public:
  IStream(const char *d, UINT32 l);
  char     getDelimit() { return delimiter; };
  UINT32   getW() { return stringWidth; };
  //Istream& Binary(gm_Bool flag=gm_True) { binaryMode = flag; return *this; };
  //gm_Bool     IsBinary() { return binaryMode; };

  IStream& operator >> (char *string);  
  IStream& operator >> (int &value);    
  IStream& operator >> (UINT32 &value); 
  IStream& operator >> (INT32 &value); 
  IStream& operator >> (const Manip manip) 
  { *(Stream*)this >> manip; return *this; };

  IStream& Extract(void *extract, UINT32 extractLength);

  gm_Bool IsDone() 
  { return (current!=NULL && data!=NULL && UINT32(current-data) < length)
      ? gm_False : gm_True;};

protected:
  void Delimit_(void* ch)   { if (this!=0) delimiter   = (char)ch;   };
  void SetW_(void* width)   { if (this!=0) stringWidth = (UINT32)width; };
  void Skip_(void* howMany) { if(IsDone()==gm_False)current+=(UINT32)howMany;};
  IStream& InputBinary(void *binaryData, UINT32 numberOfBytes);

private:
  const  char *data;
  UINT32 length, stringWidth;
  const  char *current;
  char   delimiter;
};


class OStream : public Stream {
public:
  OStream(char *buffer, UINT32 len);
  OStream();
  virtual ~OStream() 
  { if (isDynamic==gm_True && data!=NULL) { delete [] data; data=NULL; } };

  OStream& operator << (const char *string);
  OStream& operator << (char ch);
  OStream& operator << (int value);
  OStream& operator << (UINT32 value);
  OStream& operator << (INT32 value);
  OStream& operator << (const Manip manip) 
  { *(Stream*)this  << manip; return *this; };

  OStream& Append(void *append, UINT32 appendLength);
    

  char*    getData()   { return data; };
  UINT32   getLength() { return bytesWritten+1; };

protected:
  OStream& OutputBinary(const void *binaryData, UINT32 numberOfBytes);
  gm_Bool GrowBufferToAtLeast(UINT32 newLength);

private:
  char   *data;
  UINT32 length, bytesWritten;
  gm_Bool   isDynamic;
};


#define Binary(flag) Manip(Stream::Binary_,  (void*)flag)
#define Delimit(ch)  Manip(Stream::Delimit_, (void*)ch)
#define SetW(width)  Manip(Stream::SetW_,    (void*)width)
#define Skip         Manip(Stream::Skip_,    (void*)1)
#define SkipMany(no) Manip(Stream::Skip_,    (void*)no)


#endif // __PACKET_H__

