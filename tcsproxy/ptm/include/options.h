#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include "database.h"


class BasicOptionDatabase : public Database {
public:
  BasicOptionDatabase() : Database() { };
  virtual ~BasicOptionDatabase() { };

  gm_Bool Create(const char *filename);
  gm_Bool Update(const char *filename) { 
    DeleteAllRecords(); return Create(filename); 
  };

protected:
  DatabaseRecord *FindRecord(char *key);

  virtual DatabaseRecord *CreateRecord(const char *key, const char *value)=0;
};


class OptionIndex;

class OptionDatabase : public BasicOptionDatabase {
public:
  OptionDatabase();
  ~OptionDatabase();
  const char *Find(const char *key);
  UINT32 FindUINT32(const char *key);
  gm_Bool Add(const char *key, const char *value);
  gm_Bool Add(const char *key, UINT32 value);
  void    Remove(const char *key);

  typedef gm_Bool (*ForEachFunction)(OptionDatabase *options, const char *key,
				     const char *value, void *args);
  gm_Bool ForEach(ForEachFunction func, void *args);

  static OptionDatabase *mainOptionDB;
protected:
  static gm_Bool ForEachHelper(Database *db, DatabaseRecord *record, 
			       void *args);
  DatabaseRecord* CreateRecord(const char *key, const char *value);
  OptionIndex *index;
};




#endif /* __OPTIONS_H__ */
