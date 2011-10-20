#ifndef __DISTDB_H__
#define __DISTDB_H__


#include "database.h"
#include "communication.h"
#include "proxyinterface.h"
#include "distinterface.h"
#include <iostream.h>


class IStream;
class OStream;


struct Load {
  UINT32 totalLatency_ms, ignoreThreshold_ms;

  Load() : totalLatency_ms(0), ignoreThreshold_ms(0) { };
  /*Load& operator = (Load& newLoad) 
  { totalLatency_ms = newLoad.totalLatency_ms; return *this; };*/
  Load& operator += (Load &add) { 
    totalLatency_ms += add.totalLatency_ms; return *this; 
  }
  Load& operator -= (Load &sub) { 
    totalLatency_ms -= sub.totalLatency_ms; return *this; 
  }
  Load& Average(Load &sum, int total) { 
    totalLatency_ms = ((total==0) ? 0 : sum.totalLatency_ms/total); 
    return *this;
  }

  friend OStream& operator << (OStream& out, Load& load);
  friend IStream& operator >> (IStream& out, Load& load);
};


struct RemoteID : public IndexKey {
  RemoteID(const char *ip, Port p) : port(p) { strcpy(ipAddress, ip); };
  RemoteID(RemoteID &id) { strcpy(ipAddress, id.ipAddress); port = id.port; };
  RemoteID() { strcpy(ipAddress, ""); port = 0; };

  gm_Bool Equal(IndexKey *key) {
    RemoteID *ridKey = (RemoteID*) key;
    return (strcmp(ridKey->ipAddress, ipAddress)==0 && ridKey->port==port) ? 
      gm_True : gm_False; 
  };
  UINT32 Hash() { return port + HashString(ipAddress); }

  friend class ostream& operator << (ostream& out, RemoteID& rid) 
  { return out << rid.ipAddress << '/' << rid.port; }

  friend OStream& operator << (OStream& out, RemoteID& rid);
  friend IStream& operator >> (IStream& out, RemoteID& rid);

  char ipAddress[MAXIP];
  Port port;
};


struct DistillerType : public IndexKey {
  DistillerType() { strcpy(c_struct.string, ""); };
  DistillerType(const char *type) { strcpy(c_struct.string, type); };
  DistillerType(C_DistillerType &type)
  { strcpy(c_struct.string, type.string); };
  DistillerType(DistillerType &type)   
  { strcpy(c_struct.string, type.c_struct.string); };

  gm_Bool Equal(IndexKey *key) { 
    DistillerType *typeKey = (DistillerType*) key;
    return ((strcmp(c_struct.string, typeKey->c_struct.string)==0) ? 
	    gm_True:gm_False);
  };
  UINT32 Hash() { return HashString(c_struct.string); }
  
  gm_Bool CanSatisfy(DistillerType *request);

  friend ostream& operator<< (ostream& out, DistillerType& type) 
  { return out << type.c_struct.string; };

  friend OStream& operator << (OStream& out, DistillerType& type);
  friend IStream& operator >> (IStream& out, DistillerType& type);

  operator char * () { return c_struct.string; };

  C_DistillerType c_struct;
};


struct BasicDistiller : public DatabaseRecord {
  BasicDistiller(RemoteID& _rid, DistillerType& _type)
    : rid(_rid), type(_type) { };
  BasicDistiller() : rid("", 0), type("") { };
  virtual ~BasicDistiller() { };

  friend OStream& operator << (OStream& out, BasicDistiller& dist);
  friend IStream& operator >> (IStream& out, BasicDistiller& dist);

  RemoteID      rid;
  DistillerType type;
  Load          load;
};


class DistillerIndex : public Index {
public:
  DistillerIndex(gm_Bool dupsAllowed=gm_False,
		 int numberOfBuckets=DefaultNumberOfBuckets)
    : Index(dupsAllowed, numberOfBuckets) { };
protected:
  IndexKey *getIndexKey(DatabaseRecord *record) 
  { return &((BasicDistiller*)record)->rid; }
};


class BasicDistillerDatabase : public Database {
public:
  BasicDistillerDatabase();
  virtual ~BasicDistillerDatabase();

  DistillerIndex     *getMainIndex() { return mainIndex; }

protected:
  DistillerIndex     *mainIndex;
};



#endif // __DISTDB_H__
