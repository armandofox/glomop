#ifndef __PTMQUEUE_H__
#define __PTMQUEUE_H__

#include "distinterface.h"
#include "defines.h"
#include "linkedlist.h"
#include "threads.h"

class Queue {
public:
  virtual gm_Bool  Add(void *work, unsigned long client_ip) = 0;
  virtual void*    Remove() = 0;
  virtual UINT32   getSize() = 0;
};

class ProducerConsumerQueue : public Queue {
public:
  ProducerConsumerQueue() : mutex("*-pcq") { };
  gm_Bool Add(void *work, unsigned long client_ip);
  void*   Remove();
  UINT32  getSize() { return queue.getSize(); };

protected:
  LockingList<void> queue;
  Mutex             mutex;
  Condition         workInQueue;
};

class PriorityQueue : public Queue {
public:
  PriorityQueue();
  virtual ~PriorityQueue();
  gm_Bool Add(void *work, unsigned long client_ip);
  void *  Remove();
  UINT32  getSize();
protected:
  Mutex              mutex;
  LockingList<void> *classLists;
  Condition          workInQueue;
};

class LotteryQueue : public Queue {
public:
  LotteryQueue();
  virtual ~LotteryQueue();
  gm_Bool Add(void *work, unsigned long client_ip);
  void *  Remove();
  UINT32  getSize();
protected:
  Mutex              mutex;
  LockingList<void> *classLists;
  int               *tickets;
  int                tickTotal;
  Condition          workInQueue;
};

#endif
