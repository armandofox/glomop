#ifndef __DISTINTERFACE_H__
#define __DISTINTERFACE_H__

#include "config_tr.h"
#include "monitor.h"

#define MAX_ARGS 50             /* max # args allowed to send to distiller */
#define MAX_ARG_STRING 80       /* max length of a string-type arg */
#include <stdlib.h> 
#include <string.h>


#define DistillerMalloc(size)           malloc( (size) )
#define DistillerFree(pointer)          free( (pointer) )
#define DistillerRealloc(pointer, size) realloc( (pointer), (size) )

#define MAXDISTILLER_TYPE_STRING 80
typedef struct C_DistillerType {
  char string[MAXDISTILLER_TYPE_STRING];
} C_DistillerType;


#define  SET_DISTILLER_TYPE(arg,str)  strncpy((arg).string, (str), MAXDISTILLER_TYPE_STRING)
#define  GET_DISTILLER_TYPE(arg)      ((arg).string)


typedef enum ArgumentType {
  typeInt, 
  typeString, 
  typeDouble
} ArgumentType;

/*
 *  WARNING:  The following list must be consistent with the array of
 *  strings in cacheman.cc which are returned by
 *  FE_getDistillerStatusString 
 */
typedef enum DistillerStatus {
  distOk=0,
  distNoCacheManager,       /*the cache manager seems to have disappeared*/
  distDistillerNotFound,    /*No distiller found; or PTM does not exist*/
  distSendError,            /*Error occurred while sending data*/
  distReplyTimeout,         /*Timed out while waiting for reply*/
  distOutOfLocalMemory,     /*Proxy front-end ran out of memory*/
  distConnectionBroken,     /*Connection to the distiller was broken*/
  distLaunchTimeout,        /*Timed out while asking PTM for a new distiller*/

  distFatalError,           /*Fatal error at the distiller*/
  distBadInput,             /*Distiller received bad input*/
  distRedispatch,           /*Redispatch needed (chained distillation) */

  distRendezvousTimeout     /*The distill wasn't ready when it was checked */
} DistillerStatus;


typedef struct DistillationCost {
  UINT32 estimatedTime_ms; /* currently always set to 1 => load metric is
                              the same as queue length */
} DistillationCost;


typedef struct Argument {
#ifdef __cplusplus
  Argument() : type(typeInt), id(0) { };
#endif

  ArgumentType type;
  UINT32       id;
  union Union {
#ifdef __cplusplus
    Union() : arg_int(0) { };
#endif

    INT32 arg_int;
    char   arg_string[MAX_ARG_STRING];
    double arg_double;
  } a;
} Argument;

typedef struct ArgumentList {
  int nargs;
  Argument arg[MAX_ARGS];
} ArgumentList;

typedef struct DistillerBuffer {
#ifdef __cplusplus
  DistillerBuffer() : data(0), length(0), maxLength(0), freeMe(gm_True) { };
  ~DistillerBuffer() { Free(); }
  void StaticBuffer(void *data_, size_t len) {
    data = data_;
    freeMe = gm_False;
    length = maxLength = (UINT32)len;
  }
  /*
   *  StringBuffer sets the buffer to be a string of length L:
   *  - the string passed in must be guaranteed to be allocated L+1 bytes
   *  - the L+1'st byte of the string is set to a NULL terminator
   *  - the length field of the buffer reflects the length of the string (ie
   *    what strlen() would return)
   *  - the maxLength field reflects the number of bytes actually occupied by
   *    the string, which is 1+the length
   */
  void StringBuffer(char *str, size_t len) {
    data = (void *)str;
    ((char*)data)[len] = '\0';
    length = len;
    maxLength = 1+len;
    freeMe = gm_False;
  }
  void Free() {
    if (freeMe==gm_True && data!=NULL) {
      DistillerFree(data);
      data = NULL;
      freeMe = gm_False;
    }
    data = NULL;
    length = maxLength = 0;
  }
  void SetLength(UINT32 length_) {
    assert(length_ <= maxLength);
    length = length_;
  }
  gm_Bool Alloc(size_t size) {
    data = DistillerMalloc(size);
    assert(data != NULL);
    freeMe = gm_True;
    length = maxLength = (UINT32)size;
    return gm_True;
  }
  gm_Bool Realloc(size_t size) {
    if (size > maxLength) {
      if (data != NULL) {
        data = DistillerRealloc(data, size);
      } else {
        data = DistillerMalloc(size);
        memset(data, 0, size);
        length = 0;
        freeMe = gm_True;
      }
      assert(data != NULL);
      maxLength = (UINT32)size;
    }
    return gm_True;
  }
  void Clone(struct DistillerBuffer *dst) {
    dst->Alloc(maxLength);
    assert(dst->data);
    memcpy(dst->data, data, maxLength);
    dst->maxLength = maxLength;
    dst->length = length;
    dst->freeMe = freeMe;
  }

  void FreeMe(gm_Bool freeMe_=gm_True) { freeMe = freeMe_; }
  void DontFreeMe() { FreeMe(gm_False); }
#endif
  void    *data;
  UINT32  length;
  UINT32  maxLength;
  gm_Bool freeMe;
} DistillerBuffer;

#ifdef __cplusplus
extern "C" {
#endif

#define DistillerBufferLength(d) ((d)->length)
#define DistillerBufferData(d)   ((d)->data)
#define DistillerBufferFreeMe(d,w) ((d)->freeMe = (w))
gm_Bool DistillerBufferAlloc(DistillerBuffer *dBuf, size_t size);
gm_Bool DistillerBufferRealloc(DistillerBuffer *dBuf, size_t size);
void    DistillerBufferClone(DistillerBuffer *dst, DistillerBuffer *src);
void    DistillerBufferFree(DistillerBuffer *dBuf);
void    DistillerBufferSetLength(DistillerBuffer *dBuf, UINT32 len);
void    DistillerBufferSetStatic(DistillerBuffer *dBuf, void *buf_, size_t len);
void    DistillerBufferSetString(DistillerBuffer *dBuf, char *str, size_t len);

#ifdef __cplusplus
}
#endif

#define MAX_MIMETYPE 50

typedef struct DistillerData {
#ifdef __cplusplus
  DistillerData() /*: data(0), length(0), metadata(0), metalength(0)*/ {
    strcpy(mimeType, "");
  }

  void DontFreeMe() {
    data.DontFreeMe();
    metadata.DontFreeMe();
  }

  void Free() {
    data.Free();
    metadata.Free();
  }
#endif

  /*void   *data;
  UINT32 length;
  void   *metadata;
  UINT32 metalength;*/


  /* XXX: If you add any more DistillerBuffers to this data structure,
   * update the DontFreeMe() method above
   */
  DistillerBuffer data;
  DistillerBuffer metadata;
  char   mimeType[MAX_MIMETYPE];
} DistillerData;

#define DataPtr(d)      ((d)->data.data)
#define DataLength(d)    ((d)->data.length)
#define MetadataPtr(d)  ((d)->metadata.data)
#define MetadataLength(d) ((d)->metadata.length)
#define MimeType(d)     ((d)->mimeType)
#define SetData(d,t)    ((d)->data.data = (void *)(t))
#define SetDataLength(d,t)  ((d)->data.length = (UINT32)(t))
#define SetMetadata(d,t)  ((d)->metadata.data = (void *)(t))
#define SetMetadataLength(d,t) ((d)->metadata.length = (UINT32)(t))
#define SetMimeType(d,t)   (strncpy((d)->mimeType,(t),MAX_MIMETYPE))
#define DataNeedsFree(d,t) ((d)->data.freeMe = (t))
#define MetadataNeedsFree(d,t) ((d)->metadata.freeMe = (t))


typedef DistillerData DistillerInput;
typedef DistillerData DistillerOutput;



#define SET_ARG_ID(arg,i)      ((arg).id = i)
#define SET_ARG_STRING(arg,s)   { (arg).type = typeString; \
                                  strncpy((arg).a.arg_string,s,MAX_ARG_STRING-1); }
#define SET_ARG_INT(arg,i)      { (arg).type = typeInt; (arg).a.arg_int = i; }
#define SET_ARG_DOUBLE(arg,d)   { (arg).type=typeDouble; (arg).a.arg_double=d; }

#define ARG_TYPE(arg)     ((arg).type)
#define ARG_ID(arg)       ((arg).id)
#define ARG_STRING(arg)   ((arg).type == typeString? (arg).a.arg_string: "")
#define ARG_INT(arg)      ((arg).type == typeInt? (arg).a.arg_int: 0)
#define ARG_DOUBLE(arg)   ((arg).type == typeDouble? (arg).a.arg_double: 0.0)

#define _ARG_STRING(arg)  ((arg).a.arg_string)
#define _ARG_INT(arg)     ((arg).a.arg_int)
#define _ARG_DOUBLE(arg)  ((arg).a.arg_double)

#ifdef __cplusplus
extern "C" {
#endif


Argument *getArgumentFromId(Argument *args, int numberOfArgs, UINT32 id);
#define getArgumentFromIdInList(al,id)  \
                getArgumentFromId((al)->arg,(al)->nargs,(id))
/*
 * returns a pointer to the argument corresponding to id
 * NULL, if no such arg can be found
 */


DistillerStatus DistillerInit(C_DistillerType distType, 
			      int argc, const char * const * argv);
/*
 * Front-end calls this function to perform startup stuff
 * Returns distOk on success, distFatalError on failure
 */

void DistillerCleanup();
/*
 * Front-end calls this function before exiting
 */


DistillerStatus
DistillerMain(Argument *args, int numberOfArgs,
	      DistillerInput *input, DistillerOutput *output);

DistillerStatus
ComputeDistillationCost(Argument *args, int numberOfArgs,
			DistillerInput *input, DistillationCost *cost);
/*
 * This function should calculate an estimate of the time to perform a
 * distillation.
 * The function returns distOk on success.
 */

#ifdef OLD
void *DistillerMalloc(size_t size);
void *DistillerRealloc(void *ptr, size_t size);
/*
 * Use these function to malloc memory for the output buffer
 */

void DistillerFree(void *pointer);
/*
 * Use this function to free memory allocated using DistillerMalloc
 * DO NOT free the output buffer if the "freeOutputBuffer" return arg is 
 * set to gm_True inside DistillerMain.
 */
#endif


MonitorClientID DistillerGetMonitorClientID();
void DistillerRename(C_DistillerType type);

#ifdef __cplusplus
}
#endif

#endif 
