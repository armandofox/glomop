#ifndef __GM_MUTEX_H__
#define __GM_MUTEX_H__


#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include "defines.h"


class Mutex {
private:
  static void OpenLog() {
    if (log==NULL && log_file!=NULL) {
      log = fopen(log_file, "w");
    }
  }
public:
  Mutex(const char *name_) : name(NULL) { 
    name = new char[strlen(name_)+1];
    strcpy(name, name_);
    ThreadError(pthread_mutex_init(&mp, NULL)); 
  };
  virtual ~Mutex() { 
    if (name!=NULL) delete [] name;
    ThreadError(pthread_mutex_destroy(&mp)); 
  };

  void Lock() {
    int status;
    //Log("trying to lock");
    if ((status=ThreadError(pthread_mutex_lock(&mp)))!=0) {
      char buffer[100];
      sprintf(buffer, "Mutex::Lock() failed! (error status: %d)\n", status);
      gm_Abort(buffer);
    }
    //Log("has locked");
  };
  void Unlock() { 
    int status;
    //Log("trying to unlock");
    if ((status=ThreadError(pthread_mutex_unlock(&mp)))!=0) {
      char buffer[100];
      sprintf(buffer, "Mutex::Unlock() failed! (error status: %d)\n", status);
      gm_Abort(buffer);
    }
  };
  void SetName(char *name_) {
    if (name!=NULL) delete [] name;
    name = new char[strlen(name_)+1];
    strcpy(name, name_);    
  }
  void Log(char *msg) { Log_(msg, getName(), this); }
  static void Log_(char *msg, const char *mutexName, Mutex *this_) {
    OpenLog();
    if (log!=NULL) {
      struct tm t;
      time_t ti = time(NULL);
      localtime_r(&ti, &t);
      fprintf(log, "%02d:%02d:%02d: Thread %u %s %s (%p)\n", t.tm_hour, 
	      t.tm_min, t.tm_sec, (unsigned int) pthread_self(), msg, 
	      mutexName, this_);
      fflush(log);
    }
  }


  pthread_mutex_t* getID() { return &mp; };
  const char *getName() { return name; };

  static char *log_file;
private:
  static FILE *log;
  pthread_mutex_t mp;
  char *name;
};


/*class OwnerMutex : public Mutex {
public:
  OwnerMutex() : Mutex(), owner(0) { };
  void Lock() {
    Mutex::Lock();
    ownerMutex.Lock();
    owner = pthread_self();
    ownerMutex.Unlock();
  }

  void Unlock() {
    ownerMutex.Lock();
    owner = 0;
    ownerMutex.Unlock();
    Mutex::Unlock();
  }

  gm_Bool DidILock() {
    gm_Bool flag;
    pthread_t me = pthread_self();
    if (me==0) return gm_False;
    ownerMutex.Lock();
    flag = ((me==owner) ? gm_True : gm_False);
    ownerMutex.Unlock();
    return flag;
  }

protected:
  Mutex ownerMutex;
  pthread_t owner;
};*/

class Condition {

public:
  Condition()  { ThreadError(pthread_cond_init(&cond, NULL)); };
  virtual ~Condition() { ThreadError(pthread_cond_destroy(&cond)); };
  
  gm_Bool Wait(Mutex *mutex)
  { 
    pthread_mutex_t *mID=mutex->getID();
    mutex->Log("waiting on cv with mutex");
    gm_Bool returnValue = (ThreadError(pthread_cond_wait(&cond, mID))==0) ? gm_True:gm_False;
    mutex->Log("done waiting on cv with mutex");
    return returnValue;
  };
  // returns 1 on error, 0 on success, -1 on timeout
  int TimedWait(Mutex *mutex, struct timeval *tv)
  {
    struct timespec absendtime;
    pthread_mutex_t *mID=mutex->getID();
    mutex->Log("timed waiting on cv with mutex");
    int threrror;
    int retval;
    if (tv) {
	struct timeval now, end;
	gettimeofday(&now, NULL);
	absendtime.tv_sec = end.tv_sec = now.tv_sec + tv->tv_sec;
	end.tv_usec = now.tv_usec + tv->tv_usec;
	while (end.tv_usec > 1000000) {
	    end.tv_sec++;
	    end.tv_usec -= 1000000;
	}
	absendtime.tv_nsec = end.tv_usec * 1000;
	threrror = pthread_cond_timedwait(&cond, mID, &absendtime);
	gettimeofday(&now, NULL);
	if (now.tv_sec > end.tv_sec || (now.tv_sec == end.tv_sec &&
	    now.tv_usec >= end.tv_usec)) {
	    tv->tv_sec = tv->tv_usec = 0;
	} else if (end.tv_usec >= now.tv_usec) {
	    tv->tv_sec = end.tv_sec - now.tv_sec;
	    tv->tv_usec = end.tv_usec - now.tv_usec;
	} else {
	    tv->tv_sec = end.tv_sec - now.tv_sec - 1;
	    tv->tv_usec = (1000000 + end.tv_usec) - now.tv_usec;
	}
    } else {
	threrror = pthread_cond_wait(&cond, mID);
    }
    if (threrror && threrror != ETIMEDOUT) {
	ThreadError(threrror);
	retval = 1;
    } else if (!threrror) {
	retval = 0;
    } else {
	retval = -1;
    }
    mutex->Log("done waiting on cv with mutex");
    return retval;
  }
  gm_Bool Signal() 
  { return (ThreadError(pthread_cond_signal (&cond))==0) ? gm_True:gm_False; };
  gm_Bool Broadcast()
  { return (ThreadError(pthread_cond_broadcast(&cond))==0) ? gm_True:gm_False;};
  
  pthread_cond_t* getID() { return &cond; };
protected:
  pthread_cond_t cond;
};


#endif // __GM_MUTEX_H__
