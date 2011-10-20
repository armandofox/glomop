#ifndef __GARBAGE_H__
#define __GARBAGE_H__

#include "gm_mutex.h"


class GarbageCollectedObject {
public:
  GarbageCollectedObject() : numberOfRefs(0), isDeleted(gm_False),
    mutex("*-garbage") { };
  virtual ~GarbageCollectedObject() { };

  gm_Bool IsDeleted() { 
    gm_Bool returnValue;
    mutex.Lock();
    returnValue = isDeleted; 
    mutex.Unlock();
    return returnValue;
  };
  void MarkForDeletion() { 
    mutex.Lock();
    if (numberOfRefs==0) delete this; // don't unlock the mutex here, 
				      // since it's already deleted
    else {
      isDeleted = gm_True; 
      mutex.Unlock();
    }
  }
  void IncrReference() { 
    mutex.Lock();
    numberOfRefs++; 
    mutex.Unlock();
  };
  void DecrReference() { 
    mutex.Lock();
    numberOfRefs--; 
    if (isDeleted==gm_True && numberOfRefs==0) delete this;
    else mutex.Unlock();
  };
private:
  UINT32  numberOfRefs;
  gm_Bool isDeleted;
  Mutex   mutex;
};


#endif // __GARBAGE_H__
