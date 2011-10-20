#include <sys/types.h>
#include <sys/stat.h>
#include "ptm.h"
#include "options.h"

OptionsFileTimer::OptionsFileTimer(EventSystem *evs, UINT32 beaconingPeriod_ms,
                                   OptionDatabase *opt)
    : gm_Timer(evs, SECONDS(beaconingPeriod_ms),
               USECONDS(beaconingPeriod_ms)), options(opt) 
{
   struct stat buf;
   if (options->Find(Opt_OptionsFile) != NULL) {
      stat(options->Find(Opt_OptionsFile), &buf);
      lastModified = buf.st_mtime;
   } else {
      lastModified = -1;
   }
};

gm_Bool OptionsFileTimer::EvTimer(EventSystem */*evs*/)
{ 
  char filename[MAXPATH];
  struct stat buf;
  if (options->Find(Opt_OptionsFile)) {
     stat(options->Find(Opt_OptionsFile), &buf);
     if (lastModified != buf.st_mtime) {
        lastModified = buf.st_mtime;
        assert(options->Find(Opt_OptionsFile) != NULL);
        strncpy(filename, options->Find(Opt_OptionsFile), MAXPATH);
        gm_Log("Checking options database: " << filename << "\n");
        options->Update(filename);
        PTM::getInstance()->OptionsUpdate(options);
     } 
  }
  return gm_True; 
}

