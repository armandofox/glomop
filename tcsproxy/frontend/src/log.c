/*
 *  log.c
 *  Error logging and debugging
 */


#include "debug.h"
#include <stdarg.h>
#include <stdio.h>

static int debug_mask = DBG_ALL;

void set_debug_mask(mask)
     int mask;
{}
  
#ifdef LOGGING

#define LOG_CHANGE  (60*60*12)  /* seconds between logfile changes */

static char logfilename[255];
static char logfilegeneration = 0;
static time_t log_started = 0;
static FILE *logfile = NULL;

char *
init_logging(void)
{
  pid_t p = getpid();

  sprintf(logfilename, "frontend.log.%lu.%d", (UINT32)p, logfilegeneration);
  log_started = time(NULL);
  
  if ((logfile = fopen(logfilename, "w")) == NULL)
    return NULL;
  else {
    /*    setbuf(logfile, NULL); */
    logfilegeneration++;
    MON_SEND_2(MON_LOGGING,"Next logfile is %s\n", logfilename);
    return logfilename;
  }
}
    
void
gm_log(char *msg)
{
  if (logfile) {
    fprintf(logfile, "%s", msg);
    if (msg[strlen(msg)-1] != '\n')
      fprintf(logfile, "\n");
  }
  fflush(logfile);
  if (time(NULL) - log_started >= LOG_CHANGE) {
   if (fclose(logfile) == 0)
      init_logging();           /* reopen with new generation number */
  }
}

void
log_close(void)
{
  if (logfile)
    fclose(logfile);
}

#endif /* LOGGING */

void _proxy_errlog(const char *file,const int line,char *str,...)
{
  va_list ap;
  va_start(ap,str);
  fprintf(stderr, "%s(%d): ", file, line);
  vfprintf(stderr, str, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

void _proxy_debug(const char *file, const int line, int what, char *str, ...)
{
  va_list ap;
  va_start(ap,str);
  if (what & debug_mask) {
    fprintf(stderr, "Dbg %s(%d): ", file, line);
    vfprintf(stderr, str, ap);
    fprintf(stderr, "\n");
  }
  va_end(ap);
}
      
