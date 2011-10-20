#include "reqrep.h"


//RequestReplyEventSystem *RequestReply::eventSystem = NULL;


gm_Bool
RequestReplyEventSystem::AddRequestReply(RequestReply *object)
{
  gm_Bool returnValue;

  requestReplyMutex.Lock();

  if (transactionCount==NoRequestReply) transactionCount++;
  object->SetID(transactionCount);
  transactionCount++;
  returnValue = requestReplyIndex.Add(object);

  requestReplyMutex.Unlock();
  return returnValue;
}


void
RequestReplyEventSystem::RemoveRequestReply(RequestReply *object)
{
  requestReplyMutex.Lock();
  requestReplyIndex.Remove(object);
  requestReplyMutex.Unlock();
}


gm_Bool
RequestReplyEventSystem::SendReplyReceivedEvent(UINT32 requestReplyID,
						void *args)
{
  RequestReply *record;

  if (requestReplyID==NoRequestReply) return gm_True;

  requestReplyMutex.Lock();
  RequestReplyKey key;
  key.id = requestReplyID;
  record = (RequestReply*) requestReplyIndex.FindOne(&key);
  requestReplyMutex.Unlock();
  if (record==NULL) return gm_True;
  
  return SendEvent(evReplyReceived, record, args);
}


RequestReply::RequestReply(RequestReplyEventSystem *evs, 
			   long timeoutSec, long timeoutUsec)
  : timer(NULL), eventSystem(evs)
{
  if (timeoutSec==0 && timeoutUsec==0) {
    timer = NULL;
  }
  else {
    timer = new RequestReplyTimer(evs, this, timeoutSec, timeoutUsec);
    if (timer==NULL) VoidReturn(errOutOfMemory);
  }

  if (evs->AddRequestReply(this)==gm_False) return;
}


RequestReply::~RequestReply()
{
  if (timer!=NULL) { delete timer; timer = NULL; }
  eventSystem->RemoveRequestReply(this);
}


gm_Bool
RequestReply::Retry(long timeoutSec, long timeoutUsec)
{
  if (timer!=NULL) { delete timer; timer = NULL; }

  timer = new RequestReplyTimer(eventSystem, this, timeoutSec, timeoutUsec);
  if (timer==NULL) Return(gm_False, errOutOfMemory);
  return gm_True;
}


gm_Bool
RequestReply::HandleEvent(EventSystem *evs, EventType eventType, void *args)
{
  switch (eventType) {
  case evReplyReceived:
    return EvReplyReceived((RequestReplyEventSystem*)evs, args);

  default:
    Return(gm_False, errEvSysInvalidEvent);
  }
}


gm_Bool
RequestReplyTimer::EvTimer(EventSystem *evs)
{
  return requestReply->EvTimer((RequestReplyEventSystem*)evs);
}
