#ifndef __DISTLAUNCH_H__
#define __DISTLAUNCH_H__

#include "distdb.h"
#include "ptm.h"
#include "options.h"
#include "rexec.h"
#include "utils.h"


struct LaunchRecord : public DatabaseRecord {
  LaunchRecord(DistillerType &t, const char *optionsLine)
    : type(t), argv(NULL), argc(0), autoLaunchLimit(DefaultAutoLaunchLimit), 
      notificationTimeout_ms(DefaultNotificationTimeout_ms),
      loadHistorySize(DefaultLoadHistorySize),
      numberOfActiveDistillers(0), marked(gm_False), mustDie(gm_False) {

	gettimeofday(&lastLaunch, NULL);
	Parse(optionsLine);
  }
  ~LaunchRecord() {
    for (int i=0; i < argc; i++) delete [] argv[i];
    delete [] argv;
  }

  void Die();
  gm_Bool Launch(UINT32 requestID);
  void EvRegisterDistiller(DistillerRecord */*dist*/) {
    numberOfActiveDistillers++;
  }
  void EvDeregisterDistiller(DistillerRecord *dist) {
    numberOfActiveDistillers--;
    totalLoad -= dist->load;
  }
  void UpdateLoad(Load &newLoad, Load &prevLoad);
  gm_Bool Parse(const char *optionsLine);
  void ParseParam(ts_strtok_state *state, char *&token, UINT32 &variable, 
		   UINT32 defaultValue);

  DistillerType type;
  char    executable[MAXPATH];
  char    **argv;
  int     argc;
  UINT32  autoLaunchLimit, notificationTimeout_ms;
  int     loadHistorySize;
  timeval lastLaunch;

  void TryToAutoLaunch();
  void Relaunch();

  Load totalLoad;
  int  numberOfActiveDistillers;


  gm_Bool marked, mustDie;


  static OptionDatabase *options;
};


class LaunchDatabase : public Database {
public:
  LaunchDatabase() { }
  ~LaunchDatabase() { }
  gm_Bool Create(OptionDatabase *options) { 
    return options->ForEach(ForEachCreate, this);
  }
  gm_Bool Update(OptionDatabase *options) {
    gm_Log("Updating the auto-launch database:1\n");
    if (Mark()==gm_False) return gm_False;
    gm_Log("Updating the auto-launch database:2\n");
    if (options->ForEach(ForEachUpdate, this)==gm_False) return gm_False;
    gm_Log("Updating the auto-launch database:3\n");
    if (Sweep()==gm_False) return gm_False;
    gm_Log("Updating the auto-launch database:done\n");
    return gm_True;
  }

  LaunchRecord *FindMatchingDistiller(DistillerType *type);
  LaunchRecord *getLaunchRecord(DistillerType *type);
private:
  DatabaseRecord *CreateRecord(const char *key, const char *value);
  gm_Bool UpdateRecord(LaunchRecord *record, const char *optionsLine);

  struct MatchingDistillerFinder {
    MatchingDistillerFinder(DistillerType *type) 
      : distillerType(type), launchRecord(NULL) { };

    DistillerType *distillerType;
    LaunchRecord  *launchRecord;
  };


  gm_Bool Mark() { return ForEach(MarkHelper, NULL); }
  gm_Bool Sweep();
  gm_Bool Unmark(LaunchRecord *record) { 
    record->marked = gm_False; 
    return gm_True; 
  }

  static gm_Bool MarkHelper(Database */*db*/, DatabaseRecord *record, 
			    void */*args*/) {
    ((LaunchRecord*)record)->marked = gm_True;
    return gm_True;
  }

  static gm_Bool SweepHelper(Database *db, DatabaseRecord *record, 
			     void *args);
  static gm_Bool MatchEachDistiller(Database *db, DatabaseRecord *record, 
				    void *args);
  static gm_Bool getLRHelper(Database *db, DatabaseRecord *record, 
			     void *args);
  static gm_Bool ForEachCreate(OptionDatabase *options, const char *key,
			       const char *value, void *args);
  static gm_Bool ForEachUpdate(OptionDatabase *options, const char *key,
			       const char *value, void *args);
};



#ifdef OLD
class LaunchDatabase : public BasicOptionDatabase {
public:
  LaunchRecord *FindMatchingDistiller(DistillerType *type);
  LaunchRecord *getLaunchRecord(DistillerType *type);
protected:
  DatabaseRecord *CreateRecord(const char *key, const char *value);
  
  struct MatchingDistillerFinder {
    MatchingDistillerFinder(DistillerType *type) 
      : distillerType(type), launchRecord(NULL) { };

    DistillerType *distillerType;
    LaunchRecord  *launchRecord;
  };

  static gm_Bool MatchEachDistiller(Database *db, DatabaseRecord *record, 
				    void *args);
  static gm_Bool getLRHelper(Database *db, DatabaseRecord *record, 
			     void *args);
};
#endif



struct LaunchRequest {
  LaunchRequest(PrivateConnection *object, UINT32 id)
    : replyObject(object), replyID(id) { };
  PrivateConnection *replyObject;
  UINT32 replyID;
};




struct PendingLaunch : public RequestReply {
  PendingLaunch(LaunchRecord *r, DistillerLauncher *launcher);
  virtual ~PendingLaunch();

  gm_Bool Append(PrivateConnection *replyObject, UINT32 replyID);
  gm_Bool LaunchFailed();

  virtual gm_Bool EvReplyReceived(RequestReplyEventSystem *evs, void *args);
  virtual gm_Bool EvTimer(RequestReplyEventSystem *evs);

  LaunchRecord *launchRecord;
  int countOfTimeouts;
  List<LaunchRequest> waitingList;

  static DistillerLauncher *distillerLauncher;
};





class DistillerLauncher {
public:
  DistillerLauncher(OptionDatabase *options);
  gm_Bool OptionsUpdate(OptionDatabase *options);
  /*gm_Bool UpdateDatabase(char *dbFilename) 
  { launchDB.DeleteAllRecords(); return launchDB.Create(dbFilename); };*/

  RExec *getRExec() { return &rexec; };
  gm_Bool TryToLaunch(DistillerType *type, 
		   PrivateConnection *replyObject, UINT32 replyID);
  void RemoveBrokenConnection(PrivateConnection *object);

  gm_Bool AddPendingLaunch(PendingLaunch *pendingLaunch) 
  { return pendingList.InsertAtHead(pendingLaunch); };
  void RemovePendingLaunch(PendingLaunch *pendingLaunch)
  { pendingList.Remove(pendingLaunch); };
  LaunchRecord *getLaunchRecord(DistillerType *type, gm_Bool matchAny) {
    if (matchAny==gm_True) {
      return launchDB.FindMatchingDistiller(type);
    } else {
      return launchDB.getLaunchRecord(type);
    }
  }
  PendingLaunch *FindPendingLaunch(LaunchRecord *record);
protected:
  List<PendingLaunch> pendingList;
  LaunchDatabase launchDB;
  RExec rexec;
};




#endif // __DISTLAUNCH_H_
