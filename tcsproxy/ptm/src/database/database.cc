#include "database.h"


Index::~Index()
{
  if (buckets!=NULL) {
    delete [] buckets;
    buckets = NULL;
    numberOfBuckets = 0;
  }
}


/*void
Index::Remove(IndexKey *key)
{
  int     hashValue = Normalize(key->Hash());
  ListIdx idx, prev, next;
  Bucket  *bucket = &buckets[hashValue];

  prev = 0;
  idx = bucket->Head();

  while (bucket->IsDone(idx)==gm_False) {
    if (getIndexKey(bucket->Record(idx))->Equal(key)==gm_True) {
      idx = bucket->Next(idx);
      bucket->DeleteAfter(prev);
    }
    else {
      next = bucket->Next(idx);
      prev = idx;
      idx  = next;
    }
  }
}*/
void
Index::Remove(IndexKey *key)
{
  ListIndex idx, next;
  Bucket    *bucket = getBucket(key);

  idx = bucket->BeginTraversal();
  while (bucket->IsDone(idx)==gm_False) {
    if (getIndexKey(bucket->getData(idx))->Equal(key)==gm_True) {
      next = bucket->getNext(idx);
      bucket->Remove(idx);
      idx = next;
    }
    else {
      idx = bucket->getNext(idx);
    }
  }
  bucket->EndTraversal();
}


void
Index::Remove(DatabaseRecord *record)
{
  getBucket(getIndexKey(record))->Remove(record);
}


DatabaseRecord*
Index::FindOne(IndexKey *key)
{
  ListIndex      idx;
  Bucket         *bucket = getBucket(key);
  DatabaseRecord *returnValue=NULL;

  idx = bucket->BeginTraversal();
  for (; bucket->IsDone(idx)==gm_False; idx=bucket->getNext(idx)){
    if (getIndexKey(bucket->getData(idx))->Equal(key)==gm_True) {
      returnValue = bucket->getData(idx);
      break;
    }
  }
  bucket->EndTraversal();
  return returnValue;
}


void
Index::Find(IndexKey *key, List<DatabaseRecord> *list)
{
  ListIndex idx;
  Bucket *bucket = getBucket(key);

  idx = bucket->BeginTraversal();
  for (; bucket->IsDone(idx)==gm_False; idx=bucket->getNext(idx)){
    if (getIndexKey(bucket->getData(idx))->Equal(key)==gm_True) {
      list->InsertAtHead(bucket->getData(idx));
      // ignore return value here. if gm_False, then we are out of memory
      // can't do much about it!
    }
  }
  bucket->EndTraversal();
}


Database::~Database()
{
  // don't need to do anything here 'coz the destructors for 'indices' and
  // 'records' will take care of themselves

  /*while (indices.IsEmpty()==gm_False) {
    Index *index = indices.RemoveFromHead();
    delete index;
  }  

  while (records.IsEmpty()==gm_False) {
    //DatabaseRecord *record = records.Record(records.Head());
    records.RemoveFromHead();
    //delete record;
  }*/
}


gm_Bool
Database::Add(DatabaseRecord *record) 
{
  ListIndex idx;

  if (records.InsertAtHead(record)==gm_False) return gm_False;

  idx = indices.BeginTraversal();
  for (; indices.IsDone(idx)==gm_False; idx=indices.getNext(idx)) {
    Index *index = indices.getData(idx);
    if (index->Add(record)==gm_False) {
      // remove the record from the database and all previous indices
      records.Remove(record);
      for (idx=indices.getPrev(idx); 
	   indices.IsDone(idx)==gm_False; 
	   idx=indices.getPrev(idx)) {
	indices.getData(idx)->Remove(record);
      }
      indices.EndTraversal();
      return gm_False;
    }
  }
  indices.EndTraversal();
  return gm_True;
}


void
Database::Remove(DatabaseRecord *record)
{
  ListIndex idx;
  idx = indices.BeginTraversal();
  for (; indices.IsDone(idx)==gm_False; idx=indices.getNext(idx)) {
    Index *index = indices.getData(idx);
    index->Remove(record);
  }
  indices.EndTraversal();
  records.Remove(record);
}


void
Database::DeleteAllRecords()
{
  DatabaseRecord *record;
  while (records.IsEmpty()==gm_False) {
    //record = records.RemoveFromHead();
    record = records.PeekAtHead();
    Remove(record);
    if (record!=NULL) delete record;
  }
}


gm_Bool
Database::ForEach(ForEachFunction function, void *functionData)
{
  ListIndex idx, next;

  idx = records.BeginTraversal();
  while(records.IsDone(idx) == gm_False) {
    next = records.getNext(idx);
    if ((*function)(this, records.getData(idx), functionData)==gm_False) {
      records.EndTraversal();
      return gm_False;
    }
    idx = next;
  }
  records.EndTraversal();
  return gm_True;
}



