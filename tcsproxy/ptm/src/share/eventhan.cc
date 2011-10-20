#include "communication.h"
#include "eventhan.h"
#include <unistd.h>
#include <errno.h>
#include <iostream.h>
#include <signal.h>

#include "log.h"
void LogMutex::Lock() {
  gm_Log("Mutex " << getName() << ": trying to lock\n");
  Mutex::Lock();
  gm_Log("Mutex " << getName() << ": has locked\n");
}
void LogMutex::Unlock() {
  gm_Log("Mutex " << getName() << ": trying to unlock\n");
  Mutex::Unlock();
  gm_Log("Mutex " << getName() << ": has unlocked\n");
}

//#define CO_Timer_ms 5000

//EventSystem *gm_Timer::eventSystem=NULL;


gm_Timer::gm_Timer(EventSystem *evSys, long seconds, long useconds, 
		   gm_Bool repeat)
  : repeatMode(repeat), eventSystem(evSys)
{
  timeoutInterval.tv_sec  = seconds;
  timeoutInterval.tv_usec = useconds;

  GetCurrentTime(&nextTimeout);
  IncrementTimeout();

  eventSystem->AddTimer(this);
}


gm_Timer::~gm_Timer()
{
  eventSystem->RemoveTimer(this);
}



int
gm_Timer::Compare(gm_Timer *t1, gm_Timer *t2)
{
  if (t1->nextTimeout.tv_sec  < t2->nextTimeout.tv_sec)  return -1;

  if (t1->nextTimeout.tv_sec == t2->nextTimeout.tv_sec) {
    if (t1->nextTimeout.tv_usec < t2->nextTimeout.tv_usec) return -1;
    if (t1->nextTimeout.tv_usec== t2->nextTimeout.tv_usec) return 0;
  }

  return 1;
}


void
gm_Timer::IncrementTimeout()
{
  nextTimeout.tv_usec += timeoutInterval.tv_usec;

  if (nextTimeout.tv_usec >= 1000000) {
    nextTimeout.tv_usec -= 1000000;
    nextTimeout.tv_sec++;
  }

  nextTimeout.tv_sec += timeoutInterval.tv_sec;
}


void
gm_Timer::ComputeAheadOfGMT(timeval *ahead)
{
  timeval current;
  GetCurrentTime(&current);

  ahead->tv_sec  = nextTimeout.tv_sec  - current.tv_sec;
  ahead->tv_usec = nextTimeout.tv_usec - current.tv_usec;

  if (ahead->tv_usec < 0) {
    ahead->tv_usec += 1000000;
    ahead->tv_sec--;
  }

  if (ahead->tv_sec < 0) {
    ahead->tv_sec = ahead->tv_usec = 0;
  }
}


void
gm_Timer::GetCurrentTime(timeval *tv)
{
  gettimeofday(tv, NULL);
}


gm_Bool
gm_Timer::HandleEvent(EventSystem *evs, EventType eventType, void */*args*/)
{
  switch (eventType) {
  case evTimer:
    return EvTimer(evs);

  default:
    Return(gm_False, errEvSysInvalidEvent);
  }
}


/*class CO_Timer_ : public Timer {
public:
  CO_Timer_(EventSystem *evs) 
    : Timer(evs, SECONDS(CO_Timer_ms), USECONDS(CO_Timer_ms), gm_True) { };
  ~CO_Timer_() { };

protected:
  gm_Bool EvTimer(EventSystem *evs) {return evs->CheckCommunicationObjects();};
};*/



gm_Bool
EventSystem::TimerInfo::Add(gm_Timer *timer)
{
  ListIndex idx;
  gm_Bool found=gm_False, returnValue;

  idx = queue.BeginTraversal();
  for (; queue.IsDone(idx)==gm_False; idx = queue.getNext(idx)) {
    if (gm_Timer::Compare(queue.getData(idx), timer)>=0) {
      returnValue = queue.InsertBefore(idx, timer);
      found = gm_True;
      break;
    }
  }
  if (found==gm_False) {
    returnValue = queue.InsertAtTail(timer, gm_True); //gm_True=>I'm traversing
  }
  queue.EndTraversal();
  return returnValue;
}


EventSystem::CommunicationInfo::CommunicationInfo() : maxDescr(0)
{
  FD_ZERO(&descrs);
}


gm_Bool
EventSystem::CommunicationInfo::Add(CommunicationObject *comm)
{
  int id = comm->getID();
  if (objectList.InsertAtHead(comm)==gm_False) return gm_False;
  FD_SET(id, &descrs);
  if (id >= maxDescr) maxDescr = id+1;

  return gm_True;
}


void
EventSystem::CommunicationInfo::Remove(CommunicationObject *comm)
{
  ListIndex idx;
  int id;

  objectList.Remove(comm);
  FD_CLR(comm->getID(), &descrs);

  if (comm->getID()+1 >= maxDescr) {
    maxDescr = 0;

    idx = objectList.BeginTraversal();
    for (; objectList.IsDone(idx)==gm_False; idx = objectList.getNext(idx)) {
      id = objectList.getData(idx)->getID();
      if (id >= maxDescr) 
	maxDescr = id+1;
    }
    objectList.EndTraversal();
  }
}


/*gm_Bool
EventSystem::CommunicationInfo::Check(EventSystem *evs)
{
  fd_set fileDescrs;
  timeval tv;
  ListIdx idx;
  int id, returnValue;

  gm_Debug(dbgTmp, "EventSystem::CommunicationInfo::Check invoked\n");
  tv.tv_sec = tv.tv_usec = 0;
  for (idx = objectList.Head(); 
       objectList.IsDone(idx)==gm_False; 
       idx = objectList.Next(idx)) {
    FD_ZERO(&fileDescrs);
    id = objectList.Record(idx)->getID();
    FD_SET(id, &fileDescrs);

    returnValue = select(id+1, &fileDescrs, NULL, NULL, &tv);

    if (returnValue < 0) {
      if (errno==EBADF || errno==EINTR) continue;
      Return(gm_False, errEvSysGetError);
    }
    if (returnValue==0) continue;

    if (FD_ISSET(id, &fileDescrs)) {
      gm_Debug(dbgTmp, "EventSystem::CommunicationInfo::Check is selecting\n");
      if (evs->PostEventNoMutex(evSelect, objectList.Record(idx), 
				NULL)==gm_False) {
	return gm_False;
      }
    }
  }
  return gm_True;
}*/




EventSystem::EventSystem()
  : mutex("*-evs"), pipeWriteFD(-1), pipeReadFD(-1), blockedOnSelect(gm_False)
{
  int fileIDs[2];

  mutex.Lock();

  signal(SIGPIPE, SIG_IGN);
  if (pipe(fileIDs)!=0) VoidReturn(errEvSysCreationError);
  pipeReadFD  = fileIDs[0];
  pipeWriteFD = fileIDs[1];

  mutex.Unlock();
  //new CO_Timer_(this);
}


EventSystem::~EventSystem()
{
  mutex.Lock();
  close(pipeReadFD);
  close(pipeWriteFD);
  mutex.Unlock();
}




gm_Bool
EventSystem::GetEvent(Event *ev)
{
  int       maxDescr, returnValue;
  Event     *queueEv;
  fd_set    readFileDescrs;
  gm_Timer  *timerObject;
  timeval   timeVal, *tvp;
  ListIndex idx;

  mutex.Lock();
  while (gm_True) {
    // if there is something on the queue, remove it and return
    if (eventQueue.IsEmpty()==gm_False) {
      queueEv = eventQueue.RemoveFromHead();
      *ev = *queueEv;
      delete queueEv;
      mutex.Unlock();
      return gm_True;
    }

    // construct the readFD and time-to-wait structures
    

    FD_ZERO(&readFileDescrs);
    memcpy(&readFileDescrs, &ci.descrs, sizeof(ci.descrs));
    FD_SET(pipeReadFD, &readFileDescrs);
    maxDescr = (((pipeReadFD+1) > ci.maxDescr) ? pipeReadFD+1 : ci.maxDescr);

    timerObject = ti.TimerAtHead();
    if (timerObject==NULL) tvp = NULL;
    else {
      timerObject->ComputeAheadOfGMT(&timeVal);
      tvp = &timeVal;
    }

    // set a flag indicating that I am about to do a select & unlock the mutex
    
    blockedOnSelect = gm_True;

    mutex.Unlock();

    // perform a select
    if (tvp!=NULL && tvp->tv_sec==0 && tvp->tv_usec==0) {
      // timelimit is zero i.e. just about to timeout, so don't bother to
      // select
      returnValue = 0;
      FD_ZERO(&readFileDescrs);
    }
    else {
      returnValue = select(maxDescr, &readFileDescrs, NULL, NULL, tvp);
    }
    // grab the mutex & switch off the select-flag
    
    mutex.Lock();
    blockedOnSelect = gm_False;
    notifyCond.Broadcast(); // wake up all the notifying threads

    // if select fails, unlock the mutex and return gm_False
    
    if (returnValue < 0) {
      if (errno==EBADF || errno==EINTR) continue;

      mutex.Unlock();
      Return(gm_False, errEvSysGetError);
    }

    // if select times out, then a timer event has occurred
    // unlock the mutex and add a timer event to the queue

    if (returnValue==0) {
      if (ti.TimerAtHead() != timerObject) {
	// the timer object has dissapeared. Ignore this event
	continue;
      }

      ti.RemoveFromHead();
      queueEv = new Event(evTimer, timerObject, NULL);
      if (queueEv==NULL) {
	mutex.Unlock();
	Return(gm_False, errOutOfMemory);
      }

      if (eventQueue.InsertAtTail(queueEv)==gm_False) {
	mutex.Unlock();
	return gm_False; // InsertAfter has already set "Error::status"
      }
      
      if (timerObject->ShouldIRepeat()==gm_True) {
	timerObject->IncrementTimeout();
	if (ti.Add(timerObject)==gm_False) {
	  mutex.Unlock();
	  return gm_False; // AddTimer has already set "Error::status"
	}
      }

      continue;
    }

    
    // if select succeeds on "pipe", then read the next char off the pipe
    // and continue (this is used as a signal to the event system that 
    // something has changed) (e.g. stuff was added to the queue, or the file 
    // descriptors to select on have changed or a new timeout has been 
    // requested) 
    
    if (FD_ISSET(pipeReadFD, &readFileDescrs)) {
      char dummy[257];
      read (pipeReadFD, dummy, 256);

      // don't "continue" here, because we must check whether any other 
      // descriptors are ready as well
    }

    // check if any of the communication objects have succeeded on select
    idx = ci.objectList.BeginTraversal();
    for (; 
	 ci.objectList.IsDone(idx)==gm_False; 
	 idx = ci.objectList.getNext(idx)){

      if (FD_ISSET(ci.objectList.getData(idx)->getID(), &readFileDescrs)) {

	queueEv = new Event(evSelect, ci.objectList.getData(idx), NULL);
	if (queueEv==NULL) {
	  ci.objectList.EndTraversal();
	  mutex.Unlock();
	  Return(gm_False, errOutOfMemory);
	}

	if (eventQueue.InsertAtTail(queueEv)==gm_False) {
	  ci.objectList.EndTraversal();
	  mutex.Unlock();
	  return gm_False; // InsertAfter has already set "Error::status"
	}
      }
    } // for
    ci.objectList.EndTraversal();
  } // while
}



gm_Bool
EventSystem::PostEvent(EventType eventType, EventHandler *object, void *args)
{
  gm_Bool returnValue;
  mutex.Lock();
  returnValue = PostEventNoMutex(eventType, object, args);
  mutex.Unlock();
  return returnValue;
}


gm_Bool
EventSystem::PostEventNoMutex(EventType eventType, EventHandler *object, 
			      void *args)
{
  Event *queueEv = new Event(eventType, object, args);
  if (queueEv==NULL) Return(gm_False, errOutOfMemory);

  if (eventQueue.InsertAtTail(queueEv)==gm_False) {
    return gm_False;
  }
  
  Notify();
  return gm_True;
}


void 
EventSystem::Notify()
{
  // Notify assumes that the mutex is locked before calling this function!
  char dummy[257]="";

  if (blockedOnSelect==gm_True) {
    write (pipeWriteFD, dummy, 256);
    notifyCond.Wait(&mutex);
  }
}


gm_Bool
EventSystem::Run()
{
  Event event;

  while (GetEvent(&event)==gm_True) {
    if (event.type==evQuit) return gm_True;
    if (event.handler!=NULL) {
      if (event.handler->HandleEvent(this, event.type, event.args)==gm_False) {
	return gm_False;
      }
    }
  }

  return gm_False;
}


gm_Bool
EventSystem::AddTimer(gm_Timer *timer)
{
  gm_Bool returnValue;
  mutex.Lock();
  returnValue = ti.Add(timer);
  Notify();
  mutex.Unlock();
  return returnValue;
}


void
EventSystem::RemoveTimer(gm_Timer *timer)
{
  mutex.Lock();
  ti.Remove(timer);
  Notify();
  mutex.Unlock();
}


gm_Bool
EventSystem::AddCommunicationObject(CommunicationObject *comm)
{
  gm_Bool returnValue;
  mutex.Lock();
  returnValue = ci.Add(comm);
  Notify();
  mutex.Unlock();
  return returnValue;
}


void
EventSystem::RemoveCommunicationObject(CommunicationObject *comm)
{
  mutex.Lock();
  ci.Remove(comm);
  Notify();
  mutex.Unlock();
}
