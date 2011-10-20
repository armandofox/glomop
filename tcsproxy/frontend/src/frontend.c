#include "config_tr.h"
#include "thr_cntl.h"
#include "clib.h"
#include "frontend.h"
#include "userpref.h"
#include "debug.h"
#include "optdb.h"
#include "proxyinterface.h"
#include "wingreq.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static void init(int ac, char *av[]);
Options runtime_options; /* options databse */

#ifdef SIGWAITING
static void proxy_blocked(int sig); /* signal handler for threads deadlock */
#endif /* defined SIGWAITING */

const char *fe_server_name,
  *fe_set_prefs_url,
  *fe_get_prefs_url,
  *fe_prefs_change_url_string,
  *fe_prefs_set_url_string,
  *fe_agg_string;

static void
usage(void)
{
    fprintf(stderr,
            "Usage: frontend configfilename\n");
    exit(1);
}

static void
missing_field_error(const char *fld)
{
  fprintf(stderr,
          "Reading config file: property '%s' missing or illegal value\n",
	  fld);
  exit(1);
}

int
main(int ac, char *av[])
{
  gm_Bool response;
  int boost_prio = 0;
  UINT16 port;
  
#ifdef SIGWAITING
  /* deliver a signal if all threads get blocked (Solaris only) */
  (void)signal(SIGWAITING, proxy_blocked);
#endif

  init(ac,av);
  boost_prio = (int)Options_Find_UINT32(runtime_options, "frontend.acceptPrio");
  /* if not found, boost_prio will stay zero, which is fine. */


  /*
   *  Initialize the httpd listener thread.
   */
  if ((port = (UINT16)Options_Find_UINT32(runtime_options,
                                          "frontend.http_listen_port"))
      == 0) {
    missing_field_error("frontend.http_listen_port");
    /* NOTREACHED */
  }
  if ((response = httpreq_init(port,boost_prio)) != gm_True) {
    MON_SEND_3(MON_ERR,"httpreq_init %d: %s\n", port, strerror(errno));
    proxy_shutdown(0);
    /* NOTREACHED */
  }

  /*
   *  Initialize the glaftpd (GloMop Lightweight Appl. Frame Transport
   *  Protocol) listener thread.
   */
  if ((port = (UINT16)Options_Find_UINT32(runtime_options,
                                          "frontend.wing_listen_port"))
      == 0) {
    missing_field_error("frontend.wing_listen_port");
    /* NOTREACHED */
  }
  if ((response = wingreq_init(port,boost_prio)) != gm_True) {
    MON_SEND_3(MON_ERR, "wingreq_init %d: %s\n", port, strerror(errno));
    proxy_shutdown(0);
    /* NOTREACHED */
  }
  
  /* BUG::this should pthread_join() all the threads, then exit.  Should
     also set watchdog/livelock timers here. */ 
  while (1) 
    sleep(10000);
}

/*
 *  Only the options file name is parsed on the command line. All other
 *  runtime options are specified in the options file.
 */
static void
init(int ac, char *av[])
{
  clib_response cl;
  gm_Bool response;
  char *portptr;
  const char *monaddr;
  char monaddr_copy[256];
  const char *uprefs_db_file;
  int result;
  char *logfilename;
  
  if (ac < 2)
    usage();
  
  if ((result = Options_New(&runtime_options, av[1])) != 0) {
    fprintf(stderr, "Error %d opening configuration file '%s', bye bye!\n",
            result, av[1]);
    exit(1);
  }

  /*
   *  Initialize monitor client if possible
   */

  if ((monaddr = Options_Find(runtime_options, "Monitor.listen"))
      == NULL) {
    missing_field_error("Monitor.listen");
    /* NOTREACHED */
  }
  strncpy(monaddr_copy, monaddr, sizeof(monaddr_copy)-1);
  if ((portptr = strchr(monaddr_copy, '/')) == NULL) {
    usage();
    /* NOTREACHED */
  }
  *portptr = 0;

  /*
   *  Get "magic URL" namespace params
   */
  if ((fe_server_name = Options_Find(runtime_options, "frontend.webServerName"))
      == NULL)
    missing_field_error("frontend.webServerName");
  if ((fe_set_prefs_url = Options_Find(runtime_options, "frontend.prefsFormDisplayUrl"))
      == NULL)
    missing_field_error("frontend.prefsFormDisplayUrl");
  if ((fe_get_prefs_url = Options_Find(runtime_options, "frontend.getPrefsUrl"))
      == NULL)
    missing_field_error("frontend.getPrefsUrl");
  if ((fe_prefs_change_url_string =
       Options_Find(runtime_options, "frontend.prefsChangeUrlString"))
      == NULL)
    missing_field_error("frontend.prefsChangeUrlString");
  if ((fe_prefs_set_url_string =
       Options_Find(runtime_options, "frontend.prefsFormSubmitUrl"))
      == NULL)
    missing_field_error("frontend.prefsFormSubmitUrl");
  if ((fe_agg_string = Options_Find(runtime_options, "frontend.aggString"))
      == NULL)
    missing_field_error("frontend.aggString");

  /*
   *  Initialize PTM stub
   */
  proxy_debug_2(DBG_PTM, "Initializing PTM stub");
  if ((response = InitializeDistillerCache(runtime_options,NULL)) != gm_True) {
    fprintf(stderr,"PTM initialization failed: %d\n", response);
    proxy_shutdown(0);
    /* NOTREACHED */
  }

  /* BUG:: should parse out TTL, not assume it's 16! */
  if ((gMon = MonitorClient_Create(FE_GetMonitorClientUnitID(),
                                   monaddr_copy, /* monitor IP addr */
                                   strtoul(portptr+1, NULL, 0), /*  port */
                                   16)) /* ttl */
      == NULL) {
    fprintf(stderr, "Warning: couldn't create monitor client\n");
  } 
  /*
   *  Initialize partitioned Harvest
   */

  proxy_debug_2(DBG_CACHE, "Initializing partitioned Harvest");
  MON_SEND(MON_LOGGING,"Initializing partitioned Harvest\n");

  if ((cl = Clib_Initialize(runtime_options, gMon)) != CLIB_OK) {
    fprintf(stderr, "Error %d initializing harvest FE\n", (int)cl);
    proxy_shutdown(0);
    /* NOTREACHED */
  }
  /*
   *  Initialize user prefs database
   */
  proxy_debug_2(DBG_ADMIN, "Initializing user prefs database");
  if ((uprefs_db_file = Options_Find(runtime_options, "frontend.userprefs"))
      == NULL)
    uprefs_db_file = "./uprefs.gdbm";
  
  if ((response = init_prefs(uprefs_db_file)) != gm_True) {
    fprintf(stderr,"User prefs database initialization ('%s') failed\n",
            uprefs_db_file);
    proxy_shutdown(0);
    /* NOTREACHED */
  }
  /*
   *  Start logging incoming requests
   */
  if ((logfilename = init_logging()) == NULL) {
    MON_SEND_2(MON_ERR,"Couldn't initialize logging: %s\n", strerror(errno));
  }    
  /*
   *  Wait for PTM to start up before proceeding.
   */
  MON_SEND(MON_LOGGING, "Waiting for PTM\n");
  WaitForPTM();
  /*
   *  Initialize all the worker threads
   */
  {
    int nth;
    if ((nth = (int)Options_Find_UINT32(runtime_options, "frontend.nthreads"))
        == 0)
      nth = 25;
    MON_SEND_2(MON_LOGGING,"Spawning %d worker threads\n", nth);
    (void)make_threads(nth, -2);
  }
}

/*
 *  proxy_shutdown: close down the world
 *    Usually called as signal handler.
 *    Never returns; exits with status 1.
 */
void
proxy_shutdown(int sig)
{
  MON_SEND_2(MON_LOGGING,"Going down on signal %d", sig);
  /*
   *  NOTE!  We can't call shutdown() and close() before exiting, because we
   *  are in a signal handler and those things are implicitly mutex-protected
   *  by the kernel -- if someone else has the lock (which it does; the
   *  http-accept thread), we will just block here forever.
   */
  MonitorClient_Gasp(0);
  log_close();
#if 0
  shutdown(gSock, 2);
  close(gSock);
#endif
  exit(1);
}

/*
 *  This function is called as a signal handler for SIGWAITING (all
 *  threads are blocked) on systems that define that signal.
 */

#ifdef SIGWAITING
static void
proxy_blocked(int sig)
{
  extern Monitor *gMon;

  MON_SEND(MON_ERR,"Received SIGWAITING!  All threads are blocked! Help!\n");
}
#endif /* SIGWAITING */
