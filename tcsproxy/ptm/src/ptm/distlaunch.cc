#include "log.h"
#include "distlaunch.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


DistillerLauncher *PendingLaunch::distillerLauncher=NULL;
OptionDatabase *LaunchRecord::options=NULL;


DistillerLauncher::DistillerLauncher(OptionDatabase *options)
{
  if (launchDB.Create(options)==gm_False) {
    return;
  }

  const char *rsh=NULL, *rshArgs=NULL, *rhosts=NULL;
  rsh     = options->Find(Opt_Rsh);
  rshArgs = options->Find(Opt_RshArgs);
  rhosts  = options->Find(Opt_Hosts);
  
  if (rexec.Initialize(rsh, rshArgs, rhosts)==gm_False) {
    // error occurred; return
    return;
  }

  LaunchRecord::options = options;
}


gm_Bool
DistillerLauncher::OptionsUpdate(OptionDatabase *options)
{
  if (launchDB.Update(options)==gm_False) {
    return gm_False;
  }

  const char *rsh=NULL, *rshArgs=NULL, *rhosts=NULL;
  rsh     = options->Find(Opt_Rsh);
  rshArgs = options->Find(Opt_RshArgs);
  rhosts  = options->Find(Opt_Hosts);
  
  if (rexec.Update(rsh, rshArgs, rhosts)==gm_False) {
    // error occurred; return
    return gm_False;
  }

  LaunchRecord::options = options;
  return gm_True;
}


void
DistillerLauncher::RemoveBrokenConnection(PrivateConnection *object)
{
  ListIndex idx1, idx2;
  LaunchRequest *current;
  PendingLaunch *pendingLaunch;

  idx1 = pendingList.BeginTraversal();
  for (; pendingList.IsDone(idx1)==gm_False;
       idx1=pendingList.getNext(idx1)) {
    
    pendingLaunch = pendingList.getData(idx1);

    idx2 = pendingLaunch->waitingList.BeginTraversal();
    while (pendingLaunch->waitingList.IsDone(idx2)==gm_False) {
      current = pendingLaunch->waitingList.getData(idx2);
      idx2 = pendingLaunch->waitingList.getNext(idx2);
      if (current->replyObject==object) {
	pendingLaunch->waitingList.Remove(idx2);
      }
    }
    pendingLaunch->waitingList.EndTraversal();
  }
  pendingList.EndTraversal();
}


gm_Bool
DistillerLauncher::TryToLaunch(DistillerType *distillerType,
			       PrivateConnection *replyObject, 
			       UINT32 replyID)
{
  LaunchRecord  *launchRecord;
  PendingLaunch *pendingLaunch;

  // try to locate a sleeping distiller

  // locate an appropriate distiller binary to launch

  launchRecord = launchDB.FindMatchingDistiller(distillerType);

  if (launchRecord==NULL) {
    // can't find an appropriate distiller :(
    gm_Log("Could not find any distiller of type " << (*distillerType) <<'\n');
    if (replyObject!=NULL) 
      replyObject->AskForDistillerReply(replyID, ptmNoDistiller, NULL);
    // ignore the return value
    return gm_True;
  }

  // check if there is already a pending launch for that binary
  pendingLaunch = FindPendingLaunch(launchRecord);
  if (pendingLaunch!=NULL) {
    // there is a pending launch
    gm_Log("Launch already pending for distiller (type = "
	   << pendingLaunch->launchRecord->type       << "): " 
	   << pendingLaunch->launchRecord->executable << '\n');

    return pendingLaunch->Append(replyObject, replyID);
  }

  // no pending launch; so initiate one
  pendingLaunch = new PendingLaunch(launchRecord, this);
  if (pendingLaunch==NULL) return gm_False;

  if (pendingLaunch->Append(replyObject, replyID)==gm_False) return gm_False;

  gm_Log("Initiating launch of distiller (type = " 
	 << pendingLaunch->launchRecord->type       << "): " 
	 << pendingLaunch->launchRecord->executable << '\n');

  if (pendingLaunch->launchRecord->Launch(pendingLaunch->getID())==gm_False) {
    // launch didn't succeed, no need for pendingLaunch 
    // (launch will fail only if fork() fails i.e. no more processes)
    pendingLaunch->LaunchFailed();
    delete pendingLaunch;
  }

  return gm_True;
}


PendingLaunch *
DistillerLauncher::FindPendingLaunch(LaunchRecord *record)
{
  ListIndex idx;
  PendingLaunch *returnValue = NULL;

  idx = pendingList.BeginTraversal();
  for (;
       pendingList.IsDone(idx)==gm_False; 
       idx = pendingList.getNext(idx)) {
    if (pendingList.getData(idx)->launchRecord==record) {
      returnValue = pendingList.getData(idx);
      break;
    }
  }
  pendingList.EndTraversal();

  return returnValue;
}


void
LaunchRecord::ParseParam(ts_strtok_state *state, char *&token, 
			 UINT32 &variable, UINT32 defaultValue)
{
  if (token!=NULL) {
    // we've already hit the end of the param list; just set the default
    variable = defaultValue;
    return;
  }

  token = ts_strtok(" \t", state);
  if (token==NULL) {
    variable = defaultValue;
  }
  else {
    if (*token=='-') {
      // use the default value
      variable = defaultValue;
      
      if (*(token+1) == '\0') {
	// this is an explicit arg to use the default value
	token = NULL;
      }
      // else don't set token to NULL!!!
    }
    else {
      variable = strtoul(token, NULL, 10);
      if (variable==0) variable = defaultValue;
      token = NULL;
    }
  }
}


gm_Bool
LaunchRecord::Parse(const char *optionsLine)
{
  ts_strtok_state *state;
  int idx, size;
  char *token, **newArgv;
  UINT32 history_ms;
  gm_Bool beforeExtraArgs=gm_False, expectingDashNArg=gm_False;

  if ((state = ts_strtok_init((char*)optionsLine))==NULL) {
    Return(gm_False, errGenericError);
  }

  // parse out the executable pathname and load-balancing params
  token = ts_strtok(" \t", state);
  if (token==NULL) {
    Return(gm_False, errGenericError);
  }
  safe_strncpy(executable, token, MAXPATH);

  token = NULL;
  ParseParam(state, token, autoLaunchLimit, DefaultAutoLaunchLimit);
  ParseParam(state, token, history_ms, DefaultLoadHistoryPeriod_ms);
  notificationTimeout_ms = DefaultNotificationTimeout_ms;

  // parse out extra cmdline args to the distiller
  if (argv!=NULL) {
    for (idx=0; idx<argc; idx++) delete [] argv[idx];
    delete [] argv;
  }

  size = 2;
  argc = 0;
  argv = NULL;

  if (token==NULL) token = ts_strtok(" \t", state);
  while (token != NULL) {
    argc++;
    gm_Log("Parsing out argument " << argc << ": " << token << "\n");
    if (argc > size || argv==NULL) {
      size *= 2;
      newArgv = new char* [size];
      if (newArgv==NULL) Return(gm_False, errOutOfMemory);
      if (argv!=NULL) {
	for (idx=0; idx < size/2; idx++) {
	  newArgv[idx] = argv[idx];
	}
	delete [] argv;
      }
      argv = newArgv;
    }

    argv[argc-1] = new char [strlen(token)+1];
    if (argv[argc-1]==NULL) Return(gm_False, errOutOfMemory);
    strcpy(argv[argc-1], token);

    if (beforeExtraArgs==gm_True) {
      // parse out the notification timeout argument if present
      if (expectingDashNArg==gm_True) {
	expectingDashNArg = gm_False;
	notificationTimeout_ms = strtoul(argv[argc-1], NULL, 10);
	if (notificationTimeout_ms==0) 
	  notificationTimeout_ms = DefaultNotificationTimeout_ms;
      }

      if (strcmp(argv[argc-1], "--")==0) {
	beforeExtraArgs = gm_False;
      }
      else if (strcmp(argv[argc-1], "-n")==0) {
	expectingDashNArg = gm_True;
      }
    }

    token = ts_strtok(" \t", state);
  }

  ts_strtok_finish(state);

  loadHistorySize = history_ms/notificationTimeout_ms;
  if (loadHistorySize<=0) loadHistorySize = 1;

  return gm_True;
}



void
LaunchRecord::TryToAutoLaunch()
{
  timeval diff, now;
  UINT32 diff_ms;
  gettimeofday(&now, NULL);
  diff = tv_timesub(now, lastLaunch);
  diff_ms = timeval_to_ms(diff);
  if (diff_ms <= AutoLaunchHysteresisInterval_ms) {
    // we just launched a distiller of the same time, some time ago
    return;
  }
  gm_Log("Load on distiller " << type
	 << " too high. Trying to auto-launch a new one\n");
  DistillerRecord *dist = PTM::getInstance()->getDistillerDB()->
    WakeSleepingDistiller(&type);
  if (dist!=NULL) {
    gm_Log("Found a sleeping distiller at " << dist->rid << '\n');
    // update lastLaunch field, so that a new distiller doesn't get launched 
    // immediately
    gettimeofday(&lastLaunch, NULL);
  }
  else
    PTM::getInstance()->getDistillerLauncher()->TryToLaunch(&type, NULL, 0);
}


void
LaunchRecord::Relaunch()
{
 gm_Log("Distiller of type " << type
	 << " just died. Trying to launch a new one\n");
  DistillerRecord *dist = PTM::getInstance()->getDistillerDB()->
    WakeSleepingDistiller(&type);
  if (dist!=NULL) {
    gm_Log("Found a sleeping distiller at " << dist->rid << '\n');
    // update lastLaunch field, so that a new distiller doesn't get launched 
    // immediately
    gettimeofday(&lastLaunch, NULL);
  }
  else
    PTM::getInstance()->getDistillerLauncher()->TryToLaunch(&type, NULL, 0);
}



void
LaunchRecord::UpdateLoad(Load &newLoad, Load &prevLoad)
{
  Load avg;
  if (autoLaunchLimit==0) {
    // don't auto-launch; so don't bother doing anything!
    return;
  }

  totalLoad -= prevLoad;
  totalLoad += newLoad;

  if (numberOfActiveDistillers > 0) {
    avg.Average(totalLoad, numberOfActiveDistillers);
    if (avg.totalLatency_ms > autoLaunchLimit) TryToAutoLaunch();
  }
}


gm_Bool
LaunchRecord::Launch(UINT32 requestID)
{
  char **distArgv, **current; //, executablePath[1024];

  distArgv = new char* [argc+25];
  current = distArgv;
  *current++ = executable;

  char ptmAddressString[50],
    notificationTimeoutString[50],
    distillerTypeString[MAXDISTILLER_TYPE_STRING+5],
    requestIDString[25],
    optionsFileString[MAXPATH+3], *quote="";
  
  if (PTM::getInstance()->getDistillerLauncher()->getRExec()->
      IsRemoteExecution()==gm_True) quote = "\"";
  sprintf(ptmAddressString, "-p%s/%d", 
	  PTM::getInstance()->getAddress().ipAddress,
	  PTM::getInstance()->getAddress().port);
  sprintf(distillerTypeString, "%s-t%s%s", quote, type.c_struct.string, quote);
  sprintf(notificationTimeoutString, "-n%lu", notificationTimeout_ms);
  sprintf(requestIDString, "-r%lu", requestID);

  sprintf(optionsFileString, "%s-o%s%s", quote, 
	  options->Find(Opt_OptionsFile), quote);
  
  *current++ = ptmAddressString;
  *current++ = distillerTypeString;
  *current++ = notificationTimeoutString;
  *current++ = requestIDString;
  *current++ = optionsFileString;

  for (int i=0; i<argc; i++) {
    gm_Log("Setting argument: " << argv[i] << "\n");
    *current++ = argv[i];
  }

  *current++ = NULL;
  gettimeofday(&lastLaunch, NULL);
  if ( (PTM::getInstance()->getDistillerLauncher()->
	getRExec()->Exec(*distArgv, distArgv)) == gm_False) {
    delete [] distArgv;
    return gm_False;
  }
  
  delete [] distArgv;
  return gm_True;
}


void
LaunchRecord::Die()
{
  if (PTM::getInstance()->getDistillerLauncher()->
      FindPendingLaunch(this)!=NULL) {
    // there is a pending launch for this distiller! we can't die right away
    mustDie = gm_True;
  }
  else {
    delete this;
  }
}


PendingLaunch::PendingLaunch(LaunchRecord *r, DistillerLauncher *launcher)
  : RequestReply(PTM::getInstance()->getEventSystem(), 
		 SECONDS(LaunchTimeout_ms), USECONDS(LaunchTimeout_ms)), 
    launchRecord(r),
    countOfTimeouts(0)
{
  distillerLauncher = launcher;
  if (distillerLauncher->AddPendingLaunch(this)==gm_False) return;
}


PendingLaunch::~PendingLaunch()
{
  if (launchRecord->mustDie==gm_True) {
    // this distiller type no longer exists! we must destroy this object
    delete launchRecord;
  }

  distillerLauncher->RemovePendingLaunch(this);
  while(waitingList.IsEmpty()==gm_False) {
    delete waitingList.RemoveFromHead();
  }
}



gm_Bool
PendingLaunch::Append(PrivateConnection *replyObject, UINT32 replyID)
{
  LaunchRequest *request = new LaunchRequest(replyObject, replyID);
  if (request==NULL) Return(gm_False, errOutOfMemory);
  return waitingList.InsertAtHead(request);
}


gm_Bool
PendingLaunch::EvReplyReceived(RequestReplyEventSystem */*evs*/, void *args)
{
  DistillerRecord *newDistiller = (DistillerRecord*)args;

  if (newDistiller->sleepStatus==distWakeUp) {
    ListIndex idx;
    List<DatabaseRecord> list;
    if (list.InsertAtHead(newDistiller)==gm_False) {
      delete this;
      return gm_False;
    }
    
    gm_Log("Launch for distiller (type = " << launchRecord->type
	   << ") succeeded\n");
    idx = waitingList.BeginTraversal();
    for (; waitingList.IsDone(idx)==gm_False; idx=waitingList.getNext(idx)) {
      if (waitingList.getData(idx)->replyObject!=NULL && 
	  waitingList.getData(idx)->replyObject->
	  AskForDistillerReply(waitingList.getData(idx)->replyID, ptmOk, &list)
	  ==gm_False) {
	// ignore error
	Error::SetStatus(success);
      }
    }
    waitingList.EndTraversal();
    delete this;
  }
  else {
    gm_Log("Distiller of type " << newDistiller->type << " succeeded; but the "
	   "distiller is " << ((newDistiller->sleepStatus==distSleep) ?
			       "sleeping" : "trying to die") << '\n');
  }

  return gm_True;
}


gm_Bool
PendingLaunch::EvTimer(RequestReplyEventSystem */*evs*/)
{
  countOfTimeouts++;

  if (countOfTimeouts >= MaxLaunchTimeouts) {
    gm_Log("Launch of distiller (type = " << launchRecord->type<<") timed out "
	 << countOfTimeouts << " times. Aborting.\n");
    LaunchFailed();
    delete this;
    return gm_True;
  }

  gm_Log("Launch of distiller (type = " << launchRecord->type 
	 << ") timed out. Trying again.\n");

  if (launchRecord->Launch(getID())==gm_False) {
    LaunchFailed();
    delete this;
    Error::SetStatus(success);
    return gm_True;
  }

  Retry(SECONDS(LaunchTimeout_ms), USECONDS(LaunchTimeout_ms));
  return gm_True;
}


gm_Bool 
PendingLaunch::LaunchFailed()
{
  ListIndex idx;

  idx = waitingList.BeginTraversal();
  for (; waitingList.IsDone(idx)==gm_False; 
       idx=waitingList.getNext(idx)) {

    if (waitingList.getData(idx)->replyObject!=NULL && 
	waitingList.getData(idx)->replyObject->
	AskForDistillerReply(waitingList.getData(idx)->replyID, 
			     ptmDistillerLaunchTimeout, NULL)==gm_False) {
      // ignore it
      Error::SetStatus(success);
    }
  }
  waitingList.EndTraversal();
  return gm_True;
}


gm_Bool
LaunchDatabase::Sweep()
{
  List<LaunchRecord> list;
  ListIndex idx;
  LaunchRecord *record;
  if (ForEach(SweepHelper, &list)==gm_False) return gm_False;

  idx = list.BeginTraversal();
  for (; list.IsDone(idx) == gm_False;
       idx =  list.getNext(idx)) {
    
    record = list.getData(idx);
    Remove(record);
    record->Die();
  }
  list.EndTraversal();
  return gm_True;
}


gm_Bool
LaunchDatabase::SweepHelper(Database */*db*/, DatabaseRecord *r, 
			    void *args)
{
  LaunchRecord *record = (LaunchRecord*) r;
  List<LaunchRecord> *list = (List<LaunchRecord> *) args;

  if (record->marked==gm_True) {
    // this record is still marked i.e. we didn't find an entry for it in the
    // new options file
    // we should delete this record from the launch database
    gm_Log("Removing auto-launch record for " << record->type << "\n");
    list->InsertAtTail(record);
  }
  return gm_True;
}



gm_Bool
LaunchDatabase::ForEachCreate(OptionDatabase */*options*/, const char *key, 
			      const char *value, void *args)
{
  // ignore all options that do not start with "dist."
  if (strncmp(key, "dist.", 5)!=0) return gm_True;

  key += 5;
  LaunchDatabase *this_ = (LaunchDatabase*) args;
  DatabaseRecord *record = this_->CreateRecord(key, value);
  if (record==NULL) return gm_False;
  if (this_->Add(record)==gm_False) return gm_False;
  return gm_True;
}


gm_Bool
LaunchDatabase::ForEachUpdate(OptionDatabase */*options*/, const char *key, 
			      const char *value, void *args)
{
  // ignore all options that do not start with "dist."
  if (strncmp(key, "dist.", 5)!=0) return gm_True;
  
  key += 5;
  LaunchDatabase *this_ = (LaunchDatabase*) args;
  DistillerType type(key);
  LaunchRecord *record = this_->getLaunchRecord(&type);
  if (record==NULL) {
    // this is a brand new record
    DatabaseRecord *record = this_->CreateRecord(key, value);
    if (record==NULL) return gm_False;
    if (this_->Add(record)==gm_False) return gm_False;
    this_->Unmark((LaunchRecord*) record);
    return gm_True;
  }
  else {
    this_->Unmark(record);
    gm_Log("Updating launch record for " << record->type << ": " 
	   << value << "\n");
    return this_->UpdateRecord(record, value);
  }
}


DatabaseRecord*
LaunchDatabase::CreateRecord(const char *key, const char *value)
{
  /*char path[MAXPATH];
  UINT32 autoLaunchLimit=0, history_ms=0, notificationTimeout_ms=0;
  int historySize=0, itemsScanned;*/
  DistillerType type(key);

  /*itemsScanned = sscanf(value, "%s %lu %lu %lu",path, &autoLaunchLimit,
			&history_ms, &notificationTimeout_ms);
  switch (itemsScanned) {
  case 1:
    // we were able to read neither autoLaunchLimit nor history_ms
    autoLaunchLimit = DefaultAutoLaunchLimit;
    // no break here!

  case 2:
    // we weren't able to read history_ms
    history_ms = DefaultLoadHistoryPeriod_ms;
    // no break here!

  case 3:
    // we weren't able to read notificationTimeout_ms
    notificationTimeout_ms = DefaultNotificationTimeout_ms;
    // no break here!

  case 4:
    // we are fine
    break;

  default:
    // something bad happenned
    Return(NULL, errFormattingError);
  }

  if (notificationTimeout_ms==0) 
    notificationTimeout_ms = DefaultNotificationTimeout_ms;
  historySize = history_ms/notificationTimeout_ms;
  if (historySize==0) historySize = 1;*/

  gm_Log("Creating auto-launch record for distiller " << key << ": " 
	 << value << "\n");
  LaunchRecord *record = new LaunchRecord(type, value);
  if (record==NULL) Return(NULL, errOutOfMemory);
  if (Error::getStatus()!=success) {
    delete record;
    return NULL;
  }
  return record;
}


gm_Bool
LaunchDatabase::UpdateRecord(LaunchRecord *record, const char *optionsLine)
{
  return record->Parse(optionsLine);
}


LaunchRecord *
LaunchDatabase::FindMatchingDistiller(DistillerType *type)
{
  MatchingDistillerFinder finder(type);
  ForEach(MatchEachDistiller, (void*) &finder); 
  return finder.launchRecord;
}


gm_Bool
LaunchDatabase::MatchEachDistiller(Database */*db*/, DatabaseRecord *record, 
				   void *args)
{
  MatchingDistillerFinder *finder = (MatchingDistillerFinder*) args;

  if (((LaunchRecord*)record)->type.CanSatisfy(finder->distillerType)==gm_True) {
    finder->launchRecord = (LaunchRecord*) record;
    return gm_False;
  }

  return gm_True;
}


struct getLRFinder {
  getLRFinder(DistillerType *type_) 
    : record(NULL), type(type_) { };
  LaunchRecord *record;
  DistillerType *type;
};


LaunchRecord *
LaunchDatabase::getLaunchRecord(DistillerType *type)
{
  getLRFinder finder(type);
  ForEach(getLRHelper, (void*) &finder);
  return finder.record;
}


gm_Bool
LaunchDatabase::getLRHelper(Database */*db*/, DatabaseRecord *record, 
			    void *args)
{
  getLRFinder *finder = (getLRFinder*)args;
  if (((LaunchRecord*)record)->type.Equal(finder->type)==gm_True) {
    finder->record = ((LaunchRecord*)record);
    return gm_False;
  }
  return gm_True;
}
