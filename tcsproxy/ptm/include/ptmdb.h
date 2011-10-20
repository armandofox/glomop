#ifndef __PTMDB_H__
#define __PTMDB_H__

#include "distdb.h"


class OStream;
class LaunchRecord;
class PrivateConnection;


struct DistillerRecord : public BasicDistiller {
  DistillerRecord(PrivateConnection *privateConnection_,
		  RemoteID& _rid, DistillerType& _type, 
		  DistillerSleepStatus status, gm_Bool mustRestartOnDeath_);
  ~DistillerRecord();
  void Update(Load &newLoad);
  gm_Bool WakeUp();
  void EvUnnaturalDeath();

  PrivateConnection *privateConnection;
  DistillerSleepStatus sleepStatus;
private:
  Load *history;
  int  historySize;
  Load loadOverHistory;
  int  currentPos;
  LaunchRecord *launchRecord; // NULL if there isn't any

  gm_Bool mustRestartOnDeath;
};


class DistillerDatabase : public BasicDistillerDatabase {
public:
  gm_Bool FindMatchingDistillers(DistillerType *type, 
				 List<DatabaseRecord> *list);
  DistillerRecord *WakeSleepingDistiller(DistillerType *type);

protected:
  struct MatchingDistillerFinder {
    MatchingDistillerFinder(DistillerType *type,
			    List<DatabaseRecord> *list)
      : distillerType(type), distillerList(list) { };

    DistillerType        *distillerType;
    List<DatabaseRecord> *distillerList;
  };

  static gm_Bool MatchEachDistiller(Database *db, DatabaseRecord *record, 
				 void *args);
  static gm_Bool MatchSleepingDistiller(Database *db, DatabaseRecord *record, 
					void *args);
};


#endif // __PTMDB_H__
