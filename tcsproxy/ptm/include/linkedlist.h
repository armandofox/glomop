#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__


#include "error.h"
#include "gm_mutex.h"


typedef void *ListIndex;


template <class T>
class List {

public:
  List() : isTraversing(gm_False), head(0), tail(0), count(0) { };
  virtual ~List() { RemoveAll(); };

  virtual gm_Bool IsEmpty() { 
    gm_Bool returnValue;
    BeginTraversal();
    returnValue = ((head==0) && (tail==0)) ? gm_True:gm_False; 
    EndTraversal();
    return returnValue;
  }
  virtual int getSize() { 
    int returnValue;
    BeginTraversal();
    returnValue = count; 
    EndTraversal();
    return returnValue;
  };


  /* traversal functions */
  virtual ListIndex BeginTraversal() { 
    if (isTraversing==gm_True) {
      gm_Abort("List::BeginTraversal() called twice\n");
    }
    isTraversing = gm_True; 
    return (ListIndex) head;
  };
  virtual void      EndTraversal() { 
    if (isTraversing==gm_False) 
      gm_Abort("List::EndTraversal() called without invoking "
	       "BeginTraversal()\n");
    isTraversing = gm_False; 
  };
  virtual ListIndex getNext(ListIndex idx);
  virtual ListIndex getPrev(ListIndex idx);
  virtual T*        getData(ListIndex idx);
  virtual gm_Bool   IsDone (ListIndex idx);
  virtual gm_Bool   InsertAfter(ListIndex idx, T *data);
  virtual gm_Bool   InsertBefore(ListIndex idx, T *data);
  virtual void      Remove(ListIndex idx);

  /* insertion and removal functions */
  virtual gm_Bool InsertAtHead(T *data, gm_Bool amITraversing=gm_False) {
    gm_Bool returnValue;
    if (amITraversing==gm_False) BeginTraversal();
    returnValue = InsertAfter_WithNULL(0, data);
    if (amITraversing==gm_False) EndTraversal();
    return returnValue;
  }
  virtual gm_Bool InsertAtTail(T *data, gm_Bool amITraversing=gm_False) {
    gm_Bool returnValue;
    if (amITraversing==gm_False) BeginTraversal();
    returnValue = InsertAfter_WithNULL( (ListIndex) tail, data);
    if (amITraversing==gm_False) EndTraversal();
    return returnValue;
  }
  virtual T*   RemoveFromHead(gm_Bool amITraversing=gm_False);
  virtual T*   RemoveFromTail(gm_Bool amITraversing=gm_False);
  virtual void Remove(T *data, gm_Bool amITraversing=gm_False);
  virtual T*   PeekAtHead(gm_Bool amITraversing=gm_False) { 
    T* returnValue;
    if (amITraversing==gm_False) BeginTraversal();
    returnValue = ((head==0) ? 0 : head->data);
    if (amITraversing==gm_False) EndTraversal();
    return returnValue;
  }
  virtual T*   PeekAtTail(gm_Bool amITraversing=gm_False) { 
    T* returnValue;
    if (amITraversing==gm_False) BeginTraversal();
    returnValue = ((tail==0) ? 0 : tail->data);
    if (amITraversing==gm_False) EndTraversal();
    return returnValue;
  }

protected:
  gm_Bool InsertAfter_WithNULL(ListIndex idx, T *data);
  void    Remove_(ListIndex idx);
  void    RemoveAll();

  gm_Bool isTraversing; /* used as a sanity check for traversal functions */

  struct ListNode {
    ListNode(T *d=0, ListNode *n=0, ListNode *p=0) 
      : data(d), next(n), prev(p) { };
    T        *data;
    ListNode *next;
    ListNode *prev;
  };

private:
  ListNode *head;
  ListNode *tail;
  int      count;
};


/*#define LockingCall1(call) \
  BeginTraversal(); \
  call; \
  EndTraversal();


#define LockingCall2(returnType, call) \
  returnType returnValue; \
  BeginTraversal(); \
  returnValue = call; \
  EndTraversal(); \
  return returnValue; */


template <class T>
class LockingList : public List<T> {

public:
  LockingList() : List<T>(), mutex("*-ll") { };
  /*
  virtual gm_Bool IsEmpty() { 
    LockingCall2(gm_Bool, List<T>::IsEmpty()); 
  }
  virtual int getSize() { LockingCall2(int, List<T>::getSize()); }
  */

  /* traversal functions */

  virtual ListIndex BeginTraversal() { 
    ListIndex idx;
    mutex.Lock();
    idx = List<T>::BeginTraversal();

    /* do not unlock the mutex here; EndTraversal() does that! */
    return idx;
  }
  virtual void EndTraversal() { 
    List<T>::EndTraversal();
    mutex.Unlock();
  }
  /*
  virtual void Remove(ListIndex idx) {
    // need to add this function since it clashes with Remove(T*)
    // don't grab the mutex, since BeginTraversal() already has!
    List<T>::Remove(idx);
  }*/

  /* insertion and removal functions */
  /*virtual gm_Bool InsertAtHead(T *data) {
    LockingCall2(gm_Bool, List<T>::InsertAtHead(data));
  }
  virtual gm_Bool InsertAtTail(T *data) {
    LockingCall2(gm_Bool, List<T>::InsertAtTail(data));
  }
  virtual T* RemoveFromHead() { LockingCall2(T*, List<T>::RemoveFromHead()); }
  virtual T* RemoveFromTail() { LockingCall2(T*, List<T>::RemoveFromTail()); }
  virtual void Remove(T *data) { LockingCall1(List<T>::Remove(data)); }

  virtual T*   PeekAtHead() { LockingCall2(T*, List<T>::PeekAtHead()); }
  virtual T*   PeekAtTail() { LockingCall2(T*, List<T>::PeekAtTail()); }*/

private:
  Mutex mutex;
};


//#undef LockingCall1
//#undef LockingCall2


template <class T>
ListIndex
List<T>::getNext(ListIndex idx)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::getNext() without invoking BeginTraversal()\n");
  }

  if (idx==0) return 0;
  else return ((ListNode*)idx)->next;
}


template <class T>
ListIndex
List<T>::getPrev(ListIndex idx)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::getPrev() without invoking BeginTraversal()\n");
  }

  if (idx==0) return 0;
  else return ((ListNode*)idx)->prev;
}


template <class T>
T*
List<T>::getData(ListIndex idx)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::getNext() without invoking BeginTraversal()\n");
  }

  if (idx==0) return 0;
  else return ((ListNode*)idx)->data;
}


template <class T>
gm_Bool
List<T>::IsDone(ListIndex idx)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::IsDone() without invoking BeginTraversal()\n");
  }

  return ((idx==0) ? gm_True : gm_False);
}


template <class T>
gm_Bool
List<T>::InsertAfter(ListIndex idx, T *data)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::InsertAfter() without invoking BeginTraversal()\n");
  }

  if (idx==0) Return(gm_False, errNULLPointer);
  return InsertAfter_WithNULL(idx, data);
}


template <class T>
gm_Bool
List<T>::InsertBefore(ListIndex idx, T *data)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::InsertBefore() without invoking BeginTraversal()\n");
  }

  if (idx==0) Return(gm_False, errNULLPointer);
  return InsertAfter_WithNULL(getPrev(idx), data);
}


template <class T>
void
List<T>::Remove(ListIndex idx)
{
  if (isTraversing==gm_False) {
    // you can't get here unless you've invoked BeginTraversal()!
    gm_Abort("Called List::Remove() without invoking BeginTraversal()\n");
  }

  if (idx==0) VoidReturn(errNULLPointer);
  Remove_(idx);
}


template <class T>
T*
List<T>::RemoveFromHead(gm_Bool amITraversing)
{
  T *data, *returnValue;
  if (amITraversing==gm_False) BeginTraversal();
  if (head==0) returnValue = 0;
  else {
    data = head->data;
    Remove_((ListIndex)head);
    returnValue = data;
  }
  if (amITraversing==gm_False) EndTraversal();
  return returnValue;
}


template <class T>
T*
List<T>::RemoveFromTail(gm_Bool amITraversing)
{
  T *data, *returnValue;
  if (amITraversing==gm_False) BeginTraversal();
  if (tail==0) returnValue = 0;
  else {
    data = tail->data;
    Remove_((ListIndex)tail);
    returnValue = data;
  }
  if (amITraversing==gm_False) EndTraversal();
  return returnValue;
}


template <class T>
void
List<T>::Remove(T *data, gm_Bool amITraversing)
{
  ListNode *node;

  if (amITraversing==gm_False) BeginTraversal();
  for (node=head; node!=NULL; node=node->next) {
    if (node->data==data) {
      Remove_((ListIndex)node);
      break;
    }
  }
  if (amITraversing==gm_False) EndTraversal();
}


template <class T>
gm_Bool
List<T>::InsertAfter_WithNULL(ListIndex idx, T *data)
{
  ListNode *node = (ListNode *) idx;
  ListNode *newNode = new ListNode(data);
  if (newNode==0) Return(gm_False, errOutOfMemory);

  if (node==0) {
    newNode->next = head;
    if (head!=0) head->prev = newNode;
    head = newNode;

    if (tail==0) tail = newNode;
  } 
  else {
    ListNode *nextNode = node->next;
    // must add newNode between node and nextNode!

    node   ->next = newNode;
    newNode->next = nextNode;
    newNode->prev = node;
    
    if (nextNode==0) tail = newNode;
    else nextNode->prev = newNode;
  }

  count++;
  return gm_True;
}


template <class T>
void
List<T>::Remove_(ListIndex idx)
{
  ListNode *node = (ListNode*) idx, *prevNode, *nextNode;

  if (node==0 || (head==0 && tail==0)) return;

  prevNode = node->prev;
  nextNode = node->next;

  if (prevNode!=0) prevNode->next = nextNode;
  else head = nextNode;
  if (nextNode!=0) nextNode->prev = prevNode;
  else tail = prevNode;

  delete node;
  count--;
}


template <class T>
void
List<T>::RemoveAll()
{
  while (IsEmpty()==gm_False) RemoveFromHead();
}

#endif // __LINKEDLIST_H__
