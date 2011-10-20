#include "log.h"
#include "defines.h"
#include "cache.h"
#include "cachenet.h"
#include "cacheman.h"


#ifdef __INSTRUMENT__
FILE *instFile=NULL;
#endif


DistillerCache::DistillerCache()
  : BasicDistillerDatabase(), cacheMutex("dist-cache"), typeIndex(NULL)
{
#ifdef __INSTRUMENT__
  instFile = fopen("ptm/frontend.log", "w");
#endif

  typeIndex = new DistillerTypeIndex;
  if (typeIndex==NULL) VoidReturn(errOutOfMemory);
  AddIndex(typeIndex); // ignore return value
}


DistillerCache::~DistillerCache()
{
  if (typeIndex!=NULL) {
    RemoveIndex(typeIndex);
    delete typeIndex;
    typeIndex = NULL;
  }
}


CachedDistiller*
DistillerCache::getDistiller(DistillerType *type, DistillerStatus &status)
{
  CachedDistiller *distiller;

  status = distOk;

  // Lock the cache; remember to unlock it before doing any long blocking 
  // operation
  cacheMutex.Lock();
  
  // try to find a distiller in the cache
  distiller = Find_(type);
  
  if (distiller==NULL) {
    // if I can't find one, first look in the negative cache
    if (negativeCache.IsThere(type)==gm_True) {
      // it is in the negative cache; that means even the PTM doesn't
      // know of it. So just return an error status
      cacheMutex.Unlock();
      status = distDistillerNotFound;
      return NULL;
    }

    // if I can't find one, unlock the mutex, ask the PTM for a new 
    // distiller, and retry
    AskForDistillerStatus askStatus;
    cacheMutex.Unlock();
    //set_thread_state(THR_DISTPTM);
    if (CacheManager::getInstance()->AskPTMForDistiller__(type, askStatus)
	==gm_False) {
      // could not get a distiller from the PTM
      // return error status
      // the function might have set an error status; clear it
      Error::SetStatus(success);
      switch (askStatus) {
      case ptmDistillerLaunchTimeout:
      case ptmAskForDistillerTimeout:
	status = distLaunchTimeout;
	return NULL;
      default:
	// the PTM couldn't find a distiller; save this info in the 
	// negative cache
	negativeCache.Add(type);
	status = distDistillerNotFound;
	return NULL;
      }
    }
    
    // AskPTMForDistiller succeeded
    // so we probably have a new cached distiller; retry
    // lock the cache first
    
    cacheMutex.Lock();
    distiller = Find_(type);
    
    if (distiller==NULL) {
      // still could not find a distiller! We're out of luck...
      cacheMutex.Unlock();
      status = distDistillerNotFound;
      return NULL;
    }
  }
  
  // At this point, I've found a distiller (and the cache is still locked)
  // increment the ref count of the distiller
  distiller->IncrReference();
  distiller->IncrPendingCount();
  cacheMutex.Unlock();
  return distiller;
}


gm_Bool
DistillerCache::Update(DistillerInfoHandler *handler)
{
  BasicDistiller record;
  gm_Bool returnValue = gm_True;
  cacheMutex.Lock();

  if (handler->IsCompleteUpdate()==gm_True) {
    // first invalidate the entire cache; we'll validate entries as we find 
    // them from the DistillerInfo
    MarkAsValid_(gm_False);
  }

  // get the next record
  while (handler->getNext(&record)==gm_True) {
    // update that record
    if (Update_(&record)==gm_False) break;
  }

  if (handler->IsCompleteUpdate()==gm_True) {
    if (Error::getStatus()==success) {
      // we were successful; sweep out the untouched distillers
      Sweep_();
      returnValue = gm_True;
    }
    else {
      // something bad happenned; re-mark all distillers as valid and punt back
      MarkAsValid_(gm_True);
      returnValue = gm_False;
    }
  }
  cacheMutex.Unlock();
  return returnValue;
}


gm_Bool
DistillerCache::Update_(BasicDistiller *newRecord)
{
  CachedDistiller *record, *copy;
  record = Find_(&newRecord->rid);
  if (record!=NULL) { 
    // there is already a cached distiller at this address
    if (record->type.Equal(&newRecord->type)==gm_True) {
      // they are the same distiller
      record->Update(newRecord);
      record->MarkAsValid();
      return gm_True;
    }
    else {

#if OLD
      // a different distiller at the same address
      if (record->IsConnected__()==gm_True) {
	// the proxy is connected to this distiller
	// ignore the new distiller (this case shouldn't happen)
	record->MarkAsValid();
	return gm_True;
      }
      else {
	// the old distiller was a stale cache copy; remove it

	Remove_(record);
	record->MarkForDeletion();
	gm_Log("Replacing cache with new " << (char*) newRecord->type 
	       << " distiller at " << newRecord->rid << "\n");
	if ((copy=new CachedDistiller)==NULL) {
	  Return(gm_False, errOutOfMemory);
	}
	*((BasicDistiller*)copy) = *newRecord;
	copy->SetName();
	gm_Bool returnValue = Add_(copy);
	return returnValue;
      }
#endif

      // we are most likely renaming the distiller
      // be careful not to disconnect the distiller
      
      // XXX: How can I be sure that it is really the same distiller?
      
      gm_Log("Renaming " << (char*) record->type << " distiller at " 
	     << record->rid << " to " << (char*) newRecord->type << "\n");
      BasicDistillerDatabase::Remove(record);
      *((BasicDistiller*)record) = *newRecord;
      record->SetName();
      gm_Bool returnValue = Add_(record);
      return returnValue;

#if OLD
      Remove_(record);
      record->MarkForDeletion();
      gm_Log("Replacing cache with new " << (char*) newRecord->type 
	     << " distiller at " << newRecord->rid << "\n");
      if ((copy=new CachedDistiller)==NULL) {
	Return(gm_False, errOutOfMemory);
      }
      *((BasicDistiller*)copy) = *newRecord;
      copy->SetName();
      gm_Bool returnValue = Add_(copy);
      return returnValue;
#endif
    }
  }
  else {
    // this is a brand new distiller
    gm_Log("Caching new " << (char*) newRecord->type << " distiller at " 
	   << newRecord->rid << "\n");
    
    if ((copy = new CachedDistiller)==NULL) {
      Return(gm_False, errOutOfMemory);
    }
    *((BasicDistiller*)copy) = *newRecord;
    copy->SetName();
    gm_Bool returnValue = Add_(copy);
    return returnValue;
  }
}


gm_Bool
DistillerCache::Add_(CachedDistiller *dist)
{
  return BasicDistillerDatabase::Add(dist);
}


void
DistillerCache::Remove_(CachedDistiller *dist)
{
  gm_Log("Removing " << (char*) dist->type << " distiller from " << dist->rid 
	 << "\n");
  BasicDistillerDatabase::Remove(dist);
  dist->Disconnect__();
  
  // I wonder why I had commented out that piece of code earlier!
  // XXX: don't bother to disconnect; when the distiller is deleted, 
  // it'll automatically get disconnected
}


CachedDistiller*
DistillerCache::Find_Lottery_(DistillerType *type)
{
  List<DatabaseRecord> *list;
  UINT32 totalTickets, winner;
  ListIndex idx;
  CachedDistiller *dist;

  //list = typeIndex->getBucket(type);
  list = getAllRecords();
  totalTickets = 0;

  // compute the total number of tickets
  idx = list->BeginTraversal();
  for (; list->IsDone(idx)==gm_False; idx = list->getNext(idx)) {
      dist = (CachedDistiller*) list->getData(idx);
      if (dist->type.CanSatisfy(type)==gm_True 
	  && dist->ShouldIgnore()==gm_False)
	totalTickets += (dist->getTickets());
  }
  list->EndTraversal();

  // select the winning ticket
  if (totalTickets==0) return NULL;
  winner = lrand48() % totalTickets;

  // determine the winning distiller
  totalTickets = 0;
  idx = list->BeginTraversal();
  for (; list->IsDone(idx)==gm_False; idx = list->getNext(idx)) {
      dist = (CachedDistiller*) list->getData(idx);
      if (dist->type.CanSatisfy(type)==gm_True 
	  && dist->ShouldIgnore()==gm_False) {
	totalTickets += (dist->getTickets());
	if (winner < totalTickets) {
	  list->EndTraversal();
	  return dist;
	}
      }
  }
  list->EndTraversal();
  return NULL;
}


CachedDistiller*
DistillerCache::Find_Random_(DistillerType *type)
{
  CachedDistiller *dist, *found, *lastDist;
  List<DatabaseRecord> *list;
  ListIndex idx;
  int size, random, current;

  //list     = typeIndex->getBucket(type);
  list     = getAllRecords();
  size     = list->getSize();
  found    = NULL;
  dist     = NULL;
  lastDist = NULL;

  if (size > 0) {
    // there is something in the bucket

    // pick a random distiller
    random = lrand48() % size;

    // need to make sure it is the right kind
    idx = list->BeginTraversal();
    for (current=0; current<=random; idx = list->getNext(idx), current++) {
      dist = (CachedDistiller*) list->getData(idx);
      if (dist->type.CanSatisfy(type)==gm_True) lastDist = dist;
    }

    if (dist==lastDist) {
      found = dist;
    }
    else {
      // the distiller at position random was not of the reqd type
      // so look ahead

      for (; list->IsDone(idx)==gm_False; idx = list->getNext(idx)) {
	dist = (CachedDistiller*) list->getData(idx);
	if (dist->type.CanSatisfy(type)==gm_True) {
	  found = dist;
	  break;
	}
      }
      
      if (found==NULL) found = lastDist;
    }
    list->EndTraversal();
  }
  return found;
}


CachedDistiller*
DistillerCache::Find_(RemoteID *rid)
{
  return (CachedDistiller*) getMainIndex()->FindOne(rid);
}



struct SweepFinder {
  List<CachedDistiller> list;
};


void
DistillerCache::Sweep_() 
{
  SweepFinder finder;
  ForEach(SweepFunction_, &finder); 
  ListIndex idx;
  idx = finder.list.BeginTraversal();
  for (; finder.list.IsDone(idx)==gm_False; idx = finder.list.getNext(idx)) {
    //XXXgm_Log("Sweeping out old distiller\n");
    Remove_(finder.list.getData(idx));
    finder.list.getData(idx)->MarkForDeletion();
  }
  finder.list.EndTraversal();
}


gm_Bool
DistillerCache::SweepFunction_(Database */*db*/, DatabaseRecord *record,
			       void *finder_)
{
  SweepFinder *finder = (SweepFinder*) finder_;
  CachedDistiller *dist = (CachedDistiller*) record;
  if (dist->IsValid()==gm_False && dist->IsConnected__()==gm_False) {
    if (finder->list.InsertAtHead(dist)==gm_False) return gm_False;
  }

  return gm_True;
}


gm_Bool
DistillerCache::Validate_(Database */*db*/, DatabaseRecord *record,void* valid)
{
  ((CachedDistiller*)record)->MarkAsValid(*((gm_Bool*)valid));
  return gm_True;
}


DistillerStatus
CachedDistiller::Distill(Argument *args, int numberOfArgs,
			 DistillerInput *input, DistillerOutput *output)
{
  //set_thread_state(THR_DISTLOCK);
  // first lock this distiller
  distMutex.Lock();

  // check whether someone has marked this object for deletion
  if (IsDeleted()==gm_True) {
    distMutex.Unlock();
    return distSendError;
  }

  // connect to it if required
  if (IsConnected_()==gm_False) {
    if (Connect_()==gm_False) {
      if (Error::getStatus()==errOutOfMemory) {
	distMutex.Unlock();
	Error::SetStatus(success);
	return distOutOfLocalMemory;
      }
      else {
	distMutex.Unlock();
	Error::SetStatus(success);
	return distSendError;
      }
    }
  }

  //set_thread_state(THR_DISTSEND);
  // send stuff

  // this has the side effect of starting the timer
  DistillerRequest *request = new DistillerRequest(this);

  if (AddRequest_(request)==gm_False) {
    if (Error::getStatus()==errOutOfMemory) {
      Error::SetStatus(success);
      distMutex.Unlock();
      return distOutOfLocalMemory;
    }
    else {
      Error::SetStatus(success);
      distMutex.Unlock();
      return distSendError;
    }
  }

  DistillerConnection *conn = connection;
  conn->IncrReference();
  distMutex.Unlock();
  if (request->SendAndWait(conn, args, numberOfArgs, input)==gm_False) {
    if (Error::getStatus()==errOutOfMemory) {
      Error::SetStatus(success);
      conn->DecrReference();
      delete request;
      return distOutOfLocalMemory;
    }
    else {
      Error::SetStatus(success);
      conn->DecrReference();
      delete request;
      return distSendError;
    }
  }
  *output = * (request->getOutput());
  request->getOutput()->DontFreeMe();
  conn->DecrReference();
  DistillerStatus retval = request->getStatus();
  delete request;
  return retval;
}

DistillerStatus
CachedDistiller::DistillAsync(Argument *args, int numberOfArgs,
			 DistillerInput *input, DistillerRequest *& newrequest)
{
  //set_thread_state(THR_DISTLOCK);
  // first lock this distiller
  distMutex.Lock();

  // check whether someone has marked this object for deletion
  if (IsDeleted()==gm_True) {
    distMutex.Unlock();
    return distSendError;
  }

  // connect to it if required
  if (IsConnected_()==gm_False) {
    if (Connect_()==gm_False) {
      if (Error::getStatus()==errOutOfMemory) {
	distMutex.Unlock();
	Error::SetStatus(success);
	return distOutOfLocalMemory;
      }
      else {
	distMutex.Unlock();
	Error::SetStatus(success);
	return distSendError;
      }
    }
  }

  //set_thread_state(THR_DISTSEND);
  // send stuff

  // this has the side effect of starting the timer
  newrequest = new DistillerRequest(this);

  if (AddRequest_(newrequest)==gm_False) {
    if (Error::getStatus()==errOutOfMemory) {
      Error::SetStatus(success);
      distMutex.Unlock();
      delete newrequest;
      newrequest = NULL;
      return distOutOfLocalMemory;
    }
    else {
      Error::SetStatus(success);
      distMutex.Unlock();
      delete newrequest;
      newrequest = NULL;
      return distSendError;
    }
  }

  DistillerConnection *conn = connection;
  conn->IncrReference();
  distMutex.Unlock();
  if (newrequest->Send(conn, args, numberOfArgs, input)==gm_False) {
    if (Error::getStatus()==errOutOfMemory) {
      Error::SetStatus(success);
      conn->DecrReference();
      delete newrequest;
      newrequest = NULL;
      return distOutOfLocalMemory;
    }
    else {
      Error::SetStatus(success);
      conn->DecrReference();
      delete newrequest;
      newrequest = NULL;
      return distSendError;
    }
  }
  conn->DecrReference();
  return distOk;
}

gm_Bool
CachedDistiller::Connect_()
{
  /*if (connection!=NULL) {
    // there seems to be a previous connection (shouldn't happen)
    // remove it
    CacheManager::getInstance()->getEventSystem()->
      RemoveCommunicationObject(connection);
    delete connection;
  }*/
  assert (connection==NULL);

  // create a new connection object
  connection = new DistillerConnection(this);
  if (connection==NULL) {
    Return(gm_False, errOutOfMemory);
  }
  // try to connect to the remote distiller
  connection->IncrReference();
  if (connection->Connect(rid.ipAddress, rid.port)==gm_False) {
    connection->DecrReference();
    return gm_False;
  }

  /* // add the connection object to the event system
  if (CacheManager::getInstance()->getEventSystem()->
      AddCommunicationObject(connection)==gm_False) {
    connection->DecrReference();
    return gm_False;
  }*/
  connection->DecrReference();
  return gm_True;
}


void
CachedDistiller::Disconnect_()
{
  if (connection!=NULL) {
    /*connection->IncrReference();
    // remove the connection object from the event system
    CacheManager::getInstance()->getEventSystem()->
      RemoveCommunicationObject(connection);*/

    RemoveAllRequests_();

    // delete the object
    connection->MarkForDeletion();
    //connection->DecrReference();
    connection = NULL;
  }
}


void 
CachedDistiller::RemoveAllRequests_()
{
  DistillerRequest *request;
  while (pendingRequests.IsEmpty()==gm_False) {
    request = pendingRequests.RemoveFromHead();
    request->EvDistillerConnectionBroken();
  }
}



NegativeCache::~NegativeCache()
{
  Flush();
}


gm_Bool
NegativeCache::Add(DistillerType *type)
{
  NegativeCacheEntry *record;
  mutex.Lock();
  // check if this entry is already in the cache
  if (index.FindOne(type)!=NULL) {
    mutex.Unlock();
    return gm_True;
  }

  // it's not already there, so add it!
  // first check if we have overflowed the cache size
  if (list.getSize() >= MaxNCSize) {
    // remove the LRU entry from the cache
    Evict_();
  }

  // now add the new record
  record = new NegativeCacheEntry(type);
  if (record==NULL) {
    mutex.Unlock();
    Return(gm_False, errOutOfMemory);
  }
  if (list.InsertAtHead(record)==gm_False) {
    mutex.Unlock();
    return gm_False;
  }
  if (index.Add(record)==gm_False) {
    mutex.Unlock();
    return gm_False;
  }

  // mark the record as most recently used
  Touch_(record);
  mutex.Unlock();
  return gm_True;
}


void
NegativeCache::Evict_()
{
  ListIndex idx, lru;
  NegativeCacheEntry *record;
  // find the least recently used record

  idx = list.BeginTraversal();
  lru = idx;
  for (idx = list.getNext(idx); 
       list.IsDone(idx)==gm_False; 
       idx = list.getNext(idx)) {
    if (list.getData(idx)->lastTouched < list.getData(lru)->lastTouched) {
      lru = idx;
    }
  }
  record = list.getData(lru);
  list.Remove(lru);
  list.EndTraversal();   

  if (record!=NULL) {
    index.Remove(record);
    delete record;
  }
}


void
NegativeCache::Flush()
{
  NegativeCacheEntry *record;
  mutex.Lock();
  while (list.IsEmpty()==gm_False) {
    record = list.RemoveFromHead();
    if (record!=NULL) {
      index.Remove(record);
      delete record;
    }
  }

  touchCounter = 0;
  mutex.Unlock();
}


gm_Bool
NegativeCache::IsThere(DistillerType *type)
{
  NegativeCacheEntry *record;

  mutex.Lock();
  record = (NegativeCacheEntry*) index.FindOne(type);
  if (record==NULL) {
    mutex.Unlock();
    return gm_False;
  }
  Touch_(record);
  mutex.Unlock();
  return gm_True;
}
