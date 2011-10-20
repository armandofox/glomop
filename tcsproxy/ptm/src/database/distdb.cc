#include "distdb.h"
#include "packet.h"
#include "utils.h"


#define SEPARATOR "/"
#define WILDCARD  "*"


BasicDistillerDatabase::BasicDistillerDatabase()
{
  mainIndex = new DistillerIndex;
  if (mainIndex==NULL) VoidReturn(errOutOfMemory);
  AddIndex(mainIndex); // ignore return value
}


BasicDistillerDatabase::~BasicDistillerDatabase()
{
  if (mainIndex!=NULL) {
    RemoveIndex(mainIndex);
    delete mainIndex;
    mainIndex = NULL;
  }
}


#include "log.h"
gm_Bool 
DistillerType::CanSatisfy(DistillerType *request) 
{
  ts_strtok_state *s1, *s2;
  char *t1, *t2;
  gm_Bool retval = gm_False;

  s1 = s2 = NULL;

/*  gm_Log("Checking if " << c_struct.string << " can satisfy " 
	 << request->c_struct.string << "\n"); */
  if ((s1 = ts_strtok_init(c_struct.string))==NULL) {
    gm_Log("ts_strtok_init failed for 1\n");
    goto CANSATISFY_CLEAN;
  }

  if ((s2 = ts_strtok_init(request->c_struct.string))==NULL) {
    gm_Log("ts_strtok_init failed for 2\n");
    goto CANSATISFY_CLEAN;
  }

  for (t1 = ts_strtok(SEPARATOR, s1), t2 = ts_strtok(SEPARATOR, s2);
       t1 != NULL && t2 != NULL;
       ) {
    /*gm_Log("CanSatisfy comparing '" << t1 << "' and '" << t2 << "'\n");*/
    if ( ! (strcmp(t1, WILDCARD)==0 || strcmp(t1, t2)==0) ) {
      /*gm_Log("strcmp failed for " << t1 << " and " << t2 << "\n");*/
      goto CANSATISFY_CLEAN;
    }
    t1 = ts_strtok(SEPARATOR, s1);
    t2 = ts_strtok(SEPARATOR, s2);
  }

/*  gm_Log("CanSatisfy succeeded\n"); */

  // return true only if we've hit the end of both strings
  if (t1==NULL && t2==NULL) retval = gm_True;
CANSATISFY_CLEAN:
  if (s2 != NULL) 
    ts_strtok_finish(s2);
  if (s1 != NULL)
    ts_strtok_finish(s1);
  return retval;
}



OStream&
operator << (OStream& stream, RemoteID& rid)
{
  gm_Bool origMode = stream.IsBinary();
  return stream << Binary(gm_False) << rid.ipAddress << '/' << rid.port 
		<< Binary(origMode);
}


IStream&
operator >> (IStream& stream, RemoteID& rid)
{
  gm_Bool origMode    = stream.IsBinary();
  char    origDelimit = stream.getDelimit();
  UINT32  origGetW    = stream.getW();

  return stream >> Binary(gm_False) >> Delimit('/') >> SetW(MAXIP) 
		>> rid.ipAddress >> Skip >> Delimit(origDelimit) 
		>> rid.port      >> SetW(origGetW) >> Binary(origMode);
}


OStream&
operator << (OStream& stream, Load& load)
{
  gm_Bool origMode = stream.IsBinary();
  return stream << Binary(gm_False) << load.totalLatency_ms << '|'
		<< load.ignoreThreshold_ms << Binary(origMode);
}


IStream&
operator >> (IStream& stream, Load& load)
{
  gm_Bool origMode = stream.IsBinary();
  char    origDelimit = stream.getDelimit();
  return stream >> Binary(gm_False) >> Delimit('|') >> load.totalLatency_ms
		>> Skip >> Delimit(origDelimit) >> load.ignoreThreshold_ms
		>> Binary(origMode);
}


OStream&
operator << (OStream& stream, DistillerType& type)
{
  return stream << type.c_struct.string;
}


IStream&
operator >> (IStream& stream, DistillerType& type)
{
  UINT32 origGetW = stream.getW();
  return stream >> SetW(MAXDISTILLER_TYPE_STRING) >> type.c_struct.string
		>> SetW(origGetW);
}


OStream&
operator << (OStream& stream, BasicDistiller& record)
{
  return (stream << record.rid << '|' << record.type << '|' << record.load);
}


IStream&
operator >> (IStream& stream, BasicDistiller& record)
{
  char origDelimit = stream.getDelimit();

  return stream >> Delimit('|') >> record.rid >> Skip >> record.type >> Skip
		>> Delimit(origDelimit) >> record.load;
}




// puuting this function here simply because it is required by both libptmstub
// and libdist


extern "C" Argument *
getArgumentFromId(Argument *args, int numberOfArgs, UINT32 id)
{
  Argument *ptr;
  if (args==NULL) return NULL;
  for(ptr=args; ptr < args+numberOfArgs; ptr++) {
    if (ARG_ID(*ptr)==id) return ptr;
  }
  return NULL;
}


