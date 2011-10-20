#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "defines.h"
#include "linkedlist.h"
#include <stdio.h>


#define DefaultNumberOfBuckets 13


class Database;
struct DatabaseRecord {
  virtual ~DatabaseRecord() { };
};

struct IndexKey {
  virtual gm_Bool   Equal(IndexKey *key)=0;
  virtual UINT32 Hash()=0;
};


typedef gm_Bool (*ForEachFunction)(Database*, DatabaseRecord*, void*);


class Index {
private:
  typedef List<DatabaseRecord> Bucket;

  Bucket *buckets;
  int     numberOfBuckets;
  gm_Bool    duplicatesAllowed;

  virtual IndexKey *getIndexKey(DatabaseRecord *record)=0;
  inline  int Normalize(int hashValue);

public:
  inline Index(gm_Bool dupsAllowed=gm_False, 
	       int _numberOfBuckets=DefaultNumberOfBuckets);
  virtual ~Index();

  inline gm_Bool Add(DatabaseRecord *record);
  void Remove(DatabaseRecord *record);
  void Remove(IndexKey       *key);

  DatabaseRecord* FindOne(IndexKey *key);
  void            Find   (IndexKey *key, List<DatabaseRecord> *list);
  inline          List<DatabaseRecord> *getBucket(IndexKey *key); /* XXX */
};


class Database {
public:
  Database() { };
  virtual ~Database();

  gm_Bool AddIndex(Index *index) { return indices.InsertAtHead(index); }
  void RemoveIndex(Index *index) { indices.Remove(index); }

  gm_Bool Add(DatabaseRecord *record);
  void Remove(DatabaseRecord *record);
  void DeleteAllRecords();

  gm_Bool ForEach(ForEachFunction function, void *functionData);
  List<DatabaseRecord> *getAllRecords();

private:
  List<Index>          indices;
  List<DatabaseRecord> records;
};



inline
Index::Index(gm_Bool dupsAllowed, int _numberOfBuckets)
  : buckets(NULL), numberOfBuckets(_numberOfBuckets),
    duplicatesAllowed(dupsAllowed)
{
  buckets = new Bucket[numberOfBuckets];
  if (buckets==NULL) numberOfBuckets = 0;
}


inline int
Index::Normalize(int hashValue)
{
  return ((hashValue < 0) ? -hashValue:hashValue) % numberOfBuckets;
}


inline gm_Bool
Index::Add(DatabaseRecord *record)
{
  int hashValue = Normalize(getIndexKey(record)->Hash()); 
  if (duplicatesAllowed==gm_False) {
    if (FindOne(getIndexKey(record))!=NULL) 
      Return(gm_False, errDuplicateIndexEntry);
  }
  return buckets[hashValue].InsertAtHead(record);
}


inline Index::Bucket *
Index::getBucket(IndexKey *key)
{
  int hashValue = Normalize(key->Hash());
  return &buckets[hashValue];
}


inline List<DatabaseRecord> *
Database::getAllRecords()
{ 
  return &records; 
}


inline UINT32
HashString(char *string)
{
  UINT32 hash = 0;
  char   *ptr;
  for (ptr=string; *ptr!=0; ptr++) hash += (UINT32) (*ptr);
  return hash;
}


#endif // __DATABASE_H__
