#include "rexec.h"
#include "log.h"
#include "utils.h"
#include <iostream.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>


const char *
RExec::getFilenameOnly(const char* filePath)
{
  int idx = strlen(filePath)-1;
  while(idx >= 0) {
    if (filePath[idx]=='/') break;
    idx--;
  }

  return &filePath[idx+1];
}


gm_Bool
RExec::getAbsolutePath(const char *relativePath, char *absolutePath,
		       UINT32 length)
{
  if (*relativePath!='/') {
    // this is a relative pathname; so prepend the current working dir

    if (getcwd(absolutePath, length)==NULL) 
      Return(gm_False, errBufferOverflow);
    if (strlen(absolutePath)+strlen(relativePath)+2 > length)
      Return(gm_False, errBufferOverflow);
    sprintf(absolutePath, "%s/%s", absolutePath, relativePath);
  }
  else {
    // this is already an absolute path; simply copy it
    if (strlen(relativePath)+1 > length)
      Return(gm_False, errBufferOverflow);
    strcpy(absolutePath, relativePath);
  }

  return gm_True;
}


gm_Bool
RExec::Initialize(const char *rsh_, const char *rshArgs_, const char *hosts_) 
  // rsh_: name of the remote shell program to use (usually, rsh or ssh)
  // hosts_: comma-separated list of hosts
  // rsh_=="" || rsh==NULL => don't use remote execution
{
  char hostName[MAX_HOSTNAME], *hostPtr, *priorityPtr;
  const char *ptr;
  int count, priority;

  if (rsh!=NULL) {
    delete [] rsh;
    rsh = NULL;
  }

  if (rshArgs!=NULL) {
    delete [] rshArgs;
    rshArgs = NULL;
  }

  if (hosts!=NULL) {
    delete [] hosts;
    hosts = NULL;
  }

  if (rsh_==NULL || *rsh_=='\0') {
    rsh = NULL;
  }
  else {
    rsh = new char [strlen(rsh_)+1];
    if (rsh==NULL) Return(gm_False, errOutOfMemory);
    strcpy(rsh, rsh_);
  }

  if (rshArgs_==NULL || *rshArgs_=='\0') {
    rshArgs = NULL;
  }
  else {
    rshArgs = new char [strlen(rshArgs_)+1];
    if (rshArgs==NULL) Return(gm_False, errOutOfMemory);
    strcpy(rshArgs, rshArgs_);
  }

  if (hosts_==NULL) hosts_ = "";
  // compute the number of hosts
  // it's simply the # of commas + 1!
  for (count=0, ptr=hosts_; *ptr!='\0'; ptr++)
    if (*ptr==',') count++;
  count++;

  // allocate the hosts array
  if (count==0) {
    hosts = NULL;
    numberOfHosts = 0;
  }
  else {
    hosts = new Host [count];
    if (hosts==NULL) Return(gm_False, errOutOfMemory);
    numberOfHosts = count;
    
    count = 0;
    ptr = hosts_;
    while(*ptr!='\0' && count < numberOfHosts) {
      
      // get the next hostname
      //    ignore leading whitespace
      while (isspace(*ptr))
	ptr++;
      if (*ptr=='\0') break;
      
      hostPtr = hostName;
      priorityPtr = NULL;
      // go on until a comma or a whitespace
      while (*ptr!=',' && *ptr!='\0' && !isspace(*ptr)) {
	*hostPtr++ = *ptr++;
	if (*(hostPtr-1)=='*') {
	  *(hostPtr-1) = '\0';
	  priorityPtr = hostPtr;
	}
	if (hostPtr-hostName > MAX_HOSTNAME - 1) // overflowed hostName
	  Return(gm_False, errFormattingError);
      }
      *hostPtr = '\0';
      
      // ignore whitespace until the comma
      while (isspace(*ptr))
	ptr++;
      
      // error if the next char is not a comma
      if (*ptr!=',' && *ptr!='\0') Return(gm_False, errFormattingError);
      if (*ptr!='\0') ptr++;
      
      // add it to the host array
      priority = 0;
      if (priorityPtr!=NULL)
	sscanf(priorityPtr, "%d", &priority);
      hosts[count].Initialize(hostName, priority);
      count++;
    }
  }

  return gm_True;
}


gm_Bool
RExec::Update(const char *rsh_, const char *rshArgs_, const char *hosts_)
{
  Host *oldHosts = hosts;
  int oldNumberOfHosts = numberOfHosts;
  int idx1, idx2;
  hosts = NULL;
  if (Initialize(rsh_, rshArgs_, hosts_)==gm_False) return gm_False;

  for (idx1=0; idx1 < numberOfHosts; idx1++) {
    for (idx2=0; idx2 < oldNumberOfHosts; idx2++) {
      if (strcmp(hosts[idx1].getName(), oldHosts[idx2].getName())==0) {
	hosts[idx1].SetLoadLevel(oldHosts[idx2].getLoadLevel());
	break;
      }
    }
  }

  if (oldNumberOfHosts!=0) delete [] oldHosts;
  return gm_False;
}


gm_Bool
RExec::Exec(const char *executable, char **argv)
{
  pid_t returnValue;
  Host  *host;

  // first determine the host object
  // you need to do this before forking off, since any state change as a
  // result of calling FindLeastLoadedHost() must happen in the parent
  host = FindLeastLoadedHost();

  // fork a child process
  returnValue = fork();
  if (returnValue==0) {
    // fork twice to overcome the zombie process problem
    returnValue = fork();
    if (returnValue != 0) {
      exit(returnValue);
    }
  }
  else if (returnValue > 0) {
    // wait for the first forked process to return
    returnValue = waitpid(returnValue, NULL, 0);
  }

  if (returnValue < 0) {
    // error occurred while forking
    gm_Log("Could not fork off child process\n");
    Return(gm_False, errCouldNotLaunch);
  }


  if (returnValue== 0) {
    // fork succeeded; this is the child process; try to execute the binary

    if (rsh==NULL) {
      if (execvp(executable, argv) < 0) {
	gm_Log("Error starting process: "<< getFilenameOnly(executable)<<'\n');
	exit(-1);
      }
    }
    else {

      if (host==NULL) {
	gm_Log("Could not find any suitable remote host for execution\n");
	exit(-1);
      }
      host->Exec(rsh, rshArgs, argv);
    }
  }

  return gm_True;
}


void 
RExec::Host::Initialize(char *hostName_, int priority_) 
{ 
  strcpy(name, hostName_);
  CommunicationObject::GetIPAddress(hostName_, ipAddress);
  priority = priority_;
}


void
RExec::Host::Exec(const char *rsh, const char *rshArgs, char **argv)
  // this method is invoked only by the child process, so we don't need
  // a return value; we simply exit() if there is an error
{
  const char *executable = argv[0];
  const int argvLength = 32;
  char *newArgv[argvLength], **ptr, **cmdLinePtr, cwd[1024], *token;
  int  length, argc=0;
  ts_strtok_state *state;

  newArgv[argc++] = (char*) rsh;
  newArgv[argc++] = getName();

  // create the args array
  state = ts_strtok_init((char*)rshArgs);
  while ((token=ts_strtok(" \t", state)) != NULL) {
    if (argc >= argvLength-3) {
      gm_Log("Too many arguments to the rsh program\n");
      exit(-1);
    }
    newArgv[argc++] = token;
  }

  gm_Log("Trying to execute " << getFilenameOnly(executable) << " on " 
	 << newArgv[1] << '\n');

  // compute the length of the cmd line to be executed
  for (ptr=argv, length=0; *ptr!=NULL; ptr++) {
    length += strlen(*ptr) + 1;
  }
  length ++; /* for the final '\0' */

  // add the length of the cd command
  if (getcwd(cwd, sizeof(cwd))==NULL) {
    gm_Log("Buffer overflow while determining current working directory "
	   "in child process\n");
    exit(-1);
  }
  length += 3 + strlen(cwd) + 25; /* This is a little larger than needed */

  cmdLinePtr = &newArgv[argc++];
  *cmdLinePtr = new char [length];
  if (*cmdLinePtr==NULL) {
    gm_Log("Out of memory in child process\n");
    exit(-1);
  }

  sprintf(*cmdLinePtr, "cd %s && exec ", cwd);
  for (ptr=argv; *ptr!=NULL; ptr++) {
    gm_Log("Appending " << *ptr << "\n");
    strcat(*cmdLinePtr, *ptr);
    strcat(*cmdLinePtr, " ");
  }
  strcat(*cmdLinePtr, " &");

  newArgv[argc++] = NULL;

  // no need to do ts_strtok_finish, since we are exec-ing any way!


  if (execvp(rsh, newArgv) < 0) {
    gm_Log("Cannot remotely execute process " << getFilenameOnly(executable) 
	   << " using " << rsh << '\n');
    exit(-1);
  }
}


RExec::Host *
RExec::FindLeastLoadedHost()
{
  int idx, returnIdx, minPriority;
  if (hosts==NULL) return NULL;

  /*
   * round-robin
   *
  if (currentHost==-1) {
    long random = lrand48();
    currentHost = random % numberOfHosts;
  }
  else currentHost = (currentHost+1) % numberOfHosts;

  return &hosts[currentHost];
  *
  */

  if (currentHost==-1) currentHost = 0;

  idx = currentHost;
  returnIdx = -1;
  minPriority = -1;

  do {
    if (hosts[idx].getLoadLevel() <= minLoadLevel && 
	(minPriority==-1 || hosts[idx].getPriority() < minPriority)) {

      returnIdx   = idx;
      minPriority = hosts[idx].getPriority();
    }
    idx = (idx+1) % numberOfHosts;
  } while (idx!=currentHost);

  if (returnIdx >= 0) {
    // found a distiller; return it
    currentHost = (returnIdx+1) % numberOfHosts;
    return &hosts[returnIdx];
  }

  // couldn't find any distiller at minLoadLevel; this should never happen!
  return NULL;

  /*if (currentHost==-1) currentHost = 0;
  idx = currentHost;
  do {
    if (hosts[idx].getLoadLevel() <= minLoadLevel) {
      currentHost = (idx+1) % numberOfHosts;
      return &hosts[idx];
    }
    idx = (idx+1) % numberOfHosts;
  } while (idx!=currentHost);

  // couldn't find any distiller at minLoadLevel; this should never happen!
  return NULL;*/
}


void
RExec::RecomputeMinLoadLevel()
{
  int idx;
  minLoadLevel = hosts[0].getLoadLevel();
  for (idx=1; idx < numberOfHosts; idx++) {
    if (hosts[idx].getLoadLevel() < minLoadLevel) 
      minLoadLevel = hosts[idx].getLoadLevel();
  }
}


RExec::Host *
RExec::FindByIP(char *ipAddress)
{
  int idx;
  for (idx=0; idx < numberOfHosts; idx++) {
    if (strcmp(ipAddress, hosts[idx].getIPAddress())==0) return &hosts[idx];
  }
  return NULL;
}
