#include "options.h"
#include "defines.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>


gm_Bool
BasicOptionDatabase::Create(const char *filename)
{
  FILE *fp;
  char line[MAXLINE], key[MAXLINE], value[MAXLINE], *ptr, *start;
  DatabaseRecord *record;

  fp = fopen(filename, "r");
  if (fp==NULL) Return(gm_False, errFileOpenError);

  while (fgets(line, MAXLINE, fp)!=NULL) {
    // each line must be of the form "key: value"
    // comments start with ! (same syntax as the .Xdefaults file)
    ptr = line;

    // ignore leading whitespace
    while(isspace(*ptr)) ptr++;
    if (*ptr=='!') {
      // this is a comment; ignore it
      continue;
    }

    // extract the option key
    start = ptr;
    while(!isspace(*ptr) && *ptr!=':' && *ptr!='\0') ptr++;
    if (ptr==start || *ptr=='\0') {
      // invalid line; ignore it
      continue;
    }
    strncpy(key, start, ptr-start);
    key[ptr-start] = '\0';

    // ignore whitespace
    while(isspace(*ptr)) ptr++;
    if (*ptr!=':') {
      // invalid line; ignore it
      continue;
    }

    ptr++;
    // ignore whitespace
    while(isspace(*ptr)) ptr++;

    // extract the option value
    start = ptr;

    ptr = line + strlen(line) - 1;
    while(ptr >= start && isspace(*ptr)) ptr--;
    *(ptr+1) = '\0';
    strcpy(value, start);

    record = CreateRecord(key, value);
    if (record==NULL) return gm_False;
    if (Add(record)==gm_False) return gm_False;
  }

  fclose(fp);

  record = CreateRecord(Opt_OptionsFile, filename);
  if (record==NULL) return gm_False;
  return Add(record);
}




struct OptionKey : public IndexKey {
  OptionKey(const char *s) { strcpy(string, s); };
  gm_Bool Equal(IndexKey *key) {
    OptionKey *optKey = (OptionKey*) key;
    return (strcmp(optKey->string, string)==0) ? gm_True : gm_False; 
  };
  UINT32 Hash() { return HashString(string); }

  char string[MAXLINE];
};


struct Option : public DatabaseRecord {
  Option(const char *key_, const char *value_) 
    : key(key_), value(NULL) { 
      value = new char [strlen(value_)+1];
      if (value!=NULL) strcpy(value, value_); 
  }
  ~Option() {
    delete [] value;
  }

  OptionKey key;
  char *value;
};


class OptionIndex : public Index {
public:
  OptionIndex(gm_Bool dupsAllowed=gm_False,
	      int numberOfBuckets=DefaultNumberOfBuckets)
    : Index(dupsAllowed, numberOfBuckets) { };
protected:
  IndexKey *getIndexKey(DatabaseRecord *record) 
  { return &((Option*)record)->key; }
};


OptionDatabase *OptionDatabase::mainOptionDB = NULL;


OptionDatabase::OptionDatabase()
  : BasicOptionDatabase(), index(NULL)
{
  if ((index = new OptionIndex)==NULL) VoidReturn(errOutOfMemory);
  AddIndex(index); // ignore return value
  mainOptionDB = this;
}


OptionDatabase::~OptionDatabase()
{
  if (mainOptionDB==this) mainOptionDB = NULL;
  if (index!=NULL) {
    RemoveIndex(index);
    delete index;
    index = NULL;
  }

  DeleteAllRecords();
}


DatabaseRecord*
OptionDatabase::CreateRecord(const char *key, const char *value)
{
  if (strlen(key)  >= sizeof(OptionKey::string)) {
    Return(NULL, errBufferOverflow);
  }

  OptionKey okey(key);
  Option *option;
  option = (Option*)index->FindOne(&okey);
  if (option!=NULL) {
    int newLen = strlen(option->value) + strlen(value) + 1;
    char *newValue = new char [newLen];
    if (newValue==NULL) Return(NULL, errOutOfMemory);
    sprintf(newValue, "%s %s", option->value, value);
    delete [] option->value;
    option->value = newValue;
    BasicOptionDatabase::Remove(option);
  }
  else {
    option = new Option(key, value);
    if (option==NULL) Return(NULL, errOutOfMemory);
  }
  return option;
}


struct ForEachHelperStruct {
  OptionDatabase::ForEachFunction func;
  void *args;
  ForEachHelperStruct(OptionDatabase::ForEachFunction f, void *a)
    : func(f), args(a) { }
};


gm_Bool 
OptionDatabase::ForEach(ForEachFunction func, void *args)
{
  ForEachHelperStruct helper(func, args);
  return BasicOptionDatabase::ForEach(ForEachHelper, &helper);
}


gm_Bool
OptionDatabase::ForEachHelper(Database *db, DatabaseRecord *record,
			      void *args)
{
  ForEachHelperStruct *helper = (ForEachHelperStruct*) args;
  Option *option = (Option*) record;
  return (*helper->func)((OptionDatabase*)db, option->key.string, 
			 option->value, helper->args);
}




void
OptionDatabase::Remove(const char *key_)
{
  OptionKey key(key_);
  Option *option = (Option*)index->FindOne(&key);
  if (option!=NULL) {
    BasicOptionDatabase::Remove(option);
    delete option;
  }
}


const char *
OptionDatabase::Find(const char *key_)
{
  OptionKey key(key_);
  Option *option = (Option*)index->FindOne(&key);
  if (option==NULL) return NULL;
  else return option->value;
}


UINT32
OptionDatabase::FindUINT32(const char *key_)
{
  const char *value = Find(key_);
  if (value==NULL) return 0;
  else return strtoul(value, NULL, 10);
}


gm_Bool
OptionDatabase::Add(const char *key, const char *value)
{
  DatabaseRecord *record = CreateRecord(key, value);
  if (record==NULL) return gm_False;
  return BasicOptionDatabase::Add(record);
}


gm_Bool
OptionDatabase::Add(const char *key, UINT32 value)
{
  char valueString[80];

  sprintf(valueString, "%lu", value);
  return Add(key, valueString);
}



#include "optdb.h"
int
Options_New(Options *options, char *filename)
{
  OptionDatabase *db = new OptionDatabase;
  int returnValue;
  if (db==NULL) return errOutOfMemory;

  if (db->Create(filename)==gm_False) {
    delete db;
    returnValue = Error::getStatus();
    Error::SetStatus(success);
    return returnValue;
  }
  *options = (Options) db;
  return success;
}


void
Options_Delete(Options options)
{
  OptionDatabase *db = (OptionDatabase*)options;
  delete db;
}


int
Options_Add(Options options, const char *key, const char *value)
{
  OptionDatabase *db = (OptionDatabase*)options;
  int returnValue;

  if (db==NULL) {
    db = OptionDatabase::mainOptionDB;
    if (db==NULL) Return(errGenericError, errGenericError);
  }

  if (db->Add(key, value)==gm_False) {
    returnValue = Error::getStatus();
    Error::SetStatus(success);
    return returnValue;
  }
  return success;
}


int
Options_Add_UINT32(Options options, const char *key, UINT32 value)
{
  OptionDatabase *db = (OptionDatabase*)options;
  int returnValue;

  if (db==NULL) {
    db = OptionDatabase::mainOptionDB;
    if (db==NULL) Return(errGenericError, errGenericError);
  }

  if (db->Add(key, value)==gm_False) {
    returnValue = Error::getStatus();
    Error::SetStatus(success);
    return returnValue;
  }
  return success;
}


void
Options_Remove(Options options, const char *key)
{
  OptionDatabase *db = (OptionDatabase*)options;
  if (db==NULL) {
    db = OptionDatabase::mainOptionDB;
    if (db==NULL) VoidReturn(errGenericError);
  }

  db->Remove(key);
}


const char *
Options_Find(Options options, const char *key)
{
  OptionDatabase *db = (OptionDatabase*)options;
  if (db==NULL) {
    db = OptionDatabase::mainOptionDB;
    if (db==NULL) Return(NULL, errGenericError);
  }

  return db->Find(key);
}


UINT32
Options_Find_UINT32(Options options, const char *key)
{
  OptionDatabase *db = (OptionDatabase*)options;
  if (db==NULL) {
    db = OptionDatabase::mainOptionDB;
    if (db==NULL) Return(0, errGenericError);
  }

  return db->FindUINT32(key);
}


Options
Options_DefaultDb()
{
  return (Options)OptionDatabase::mainOptionDB;
}
