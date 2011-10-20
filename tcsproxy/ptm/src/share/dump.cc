#include "dump.h"
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#define SafetyMargin 1000


Dump::Dump(int bufferSize_, char *logFile) 
  : buffer(NULL), mirror(NULL), bufferSize(0), writtenSoFar(0), fd(-1)
{
  pthread_attr_t threadAttr;

  if (bufferSize_ <= SafetyMargin) Abort();
  buffer = new char [bufferSize_];
  if (buffer==NULL) return;
  mirror = new char [bufferSize_];
  if (mirror==NULL) return;
  bufferSize = bufferSize_;
  fd = open(logFile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
  if (fd==-1) return;

  if (pthread_mutex_init(&mutex, NULL)!=0) Abort();
  if (pthread_cond_init(&bufferFull, NULL)!=0) Abort();
  if (pthread_cond_init(&bufferEmpty, NULL)!=0) Abort();

  if (pthread_attr_init(&threadAttr)!=0) Abort();
  int te = pthread_attr_setscope(&threadAttr,PTHREAD_SCOPE_SYSTEM);
#ifdef ENOSYS
  if (te!=0 && te!=ENOSYS) { 
    // ENOSYS => this function is not implemented; ignore it
    Abort();
  }
#else
  if (te!=0) Abort();
#endif

  if (pthread_create(&threadID, &threadAttr, ThreadMain_, this)!=0) Abort();
}


Dump::~Dump()
{
  if (pthread_cond_destroy(&bufferFull)!=0) Abort();
  if (pthread_cond_destroy(&bufferEmpty)!=0) Abort();
  if (pthread_mutex_destroy(&mutex)!=0) Abort();

  if (fd!=-1) close(fd);
  if (buffer!=NULL) delete [] buffer;
  if (mirror!=NULL) delete [] mirror;
}


void
Dump::Write(char *string)
{
  int len = strlen(string);
  if (pthread_mutex_lock(&mutex)!=0) Abort();
  while (writtenSoFar + len + 1 > bufferSize) {
    // the buffer is full; wait for the writer thread to flush out the buffer
    if (pthread_cond_wait(&bufferEmpty, &mutex)!=0) Abort();
  }

  strcpy(buffer+writtenSoFar, string);
  writtenSoFar += len;
  if (writtenSoFar >= bufferSize - SafetyMargin) {
    // signal the writer thread
    if (pthread_cond_signal(&bufferFull)!=0) Abort();
  }

  if (pthread_mutex_unlock(&mutex)!=0) Abort();
}


void
Dump::ThreadMain()
{
  char *tmp;
  int  mirrorWrittenSoFar;

  while (1) {
    if (pthread_mutex_lock(&mutex)!=0) Abort();

    // wait for buffer to get full
    while (writtenSoFar < bufferSize - SafetyMargin) {
      if (pthread_cond_wait(&bufferFull, &mutex)!=0) Abort();
    }

    // switch the main and mirror buffers
    tmp = mirror;
    mirror = buffer;
    buffer = tmp;

    // reset the write pointer
    mirrorWrittenSoFar = writtenSoFar;
    writtenSoFar = 0;

    // inform all waiting threads that the buffer is empty
    if (pthread_cond_broadcast(&bufferEmpty)!=0) Abort();

    // unlock the mutex
    if (pthread_mutex_unlock(&mutex)!=0) Abort();

    // write out the buffer
    int fileWritten = 0, thisTime;
    while (fileWritten < mirrorWrittenSoFar) {
      thisTime = write(fd, mirror, mirrorWrittenSoFar);
      if (thisTime==-1) Abort();
      fileWritten += thisTime;
    }
  }
}


DumpID 
Dump_Create(int bufferSize, char *logFile)
{
  Dump *dump = new Dump(bufferSize, logFile);
  if (dump==NULL || !dump->ConstructorSucceeded()) return (DumpID) NULL;
  return (DumpID) dump;
}


void
Dump_Write(DumpID id, char *string)
{
  if (id!=NULL) {
    ((Dump*)id)->Write(string);
  }
}



class EWMA {
public:
  EWMA(double weight_) : average(0.0), weight(weight_) { };
  void NewDataPoint(double dataPoint) {
    average = weight * dataPoint + (1 - weight)*dataPoint;
  }  
  double getAverage_double() { return average; };
  int getAverage_int() { return (int) (getAverage_double()+0.5); };
  
private:
  double average;
  double weight;
};




