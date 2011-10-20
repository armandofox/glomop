#include "error.h"
#include "log.h"
#include "threads.h"
#include <errno.h>
#include <iostream.h>

static pthread_key_t errorKey;


static class ErrorInitializer {
public:
  ErrorInitializer() { Error::Initialize(); };
} __error__init__;


Error *
Error::get()
{
  Error *error;
  error = (Error*) pthread_getspecific(errorKey);
  if (error==NULL) {
    error = new Error;
    if (pthread_setspecific(errorKey, (void*) error)!=0) {
      return 0;
    }
  }
  return error;
}


void
Error::Initialize()
{
  ThreadError(pthread_key_create(&errorKey, Error::Destroy));
}


void 
Error::Destroy(void *value)
{
  Error *error = (Error*) value;
  if (error!=0) delete error;
}


void
Error::Print_()
{
  gm_Log("Program status:\n");
  gm_Log("           Error status        = " << Error::getStatus() << "\n");
  gm_Log("           Thread error status = " << Error::getThreadStatus()
	 << "\n");

  if (Error::getStatus()!=success) {
    gm_Log("           Error file-name     = " << Error::getFilename()<< "\n");
    gm_Log("           Error line number   = " << Error::getLineNo() << "\n");
  }

  gm_Log(   "           errno = " << errno << "\n");
  if (errno!=0) {
    gm_Log( "           Errno explanation: " << strerror(errno) << "\n");
  }
}


void 
Error::Print() {
  Error *error = get();
  if (error!=NULL) error->Print_();
  else {
    gm_Log("Cannot locate error object!\n");
  }
}


void
gm_Abort(char *message)
{
  cerr << "Fatal error occurred:\n" << message << "\nAborting program...\n";
  abort();
}



void new_error()
{
  cerr << "operator new failed: aborting program\n";
  abort();
}


typedef void (*PF)();
extern "C" PF set_new_handler(PF);


static class NewErrorInitializer {
public:
  NewErrorInitializer() {
    set_new_handler(new_error);
  }

} __newErrorInitializer__;




Debug_ Debug_::instance_;
