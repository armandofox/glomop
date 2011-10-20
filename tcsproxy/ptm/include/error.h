#ifndef __ERROR_H__
#define __ERROR_H__


enum Errors {
  success=0,
  errNoErrorObject,
  errThreadError,
  errOutOfMemory,
  errBufferOverflow,
  errNULLPointer,
  errDuplicateIndexEntry,

  errIPAddressError,

  errSocketCreationError, 
  errSocketBindError, 
  errSocketListenError, 
  errSocketConnectError, 
  errSocketReadError,
  errSocketEOF, 
  errSocketWriteError, 
  errSocketReadNothingAvailable, 
  errSocketReadNotEnoughData,
  errSocketTimeout,

  errTcpAddressError, 
  errUdpAddressError, 
  errMcastAddressError,
  errMcastTTLError,
  errMcastAddMembershipError,

  errEvSysCreationError,
  errEvSysPostError,
  errEvSysSendError,
  errEvSysGetError,
  errEvSysInvalidEvent,

  errTooLargeDatagram,
  errInvalidMagicKey,
  errChecksumError,
  errInvalidPacketType,
  errFormattingError,
  errOutOfContextPacket,

  errCouldNotLaunch,
  errFileOpenError,

  errGenericError
};



#ifndef __cplusplus
typedef enum Errors Errors;
#else

class Error {
public:
  Error() : status(success), threadStatus(0), filename(0), lineNo(0) { };
  void Set(int status_, char *filename_, int lineNo_) {
    status   = status_;
    threadStatus = 0;
    filename = filename_;
    lineNo   = lineNo_;
  }

  void Set(int status_, int threadStatus_, char *filename_,int lineNo_) {
    status       = status_;
    threadStatus = threadStatus_;
    filename     = filename_;
    lineNo       = lineNo_;
  }
  
  void Print_();

  static void Initialize();
  static void Destroy(void *value);
  static Error *get();
  static int getStatus() {
    Error *error = get();
    if (error!=0) return error->status;
    else return errNoErrorObject;
  }

  static int getThreadStatus() {
    Error *error = get();
    if (error!=0) return error->threadStatus;
    else return -1;
  }

  static char *getFilename() {
    Error *error = get();
    if (error!=0) return error->filename;
    else return "<unknown>";
  }

  static int getLineNo() {
    Error *error = get();
    if (error!=0) return error->lineNo;
    else return 0;
  }

  static int ThreadError_(int threadStatus_, char *filename_, int lineNo_) {
    if (threadStatus_!=0) {
      Error *error = Error::get();
      if (error!=0) error->Set(errThreadError,threadStatus_,filename_,lineNo_);
    }
    return threadStatus_;
  }

  static void SetStatus(int status_) {
    Error *error = Error::get();
    if (error!=0) error->Set(status_, "", 0);
  }

  static void Print();

protected:
  int   status, threadStatus;
  char* filename;
  int   lineNo;
};


void gm_Abort(char *message);

#define Return(returnValue, errorStatus) \
{ \
  Error *error = Error::get(); \
  if (error!=0) error->Set(errorStatus, __FILE__, __LINE__); \
  return returnValue; \
}


#define VoidReturn(errorStatus)  \
{ \
  Error *error = Error::get(); \
  if (error!=0) error->Set(errorStatus, __FILE__, __LINE__); \
  return; \
}


#define ThreadError(func) Error::ThreadError_(func, __FILE__, __LINE__)


#endif  /* __cplusplus */


#endif /* __ERROR_H__ */
