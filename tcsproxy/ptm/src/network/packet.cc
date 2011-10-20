#include "packet.h"
#include "communication.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream.h>
#include <stdio.h>
#include <errno.h>


const int checksumSize = 4; // 4 bytes

void
gm_PacketHeader::HostToNetwork()
{
  magic  = htonl(magic);
  type   = htonl(type);
  length = htonl(length);
  requestReplyID = htonl(requestReplyID);
}


void
gm_PacketHeader::NetworkToHost()
{
  magic  = ntohl(magic);
  type   = ntohl(type);
  length = ntohl(length);
  requestReplyID = ntohl(requestReplyID);
}


UINT32
gm_PacketHeader::AddToPacket(char *pktStream)
{
  UINT32 bytesAdded;
  gm_PacketHeader tmpHeader = *this;
  tmpHeader.HostToNetwork();

  bytesAdded = 0;
  memcpy(pktStream+bytesAdded, (char*) &tmpHeader.magic,  sizeof(UINT32));
  bytesAdded += sizeof(UINT32);

  memcpy(pktStream+bytesAdded, (char*) &tmpHeader.type,   sizeof(UINT32));
  bytesAdded += sizeof(UINT32);

  memcpy(pktStream+bytesAdded, (char*) &tmpHeader.length, sizeof(UINT32));
  bytesAdded += sizeof(UINT32);

  memcpy(pktStream+bytesAdded, (char*) &tmpHeader.requestReplyID, 
	 sizeof(UINT32));
  bytesAdded += sizeof(UINT32);

  return bytesAdded;
}


UINT32
gm_PacketHeader::ExtractFromPacket(const char *pktStream)
{
  UINT32 bytesRead=0;

  memcpy((char*) &magic,  pktStream+bytesRead, sizeof(UINT32));
  bytesRead += sizeof(UINT32);

  memcpy((char*) &type,   pktStream+bytesRead, sizeof(UINT32));
  bytesRead += sizeof(UINT32);

  memcpy((char*) &length, pktStream+bytesRead, sizeof(UINT32));
  bytesRead += sizeof(UINT32);

  memcpy((char*) &requestReplyID, pktStream+bytesRead, sizeof(UINT32));
  bytesRead += sizeof(UINT32);

  NetworkToHost();
  return bytesRead;
}


gm_Packet::~gm_Packet()
{
  if (isDynamicallyAllocated==gm_True && data!=NULL) {
    delete [] data;
    data = NULL;
  }
}


/*gm_Bool
Socket::Write(gm_Packet *pkt)
{
  int  objectType, bytes;
  char *sendBuffer;

  if (pkt->header.length < 0 || (pkt->header.length > 0 && pkt->data==NULL))
    Return(gm_False, errSocketWriteError);

  if ((objectType=getObjectType())==-1) 
    Return(gm_False, errSocketWriteError);

  if (objectType==SOCK_STREAM) {
    sendBuffer = new char [gm_PacketHeader::Size()];
    if (sendBuffer==NULL) Return(gm_False, errOutOfMemory);

    bytes = pkt->header.AddToPacket(sendBuffer);
    if (Write(sendBuffer, bytes)==gm_False) {
      delete [] sendBuffer;
      return gm_False;
    }

    delete [] sendBuffer;

    if (pkt->header.length== 0) return gm_True;
    if (Write(pkt->data, pkt->header.length)==gm_False) return gm_False;
    return gm_True;
  }
  else {
    if (checksumSize + gm_PacketHeader::Size() + pkt->header.length>MAXDGRAMSIZE)
      Return(gm_False, errTooLargeDatagram);

    sendBuffer = new char[checksumSize + gm_PacketHeader::Size() 
			 + pkt->header.length];
    if (sendBuffer==NULL) Return(gm_False, errOutOfMemory);

    bytes = checksumSize; // skip the first few bytes for the checksum

    bytes += pkt->header.AddToPacket(sendBuffer+bytes);
    memcpy(sendBuffer+bytes, pkt->data, pkt->header.length);
    gm_Packet::CreateChecksum(sendBuffer, sendBuffer + checksumSize, 
			   bytes+pkt->header.length-checksumSize);
    if (Write(sendBuffer, bytes+pkt->header.length)==gm_False) {
      delete [] sendBuffer;
      return gm_False;
    }

    delete [] sendBuffer;
    return gm_True;
  }
}


gm_Bool
Socket::Read(gm_Packet *pkt, timeval *tv)
{
  int objectType, dataRead;
  UINT32 bytesExtracted;

  if ((objectType=getObjectType())==-1) 
    Return(gm_False, errSocketReadError);

  if (objectType==SOCK_STREAM) {
    char *buffer = new char [gm_PacketHeader::Size()];
    if (buffer==NULL) Return(gm_False, errOutOfMemory);
    
    dataRead = Read(buffer, gm_PacketHeader::Size(), tv);
    if (dataRead < 0) { delete [] buffer; return gm_False; }

    pkt->header.ExtractFromPacket(buffer);
    delete [] buffer;

    if (pkt->header.magic!=MAGIC_KEY) Return(gm_False, errInvalidMagicKey);
    if (pkt->header.length < 0) Return(gm_False, errSocketReadError);

    if (pkt->data!=NULL && pkt->isDynamicallyAllocated==gm_True) {
      delete [] pkt->data;
      pkt->data = NULL;
    }

    if (pkt->header.length==0) {
      pkt->data = NULL;
      return gm_True;
    }

    pkt->data = new char[pkt->header.length];
    if (pkt->data==NULL) Return(gm_False, errOutOfMemory);
    pkt->isDynamicallyAllocated = gm_True;

    dataRead = Read(pkt->data, pkt->header.length, tv);

    if (dataRead < 0) return gm_False;
    if (UINT32(dataRead) < pkt->header.length) {
      Return(gm_False, errSocketReadNotEnoughData);
    }
    return gm_True;
  }
  else {
    char buffer[MAXDGRAMSIZE];
    dataRead = Read(buffer, MAXDGRAMSIZE, tv);

    if (dataRead < 0) return gm_False;
    Error::SetStatus(success);

    if (UINT32(dataRead) < checksumSize + gm_PacketHeader::Size()) 
      Return(gm_False, errSocketReadNotEnoughData);

    if (gm_Packet::VerifyChecksum(buffer, buffer+checksumSize, 
			       dataRead-checksumSize)!=gm_True)
      Return(gm_False, errChecksumError);
    bytesExtracted  = checksumSize;
    bytesExtracted += pkt->header.ExtractFromPacket(buffer+bytesExtracted);
    if (pkt->header.magic!=MAGIC_KEY) Return(gm_False, errInvalidMagicKey);
    if (pkt->header.length > UINT32(dataRead-bytesExtracted))
      Return(gm_False, errSocketReadNotEnoughData);
    if (pkt->header.length < UINT32(dataRead-bytesExtracted))
      Return(gm_False, errFormattingError);

    if (pkt->data!=NULL && pkt->isDynamicallyAllocated==gm_True) {
      delete [] pkt->data;
      pkt->data = NULL;
    }

    if (pkt->header.length==0) {
      pkt->data = NULL;
      return gm_True;
    }

    pkt->data = new char[pkt->header.length];
    if (pkt->data==NULL) Return(gm_False, errOutOfMemory);
    pkt->isDynamicallyAllocated = gm_True;

    memcpy(pkt->data, buffer+bytesExtracted, pkt->header.length);
    return gm_True;
  }
}*/


gm_Bool
Socket::Write(gm_Packet *pkt)
{
  int  bytes;
  char *sendBuffer;

  if (pkt->header.length < 0 || (pkt->header.length > 0 && pkt->data==NULL))
    Return(gm_False, errSocketWriteError);

  sendBuffer = new char [gm_PacketHeader::Size()];
  if (sendBuffer==NULL) Return(gm_False, errOutOfMemory);
  
  bytes = pkt->header.AddToPacket(sendBuffer);
  if (Write(sendBuffer, bytes)==gm_False) {
    delete [] sendBuffer;
    return gm_False;
  }
  
  delete [] sendBuffer;
  
  if (pkt->header.length== 0) return gm_True;
  if (Write(pkt->data, pkt->header.length)==gm_False) return gm_False;
  return gm_True;
}


gm_Bool
MulticastSocket::Write(gm_Packet *pkt)
{
  int  bytes;
  char *sendBuffer;

  if (pkt->header.length < 0 || (pkt->header.length > 0 && pkt->data==NULL))
    Return(gm_False, errSocketWriteError);

  if (checksumSize + gm_PacketHeader::Size() + pkt->header.length>MAXDGRAMSIZE)
    Return(gm_False, errTooLargeDatagram);
  
  sendBuffer = new char[checksumSize + gm_PacketHeader::Size() 
		       + pkt->header.length];
  if (sendBuffer==NULL) Return(gm_False, errOutOfMemory);
  
  bytes = checksumSize; // skip the first few bytes for the checksum
  
  bytes += pkt->header.AddToPacket(sendBuffer+bytes);
  memcpy(sendBuffer+bytes, pkt->data, pkt->header.length);
  gm_Packet::CreateChecksum(sendBuffer, sendBuffer + checksumSize, 
			 bytes+pkt->header.length-checksumSize);
  if (Write(sendBuffer, bytes+pkt->header.length)==gm_False) {
    delete [] sendBuffer;
    return gm_False;
  }
  
  delete [] sendBuffer;
  return gm_True;
}


gm_Bool
Socket::Read(gm_Packet *pkt, timeval *tv)
{
  int dataRead;

  char *buffer = new char [gm_PacketHeader::Size()];
  if (buffer==NULL) Return(gm_False, errOutOfMemory);
  
  dataRead = Read(buffer, gm_PacketHeader::Size(), tv);
  if (dataRead < 0) { delete [] buffer; return gm_False; }
  
  pkt->header.ExtractFromPacket(buffer);
  delete [] buffer;
  
  if (pkt->header.magic!=MAGIC_KEY) Return(gm_False, errInvalidMagicKey);
  if (pkt->header.length < 0) Return(gm_False, errSocketReadError);
  
  if (pkt->data!=NULL && pkt->isDynamicallyAllocated==gm_True) {
    delete [] pkt->data;
    pkt->data = NULL;
  }
  
  if (pkt->header.length==0) {
    pkt->data = NULL;
    return gm_True;
  }
  
  pkt->data = new char[pkt->header.length];
  if (pkt->data==NULL) Return(gm_False, errOutOfMemory);
  pkt->isDynamicallyAllocated = gm_True;
  
  dataRead = Read(pkt->data, pkt->header.length, tv);
  
  if (dataRead < 0) return gm_False;
  if (UINT32(dataRead) < pkt->header.length) {
    Return(gm_False, errSocketReadNotEnoughData);
  }
  return gm_True;
}


gm_Bool
MulticastSocket::Read(gm_Packet *pkt, timeval *tv)
{
  int dataRead;
  UINT32 bytesExtracted;

  char buffer[MAXDGRAMSIZE];
  dataRead = Read(buffer, MAXDGRAMSIZE, tv);
  
  if (dataRead < 0) return gm_False;
  Error::SetStatus(success);
  
  if (UINT32(dataRead) < checksumSize + gm_PacketHeader::Size()) 
    Return(gm_False, errSocketReadNotEnoughData);
  
  if (gm_Packet::VerifyChecksum(buffer, buffer+checksumSize, 
			     dataRead-checksumSize)!=gm_True)
    Return(gm_False, errChecksumError);
  bytesExtracted  = checksumSize;
  bytesExtracted += pkt->header.ExtractFromPacket(buffer+bytesExtracted);
  if (pkt->header.magic!=MAGIC_KEY) Return(gm_False, errInvalidMagicKey);
  if (pkt->header.length > UINT32(dataRead-bytesExtracted))
    Return(gm_False, errSocketReadNotEnoughData);
  if (pkt->header.length < UINT32(dataRead-bytesExtracted))
    Return(gm_False, errFormattingError);
  
  if (pkt->data!=NULL && pkt->isDynamicallyAllocated==gm_True) {
    delete [] pkt->data;
    pkt->data = NULL;
  }
  
  if (pkt->header.length==0) {
    pkt->data = NULL;
    return gm_True;
  }
  
  pkt->data = new char[pkt->header.length];
  if (pkt->data==NULL) Return(gm_False, errOutOfMemory);
  pkt->isDynamicallyAllocated = gm_True;
  
  memcpy(pkt->data, buffer+bytesExtracted, pkt->header.length);
  return gm_True;
}


void
gm_Packet::CreateChecksum(char *checksumLocation, const char *buffer, 
		       UINT32 bufferLength)
{
  UINT32 checksum = 0, idx;
  int    offset = sizeof(UINT32)-checksumSize;

  if (offset < 0) offset = 0;
  for(idx=0; idx < bufferLength; idx++)
    checksum += *(buffer++);
  checksum = htonl(checksum);

  memcpy(checksumLocation, ((char*)&checksum) + offset, checksumSize);
}


gm_Bool
gm_Packet::VerifyChecksum(const char *checksumLocation, const char *buffer, 
		       UINT32 bufferLength)
{
  UINT32 checksum = 0, idx;
  int    offset = sizeof(UINT32)-checksumSize;

  if (offset < 0) offset = 0;
  for(idx=0; idx < bufferLength; idx++)
    checksum += *(buffer++);
  checksum = htonl(checksum);

  return (memcmp(checksumLocation, ((char*)&checksum) + offset, 
		 checksumSize)==0) ? gm_True:gm_False;
}


void 
gm_PacketHeader::Print()
{
  gm_Debug(dbgTmp, "Magic: 0x" << hex << magic << "   Type: " << dec << type
	   << '\n' << "Length: " << length << "   ReplyID: " << requestReplyID
	   << '\n');
}


void 
gm_Packet::PrintDump(const char *msg, const char *data_, UINT32 len)
{
  char buffer[100];
  OStream stream;
  stream << msg;
  for (UINT32 idx=0; idx < len; idx++) {
    unsigned int ch = (unsigned char) data_[idx];
    sprintf(buffer, "%02X(%c) ", ch, ((ch<32 || ch >=127) ? '?' 
				      : (char) ch));
    stream << buffer;
  }
  stream << '\n';
  gm_Debug(dbgTmp, stream.getData());
}


void
gm_Packet::Print(UINT32 start, UINT32 len)
{
  if (len==0) len = header.length - start;
  PrintDump("Data:\n", &data[start], len);
}


gm_Bool
gm_PacketHandler::EvSelect(EventSystem *evs)
{
  gm_Packet packet;

  if (ReadPacket(&packet)==gm_True) {
    if (EvPacketReceived(evs, &packet)==gm_True) return gm_True;
  }

  if (Error::getStatus()==errSocketEOF) {
    Error::SetStatus(success);
    return EvConnectionBroken(evs);
  }
  else return HandleSelectError(evs, &packet);
}


gm_Bool
gm_PacketHandler::HandleSelectError(EventSystem *evs, gm_Packet *packet)
{
  char buffer[256];

  switch (Error::getStatus()) {
  case errSocketReadNotEnoughData:
    sprintf(buffer, "Ignoring incomplete packet (packet type was '%lu')\n",
	    packet->getType());
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case errInvalidMagicKey:
    sprintf(buffer, "Packet with invalid magic key received\n");
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case errChecksumError:
    sprintf(buffer, "Error in datagram checksum. Ignoring this packet\n");
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case errInvalidPacketType:
    sprintf(buffer, "Ignoring invalid packet type '%lu'\n", packet->getType());
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;
    
  case errFormattingError:
    sprintf(buffer, "Error in packet format (packet type was '%lu')\n"
	    "Detected in %s at line %d\n",
	    packet->getType(), Error::getFilename(), Error::getLineNo());
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case errOutOfContextPacket:
    sprintf(buffer,"Packet received out of context (packet type was '%lu')\n",
	    packet->getType());
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case errSocketEOF:
    Error::SetStatus(success);
    return EvConnectionBroken(evs);

  case errSocketReadError:
    sprintf(buffer, "Error occurred while reading packet: %s\n",
	    strerror(errno));
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case errSocketWriteError:
    sprintf(buffer, "Error occurred while writing packet: %s\n",
	    strerror(errno));
    ErrorOutput(buffer);
    Error::SetStatus(success);
    return gm_True;

  case success:
    // that's weird! just abort...
    sprintf(buffer, "Some function returned gm_False without setting any "
	    "errorcode (packet type %lu)\nAborting the program...\n", 
	    packet->getType());
    ErrorOutput(buffer);
    abort();

  default:
    return gm_False;
  }
}


#define StreamReturn(returnValue, errorCode) \
{ \
  if (errorCode==success) { \
    SetGoodFlag(gm_True); \
    return returnValue; \
  } \
  else { \
    SetGoodFlag(gm_False); \
    Return(returnValue, errorCode); \
  } \
}
#define VOID

IStream::IStream(const char *d, UINT32 l) 
  : Stream(), data(d), length(l), stringWidth(l), 
    current(NULL), delimiter('\0')
{
  if (data!=NULL && length>0) current = data;
}


IStream& 
IStream::InputBinary(void *binaryData, UINT32 numberOfBytes)
{
  if (IsGood()==gm_False) return *this;

  if ((current-data) + numberOfBytes > length) {
    // not enough data
    StreamReturn(*this, errFormattingError);
  }

  memcpy(binaryData, current, numberOfBytes);
  current += numberOfBytes;

  StreamReturn(*this, success);
}


IStream& 
IStream::operator >> (char *string)
{
  if (IsGood()==gm_False) return *this;
  const  char *original = current;
  UINT32 strPos=0;
  
  while (IsDone()==gm_False && *current!=delimiter && strPos < getW()-1) {
    string[strPos++] = *current;
    current++;
  }
  string[strPos] = '\0';
  
  if (*current!=delimiter) { 
    if (IsDone()==gm_True) { // delimiter not found
      current = original; 
      StreamReturn(*this, errFormattingError);
    }
    else { // getW() exceeded
      current = original; 
      StreamReturn(*this, errBufferOverflow);
    }
  }

  //current++; do not skip the delimiter
  StreamReturn(*this, success);
}

  
IStream& 
IStream::operator >> (int &integer)
{
  const UINT32 intWidth=20;
  char intStr[intWidth];

  if (IsGood()==gm_False) return *this;

  if (IsBinary()==gm_True) {
    UINT32 uinteger;
    InputBinary(&uinteger, sizeof(UINT32));
    if (IsGood()==gm_False) return *this;
    uinteger = ntohl(uinteger);
    integer  = (int) uinteger;
    return *this;
  }
  else {
    UINT32 origGetW = getW();
    (*this) >> SetW(intWidth) >> intStr >> SetW(origGetW);
    if (IsGood()==gm_False) return *this;
    if (sscanf(intStr, "%d", &integer)!=1) { 
      StreamReturn(*this, errFormattingError);
    }
    return *this;
  }
}


IStream& 
IStream::operator >> (UINT32 &value)
{
  const int uintWidth=20;
  char uintStr[uintWidth];

  if (IsGood()==gm_False) return *this;

  if (IsBinary()==gm_True) {
    InputBinary(&value, sizeof(UINT32));
    if (IsGood()==gm_False) return *this;
    value = ntohl(value);
    return *this;
  }
  else {
    UINT32 origGetW = getW();
    (*this) >> SetW(uintWidth) >> uintStr >> SetW(origGetW);
    if (IsGood()==gm_False) return *this;
    if (sscanf(uintStr, "%lu", &value)!=1) { 
      StreamReturn(*this, errFormattingError);
    }
    return *this;
  }
}

IStream& 
IStream::operator >> (INT32 &value)
{
  const int uintWidth=20;
  char uintStr[uintWidth];

  if (IsGood()==gm_False) return *this;

  if (IsBinary()==gm_True) {
    InputBinary(&value, sizeof(UINT32));
    if (IsGood()==gm_False) return *this;
    value = ntohl(value);
    return *this;
  }
  else {
    UINT32 origGetW = getW();
    (*this) >> SetW(uintWidth) >> uintStr >> SetW(origGetW);
    if (IsGood()==gm_False) return *this;
    if (sscanf(uintStr, "%ld", &value)!=1) { 
      StreamReturn(*this, errFormattingError);
    }
    return *this;
  }
}


IStream&
IStream::Extract(void *extract, UINT32 extractLength)
{
  if (IsGood()==gm_False) return *this;

  if ((current-data) + extractLength > length) {
    // not enough data
    StreamReturn(*this, errFormattingError);
  }

  memcpy(extract, current, extractLength);
  current += extractLength;

  return *this;
}


OStream::OStream()
  : Stream(), data(NULL), length(0), bytesWritten(0), isDynamic(gm_True)
{
  const int startBufferSize=64;
  data = new char [startBufferSize];
  if (data==NULL) { StreamReturn(VOID, errOutOfMemory); }

  *data = '\0';
  length  = startBufferSize;
}


OStream::OStream(char *buffer, UINT32 len)
    : Stream(), data(buffer), length(len), bytesWritten(0), isDynamic(gm_False)
{
  *data = '\0';
}



gm_Bool
OStream::GrowBufferToAtLeast(UINT32 newLength)
{
  char *newData;

  if (isDynamic==gm_False) StreamReturn(gm_False, errBufferOverflow);
  if (data==NULL) StreamReturn(gm_False, errOutOfMemory);

  while (newLength > length) length *= 2;
  newData = new char [length];
  if (newData==NULL) {
    delete [] data;
    length = 0;
    data = NULL;
    StreamReturn(gm_False, errOutOfMemory);
  }
  memcpy(newData, data, bytesWritten+1);
  delete [] data;
  data = newData;
  return gm_True;
}


OStream& 
OStream::OutputBinary(const void *binaryData, UINT32 numberOfBytes)
{
  if (IsGood()==gm_False) return *this;
  UINT32 newLength;
  newLength = bytesWritten + numberOfBytes + 1;

  if (newLength > length) {
    if (GrowBufferToAtLeast(newLength)==gm_False) return *this;
  }

  memcpy(data + bytesWritten, binaryData, numberOfBytes);
  bytesWritten += numberOfBytes;
  *(data + bytesWritten) = '\0';

  StreamReturn(*this, success);
}


OStream& 
OStream::operator << (const char *string)
{
  if (IsGood()==gm_False || string==NULL) return *this;
  UINT32 newLength, lengthOfString;
  lengthOfString = strlen(string);
  newLength      = bytesWritten + lengthOfString + 1;

  if (newLength > length) {
    if (GrowBufferToAtLeast(newLength)==gm_False) return *this;
  }

  strcpy(data + bytesWritten, string);
  bytesWritten += lengthOfString;

  StreamReturn(*this, success);
}


OStream&
OStream::operator << (char ch)
{
  if (IsGood()==gm_False) return *this;

  char dummy[2] = " ";
  UINT32 chPos = bytesWritten;
  *this << dummy;

  if (IsGood()==gm_True) data[chPos] = ch;
  return *this;
}


OStream&
OStream::operator << (int integer)
{
  if (IsGood()==gm_False) return *this;

  if (IsBinary()==gm_True) {
    UINT32 uinteger = (UINT32)integer;
    uinteger = htonl(uinteger);
    return OutputBinary((char*) &uinteger, sizeof(UINT32));
  }
  else {
    char string[25];
    sprintf(string, "%d", integer);
    return (*this << string);
  }
}


OStream&
OStream::operator << (UINT32 uinteger)
{
  if (IsGood()==gm_False) return *this;

  if (IsBinary()==gm_True) {
    uinteger = htonl(uinteger);
    return OutputBinary((char*) &uinteger, sizeof(UINT32));
  }
  else {
    char string[25];
    sprintf(string, "%lu", uinteger);
    return (*this << string);
  }
}

OStream&
OStream::operator << (INT32 integer)
{
  if (IsGood()==gm_False) return *this;

  if (IsBinary()==gm_True) {
    integer = htonl(integer);
    return OutputBinary((char*) &integer, sizeof(INT32));
  }
  else {
    char string[25];
    sprintf(string, "%ld", integer);
    return (*this << string);
  }
}


OStream&
OStream::Append(void *append, UINT32 appendLength)
{
  if (IsGood()==gm_False) return *this;
  UINT32 newLength;
  newLength = bytesWritten + appendLength + 1;

  if (newLength > length) {
    if (GrowBufferToAtLeast(newLength)==gm_False) return *this;
  }

  memcpy(data + bytesWritten, append, appendLength);
  bytesWritten += appendLength;
  *(data + bytesWritten) = '\0';

  StreamReturn(*this, success);
}
