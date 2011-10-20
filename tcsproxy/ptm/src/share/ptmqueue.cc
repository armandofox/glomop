#include "defines.h"
#include "log.h"
#include "ptmqueue.h"
#include "utils.h"
#include "linkedlist.h"
#include "threads.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/*****************************
** Producer/Consumer Queues **
*****************************/
gm_Bool
ProducerConsumerQueue::Add(void *work, unsigned long /*client_ip*/)
{
  mutex.Lock();

  if (queue.InsertAtTail(work)==gm_False) {
    mutex.Unlock();
    return gm_False;
  }
  workInQueue.Signal();
  mutex.Unlock();
  return gm_True;
}


void*
ProducerConsumerQueue::Remove()
{
  mutex.Lock();

  while (queue.IsEmpty()==gm_True) {
    workInQueue.Wait(&mutex);
  }

  void *work = queue.RemoveFromHead();
  mutex.Unlock();
  return work;
}

/****************************
**    Priority Queues      **
*****************************/

PriorityQueue::PriorityQueue(void) : mutex("*-prq")
{
  /* For now, we'll just hack up three queues - highest priority
     (which will simulate Soda Hall traffic), medium priority
     (which will simulate home-IP traffic), and lowest priority
     (which is the rest of the world). */
  
  classLists = new LockingList<void>[3];
}

PriorityQueue::~PriorityQueue(void)
{
  delete []classLists;
}


gm_Bool
PriorityQueue::Add(void *work, unsigned long client_ip)
{
  unsigned long h_ip = ntohl(client_ip);
  LockingList<void>  *theList;
  LockingList<void>  *highList, *medList, *lowList;
  timeval now;
  UINT32  highlen, medlen, lowlen;

  highList = &(classLists[0]);
  medList = &(classLists[1]);
  lowList = &(classLists[2]);

  mutex.Lock();
  if (h_ip == 2149590592) {
    /* coming from dawn20 */
    theList = &(classLists[0]);
  } else if (h_ip == 2149590593) {
    /* coming from dawn21 */
    theList = &(classLists[1]);
  } else {
    /* coming from dawn22 or elsewhere */
    theList = &(classLists[2]);
  }

  if (theList->InsertAtTail(work) == gm_False) {
    mutex.Unlock();
    return gm_False;
  }

  gettimeofday(&now, NULL);
  highlen = highList->getSize();
  medlen = medList->getSize();
  lowlen = lowList->getSize();
//  fprintf(stderr, "%ld.%06ld\t%lu\t%lu\t%lu\n",
//	  now.tv_sec, now.tv_usec, highlen, medlen, lowlen);
  workInQueue.Signal();
  mutex.Unlock();
  return gm_True;
}

void*
PriorityQueue::Remove()
{
  LockingList<void>  *highList, *medList, *lowList;
  UINT32  highlen, medlen, lowlen;
  timeval now;

  mutex.Lock();

  highList = &(classLists[0]);
  medList = &(classLists[1]);
  lowList = &(classLists[2]);
  while ((highList->IsEmpty() == gm_True) &&
	 (medList->IsEmpty() == gm_True) &&
	 (lowList->IsEmpty() == gm_True))
    workInQueue.Wait(&mutex);

  void *work;

  if (highList->IsEmpty() != gm_True) {
    work = highList->RemoveFromHead();
  } else if (medList->IsEmpty() != gm_True) {
    work = medList->RemoveFromHead();
  } else {
    work = lowList->RemoveFromHead();
  }
  gettimeofday(&now, NULL);
  highlen = highList->getSize();
  medlen = medList->getSize();
  lowlen = lowList->getSize();
//  fprintf(stderr, "%ld.%06ld\t%lu\t%lu\t%lu\n",
//	  now.tv_sec, now.tv_usec, highlen, medlen, lowlen);
  mutex.Unlock();
  return work;
}

UINT32
PriorityQueue::getSize()
{
  UINT32 size;

  LockingList<void>  *highList, *medList, *lowList;
  mutex.Lock();

  highList = &(classLists[0]);
  medList = &(classLists[1]);
  lowList = &(classLists[2]);
  
  size = highList->getSize() + medList->getSize() + lowList->getSize();
  mutex.Unlock();
  return size;
}

/****************************
**    Lottery Queues       **
*****************************/

LotteryQueue::LotteryQueue(void) : mutex("*-prq")
{
  /* For now, we'll just hack up three queues - highest priority
     (which will simulate Soda Hall traffic), medium priority
     (which will simulate home-IP traffic), and lowest priority
     (which is the rest of the world). */
  
  timeval now;

  classLists = new LockingList<void>[3];
  tickets = new int[3];

  tickets[0] = 80;
  tickets[1] = 60;
  tickets[2] = 10;
  tickTotal = 150;

  gettimeofday(&now, NULL);
  srand48(now.tv_usec);
}

LotteryQueue::~LotteryQueue(void)
{
  delete []classLists;
  delete []tickets;
}


gm_Bool
LotteryQueue::Add(void *work, unsigned long client_ip)
{
  unsigned long h_ip = ntohl(client_ip);
  LockingList<void>  *theList;
  LockingList<void>  *highList, *medList, *lowList;
  timeval now;
  UINT32  highlen, medlen, lowlen;

  highList = &(classLists[0]);
  medList = &(classLists[1]);
  lowList = &(classLists[2]);

  mutex.Lock();
  if (h_ip == 2149590592) {
    /* coming from dawn20 */
    theList = &(classLists[0]);
  } else if (h_ip == 2149590593) {
    /* coming from dawn21 */
    theList = &(classLists[1]);
  } else {
    /* coming from dawn22 or elsewhere */
    theList = &(classLists[2]);
  }

  if (theList->InsertAtTail(work) == gm_False) {
    mutex.Unlock();
    return gm_False;
  }

  gettimeofday(&now, NULL);
  highlen = highList->getSize();
  medlen = medList->getSize();
  lowlen = lowList->getSize();
//  fprintf(stderr, "%ld.%06ld\t%lu\t%lu\t%lu\n",
//	  now.tv_sec, now.tv_usec, highlen, medlen, lowlen);
  workInQueue.Signal();
  mutex.Unlock();
  return gm_True;
}

void*
LotteryQueue::Remove()
{
  LockingList<void>  *highList, *medList, *lowList;
  UINT32  highlen, medlen, lowlen;
  timeval now;
  int     done = 0, which;
  double  randnum;

  mutex.Lock();

  highList = &(classLists[0]);
  medList = &(classLists[1]);
  lowList = &(classLists[2]);
  while ((highList->IsEmpty() == gm_True) &&
	 (medList->IsEmpty() == gm_True) &&
	 (lowList->IsEmpty() == gm_True))
    workInQueue.Wait(&mutex);

  void *work;

  done = 0;
  while (!done) {
    randnum = drand48();
//    fprintf(stderr, "randnum is %lf\n", randnum);
    if (randnum <= ((double)tickets[0] / (double) tickTotal)) {
      which = 0;
    } else if (randnum <= (((double)(tickets[0] + tickets[1])) /
			    (double) tickTotal)) {
      which = 1;
    } else {
      which = 2;
    }
//    fprintf(stderr, "which is: %d (%d %d %d)\n", which,
//	    highList->IsEmpty(), medList->IsEmpty(),
//	    lowList->IsEmpty());
    if ((which == 0) && (highList->IsEmpty() != gm_True)) {
//      fprintf(stderr, "getting from 0\n");
      work = highList->RemoveFromHead();
      done = 1;
    } else if ((which == 1) && (medList->IsEmpty() != gm_True)) {
//      fprintf(stderr, "getting from 1\n");
      work = medList->RemoveFromHead();
      done = 1;
    } else if ((which == 2) && (lowList->IsEmpty() != gm_True)) {
//      fprintf(stderr, "getting from 2\n");
      work = lowList->RemoveFromHead();
      done = 1;
    }
//    fprintf(stderr, "Done is %d\n", done);
  }
  gettimeofday(&now, NULL);
  highlen = highList->getSize();
  medlen = medList->getSize();
  lowlen = lowList->getSize();
//  fprintf(stderr, "%ld.%06ld\t%lu\t%lu\t%lu\n",
//	  now.tv_sec, now.tv_usec, highlen, medlen, lowlen);
  mutex.Unlock();
  return work;
}

UINT32
LotteryQueue::getSize()
{
  UINT32 size;

  LockingList<void>  *highList, *medList, *lowList;
  mutex.Lock();

  highList = &(classLists[0]);
  medList = &(classLists[1]);
  lowList = &(classLists[2]);
  
  size = highList->getSize() + medList->getSize() + lowList->getSize();
  mutex.Unlock();
  return size;
}
