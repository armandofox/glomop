#include "threads.h"


//ThreadDatabase Thread::allThreads;
char* Mutex::log_file=NULL;
FILE* Mutex::log = NULL;

/*struct ThreadArguments {
  ThreadArguments(Thread *t, void * (*p)(void*), void *a)
    : thread(t), proc(p), args(a) { };

  Thread *thread;
  void * (*proc)(void*);
  void *args;
};*/


gm_Bool
Thread::Fork(void* (*proc)(void*), void* args)
{
  //ThreadArguments *realArgs = new ThreadArguments(this, proc, args);

  /*return (ThreadError(pthread_create(&id.id, &attr, ThreadProcedure, 
				     (void *)realArgs))==0) ? gm_True : gm_False;*/
  return (ThreadError(pthread_create(&id, &attr, proc, 
				     (void *)args))==0) ? gm_True : gm_False;
}


/*void *
Thread::ThreadProcedure(void *args)
{
  ThreadArguments *realArgsPtr = (ThreadArguments*) args;
  ThreadArguments realArgs = *realArgsPtr;

  delete realArgsPtr;
  allThreads.Add(realArgs.thread);
  return (*realArgs.proc)(realArgs.args);
}*/
