#include "log.h"
#include "distdb.h"
#include "cacheman.h"
#include "cachenet.h"
#include "cache.h"
#include "reqrep.h"
#include "threads.h"
#include "proxyinterface.h"
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


CacheManager* CacheManager::instance=NULL;

CacheManager::CacheManager(OptionDatabase *options_, const char *monitorUnitID)
  : evs(NULL), 
    ptmConnection_(NULL), 
    bus(NULL),
    managerThread(NULL),
    cache(NULL),
    monitorClient(NULL),
    noBeaconTimer(NULL),
    options(options_),
    mgrMutex("*-mgr"),
    ptmFoundMutex("*-ptm-found")
{
  // expand options in the options database
  // install default options if required
  if (ExpandOptions(options_)==gm_False) return;

  const char *monitorAddr;
  Port monitorPort;
  int  monitorTTL;

  // initialize the random number generator
  timeval now;
  gettimeofday(&now, NULL);
  srand48(now.tv_usec + getpid());

  // create objects
  instance = this;
  NEW(cache, DistillerCache);
  NEW(evs,   RequestReplyEventSystem);
  NEW(managerThread, Thread);

  // initialize the remote executor
  if (rexec.Initialize(options->Find(Opt_Rsh), options->Find(Opt_RshArgs),
		       options->Find(Opt_Hosts))
      ==gm_False) return;

  // initialize the multicast socket
  NEW(bus, SharedBus(evs, options->Find(Opt_PTMMulticast_IP),
		     (Port) options->FindUINT32(Opt_PTMMulticast_port),
		     (int)  options->FindUINT32(Opt_PTMMulticast_TTL)));

  // initialize timers
  UINT32 beaconingPeriod_ms = options->FindUINT32(Opt_PTMBeacon_ms);
  if (beaconingPeriod_ms==0) beaconingPeriod_ms = DefaultBeaconingPeriod_ms;
  UINT32 lostBeacons = options->FindUINT32(Opt_LostBeacons);
  if (lostBeacons==0) lostBeacons = DefaultLostBeacons;
  UINT32 noBeaconTimeout = lostBeacons*beaconingPeriod_ms;
  NEW(noBeaconTimer, NoBeaconTimer(evs, noBeaconTimeout));
  startPTMTimer.Initialize(noBeaconTimeout);

  // initialize the monitor
  monitorAddr = options->Find(Opt_MonitorMulticast_IP);
  monitorPort = (Port) options->FindUINT32(Opt_MonitorMulticast_port);
  monitorTTL  = (int)  options->FindUINT32(Opt_MonitorMulticast_TTL);
  if (monitorAddr!=NULL && *monitorAddr!='\0') {
    char unitID[256];
    char myName[256], *ptr;

    if (monitorUnitID==NULL) {
      if (CommunicationObject::GetHostName(myName)==gm_False) return;
      if ((ptr = strchr(myName, '.'))!=NULL) *ptr = '\0';
      sprintf(unitID, "Front End (%s - %ld)", myName, (long)getpid());
      monitorUnitID = unitID;
    }
    monitorClient = new MonitorClient(monitorUnitID, monitorAddr, monitorPort, 
				      monitorTTL);
    if (monitorClient==NULL) VoidReturn(errOutOfMemory);
    if (Error::getStatus()!=success) return;

    SetRemoteLogging(monitorClient);
  }
  else {
    SetStderrLogging("Front end: ");
  }


  gm_Log("Initialized Distiller cache with following parameters:\n"
	 << "    Options file: " << options->Find(Opt_OptionsFile) << '\n'
	 << "    PTM executable: " << options->Find(Opt_PTMExecutable) << '\n'
	 << "    PTM beaconing interval: " 
	 << options->FindUINT32(Opt_PTMBeacon_ms) << '\n'
	 << "    Lost beacon count: " << options->FindUINT32(Opt_LostBeacons)
	 <<'\n');

  
  RemoteID unicast;
  gm_Bool connectSucceeded = gm_False;
  const char *unicastIP;

  unicastIP = options->Find(Opt_PTMUnicast_IP);
  if (unicastIP!=NULL) strcpy(unicast.ipAddress, unicastIP);
  else strcpy(unicast.ipAddress, "");
  unicast.port = options->FindUINT32(Opt_PTMUnicast_port);
  
  if (*unicast.ipAddress!='\0' && unicast.port!=0) {
    // we have an IP address to connect to
    if (EvPTMLocationReceived__(unicast)==gm_True) {
      // we were successful in connecting to the PTM
      connectSucceeded = gm_True;
    }
  }

  if (connectSucceeded==gm_False) {
    // we were never connected to the PTM; we must try to start it up if 
    // necessary
    if (EvPTMConnectionLost__()==gm_False) {
      // could not initialize timers for restarting the PTM
      return;
    }
    gm_Log("Trying to listen for new PTM address\n");
  }
  else {
    gm_Log("Successfully connected to the PTM at " << unicast.ipAddress
	   << "/" << unicast.port << "\n");
  }

  managerThread->Fork(ForkHelper, (void*)this);
}


CacheManager::~CacheManager()
{
  DELETE(cache);
  if (bus!=NULL) {
    //evs->RemoveCommunicationObject(bus->getListener());
    delete bus;
    bus = NULL;
  }
  if (ptmConnection_!=NULL) {
    //evs->RemoveCommunicationObject(ptmConnection_);
    ptmConnection_->MarkForDeletion();
    ptmConnection_ = NULL;
  }
  DELETE(managerThread);	// I don't think this actually kills the thread
				// I might want to explicitly kill it first
  DELETE(evs);
  DELETE(monitorClient);
}



gm_Bool
CacheManager::ExpandOptions(OptionDatabase *optDB)
{
  const char *value;
  char *slash1, *slash2;

  if (optDB->Find(Opt_PTMExecutable)==NULL) {
    if (optDB->Add(Opt_PTMExecutable, DefaultPTMExecutable)==gm_False) 
      return gm_False;
  }

  value = optDB->Find(Opt_PTMMulticast);
  if (value==NULL) {
    optDB->Add(Opt_PTMMulticast_IP,   DefaultMulticastAddress);
    optDB->Add(Opt_PTMMulticast_port, DefaultMulticastPort);
    optDB->Add(Opt_PTMMulticast_TTL,  DefaultTTL);
  }
  else {
    RemoteID multicast;
    int ttl;

    slash1 = strchr(value, '/');
    if (slash1==NULL || slash1==value) Return(gm_False, errFormattingError);

    if (slash1 >= value + MAXIP) Return(gm_False, errBufferOverflow);
    strncpy(multicast.ipAddress, value, slash1-value);
    multicast.ipAddress[slash1-value] = '\0';
    
    slash2 = strchr(slash1+1, '/');
    if (slash2==NULL) {
      multicast.port = atoi(slash1+1);
      if (multicast.port==0) Return(gm_False, errFormattingError);
      ttl = DefaultTTL;
    }
    else {
      if (sscanf(slash1+1, "%d/%d", &multicast.port, &ttl)!=2) 
	Return(gm_False, errFormattingError);
      if (multicast.port==0 || ttl==0) Return(gm_False, errFormattingError);
    }
    optDB->Add(Opt_PTMMulticast_IP,   multicast.ipAddress);
    optDB->Add(Opt_PTMMulticast_port, (UINT32) multicast.port);
    optDB->Add(Opt_PTMMulticast_TTL,  (UINT32) ttl);
  }

  value = optDB->Find(Opt_MonitorMulticast);
  if (value==NULL) {
    optDB->Add(Opt_MonitorMulticast_IP,   DefaultMonitorAddress);
    optDB->Add(Opt_MonitorMulticast_port, DefaultMonitorPort);
    optDB->Add(Opt_MonitorMulticast_TTL,  DefaultTTL);
  }
  else if (*value=='\0') {
    // we have explicitly mentioned that we don't want to use a monitor
    // All log messages will be output to stderr
    optDB->Add(Opt_MonitorMulticast_IP,   "");
    optDB->Add(Opt_MonitorMulticast_port, (UINT32)-1);
    optDB->Add(Opt_MonitorMulticast_TTL,  (UINT32)-1);
  }
  else {
    RemoteID multicast;
    int ttl;

    slash1 = strchr(value, '/');
    if (slash1==NULL || slash1==value) Return(gm_False, errFormattingError);

    if (slash1 >= value + MAXIP) Return(gm_False, errBufferOverflow);
    strncpy(multicast.ipAddress, value, slash1-value);
    multicast.ipAddress[slash1-value] = '\0';
    
    slash2 = strchr(slash1+1, '/');
    if (slash2==NULL) {
      multicast.port = atoi(slash1+1);
      if (multicast.port==0) Return(gm_False, errFormattingError);
      ttl = DefaultTTL;
    }
    else {
      if (sscanf(slash1+1, "%d/%d", &multicast.port, &ttl)!=2) 
	Return(gm_False, errFormattingError);
      if (multicast.port==0 || ttl==0) Return(gm_False, errFormattingError);
    }
    optDB->Add(Opt_MonitorMulticast_IP,   multicast.ipAddress);
    optDB->Add(Opt_MonitorMulticast_port, (UINT32) multicast.port);
    optDB->Add(Opt_MonitorMulticast_TTL,  (UINT32) ttl);
  }

  if (optDB->Find(Opt_DistillerDB)==NULL) {
    if (optDB->Add(Opt_DistillerDB, DefaultDistillerDB)==gm_False) 
      return gm_False;
  }
  if (optDB->Find(Opt_Rsh)==NULL) {
    if (optDB->Add(Opt_Rsh, DefaultRsh)==gm_False)
      return gm_False;
  }
  if (optDB->Find(Opt_RshArgs)==NULL) {
    if (optDB->Add(Opt_RshArgs, DefaultRshArgs)==gm_False)
      return gm_False;
  }
  if (optDB->Find(Opt_Hosts)==NULL) {
    // default hosts is myself
    char myself[256];
    if (CommunicationObject::GetHostName(myself)==gm_False) return gm_False;
    if (optDB->Add(Opt_Hosts, myself)==gm_False)
      return gm_False;
  }
  if (optDB->Find(Opt_PTMBeacon_ms)==NULL) {
    if (optDB->Add(Opt_PTMBeacon_ms, DefaultBeaconingPeriod_ms)==gm_False)
      return gm_False;
  }
  if (optDB->Find(Opt_LostBeacons)==NULL) {
    if (optDB->Add(Opt_LostBeacons, DefaultLostBeacons)==gm_False)
      return gm_False;
  }
  return gm_True;
}


gm_Bool
CacheManager::ConnectToPTM_(RemoteID &ptmAddress_)
{
  ptmConnection_ = new FE_PTMConnection(evs);
  if (ptmConnection_==NULL) Return(gm_False, errOutOfMemory);
  if (ptmConnection_->Connect(ptmAddress_.ipAddress, 
			      ptmAddress_.port)==gm_False){
    ptmConnection_->MarkForDeletion();
    ptmConnection_ = NULL;
    return gm_False;
  }

  /*if (evs->AddCommunicationObject(ptmConnection_)==gm_False) {
    ptmConnection_->MarkForDeletion();
    ptmConnection_ = NULL;
    return gm_False;
  }*/

  return gm_True;
}


gm_Bool
CacheManager::ClosePTMConnection__()
{
  gm_Log("PTM connection lost; trying to listen for new PTM address\n");
  mgrMutex.Lock();

  if (ptmConnection_!=NULL) {
    //evs->RemoveCommunicationObject(ptmConnection_);
    ptmConnection_->MarkForDeletion();
    ptmConnection_ = NULL;
    if (EvPTMConnectionLost_()==gm_False) {
      mgrMutex.Unlock();
      return gm_False;
    }
  }

  mgrMutex.Unlock();
  return gm_True;
}


gm_Bool
CacheManager::NoBeaconTimer::EvTimer(EventSystem */*evs*/)
{
  if (beaconRecvd==gm_False 
      && CacheManager::getInstance()->IsConnectedToPTM__()==gm_True) {

    if (askedForUnicastBeacon==gm_True) {
      // i have already asked for a unicast beacon
      // looks like I didn't get any! so PTM must be dead

      askedForUnicastBeacon = gm_False;
      return CacheManager::getInstance()->ClosePTMConnection__();
    }
    else {
      // before saying that I have lost the PTM connection, I should try to
      // ask for a private beacon via TCP
      gm_Log("Asking for unicast beacon packet from the PTM\n");
      if (CacheManager::getInstance()->AskPTMForUnicastBeacon()==gm_False) {
	// I couldn't send the packet to the PTM; it must be dead
	return CacheManager::getInstance()->ClosePTMConnection__();
      }
      else
	askedForUnicastBeacon = gm_True;
    }
  }
  else {
    // I have received a beacon some time in the last interval
    // so I'm still connected
    // reset the beaconRecvd flag

    beaconRecvd = gm_True;
    askedForUnicastBeacon = gm_False;
  }

  return gm_True;
}


gm_Bool
CacheManager::EvPTMLocationReceived__(RemoteID &newPtmAddress)
{
  noBeaconTimer->EvBeaconReceived(evs);

  mgrMutex.Lock();
  if (IsConnectedToPTM_()==gm_True) {
    // PTM connection already exists; ignore this beacon
    mgrMutex.Unlock();
    return gm_True;
  }

  gm_Log("Found PTM at " << newPtmAddress.ipAddress << '/' 
	<< newPtmAddress.port << '\n');
  
  if (ConnectToPTM_(newPtmAddress)==gm_False) {
    // only errOutOfMemory is a real error
    if (Error::getStatus()==errOutOfMemory) {
      mgrMutex.Unlock();
      return gm_False;
    }
    gm_Log("Could not connect. Ignoring new location\n");
    Error::Print();
    Error::SetStatus(success);
  }

  // flush the negative cache!
  FlushNCache();

  // disable the startPTMTimer
  startPTMTimer.Disable();

  // signal the waiting guy that a PTM has been found
  ptmFoundMutex.Lock();
  ptmFoundCond.Broadcast();
  ptmFoundMutex.Unlock();

  mgrMutex.Unlock();
  return gm_True;
}


void
CacheManager::WaitForPTM__()
{
  ptmFoundMutex.Lock();
  if (ptmConnection_!=NULL) {
    ptmFoundMutex.Unlock();
    return;
  }

  if (ptmFoundCond.Wait(&ptmFoundMutex)==gm_False) {
    Error::Print();
  }

  ptmFoundMutex.Unlock();
}


gm_Bool 
CacheManager::EvPTMConnectionLost_()
{
  ptmConnection_ = NULL;
  gm_Bool retval = startPTMTimer.NewTimer(evs);
  return retval;
}


UINT32
StartPTMTimer::RandomDelay()
{
  return timeout_ms + (lrand48()%NoPTMRandom_ms);
}


gm_Bool
StartPTMTimer::EvTimer(EventSystem *evs)
{
  // don't need a lock here because NetTimer() grabs the lock
  if (NewTimer(evs)==gm_False) return gm_False;
  return CacheManager::getInstance()->StartPTM__();
}


gm_Bool
CacheManager::RestartPTMTimer__()
{
  mgrMutex.Lock();
  gm_Bool returnValue = startPTMTimer.NewTimer(evs);
  mgrMutex.Unlock();
  return returnValue;
}


gm_Bool
CacheManager::StartPTM__()
{
  const char *ptmExecutableName;

  mgrMutex.Lock();

  /*
   * first notify everyone that I'm about to start the PTM
   */
  gm_Packet packet(pktWillStartPTM, 0, 0);
  if (bus->getSender()->Write(&packet)==gm_False) {
    mgrMutex.Unlock();
    return gm_False;
  }


  ptmExecutableName = options->Find(Opt_PTMExecutable);
  if (ptmExecutableName==NULL || *ptmExecutableName=='\0') {
    mgrMutex.Unlock();
    Return(gm_False, errCouldNotLaunch);
  }

  gm_Log("Timed out while waiting for PTM; trying to start " 
	 << ptmExecutableName << '\n');
  
  char *argv[] = {
    (char*) ptmExecutableName,
    "-o",
    (char*) options->Find(Opt_OptionsFile),
    NULL
  };
  
  if (rexec.Exec(*argv, argv)==gm_False) {
    mgrMutex.Unlock();
    return gm_False;
  }

  mgrMutex.Unlock();
  return gm_True;
}


gm_Bool
CacheManager::AskPTMForDistiller__(DistillerType *type, 
				   AskForDistillerStatus &status)
{
  FE_PTMConnection *conn;
  mgrMutex.Lock();
  if (IsConnectedToPTM_()==gm_False) {
    status = ptmNoPTM;
    mgrMutex.Unlock();
    return gm_False;
  }
  
  conn = ptmConnection_;
  conn->IncrReference();
  mgrMutex.Unlock();
  AskForDistiller askRequest(evs);
  if (askRequest.SendAndWait(conn, type)==gm_False) {
    conn->DecrReference();
    return gm_False;
  }
  conn->DecrReference();
  status = askRequest.Status();
  char buf[100];
  sprintf(buf, "got reply from PTM: %d", status);
  Mutex::Log_(buf, "", NULL);
  return (status==ptmOk) ? gm_True : gm_False;
}


gm_Bool
CacheManager::AskForDistiller::SendAndWait(FE_PTMConnection *connection,
					   DistillerType *type)
{
  mutex.Lock();


  OStream stream;
  stream << (*type);
  if (stream.IsGood()==gm_False) {
    mutex.Unlock();
    return gm_False;
  }
  
  gm_Packet packet(pktAskForDistiller, stream.getLength(), getID(),
		stream.getData());
  
  if (connection->Write(&packet)==gm_False) {
    if (Error::getStatus()==errOutOfMemory) {
      mutex.Unlock();
      return gm_False;
    }

    // any other error means the connection is down
    Error::SetStatus(success);
    gm_Bool returnValue = connection->
      EvConnectionBroken(CacheManager::getInstance()->getEventSystem());
    mutex.Unlock();
    return returnValue;
  }

  condition.Wait(&mutex);
  mutex.Unlock();
  return gm_True;
}


gm_Bool
CacheManager::AskForDistiller::EvReplyReceived(RequestReplyEventSystem *,
					       void *args)
{
  mutex.Lock();
  status = (AskForDistillerStatus) args;
  condition.Signal();
  mutex.Unlock();
  return gm_True;
}


gm_Bool
CacheManager::AskForDistiller::EvTimer(RequestReplyEventSystem */*evs*/)
{
  mutex.Lock();
  status = ptmAskForDistillerTimeout;
  condition.Signal();
  mutex.Unlock();
  return gm_True;
}


void
CacheManager::Main()
{
  if (evs->Run()==gm_False) {
    Abort("Fatal cache-manager error");
  }
  gm_Log("Cache Manager has quit. This shouldn't have happened\n");

  monitorClient->Gasp(0);
  DELETE(instance);
}


void*
CacheManager::ForkHelper(void *args)
{
  ((CacheManager*)args)->Main();
  return 0;
}


gm_Bool
CacheManager::CreateInstance(OptionDatabase *options, 
			     const char *monitorUnitID)
{
  instance = new CacheManager(options, monitorUnitID);
  if (instance==NULL) Return(gm_False, errOutOfMemory);
  if (Error::getStatus()!=success) return gm_False;

  //signal(SIGINT, CtlCPressed);
  return gm_True;
}


void
CacheManager::CtlCPressed(int /*sig*/)
{
  Abort("Ctl-C pressed");
}


void
CacheManager::Abort(const char *string)
{
  gm_Log("Aborting program: " << string << "\n\n");
  Error::Print();

  //if (instance!=NULL) delete instance;
  getInstance()->getMonitorClient()->Gasp(0);
  exit(-1);
}


gm_Bool
InitializeDistillerCache(Options options, const char *monitorUnitID)
{
  gm_Bool returnValue;
  Debug_::getInstance()->Initialize("et", "PTM stub: ");
  //Mutex::log_file = "/disks/barad-dur/roboline/yatin/mutex.log";
  returnValue = CacheManager::CreateInstance((OptionDatabase*)options, 
					     monitorUnitID);
  if (returnValue==gm_False) {
    gm_Log("Could not initialize the Distiller Cache\n");
    Error::Print();
  }
  return returnValue;
}


DistillerStatus
Distill(C_DistillerType *type,  Argument *args, int numberOfArgs,
	DistillerInput  *input, DistillerOutput *output)
{
  DistillerType distType(*type);

  Mutex::Log_("entering Distill()", "", NULL);
  output->data.data = NULL;
  output->data.length = 0;
  strcpy(output->mimeType, input->mimeType);

  if (CacheManager::getInstance()==NULL) return distNoCacheManager;
  DistillerStatus returnValue = 
    CacheManager::getInstance()->Distill(&distType, args, numberOfArgs, 
					 input, output);

  if (returnValue!=distOk && returnValue!=distDistillerNotFound) {
    gm_Log("Error " << returnValue << " (" << type->string << "): " 
	   << FE_getDistillerStatusString(returnValue) << "\n");
  }

  Mutex::Log_("leaving Distill()", "", NULL);
  return returnValue;
}

DistillerStatus
DistillAsync(C_DistillerType *type,  Argument *args, int numberOfArgs,
	DistillerInput  *input, DistillerRequestType *request)
{
  DistillerType distType(*type);
  DistillerRequest *newrequest = (DistillerRequest *)*request;

  Mutex::Log_("entering DistillAsync()", "", NULL);

  if (CacheManager::getInstance()==NULL) return distNoCacheManager;
  DistillerStatus returnValue = 
    CacheManager::getInstance()->DistillAsync(&distType, args, numberOfArgs, 
					 input, newrequest);

  if (returnValue!=distOk && returnValue!=distDistillerNotFound) {
    gm_Log("Error " << returnValue << " (" << type->string << "): " 
	   << FE_getDistillerStatusString(returnValue) << "\n");
  }

  Mutex::Log_("leaving Distill()", "", NULL);
  *request = newrequest;
  return returnValue;
}

DistillerStatus
DistillRendezvous(DistillerRequestType *request, DistillerOutput *output,
    struct timeval *tv)
{
    output->data.data = NULL;
    output->data.length = 0;
    output->mimeType[0] = '\0';

    DistillerStatus status = distFatalError;
    if (!request) return status;
    CacheManager *inst = CacheManager::getInstance();
    DistillerRequest *newrequest = (DistillerRequest *)*request;
    if (!newrequest) return status;
    if (inst) {
	status = inst->DistillRendezvous(newrequest, output, tv);
    } else {
	return distNoCacheManager;
    }
    *request = newrequest;
    return status;
}

void DistillDestroy(DistillerRequestType *request)
{
    if (!request) return;
    CacheManager *inst = CacheManager::getInstance();
    DistillerRequest *newrequest = (DistillerRequest *)*request;
    if (inst) {
	inst->DistillDestroy(newrequest);
    }
    *request = newrequest;
}

const char *
FE_getDistillerStatusString(DistillerStatus status)
{
  static const char * const array [] = {
    "200 Distiller ok",		// distOk
    "401 InitializeDistillerCache() not invoked at proxy frontend or "
    "DistillerCache thread has exited",      /* distNoCacheManager */
    "101 Could not locate distiller",            /* distDistillerNotFound */
    "402 Error sending data to distiller",       /* distSendError */
    "403 Timed out while waiting for distiller", /* distReplyTimeout */
    "501 Out of memory at the frontend",         /* distOutOfLocalMemory */
    "404 Connection to distiller was broken",    /* distConnectionBroken */
    "405 Timed out while waiting for PTM to launch a new distiller", 
                                             /* distLaunchTimeout */
    "102 Unrecoverable error occurred in distiller",/* distFatalError */
    "103 Bad input to distiller",                   /* distBadInput */
    "104 Redispatch needed (chained distill)",      /* distRedispatch */
#ifdef whaaaa
    "Asynchronously queue fetches"              /* distAsyncBegin */
#endif
    "406 Rendezvous timeout"                     /* distRendezvousTimeout */
  };

  if (status >= sizeof(array)/sizeof(const char *))
    return "Unknown error";
  return array[status];
}


const char*
FE_GetMonitorClientUnitID()
{
  return CacheManager::getInstance()->getMonitorClient()->getUnitID();
}



/*void
FreeOutputBuffer(DistillerOutput *output)
{
  if (output->data!=NULL) delete [] ((char*)output->data);
}*/


void
WaitForPTM()
{
  CacheManager::getInstance()->WaitForPTM__();
}








DistillerStatus
CacheManager::Distill(DistillerType *type, Argument *args, int numberOfArgs,
		      DistillerInput *input, DistillerOutput *output)
  // if connection fails between send and reply, then this function returns
  // an error; perform the retry option at a higher level function
{
  DistillerStatus status;
  CachedDistiller *distiller;

  while (gm_True) {
    //set_thread_state(THR_DISTCACHE);
    distiller = cache->getDistiller(type, status);
    if (distiller==NULL) return status;

    // got a real distiller; its ref count has already been incremented
    // remember to decrement it when you're done
    status = distiller->Distill(args, numberOfArgs, input, output);
    distiller->DecrPendingCount();
    if (status!=distSendError) {
      distiller->DecrReference();
      return status;
    }

    gm_Log("Problems connecting to distiller\n");
    cache->Remove__(distiller);
    distiller->MarkForDeletion();
    distiller->DecrReference();
  }
  // never reached! 
  // added the return statement to prevent the compiler from generating a 
  // warning
  return distOk;
}

DistillerStatus
CacheManager::DistillAsync(DistillerType *type, Argument *args,
			    int numberOfArgs, DistillerInput *input,
			    DistillerRequest *& newrequest)
{
  DistillerStatus status;
  CachedDistiller *distiller;
  int i;

  for (i=0;i<5;++i) {
    //set_thread_state(THR_DISTCACHE);
    distiller = cache->getDistiller(type, status);
    if (distiller==NULL) return status;

    // got a real distiller; its ref count has already been incremented
    // remember to decrement it when you're done
    status = distiller->DistillAsync(args, numberOfArgs, input, newrequest);
    if (status == distSendError) {
	distiller->DecrPendingCount();
	gm_Log("Problems connecting to distiller\n");
	cache->Remove__(distiller);
	distiller->MarkForDeletion();
	distiller->DecrReference();
    } else {
	break;
    }
  }
  return status;
}

DistillerStatus
CacheManager::DistillRendezvous(DistillerRequest *& newrequest,
    DistillerOutput *output, struct timeval *tv)
{
    gm_Bool waitres;
    waitres = newrequest->Wait(tv);
    if (!waitres) {
	return distRendezvousTimeout;
    }
    *output = * (newrequest->getOutput());
    newrequest->getOutput()->DontFreeMe();
    DistillerStatus retval = newrequest->getStatus();
    DistillDestroy(newrequest);
    return retval;
}

void
CacheManager::DistillDestroy(DistillerRequest *& newrequest)
{
    newrequest->Destroy();
    delete newrequest;
    newrequest = NULL;
}
