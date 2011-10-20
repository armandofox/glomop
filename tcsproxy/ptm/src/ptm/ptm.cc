#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "defines.h"
#include "log.h"
#include "ptm.h"
#include "distlaunch.h"
#include "utils.h"


PTM *PTM::instance=NULL;
static OptionDatabase options;

gm_Bool
PTM::CreateInstance(OptionDatabase *options)
{
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd))==NULL) strcpy(cwd, "<error>");
  instance = new PTM(options);
  if (instance==NULL) Return(gm_False, errOutOfMemory);
  if (Error::getStatus()!=success) return gm_False;

  signal(SIGINT, CtlCPressed);
  signal(SIGHUP,OptionsUpdate_static);
  return gm_True;
}


PTM::PTM(OptionDatabase *options)
  : evs(NULL), server(NULL), bus(NULL), beaconTimer(NULL),
    distillerDB(NULL), distillerLauncher(NULL),
    monitorClient(NULL)
{
  if (ExpandOptions(options)==gm_False) return;

  timeval now;
  gettimeofday(&now, NULL);
  srand48(now.tv_usec + getpid());
  myRandomID = (UINT32) lrand48();

  // create the event system object
  NEW(evs, RequestReplyEventSystem);

  // set up the PTM server socket
  NEW(server, PrivateConnectionServer);
  if (evs->AddCommunicationObject(server)==gm_False) return;

  if (CommunicationObject::GetHostIPAddress(myAddress.ipAddress)==gm_False)
    return;
  myAddress.port = server->getLocalPort();

  // set up the multicast channel
  safe_strncpy(myMulticast.ipAddress, options->Find(Opt_PTMMulticast_IP), 
	       MAXIP);
  myMulticast.port = (Port) options->FindUINT32(Opt_PTMMulticast_port);
  int ttl = (int) options->FindUINT32(Opt_PTMMulticast_TTL);

  NEW(bus, SharedBus(myMulticast, ttl));
  if (evs->AddCommunicationObject(bus->getListener())==gm_False) return;

  // set up a timer for perioding beacons
  UINT32 beaconingPeriod_ms = options->FindUINT32(Opt_PTMBeacon_ms);
  if (beaconingPeriod_ms==0) beaconingPeriod_ms = DefaultBeaconingPeriod_ms;
  NEW(beaconTimer,       BeaconTimer(evs, beaconingPeriod_ms));

  // set up a timer for checking to see if the options file has been
  // modified.
  UINT32 optionRefresh_ms = options->FindUINT32(Opt_PTMOptionsReRead_ms);
  if (optionRefresh_ms==0) optionRefresh_ms = 10000; // default to 10 secs
  
  NEW(optionsTimer,      OptionsFileTimer(evs, optionRefresh_ms,options));

  // create the monitor object
  RemoteID log;
  safe_strncpy(log.ipAddress, options->Find(Opt_MonitorMulticast_IP), MAXIP);
  log.port = (Port) options->FindUINT32(Opt_MonitorMulticast_port);
  int logTTL = (int) options->FindUINT32(Opt_MonitorMulticast_TTL);
  
  if (strcmp(log.ipAddress, "")!=0) {
    char unitID[256];
    sprintf(unitID, "PTM (Port %d)", myAddress.port);
    monitorClient = new MonitorClient(unitID, log.ipAddress,log.port,
				      logTTL);
    if (monitorClient==NULL) VoidReturn(errOutOfMemory);
    if (Error::getStatus()!=success) return;
  
    monitorClient->GaspOnStdSignals();
    SetRemoteLogging(monitorClient);
  }
  else {
    SetStderrLogging("PTM: ");
  }

  // create a database for storing all active distiller objects
  NEW(distillerDB,       DistillerDatabase);

  // create the distiller launcher object
  NEW(distillerLauncher, DistillerLauncher(options));

  gm_Log("Started up the PTM with following parameters:\n"
	 << "    PTM Location = " << getAddress() << '\n'
	 << "    Multicast address = " << getMulticast() << '/' << ttl << '\n'
	 << "    Monitor address = " << log << '/' << logTTL << '\n'
	 << "    Beaconing interval= " << beaconingPeriod_ms << " ms\n");
}


PTM::~PTM()
{
  if (server!=NULL) {
    evs->RemoveCommunicationObject(server);
    delete server;
    server = NULL;
  }

  if (bus!=NULL) {
    evs->RemoveCommunicationObject(bus->getListener());
    delete bus;
    bus = NULL;
  }

  if (bus!=NULL) {
    delete bus;
    bus = NULL;
  }

  DELETE(beaconTimer);
  DELETE(distillerDB);
  DELETE(distillerLauncher);
  DELETE(evs);
  DELETE(monitorClient);
}


gm_Bool
PTM::OptionsUpdate(OptionDatabase *options)
{
  if (distillerLauncher->OptionsUpdate(options)==gm_False) return gm_False;
  ListIndex idx;
  PrivateConnection *socket;
  gm_Packet packet(pktFlushNCache, 0, NULL);
  idx = listOfConnections.BeginTraversal();
  for (; listOfConnections.IsDone(idx)==gm_False;
       idx = listOfConnections.getNext(idx)) {
    socket = listOfConnections.getData(idx);
    if (socket->IsDistillerConnection()==gm_False) {
      gm_Log("Sending flush packet to frontend\n");
      if (socket->Write(&packet)==gm_False) {
	// ignore the error
	Error::SetStatus(success);
      }
    }
  }
  listOfConnections.EndTraversal();
  return gm_True;
}


void
PTM::OptionsUpdate_static(int /*sig*/)
{
  extern OptionDatabase options;
  char filename[MAXPATH];
  strncpy(filename, options.Find(Opt_OptionsFile), MAXPATH);
  gm_Log("Updating options database: " << filename << "\n");
  options.Update(filename);
  PTM::getInstance()->OptionsUpdate(&options);
  signal(SIGHUP, OptionsUpdate_static);
}


gm_Bool
PTM::Run()
{
  return evs->Run();
}


void
PTM::CtlCPressed(int /*sig*/)
{
  Abort("Ctl-C pressed");
}


void
PTM::Abort(char *string)
{
  gm_Log("Aborting program: " << string << "\n\n");
  Error::Print();
  //if (instance!=NULL) delete instance; //this doesn't work, so it's commented
  if (getInstance()!=NULL) {
    MonitorClient *monitorClient = getInstance()->getMonitorClient();
    if (monitorClient!=NULL) monitorClient->Gasp(0);
  }
  exit(-1);
}


gm_Bool
PTM::Register(DistillerRecord *distillerRecord)
{
  if (distillerDB->Add(distillerRecord)==gm_False) {
    if (Error::getStatus()==errDuplicateIndexEntry) {
      gm_Log("Cannot register distiller at " << distillerRecord->rid 
	     << ". Distiller already exists.\n");
    }
    return gm_False;
  }
  distillerLauncher->getRExec()->UpLevel(distillerRecord->rid.ipAddress);
  gm_Log("Registering distiller for " << distillerRecord->type 
	 << " at " << distillerRecord->rid << '\n');
  return gm_True;
}


void
PTM::Deregister(DistillerRecord *distillerRecord)
{
  if (distillerRecord==NULL) return;
  gm_Log("De-registering distiller for " << distillerRecord->type 
	 << " at " << distillerRecord->rid << '\n');
  distillerLauncher->getRExec()->DownLevel(distillerRecord->rid.ipAddress);
  distillerDB->Remove(distillerRecord);
}


// This function does not block until it finds a distiller
// It simply arranges for the PTM to eventually respond to the proxy
gm_Bool
PTM::EvAskForDistiller(DistillerType *distillerType,
		       PrivateConnection *replyObject,
		       UINT32 replyID)
{
  List<DatabaseRecord> list;

  if (distillerDB->FindMatchingDistillers(distillerType, &list)==gm_False)
    return gm_False;
  if (list.IsEmpty()==gm_True) {
    DistillerRecord *dist = distillerDB->WakeSleepingDistiller(distillerType);
    if (dist==NULL) {
      return distillerLauncher->TryToLaunch(distillerType, replyObject, 
					    replyID);
    }
    else {
      // we found a sleeping distiller;
      list.InsertAtHead(dist);
    }
  }

  return replyObject->AskForDistillerReply(replyID, ptmOk, &list);
}


/*gm_Bool
PTM::SendBeaconPacket(CommunicationObject *object, 
		      UINT32 requestReplyID, //can be set to NoRequestReply
		      LinkedList<DatabaseRecord> *list)
{
  OStream stream;
  UINT32  distillersPos, numberOfDistillers=0;
  ListIdx idx;

  stream << myAddress << '|';
  if (stream.IsGood()==gm_False) return gm_False;
  distillersPos = stream.getLength() - 1; // length includes trailing '\0'

  stream << Binary(gm_True) << (UINT32) 0 << Binary(gm_False);
  if (stream.IsGood()==gm_False) return gm_False;

  for (idx=list->Head(); list->IsDone(idx)==gm_False; idx = list->Next(idx)) {
    stream << *((DistillerRecord*)list->Record(idx)) << '|';
    if (stream.IsGood()==gm_False) return gm_False;
    numberOfDistillers++;
  }

  char *data = stream.getData();
  numberOfDistillers = htonl(numberOfDistillers);
  memcpy(data + distillersPos, (char*) &numberOfDistillers, sizeof(UINT32));

  gm_Packet packet(pktPTMBeacon, stream.getLength(), requestReplyID, data);
  if (packet.Send(object)==gm_False) return gm_False;

  gm_BoolErrorReturn(success);
}*/



void
PTM::Beacon(PrivateConnection *privateConnection)
{
  List<DatabaseRecord> *list;
  gm_Bool returnValue;

  list = distillerDB->getAllRecords();
  if (privateConnection==NULL) {
    returnValue = bus->Beacon(myAddress, myRandomID, list);
  }
  else {
    returnValue = privateConnection->Beacon(myAddress, myRandomID, list);
  }

  if (returnValue==gm_False) {
    gm_Log("Error sending beaconing packet (error " << Error::getStatus()
	   << " in " << Error::getFilename() << " at line " 
	   << Error::getLineNo() << ")\n");
    Error::SetStatus(success);
  }
}


int
PTM::CompareBeacon(RemoteID &otherRid, UINT32 otherRandomID)
{
  UINT32 myIP, otherIP;

  if (myRandomID < otherRandomID) return -1;
  if (myRandomID > otherRandomID) return +1;

  if (myAddress.port < otherRid.port) return -1;
  if (myAddress.port > otherRid.port) return +1;

  myIP    = ntohl(inet_addr(myAddress.ipAddress));
  otherIP = ntohl(inet_addr(otherRid .ipAddress));

  if (myIP < otherIP) return -1;
  if (myIP > otherIP) return +1;

  return 0;
}


void
PTM::Usage()
{
  gm_Log("Usage: ptm [options]\n\n"
	 << "Options: -o <options-file>\n");
  exit(-1);
}


gm_Bool
PTM::ExpandOptions(OptionDatabase *optDB)
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

  return gm_True;
}


int 
main(int argc, char **argv)
{
  char     optionsFile[MAXPATH]="";
  char     *optionString = "o:";
  int      optCh;

  optind = 1;
  while ( (optCh = getopt(argc, argv, optionString))!=-1) {
    switch (optCh) {
    case 'o':
      strcpy(optionsFile, optarg);
      break;

    case ':':
    case '?':
    default:
      PTM::Usage();
    }
  }



  if (*optionsFile!='\0') {
    if (options.Create(optionsFile)==gm_False)
      PTM::Abort("Error occurred while reading options file");
  }

  Debug_::getInstance()->Initialize("et", "PTM: ");
  if (PTM::CreateInstance(&options)==gm_False) {
    PTM::Abort("Could not start up the PTM");
  }

  /*if (InstallAllOptions(options)==gm_False) 
    PTM::Abort("Error occurred while installing default options");


  const char *mcastAddr;
  if ((mcastAddr=options.Find(Opt_PTMMulticast_IP))!=NULL) {
    if (strlen(mcastAddr) < MAXIP) strcpy(multicast.ipAddress,mcastAddr);
    else {
      strncpy(multicast.ipAddress, mcastAddr, MAXIP-1);
      multicast.ipAddress[MAXIP-1] = '\0';
    }
    multicast.port = (Port) options.FindUINT32(Opt_PTMMulticast_port);
    ttl = (int) options.FindUINT32(Opt_PTMMulticast_TTL);
  }

  mcastAddr=options.Find(Opt_MonitorMulticast_IP);
  if (mcastAddr!=NULL && *mcastAddr!='\0') {
    if (strlen(mcastAddr) < MAXIP) strcpy(log.ipAddress, mcastAddr);
    else {
      strncpy(log.ipAddress, mcastAddr, MAXIP-1);
      log.ipAddress[MAXIP-1] = '\0';
    }
    log.port = (Port) options.FindUINT32(Opt_MonitorMulticast_port);
    logTTL = (int) options.FindUINT32(Opt_MonitorMulticast_TTL);
  }
  

  if ((launchDBPath = options.Find(Opt_DistillerDB))==NULL) {
    // this should not happen
    PTM::Abort("Could not read option from the options database");    
  }

  rsh     = options.Find(Opt_Rsh);
  rshArgs = options.Find(Opt_RshArgs);
  rhosts  = options.Find(Opt_Hosts);

  beaconingPeriod_ms = options.FindUINT32(Opt_PTMBeacon_ms);
  optionsReRead = options.FindUINT32(Opt_PTMOptionsReRead_ms);
  if (beaconingPeriod_ms==0) beaconingPeriod_ms = DefaultBeaconingPeriod_ms;


  Debug_::getInstance()->Initialize("et", "PTM: ");
  if (PTM::CreateInstance(multicast, ttl, launchDBPath, rsh, rshArgs, rhosts,
			  log, logTTL, beaconingPeriod_ms)==gm_False) {
    PTM::Abort("Could not start up the PTM");
  }*/

  if (PTM::getInstance()->Run()==gm_False)
    PTM::Abort("Error occurred while running the PTM");

  gm_Log("Ending the PTM normally\n");
  PTM::getInstance()->getMonitorClient()->Gasp(0);
  return 0;
}
