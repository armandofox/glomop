#ifndef __REXEC_H__
#define __REXEC_H__

#include "defines.h"
#define MAX_HOSTNAME 256

class RExec {
public:
  RExec() : rsh(0), rshArgs(0), hosts(0), numberOfHosts(0), minLoadLevel(0), 
    currentHost(-1) { };
  ~RExec() { 
    if (rsh!=0) delete [] rsh;
    if (rshArgs!=0) delete [] rshArgs;
    if (hosts!=0) delete [] hosts;
  }
  gm_Bool Initialize(const char *rsh_="", const char *rshArgs_="", 
		     const char *hosts_="");
  gm_Bool Update(const char *rsh_="", const char *rshArgs_="", 
		 const char *hosts_="");
  void UpLevel(char *ipAddress) {
    Host *host = FindByIP(ipAddress);
    if (host!=0) {
      host->UpLevel();
      RecomputeMinLoadLevel();
    }
  }
  void DownLevel(char *ipAddress) {
    Host *host = FindByIP(ipAddress);
    if (host!=0) {
      host->DownLevel();
      RecomputeMinLoadLevel();
    }
  }

  gm_Bool Exec(const char *executable, char **argv);
  gm_Bool IsRemoteExecution() { return ((rsh==NULL) ? gm_False : gm_True); }

  static gm_Bool getAbsolutePath(const char *relativePath, 
				 char *absolutePath, UINT32 length);
  static const char *getFilenameOnly(const char* filePath);

  
protected:
  class Host {
  public:
    Host() : loadLevel(0) { *name = '\0'; *ipAddress = '\0'; };
    void Initialize(char *hostName_, int priority_);
    void Exec(const char *rsh, const char *rshArgs, char **argv);
    
    void UpLevel()   { loadLevel++; };
    void DownLevel() { if (loadLevel > 0) loadLevel--; };
    char *getName() { return name; };
    char *getIPAddress() { return ipAddress; };
    int  getLoadLevel()  { return loadLevel; };
    int  getPriority()   { return priority;  };
    void SetLoadLevel(int loadLevel_)  { loadLevel = loadLevel_; }
  protected:
    char name[MAX_HOSTNAME];
    char ipAddress[MAXIP];
    int  priority;
    int  loadLevel;		// # of processes currently started up by the 
				// Proxy system on this host
  };

  Host *FindByIP(char *ipAddress);
  Host *FindLeastLoadedHost();
  void RecomputeMinLoadLevel();
private:
  char    *rsh, *rshArgs;
  Host    *hosts;
  int     numberOfHosts;
  int     minLoadLevel;
  int     currentHost;
};


#endif // __REXEC_H__
