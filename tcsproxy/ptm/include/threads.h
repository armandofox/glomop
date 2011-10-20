#ifndef __THREADS_H__
#define __THREADS_H__


#include <pthread.h>
#include "database.h"
#include "defines.h"
#include "gm_mutex.h"

#if 0
class Thread;

class ThreadDatabase : public Database {
public:
  ThreadDatabase();
  ~ThreadDatabase();

  gm_Bool Add   (DatabaseRecord *record);
  void    Remove(DatabaseRecord *record);
  Thread *Find  (pthread_t id);

protected:
  class ThreadIndex : public Index {
  protected:
    IndexKey *getIndexKey(DatabaseRecord *record);
  };

private:
  Mutex       mutex;
  ThreadIndex index;
};




struct ThreadID : public IndexKey {
  pthread_t id;

  gm_Bool   Equal(IndexKey *key) {
    ThreadID *idKey = (ThreadID*) key;
    return ((pthread_equal(idKey->id, id)!=0) ? gm_True : gm_False);
  };
  UINT32 Hash() { return (UINT32) id; };
};
#endif


class Thread /*: public DatabaseRecord*/ {
public:
  Thread();
  virtual ~Thread();
  gm_Bool Fork(void* (*proc)(void*), void* args);
  gm_Bool Join(void *&returnStatus);
  gm_Bool Join();
  gm_Bool Exit(void *returnStatus, gm_Bool deleteThis=gm_False);

  pthread_t *getID()  { return &id; };
  /*pthread_t *getID()  { return &id.id; };
  ThreadID  *getKey() { return &id;    };
  static Thread* CurrentThread();*/
  static void ThreadExit(Thread *exitThread, void *returnStatus, 
			 gm_Bool deleteThread);
  static void* ThreadProcedure(void *args);
private:
  pthread_attr_t attr;
  //ThreadID id;
  pthread_t id;

  //static ThreadDatabase allThreads;
};




/*inline
ThreadDatabase::ThreadDatabase()
  : mutex("*-thrdb")
{
  AddIndex(&index);
}


inline
ThreadDatabase::~ThreadDatabase()
{
  RemoveIndex(&index);
}



inline gm_Bool
ThreadDatabase::Add(DatabaseRecord *record)
{
  gm_Bool returnValue;
  mutex.Lock();
  returnValue = Database::Add(record);
  mutex.Unlock();

  return returnValue;
}


inline void
ThreadDatabase::Remove(DatabaseRecord *record)
{
  mutex.Lock();
  Database::Remove(record);
  mutex.Unlock();
}


inline Thread*
ThreadDatabase::Find(pthread_t id)
{
  Thread  *thread;
  ThreadID key;
  key.id = id;

  mutex.Lock();
  thread = (Thread*) index.FindOne(&key);
  mutex.Unlock();
  return thread;
}


inline IndexKey *
ThreadDatabase::ThreadIndex::getIndexKey(DatabaseRecord *record) 
{ 
  return ((Thread*)record)->getKey(); 
}*/





inline
Thread::Thread()
{
  int te;
  if (ThreadError(pthread_attr_init(&attr)) != 0) return;

  if (ThreadError(pthread_attr_setstacksize(&attr,4000000))!=0) return;
  te = pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);
#ifdef ENOSYS
  if (te!=ENOSYS) { // ENOSYS => this function is not implemented; ignore it
    if (ThreadError(te)!=0) return;
  }
#else
  if (ThreadError(te)!=0) return;
#endif
}


inline
Thread::~Thread()
{
  pthread_attr_destroy(&attr);
  //allThreads.Remove(this);
}


inline gm_Bool 
Thread::Join(void *&returnStatus)
{ 
  return (ThreadError(pthread_join(id, &returnStatus))==0) ? gm_True : gm_False; 
}


inline gm_Bool 
Thread::Join()
{ 
  void *dummyReturnStatus;
  return Join(dummyReturnStatus);
}


/*inline Thread* 
Thread::CurrentThread() 
{ 
  return allThreads.Find(pthread_self()); 
}*/


inline void
Thread::ThreadExit(Thread *exitThread, void *returnStatus, gm_Bool deleteThread)
{
  if (deleteThread==gm_True) delete exitThread;
  pthread_exit(returnStatus);
}


inline gm_Bool
Thread::Exit(void *returnStatus, gm_Bool deleteThis)
{
  if (id==pthread_self()) ThreadExit(this, returnStatus, deleteThis);
  Return(gm_False, errThreadError);
}


#endif // __THREADS_H__
