/* This file describes some structures used in the wingman GLALFTP
   protocol. */

#ifndef WING_REQ_H
#define WING_REQ_H

typedef struct {
  UINT32 version;      /* Protocol version */
  UINT32 request_id;   /* The request id */
  UINT32 metadatasize; /* How much metadata has been sent */
  UINT32 datasize;     /* How much data has been sent */
  UINT32 comp;         /* ??? */
} gl_hdr;

#define sizeof_gl_hdr 20


typedef struct {
  unsigned char out_nocache;
  unsigned char out_sendagglist;

  unsigned char in_nocache;
  /*unsigned char in_isdatabase;*/
} wing_metadata_flags;


#define BACKWARD_COMPATIBLE

typedef struct {
  task_t          *task;
  gl_hdr          wing_hdr;
  DistillerBuffer wing_metadata;
  DistillerBuffer wing_data;
  Request         http_req;
  ArgumentList    prefs;

  wing_metadata_flags flags;

#ifdef BACKWARD_COMPATIBLE
  char *saveoldurl;
  char *agglistdata;
  int  agglistlen;
#endif

} wing_request;


typedef enum {
  MetaCacheExpire=1, 
  MetaNoCache, 
  MetaPageType, 
  MetaAggList
} MetaType;


#define PageTypeRegular       0
#define PageTypeDatabase      1
#define PageTypeAgglist       2


gm_Bool       wingreq_init(int port, int boost_prio);

#endif
