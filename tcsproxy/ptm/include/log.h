#ifndef __LOG_H__
#define __LOG_H__

#include "packet.h"
#include "communication.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>


class MonitorClient {
public:
  MonitorClient(const char *unitID_, const char *ipAddr, Port port, 
		int ttl);
  gm_Bool Send(const char *fieldID, const char *value, const char *script) {
    return Send(unitID, fieldID, value, script);
  };
  gm_Bool Send(const char *unitID, const char *fieldID, const char *value, 
	    const char *script);
  
  void GaspOnSignals(int nsigs, int sigs[]);
  void GaspOnStdSignals();
  void Gasp(int sig);

  const char *getAddress() { return address; };
  const char *getUnitID() { return unitID; };
protected:
  static void SigHandler(int);

  char   hostName[MAXLINE];
  UINT32 pid;

  char   unitID[MAXLINE];
  char   address[MAXLINE];
  Socket *socket;		// may be a UdpSocket or a MulticastSocket

  static MonitorClient *defaultClient;
  static char Separator;
};

class Log {
protected:
  MonitorClient *client;
  FILE *logFD;
  char *prefix;
  static Log log_;

public:
  Log() : client(NULL), logFD(NULL), prefix("") { };
  ~Log() { if (logFD!=NULL) { fclose(logFD); logFD = NULL; } };
  void LogRemotely(MonitorClient *client_) { client = client_; };
  void LogToFile(char *filename) { logFD = fopen(filename, "w"); };
  void LogToStderr(char *prefix_) { prefix = prefix_; };
  Log& operator << (const char *string);
  static Log& getInstance() { return log_; };

};


#define gm_Log(x) \
{ \
  OStream stream; \
  stream << x; \
  Log::getInstance() << stream.getData(); \
}


#define SetRemoteLogging(monitorClient) \
  Log::getInstance().LogRemotely(monitorClient)


#define SetFileLogging(filename) \
  Log::getInstance().LogToFile(filename)


#define SetStderrLogging(prefix) \
  Log::getInstance().LogToStderr(prefix)


#endif // __LOG_H__
