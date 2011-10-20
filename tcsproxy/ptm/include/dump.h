#ifndef __DUMP_H__
#define __DUMP_H__
#include <pthread.h>
#include <errno.h>
#include <stdio.h>

#ifdef __cplusplus

class Dump {
public:
  Dump(int bufferSize_, char *logFile);
  ~Dump();
  void Write(char *string);
  int ConstructorSucceeded() {
    return ((buffer==NULL||mirror==NULL||fd==-1) ? 0 : 1);
  }
private:
  static void *ThreadMain_(void *this_) {
    ((Dump*)this_)->ThreadMain();
    return 0;
  }
  static void Abort() {
    fprintf(stderr, "Error occurred in class Dump: %s\n", strerror(errno));
    abort();
  }

  void ThreadMain();

  char *buffer, *mirror;
  int  bufferSize;
  int  writtenSoFar;
  int  fd;

  pthread_mutex_t mutex;
  pthread_cond_t  bufferFull, bufferEmpty;
  pthread_t threadID;
};


#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

typedef void *DumpID;

/*
 * Dump_Create: Create a new Dump object
 *   bufferSize: the size of the buffer used to hold log data before
 *               spewing it out to the log file
 *   logFile: pathname of the file to dump the log to; the file is truncated
 *           when the object is created
 *   Returns: the object id on success; NULL on failure
 */
DumpID Dump_Create(int bufferSize, char *logFile);

/*
 * Dump_Write: output a 'string' to the log
 */
void   Dump_Write(DumpID id, char *string);


#ifdef __cplusplus
}
#endif


#endif /* __DUMP_H__ */
