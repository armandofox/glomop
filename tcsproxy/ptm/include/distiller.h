#ifndef __DISTILLER_H__
#define __DISTILLER_H__

#include "distinterface.h"
#include "defines.h"
#include "linkedlist.h"
#include "threads.h"
#include "eventhan.h"
#include "distdb.h"
#include "packet.h"
#include "ptmqueue.h"

//#define __INSTRUMENT__

class  CommunicationObject;
class  ProxyServer;
class  ProxyConnection;
class  PTMConnection;
class  Listener;
class  LoadNotificationTimer;
class  OptionDatabase;
struct Work;


static const EventType evDistillerReply      = evUserDefinedEvent+1;
static const EventType evDistillerReregister = evUserDefinedEvent+2;


#define _REPORT_STATS
#ifdef _REPORT_STATS

#define No_Of_QLengthWindows       10
#define Size_Of_QLengthWindow_usec 10000000

struct Statistics {
public:
  Statistics(EventSystem *evs);
  ~Statistics() {
    DELETE(timer);
  }
  void IncrementTotalRequests() {
    mutex.Lock();
    totalRequests++;
    mutex.Unlock();
  }
  void IncrementDoneRequests() {
    mutex.Lock();
    doneRequests++;
    mutex.Unlock();
  }
  void IncrementBadRequests() {
    mutex.Lock();
    badRequests++;
    mutex.Unlock();
  }
  /*void   AdjustAvgQLength(UINT32 length);
  double getAvgQLength();*/
  void   Report();

private:
  class StatsTimer : public gm_Timer {
  public:
    StatsTimer(EventSystem *evs) : gm_Timer(evs, SECONDS(ReportStats_ms),
					    USECONDS(ReportStats_ms)) { };
  protected:
    gm_Bool EvTimer(EventSystem *evs);
  };

  /*struct QLengthWindow {
    QLengthWindow() { Reset(); };
    void Reset() {
      lengthTimeProduct = 0;
      totalTime  = 0;
      gettimeofday(&lastUpdate, NULL);
    };
    void   AdjustAvgQLength(UINT32 length);

    UINT32  lengthTimeProduct;
    UINT32  totalTime;
    timeval lastUpdate;
  };
  QLengthWindow windows[No_Of_QLengthWindows];
  int currentQLengthWindow;*/

  UINT32  totalRequests;
  UINT32  doneRequests;
  UINT32  badRequests;

  StatsTimer *timer;
  Mutex mutex;
};
#define Stats(x) Distiller::getInstance()->getStats()->x
#endif


class LoadReporter {
public:
#ifndef __INSTRUMENT__
  LoadReporter(EventSystem *evs);
#else
  LoadReporter(EventSystem *evs, DistillerType *type);
#endif
  ~LoadReporter() {
#ifdef __INSTRUMENT__
    if (instFile!=NULL) fclose(instFile);
#endif
  }

  void    Report(Load *load);
  void    NewWork (Work *work);
  void    WorkDone(Work *work);
  gm_Bool IsThereWork() {
    // if the instTotalLatency_ms is zero, that means there is no more 
    // work to do
    return ((instTotalLatency_ms==0) ? gm_False : gm_True);
  }
  UINT32 getAvgLatency();
private:
  void AdjustAvgLatency();


  class NoLoadTimer : public gm_Timer {
  public:
    NoLoadTimer(EventSystem *evs, long seconds)
      : gm_Timer(evs, seconds, 0), workReceived(gm_False) { };
    gm_Bool EvTimer(EventSystem *evs);
    gm_Bool workReceived;
  };


  UINT32 instTotalLatency_ms, area;
  timeval lastUpdate, start;
  NoLoadTimer *noLoadTimer;
#ifdef __INSTRUMENT__
  FILE *instFile;
#endif
};

class MyDistiller : public EventHandler {
public:
  /*Distiller(RemoteID &ptm, RemoteID &multicast,
	    RemoteID &monitorAddr, int monitorTTL, DistillerType &type, 
	    UINT32 notificationTimeout_ms, gm_Bool sleepOnStartup,
	    char *optionsDBfile, UINT32 requestID, 
	    int argc, const char*const* argv);*/
  MyDistiller(OptionDatabase *options_, int argc, const char * const *argv);
  virtual ~MyDistiller();
  
  gm_Bool IsPTMFound() { return (strcmp(ptmUnicast.ipAddress, "")==0) ?
			gm_False : gm_True; };
  void NoPTM() { strcpy(ptmUnicast.ipAddress, ""); ptmUnicast.port = 0; };
  gm_Bool PTMLocationReceived(RemoteID &newPtmAddress);
  gm_Bool WorkReceived(Work *work);

  gm_Bool ConnectToPTM();
  void    ClosePTMConnection();
  gm_Bool ListenOnSharedBus();
  gm_Bool Register(UINT32 requestID);
  void    Deregister();
  gm_Bool Reregister(DistillerType *type);
  void    ConsumerMain();
  gm_Bool Run();
  gm_Bool SendLoadInformation();
  EventSystem *getEventSystem() { return evs; };
  OptionDatabase *getOptions() { return options; };
  MonitorClient *getMonitorClient() { return monitorClient; };

  gm_Bool Sleep_WakeUp(DistillerSleepStatus status);
  void Die(const char *message);

#ifdef _REPORT_STATS
  void ReportStats();
  Statistics *getStats() { return stats; };
#endif

  static void *ForkHelper(void *args);
  static void Abort(char *string);
  static void Usage(char *programPath);
  static void SigInt(int sig);
  static void SigTerm(int sig);
  static MyDistiller *getInstance() { return instance; };
  static gm_Bool ExpandOptions(OptionDatabase *optDB);
  static gm_Bool CreateInstance(OptionDatabase *options, int argc, 
				const char * const * argv);
  /*static gm_Bool CreateInstance(RemoteID &ptmAddress, RemoteID &multicast,
				RemoteID &monitorAddr, int monitorTTL,
				DistillerType &distillerType, 
				UINT32 notificationTimeout_ms,
				gm_Bool sleepStatus,
                                char *optionsDBfile,
				UINT32 requestID,
				int argc, const char*const* argv);*/

protected:
  DistillerStatus DefaultDistillationCost(DistillationCost *cost) {
    cost->estimatedTime_ms = 1;
    return distOk;
  }
  virtual gm_Bool HandleEvent(EventSystem *evs, EventType evType, void *args);
  
  LoadReporter *reporter;
  Thread *consumer;
  Queue *workQueue;
  
  RemoteID      ptmUnicast, ptmMulticast;
  EventSystem   *evs;
  ProxyServer   *server;
  PTMConnection *ptmConnection;
  Listener      *listener;
  MonitorClient *monitorClient;
  LoadNotificationTimer *loadNotificationTimer;
  DistillerSleepStatus   sleepStatus;
  OptionDatabase         *options;
#ifdef _REPORT_STATS
  Statistics    *stats;
#endif

  static MyDistiller *instance;
};


class ProxyServer : public TcpServer {
public:
  ProxyServer() : TcpServer(0) { }; 
  // let the system pick a port number
protected:
  gm_Bool EvConnectionReceived(EventSystem *evs, int newSocketID);
  
};


class GarbageCollectedObject {
public:
  GarbageCollectedObject() : numberOfRefs(0), isDeleted(gm_False) { };
  virtual ~GarbageCollectedObject() { };

  gm_Bool IsDeleted() { return isDeleted; };
  void MarkForDeletion()
  { if (numberOfRefs==0) delete this; else isDeleted = gm_True; }
  void AddReference()    { numberOfRefs++; };
  void RemoveReference() 
  { numberOfRefs--; if (isDeleted==gm_True && numberOfRefs==0) delete this; };
protected:
  UINT32 numberOfRefs;
  gm_Bool   isDeleted;
};


class ProxyConnection 
  : public TcpSocket,
    public gm_PacketHandler,
    public GarbageCollectedObject
{
public:
  ProxyConnection(int newSocketID) 
    : TcpSocket(newSocketID), gm_PacketHandler() { };
  gm_Bool SendReply(DistillerStatus status, DistillerOutput *output,
		    UINT32 replyID);
  
protected:
  gm_Bool EvSelect(EventSystem *evs) {return gm_PacketHandler::EvSelect(evs);};
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);
  
  gm_Bool EvConnectionBroken(EventSystem *evs);
  gm_Bool ReadPacket(gm_Packet *packet) { return Read(packet); };
  void ErrorOutput(const char *string) { gm_Log(string); };

  gm_Bool UnmarshallDistillerInput(Work *work, gm_Packet *packet);
};


class PTMConnection 
  : public TcpSocket,
    public gm_PacketHandler
{
public:
  gm_Bool EvConnectionBroken(EventSystem *evs);
protected:
  gm_Bool EvSelect(EventSystem *evs)
  { return gm_PacketHandler::EvSelect(evs); };
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { return Read(packet); };
  void ErrorOutput(const char *string) { gm_Log(string); };
};


class Listener 
  : public MulticastSocket,
    public gm_PacketHandler
{
public:
  Listener(char *ip, Port port) : MulticastSocket(), gm_PacketHandler()
  { if (CreateListener(ip, port)==gm_False) return; };
  
protected:
  gm_Bool EvSelect(EventSystem *evs)
  { return gm_PacketHandler::EvSelect(evs); };
  gm_Bool EvPacketReceived(EventSystem *evs, gm_Packet *packet);

  gm_Bool ReadPacket(gm_Packet *packet) { return Read(packet); };
  void ErrorOutput(const char *string) { gm_Log(string); };
};


class LoadNotificationTimer : public gm_Timer {
public:
  LoadNotificationTimer(EventSystem *evs, UINT32 notificationTimeout_ms) 
    : gm_Timer(evs, SECONDS(notificationTimeout_ms), 
	       USECONDS(notificationTimeout_ms)){};
protected:
  gm_Bool EvTimer(EventSystem *evs);
};


struct Work {
  Work(ProxyConnection *replyObj) 
    : args(0), numberOfArgs(0), 
      //freeOutputBuffer(gm_True),
      status(distOk), replyObject(replyObj), 
      replyID(0) { 
	cost.estimatedTime_ms = 0; 
	replyObject->AddReference(); 
#ifdef __INSTRUMENT__
	distillTime.tv_sec  = 0;
	distillTime.tv_usec = 0;
#endif
  };
  ~Work();

  Argument         *args;
  UINT32           numberOfArgs;

  DistillerInput   input;
  DistillerOutput  output;
  //gm_Bool          freeOutputBuffer;

  DistillerStatus  status;
  DistillationCost cost;

  ProxyConnection  *replyObject;
  UINT32           replyID;

#ifdef __INSTRUMENT__
  timeval          distillTime;
#endif
};


#endif // __DISTILLER_H__
