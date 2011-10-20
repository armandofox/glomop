#include "defines.h"
#include "log.h"
#include "distiller.h"
#include "distinterface.h"
#include "packet.h"
#include "options.h"
#include "utils.h"
extern "C" {
#  include "libmon.h"
#  include "clib.h"
}
#include "../../../frontend/include/ARGS.h"
#include <signal.h>
#include <errno.h>
#include <string.h>



#define Opt_NotificationTimeout_ms    "private.notification_timeout"
#define Opt_DistillerMainTimeout_ms   "private.distiller_main_timeout"
#define Opt_NoLoadTimeout_sec         "private.no_load_timeout"
//#define Opt_PTMUnicast_IP             "private.ptm_unicast.ip"
//#define Opt_PTMUnicast_port           "private.ptm_unicast.port"
#define Opt_DistillerType             "private.distiller_type"
#define Opt_SleepOnStartup            "private.sleep_on_startup"
#define Opt_RestartOnDeath            "private.restart_on_death"
#define Opt_RequestID                 "private.request_id"

extern "C" {
pthread_key_t index_key;
}


static char end_string_[256]="";


class DistillerMainTimer : public gm_Timer {
public:
  DistillerMainTimer(MyDistiller *dist)
    : gm_Timer(dist->getEventSystem(), 
	       SECONDS (dist->getOptions()->
			FindUINT32(Opt_DistillerMainTimeout_ms)), 
	       USECONDS(dist->getOptions()->
			FindUINT32(Opt_DistillerMainTimeout_ms)), gm_False) {}

private:
  virtual gm_Bool EvTimer(EventSystem */*evs*/) {
    MyDistiller::Abort("Distiller seems stuck in DistillerMain()");
    return gm_True;
  }
};



void LogArgs(OptionDatabase *options, int argc, const char*const*argv)
{
  gm_Log("Distiller type: " << options->Find(Opt_DistillerType) << "\n");
  gm_Log("PTM Unicast Address: " << options->Find(Opt_PTMUnicast_IP) << "/"
	 << options->Find(Opt_PTMUnicast_port) << "\n");
  gm_Log("Notification timeout: " 
	 << options->Find(Opt_NotificationTimeout_ms) << " ms\n");
  gm_Log("DistillerMain timeout: "
	 << options->Find(Opt_DistillerMainTimeout_ms) << " ms\n");
  gm_Log("Sleep on startup: " << options->Find(Opt_SleepOnStartup) << "\n");
  gm_Log("Restart on death: " << options->Find(Opt_RestartOnDeath) << "\n");
  char buf[2048];
  buf[0] = '\0';
  for(int i=0; i<argc; i++) {
    sprintf(buf, "%s %s", buf, argv[i]);
  }
  gm_Log(argc << " additional arguments: " << buf << "\n");
}


Monitor gMon;

Work::~Work()
{
  if (args!=NULL) delete [] args;
  /*if (input.data!=NULL) delete [] ((char*)input.data);
  if (freeOutputBuffer==gm_True) DistillerFree(output.data);*/

  // the input and output data structures ought to free themselves!
  replyObject->RemoveReference();
}


#ifdef _REPORT_STATS
Statistics::Statistics(EventSystem *evs)
 : /*currentQLengthWindow(0),*/ totalRequests(0), doneRequests(0), 
   badRequests(0), timer(NULL), mutex("*-stats")
{
  timer = new StatsTimer(evs);
  if (timer==NULL) VoidReturn(errOutOfMemory);
}


/*void
Statistics::AdjustAvgQLength(UINT32 length)
{
  mutex.Lock();
  windows[currentQLengthWindow].AdjustAvgQLength(length);
  if (windows[currentQLengthWindow].totalTime > Size_Of_QLengthWindow_usec) {
    currentQLengthWindow = (currentQLengthWindow+1) % No_Of_QLengthWindows;
    windows[currentQLengthWindow].Reset();
  }

  mutex.Unlock();
}


void
Statistics::QLengthWindow::AdjustAvgQLength(UINT32 length)
{
  timeval now, diff;
  UINT32  uintDiff;

  // compute the time diff between now and the last time the avgQLength was
  // updated
  gettimeofday(&now, NULL);
  diff.tv_sec  = now.tv_sec  - lastUpdate.tv_sec;
  diff.tv_usec = now.tv_usec - lastUpdate.tv_usec;

  if (diff.tv_usec < 0) {
    diff.tv_usec += 1000000;
    diff.tv_sec--;
  }

  if (diff.tv_sec < 0) {
    diff.tv_sec = diff.tv_usec = 0;
  }
  
  uintDiff  = diff.tv_sec*1000000 + diff.tv_usec;
  totalTime+= uintDiff;
  lengthTimeProduct += length * uintDiff;

  lastUpdate = now;
}


double
Statistics::getAvgQLength()
{
  double avg;
  UINT32 totalTime=0, lengthTimeProduct=0;
  int i;
  mutex.Lock();
  
  for (i=0; i< No_Of_QLengthWindows; i++) {
    totalTime         += windows[i].totalTime;
    lengthTimeProduct += windows[i].lengthTimeProduct;
  }

  if (totalTime==0) avg = 0.0;
  else avg = double(lengthTimeProduct)/double(totalTime);
  mutex.Unlock();
  return avg;
}*/


void
Statistics::Report()
{
  //double avg = getAvgQLength();
  mutex.Lock();
  MonitorClient *monitorClient = 
    MyDistiller::getInstance()->getMonitorClient();
  if (monitorClient!=NULL) {
    char buffer[100];
    sprintf(buffer, "%lu", totalRequests);
    monitorClient->Send("Total requests", buffer, "");
    sprintf(buffer, "%lu", doneRequests);
    monitorClient->Send("Completed requests", buffer, "");
    sprintf(buffer, "%lu", badRequests);
    monitorClient->Send("Bad requests", buffer, "");
    /*sprintf(buffer, "%0.3f", (float) avg);
    monitorClient->Send("Avg queue length", buffer, "");*/
  }
  mutex.Unlock();
}


gm_Bool
Statistics::StatsTimer::EvTimer(EventSystem */*evs*/)
{
  MyDistiller::getInstance()->ReportStats();
  return gm_True;
}


void
MyDistiller::ReportStats()
{
  //getStats()->AdjustAvgQLength(workQueue->getSize());
  char buffer[100];
  sprintf(buffer, "%lu", reporter->getAvgLatency());
  if (monitorClient!=NULL) monitorClient->Send("Avg queue length", buffer, "");
  getStats()->Report();
}

#endif


#ifndef __INSTRUMENT__
LoadReporter::LoadReporter(EventSystem *evs)
#else
LoadReporter::LoadReporter(EventSystem *evs, DistillerType *type)
#endif
  : instTotalLatency_ms(0), area(0),
    noLoadTimer(NULL)
#ifdef __INSTRUMENT__
  , instFile(NULL)
#endif
{
  gettimeofday(&lastUpdate, NULL);
  start = lastUpdate;
  
  long noLoadTimeout_sec = MyDistiller::getInstance()->getOptions()->
	  FindUINT32(Opt_NoLoadTimeout_sec);
  if (noLoadTimeout_sec!=0) {
	  // create timer object only if the timeout value is non-zero
	  NEW(noLoadTimer, NoLoadTimer(evs, noLoadTimeout_sec));
  }
  
#ifdef __INSTRUMENT__
  char filename[256], hostname[256], distType[256], *ptr;
  CommunicationObject::GetHostName(hostname);
  ptr = strchr(hostname, '.');
  if (ptr!=NULL) *ptr = '\0';
  strcpy(distType, (char*)(*type));
  ptr = distType;
  while (*ptr) {
    if (*ptr=='/') *ptr = '.';
    ptr++;
  }
  sprintf(filename, "ptm/%s.%s.%ld", distType, hostname, getpid());
  gm_Log("Trying to open inst file: " << filename << "\n");
  instFile = fopen(filename, "w");
#endif
}


void
LoadReporter::AdjustAvgLatency()
{
  timeval now, diff;
  UINT32 diff_ms;
  gettimeofday(&now, NULL);
  diff    = tv_timesub(now, lastUpdate);
  diff_ms = timeval_to_ms(diff);
  area += instTotalLatency_ms * diff_ms;
  lastUpdate = now;
}


UINT32
LoadReporter::getAvgLatency()
{
  timeval now, diff;
  UINT32 diff_ms;
  AdjustAvgLatency();
  gettimeofday(&now, NULL);
  diff    = tv_timesub(now, start);
  diff_ms = timeval_to_ms(diff);
  return ((diff_ms==0) ? instTotalLatency_ms : ( (area + diff_ms/2)/diff_ms));
}


void
LoadReporter::NewWork(Work *work)
{
  AdjustAvgLatency();
  instTotalLatency_ms += work->cost.estimatedTime_ms;
  if (noLoadTimer!=NULL) noLoadTimer->workReceived = gm_True;
#ifdef __INSTRUMENT__
  if (instFile!=NULL) {
    timeval now, diff;
    UINT32 diff_ms, avg;
    gettimeofday(&now, NULL);
    diff    = tv_timesub(now, start);
    diff_ms = timeval_to_ms(diff);
    avg = ((diff_ms==0) ? instTotalLatency_ms : ( (area + diff_ms/2)/diff_ms));
    fprintf(instFile, "%ld.%06ld\t%lu\t%lu\n", now.tv_sec, now.tv_usec, 
	    instTotalLatency_ms, avg);
  }
#endif
}


void
LoadReporter::WorkDone(Work *work)
{
  AdjustAvgLatency();
  instTotalLatency_ms -= work->cost.estimatedTime_ms;
#ifdef __INSTRUMENT__
  if (instFile!=NULL) {
    timeval now, diff;
    UINT32 diff_ms, avg;
    gettimeofday(&now, NULL);
    diff    = tv_timesub(now, start);
    diff_ms = timeval_to_ms(diff);
    avg = ((diff_ms==0) ? instTotalLatency_ms : ( (area + diff_ms/2)/diff_ms));
    fprintf(instFile, "%ld.%06ld\t%lu\t%lu\t%lu\t%lu\t%ld.%06ld\n", now.tv_sec,
	    now.tv_usec, instTotalLatency_ms, avg, work->input.data.length, 
	    work->output.data.length, work->distillTime.tv_sec, 
	    work->distillTime.tv_usec);
  }
#endif
}


void
LoadReporter::Report(Load *load)
{
  load->totalLatency_ms = getAvgLatency();
  // reset the area and start of time-interval
  area = 0;
  start = lastUpdate;
}


gm_Bool
LoadReporter::NoLoadTimer::EvTimer(EventSystem */*evs*/)
{
  if (workReceived==gm_False) {
    // no work has been received for a long time! I should kill myself
    MyDistiller::getInstance()->Die("Distiller has been idle");
  }
  else workReceived = gm_False;
  return gm_True;
}



MyDistiller* MyDistiller::instance=NULL;



MyDistiller::MyDistiller(OptionDatabase *options_, 
		     int argc, const char *const *argv)
  : reporter(NULL), consumer(NULL), 
    ptmUnicast(options_->Find(Opt_PTMUnicast_IP), 
	       options_->FindUINT32(Opt_PTMUnicast_port)), 
    ptmMulticast(options_->Find(Opt_PTMMulticast_IP), 
		 options_->FindUINT32(Opt_PTMMulticast_port)), 
    evs(NULL), server(NULL), ptmConnection(NULL), listener(NULL), 
    monitorClient(NULL), loadNotificationTimer(NULL),
    sleepStatus(distWakeUp), options(options_)
#ifdef _REPORT_STATS
    , stats(NULL)
#endif
{
  DistillerType distillerType(options_->Find(Opt_DistillerType));
  instance = this;

//  NEW(workQueue, ProducerConsumerQueue);
  NEW(workQueue, LotteryQueue);
  NEW(evs, EventSystem);
#ifdef __INSTRUMENT__
  NEW(reporter, LoadReporter(evs, &distillerType));
#else
  NEW(reporter, LoadReporter(evs));
#endif
  NEW(loadNotificationTimer, 
      LoadNotificationTimer(evs,
			    options->FindUINT32(Opt_NotificationTimeout_ms)));
  NEW(server, ProxyServer);
  if (evs->AddCommunicationObject(server)==gm_False) return;

#ifdef _REPORT_STATS
  NEW(stats, Statistics(evs));
#endif

  RemoteID monitorMulticast;
  int monitorTTL;
  strcpy(monitorMulticast.ipAddress, options->Find(Opt_MonitorMulticast_IP));
  monitorMulticast.port = options->FindUINT32(Opt_MonitorMulticast_port);
  monitorTTL = options->FindUINT32(Opt_MonitorMulticast_TTL);

  if (strcmp(monitorMulticast.ipAddress, "")!=0) {
    char unitID[80];
    RemoteID myAddress;
    if (CommunicationObject::GetHostIPAddress(myAddress.ipAddress)==gm_False)
      return;
    if ((myAddress.port = server->getLocalPort())==0) return;

    sprintf(unitID, "%s @ %d", (char*)distillerType, 
	    myAddress.port);
    monitorClient = new MonitorClient(unitID, monitorMulticast.ipAddress,
				      monitorMulticast.port, monitorTTL);
    if (monitorClient==NULL) VoidReturn(errOutOfMemory);
    if (Error::getStatus()!=success) return;

    monitorClient->GaspOnStdSignals();
    SetRemoteLogging(monitorClient);
  }
  else {
    //SetFileLogging("/tmp/log.distiller");
    SetStderrLogging("Dist: ");
  }

  gm_Log("Starting up distiller on port " << server->getLocalPort() << "\n");
  LogArgs(options_, argc, argv);


  pthread_key_create(&index_key, NULL);
  pthread_setspecific(index_key, (void*)0);


  if (DistillerInit(distillerType.c_struct, argc, argv)!=distOk) {
    VoidReturn(errGenericError);
  }

  clib_response result;
  if ((result = Clib_Initialize((Options)options, NULL)) == CLIB_OK) {
      gm_Log("Cache client library initialized\n");
  } else {
    gm_Log("Cache client library wasn't initialized: error "
	   << (int)result << "\n");
  }

  // make sure the timers are disabled if we are sleeping
  DistillerSleepStatus sleepStatus_ =((options->FindUINT32(Opt_SleepOnStartup))
				      ? distSleep : distWakeUp);
  if (sleepStatus_==distSleep) Sleep_WakeUp(sleepStatus_);
  sleepStatus = sleepStatus_;

  if (IsPTMFound()==gm_True) {
    if (ConnectToPTM()==gm_True) {
      if (Register(options->FindUINT32(Opt_RequestID))==gm_True) return;
    }
  }

  Error::SetStatus(success);
  NoPTM();
  gm_Log("Could not register with the PTM; "
	 "trying to listen for new PTM address\n");
  ListenOnSharedBus();
}


MyDistiller::~MyDistiller()
{
  if (server!=NULL) {
    evs->RemoveCommunicationObject(server);
    delete server;
    server = NULL;
  }

  if (ptmConnection!=NULL) {
    evs->RemoveCommunicationObject(ptmConnection);
    delete ptmConnection;
    ptmConnection = NULL;
  }

  if (listener!=NULL) {
    evs->RemoveCommunicationObject(listener);
    delete listener;
    listener = NULL;
  }

  DELETE(loadNotificationTimer);
  DELETE(evs);
  DELETE(monitorClient);
  DELETE(workQueue);
}


gm_Bool
MyDistiller::Sleep_WakeUp(DistillerSleepStatus status)
{
  switch (status) {
  case distSleep:
  case distKill:
    if (sleepStatus==distSleep || sleepStatus==distKill) return gm_True;

    if (status==distSleep) {
      gm_Log("Distiller is going to sleep\n");
    }
    else {
      gm_Log("Distiller is trying to die. It'll exit after all current "
	     "requests have been satisfied\n");
    }
    DELETE(loadNotificationTimer);
#ifdef _REPORT_STATS
    DELETE(stats); // stats has an internal timer; must destroy it as well!
#endif
    sleepStatus = status;

    if (sleepStatus==distKill && reporter->IsThereWork()==gm_False) {
      // we are done; we can exit!
      Die("PTM is killing me");
    }

    return gm_True;

  case distWakeUp:
    if (sleepStatus==distWakeUp) return gm_True;

    gm_Log("Distiller is waking up\n");
    if (loadNotificationTimer==NULL) {
      loadNotificationTimer = new 
	LoadNotificationTimer(evs, 
			      options->FindUINT32(Opt_NotificationTimeout_ms));
      if (loadNotificationTimer==NULL) Return(gm_False, errOutOfMemory);
    }
#ifdef _REPORT_STATS
    if (stats==NULL) {
      stats = new Statistics(evs);
      if (stats==NULL) Return(gm_False, errOutOfMemory);
    }
#endif
    
    sleepStatus = status;
    return gm_True;
  }

  // this path should never get taken!
  Return(gm_False, errGenericError);
}


gm_Bool
MyDistiller::ConnectToPTM()
{
  ptmConnection = new PTMConnection;
  if (ptmConnection==NULL) Return(gm_False, errOutOfMemory);
  if (ptmConnection->Connect(ptmUnicast.ipAddress, ptmUnicast.port)==gm_False){
    delete ptmConnection;
    ptmConnection = NULL;
    return gm_False;
  }

  if (evs->AddCommunicationObject(ptmConnection)==gm_False) {
    delete ptmConnection;
    ptmConnection = NULL;
    return gm_False;
  }

  // we were able to set up a PTM connection - no need to listen on the shared
  // bus any more

  if (listener!=NULL) {
    evs->RemoveCommunicationObject(listener);
    delete listener;
    listener = NULL;
  }

  return gm_True;
}


gm_Bool
MyDistiller::ListenOnSharedBus()
{
  if (listener!=NULL) return gm_True;

  listener = new Listener(ptmMulticast.ipAddress, ptmMulticast.port);
  if (listener==NULL) Return(gm_False, errOutOfMemory);
  if (Error::getStatus()!=success) return gm_False;

  if (evs->AddCommunicationObject(listener)==gm_False) return gm_False;
  return gm_True;
}


void
MyDistiller::ClosePTMConnection()
{
  if (ptmConnection!=NULL) {
    evs->RemoveCommunicationObject(ptmConnection);
    delete ptmConnection;
    ptmConnection = NULL;
    NoPTM();
  }
}


gm_Bool
MyDistiller::PTMLocationReceived(RemoteID &newPtmAddress)
{
  if (ptmConnection!=NULL) return gm_True;

  gm_Log("Found PTM at " << newPtmAddress.ipAddress << '/' 
	 << newPtmAddress.port << '\n');
  strcpy(ptmUnicast.ipAddress, newPtmAddress.ipAddress);
  ptmUnicast.port = newPtmAddress.port;

  if (ConnectToPTM()==gm_False) {
    // only errOutOfMemory is a real error
    if (Error::getStatus()==errOutOfMemory) return gm_False;
    Error::SetStatus(success);
  }

  return Register(0);
}


gm_Bool
MyDistiller::Register(UINT32 requestID)
{
  RemoteID myAddress;
  OStream  ostream;

  if (ptmConnection==NULL) Return(gm_False, errGenericError);

  if (CommunicationObject::GetHostIPAddress(myAddress.ipAddress)==gm_False)
    return gm_False;
  if ((myAddress.port = server->getLocalPort())==gm_False) return gm_False;

  ostream << myAddress << '|' << options->Find(Opt_DistillerType) << '|' 
	  << (UINT32)sleepStatus << '|' 
	  << options->FindUINT32(Opt_RestartOnDeath);
  if (ostream.IsGood()==gm_False) return gm_False;

  gm_Packet packet(pktRegisterDistiller, ostream.getLength(), requestID, 
		ostream.getData());
  if (ptmConnection->Write(&packet)==gm_False) {
    // could not send to the PTM
    // probably connection just went down
    // let the event handler detect it
    // for now simply return success
  
    return gm_True;
  }

  gm_Log("Registered successfully with the PTM\n");
  return gm_True;
}


void
MyDistiller::Deregister()
{
  if (ptmConnection==NULL) return;

  gm_Packet packet(pktDeregisterDistiller, 0, 0, NULL);
  if (ptmConnection->Write(&packet)==gm_False) {
    // could not send to the PTM
    // probably connection just went down
    // let the event handler detect it
    // for now simply return success
  
    return;
  }

  gm_Log("Deregistered successfully from the PTM\n");
}


gm_Bool
MyDistiller::Reregister(DistillerType *type)
{
  options->Remove(Opt_DistillerType);
  if (options->Add(Opt_DistillerType, (char*)(*type))==gm_False) 
    return gm_False;

  gm_Log("Reregistering a new type: " << (*type) << "\n");
  if (ptmConnection==NULL) return gm_True;

  // we are connected to the PTM; send a reregister message

  OStream  ostream;

  ostream << (*type);
  if (ostream.IsGood()==gm_False) return gm_False;

  gm_Packet packet(pktReregisterDistiller, ostream.getLength(), 0, 
		   ostream.getData());
  if (ptmConnection->Write(&packet)==gm_False) {
    // could not send to the PTM
    // probably connection just went down

    return ptmConnection->EvConnectionBroken(evs);
  }

  return gm_True;
}


gm_Bool
MyDistiller::WorkReceived(Work *work)
{
  DistillerStatus status;
  gm_Bool         res;
  Statistics     *stats;
  unsigned long   i;
  unsigned long   ip = 0;

  status = DefaultDistillationCost(&work->cost);
  /*status = ComputeDistillationCost(work->args, work->numberOfArgs,
				   &work->input, &work->cost);*/
  if (status!=distOk) {
    work->status = status;
    return evs->PostEvent(evDistillerReply, this, work);
  }

  reporter->NewWork(work);

  /* Let's pull out the client IP address out of the Work prefs structure.
     If it's not in there, leave it as 0.0.0.0 */
  for (i=0; i<work->numberOfArgs; i++) {
    if (ARG_ID(work->args[i]) == FRONT_CLIENT_IP) {
      ip = (unsigned long) ARG_INT(work->args[i]);
//      fprintf(stderr, "Got ip %lu\n", ntohl(ip));
      break;
    }
  }
  res = workQueue->Add((void *) work, ip);
#ifdef _REPORT_STATS
  stats = MyDistiller::getInstance()->getStats();
  if (stats != NULL) stats->IncrementTotalRequests();
  //MyDistiller::getInstance()->getStats()->AdjustAvgQLength(queue.getSize());
#endif
  return res;
}


gm_Bool
MyDistiller::SendLoadInformation()
{
  Load load;
  reporter->Report(&load);
  if (ptmConnection==NULL || IsPTMFound()==gm_False) return gm_True;
  OStream stream;
  stream << load;
  if (stream.IsGood()==gm_False) return gm_False;
  gm_Packet packet(pktDistillerLoad, stream.getLength(), stream.getData());
  if (ptmConnection->Write(&packet)==gm_False) {
    // ignore network errors
    Error::SetStatus(success);
    return gm_True;
  }
  return gm_True;
}


gm_Bool
LoadNotificationTimer::EvTimer(EventSystem */*evs*/)
{
  return MyDistiller::getInstance()->SendLoadInformation();
}


gm_Bool
MyDistiller::HandleEvent(EventSystem */*evs*/, EventType evType, void *args)
{
  switch (evType) {
  case evDistillerReply:
    {
      Work *work = (Work*)args;
      if (work==NULL) Return(gm_False, errGenericError);
      reporter->WorkDone(work);
      if (work->replyObject->IsDeleted()==gm_False) {
	// only if the connection is still alive, should we try to send
	// stuff on it
	work->replyObject->SendReply(work->status, &work->output, 
				     work->replyID);
      }
      delete work;
      if (sleepStatus==distKill && reporter->IsThereWork()==gm_False) {
	// we are done; we can exit!
	Die("PTM is killing me");
      }
      return gm_True;
    }

  case evDistillerReregister:
    {
      DistillerType *type = (DistillerType*) args;
      gm_Bool retval = Reregister(type);
      delete type;
      return retval;
    }

  default:
    Return(gm_False, errEvSysInvalidEvent);
  }
}


gm_Bool
MyDistiller::Run()
{
  consumer = new Thread;
  if (consumer==NULL) Return(gm_False, errOutOfMemory);
  if (consumer->Fork(ForkHelper, (void*)this)==gm_False) return gm_False;

  return evs->Run();
}


void*
MyDistiller::ForkHelper(void *args)
{
  ((MyDistiller*)args)->ConsumerMain();
  return 0;
}


void
MyDistiller::ConsumerMain()
{
  Work *work;

  while (gm_True) {
    work = (Work *) workQueue->Remove();

#ifdef _REPORT_STATS
  //MyDistiller::getInstance()->getStats()->AdjustAvgQLength(queue.getSize());
#endif

    if (work==NULL) {
      // there is no more work! so finish this thread
      return;
    }

    if (work->replyObject->IsDeleted()==gm_True) {
      // the reply connection is gone; no point in distilling this stuff
      delete work;
      continue;
    }

    strcpy(work->output.mimeType, work->input.mimeType);
#ifdef __INSTRUMENT__
    timeval start, end;
    gettimeofday(&start, NULL);
#endif
    DistillerMainTimer *timer = NULL;
    timer = new DistillerMainTimer(this);
    work->status = DistillerMain(work->args,   work->numberOfArgs,
				 &work->input, &work->output);
    if (timer!=NULL) delete timer;
#ifdef __INSTRUMENT__
    gettimeofday(&end, NULL);
    work->distillTime = tv_timesub(end, start);
#endif
    evs->PostEvent(evDistillerReply, this, work);
#ifdef _REPORT_STATS
    Statistics *stats = MyDistiller::getInstance()->getStats();
    if (stats!=NULL) stats->IncrementDoneRequests();
#endif
  }
}


gm_Bool
MyDistiller::ExpandOptions(OptionDatabase *optDB)
{
  const char *value;
  char *slash1, *slash2;

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

  return gm_True;
}


gm_Bool
MyDistiller::CreateInstance(OptionDatabase *options,
                          int argc, const char *const *argv)
{
  /*instance = new MyDistiller(ptmAddress, multicast, monitorAddr, monitorTTL,
			   distillerType, notificationTimeout_ms, 
			   sleepOnStartup, optionsDBfile, requestID,
			   argc, argv);*/
  instance = new MyDistiller(options, argc, argv);
  if (instance==NULL) Return(gm_False, errOutOfMemory);
  if (Error::getStatus()!=success) return gm_False;

  signal(SIGINT,  SigInt);
  /*void (*handler)(int);
  handler = signal(SIGTERM, SigTerm);
  printf("Prev handler (%p) %s SIG_ERR\n", handler, 
	 (handler==SIG_ERR) ? "is" : "is not");*/
  return gm_True;
}


void
MyDistiller::SigInt(int /*sig*/)
{
  //MyDistiller::getInstance()->Die("Ctl-C pressed");
  Abort("Ctl-C pressed");
}


void
MyDistiller::SigTerm(int /*sig*/)
{
  //printf("Starting to die\n");
  MyDistiller::getInstance()->Die("Normal termination");
  //printf("Dying\n");
}


void
MyDistiller::Die(const char *message)
{
  /*Deregister();

  gm_Log("Ending the distiller: " << message << "\n");
  Error::Print();
  MonitorClient *client = getMonitorClient();
  if (client!=NULL) client->Gasp(0);*/
  
  strcpy(end_string_, message);
  evs->PostEvent(evQuit, NULL, NULL);
}


void
MyDistiller::Abort(char *string)
{
  gm_Log("Aborting program: " << string << "\n\n");
  Error::Print();
  //if (instance!=NULL) delete instance;
  MonitorClient *client = MyDistiller::getInstance()->getMonitorClient();
  if (client!=NULL) client->Gasp(0);
  exit(-1);
}


void
MyDistiller::Usage(char *programPath)
{
  char *programName;
  programName = strrchr(programPath, '/');
  programName = ((programName==NULL) ? programPath : programName+1);

  gm_Log("Usage: " << programName << " [options]\n\n"
	 <<"Options:\n"
         <<"    -o <options-file-name> /*optional*/\n"
	 <<"    -p <ptm-ip-address/port-number> /*optional*/\n"
	 <<"    -t <distiller-type> /****required****/\n"
	 <<"    -n <notification-timeout-ms> /*optional*/\n"
	 <<"    -m <DistillerMain-timeout-ms> /*optional*/\n"
	 <<"    -d <die-timer-sec> /*optional*/\n"
	 <<"    -s /* optional:indicates whether to go to sleep on startup*/\n"
	 <<"    -e /* optional:indicates to PTM whether to go to try \n"
	 <<"          restarting this distiller if it dies unnaturally*/\n"
	 <<"    -r <request-id> /*optional*/\n"
	 <<"       /*defaults to zero; used only when the PTM spawns a "
	   "distiller*/\n");
  exit(-1);
}


MonitorClientID
DistillerGetMonitorClientID()
{
  return (MonitorClientID) MyDistiller::getInstance()->getMonitorClient();
}


void
DistillerRename(C_DistillerType type_)
{
  MyDistiller *instance = MyDistiller::getInstance();
  DistillerType *type = new DistillerType(type_);
  instance->getEventSystem()->PostEvent(evDistillerReregister, instance, type);
}


extern "C" {
#include "thr_cntl.h"
}

void INST_set_thread_state(int thrindex, thread_state_t state)
{
  char msg[32], args[256];

  /*
   *  Generate thread state "deltas" vector
   */
  thrindex = 0;
  sprintf(msg,"%d=%d", thrindex, state);
  sprintf(args, "Array 1 %d", THREAD_STATE_MAX);

  MyDistiller::getInstance()->getMonitorClient()->
    Send("Relaxen und watchen das Blinkenlights", msg, args);
}



int
main(int argc, char *argv[])
{
  char     distillerType[MAXDISTILLER_TYPE_STRING]="";
  UINT32   requestID=0, notificationTimeout_ms=DefaultNotificationTimeout_ms,
    distillerMainTimeout_ms=DefaultDistillerMainTimeout_ms,
    noLoadTimeout_sec=NoLoadTimeout_sec;
  RemoteID ptmUnicast("", 0);
  char     *cmdLineOptions = "o:p:t:n:m:d:r:se";
  char     optionsDBfile[MAXPATH]="";
  int      optCh;
  gm_Bool  sleepOnStartup = gm_False, mustRestartOnDeath = gm_False;

  optind = 1;
  while ( (optCh = getopt(argc, argv, cmdLineOptions))!=-1) {
    switch (optCh) {
    case 'o':
      strcpy(optionsDBfile, optarg);
      break;
      
    case 'p':
      if (sscanf(optarg,"%[^/]/%d",ptmUnicast.ipAddress,&ptmUnicast.port)!=2)
	MyDistiller::Usage(argv[0]);
      break;

    case 't':
      strcpy(distillerType, optarg);
      break;

    case 'n':
      sscanf(optarg, "%lu", &notificationTimeout_ms);
      if (notificationTimeout_ms==0) 
	notificationTimeout_ms = DefaultNotificationTimeout_ms;
      break;

    case 'm':
      sscanf(optarg, "%lu", &distillerMainTimeout_ms);
      if (distillerMainTimeout_ms==0) 
	distillerMainTimeout_ms = DefaultDistillerMainTimeout_ms;
      break;

    case 'd':
      sscanf(optarg, "%lu", &noLoadTimeout_sec);
      break;

    case 'r':
      sscanf(optarg, "%lu", &requestID);
      break;

    case 's':
      sleepOnStartup = gm_True;
      break;

    case 'e':
      mustRestartOnDeath = gm_True;
      break;

    case ':':
    case '?':
    default:
      MyDistiller::Usage(argv[0]);
      break;
    }
  }

  if (*distillerType=='\0') MyDistiller::Usage(argv[0]);

  OptionDatabase options;
  if (*optionsDBfile!='\0') {
    if (options.Create(optionsDBfile)==gm_False)
      MyDistiller::Abort("Could not open the options file");
  }
      
  options.Add(Opt_DistillerType, distillerType);
  options.Add(Opt_PTMUnicast_IP, ptmUnicast.ipAddress);
  options.Add(Opt_PTMUnicast_port, ptmUnicast.port);
  options.Add(Opt_NotificationTimeout_ms, notificationTimeout_ms);
  options.Add(Opt_DistillerMainTimeout_ms, distillerMainTimeout_ms);
  options.Add(Opt_NoLoadTimeout_sec, noLoadTimeout_sec);
  options.Add(Opt_RequestID, requestID);
  options.Add(Opt_SleepOnStartup, sleepOnStartup);
  options.Add(Opt_RestartOnDeath, mustRestartOnDeath);
  if (MyDistiller::ExpandOptions(&options)==gm_False) {
    MyDistiller::Abort("Could not expand options database");
  }

  char debugString[256];
  sprintf(debugString, "%s: ", distillerType);
  /*char debugName[256], *slash;
  slash = strchr(distillerType, '/');
  if (slash==NULL) slash = distillerType-1;
  sprintf(debugName, "debug.%s", slash+1);
  Debug_::getInstance()->Initialize("et", debugString, debugName);*/
  Debug_::getInstance()->Initialize("et", debugString);
  if (MyDistiller::CreateInstance(&options, 
				argc - optind, argv+optind)==gm_False) {
    MyDistiller::Abort("Could not start up the distiller");
  }

  if (MyDistiller::getInstance()->Run()==gm_False)
    MyDistiller::Abort("Error occurred while running the distiller");

  //printf("Finished the Run() call\n");
  // the distiller is ending normally
  MyDistiller::getInstance()->Deregister();

  gm_Log("Ending the distiller: " << end_string_ << "\n");
  Error::Print();
  MonitorClient *client = MyDistiller::getInstance()->getMonitorClient();
  if (client!=NULL) client->Gasp(0);

  return 0;
}
