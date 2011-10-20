/*
 *  Data structures and symbolic definitions used by client side
 */

/*
 *  GM_PProto identifies the protocol the proxy should use when
 *  retrieving a particular document (FTP, HTTP, others...)
 */
typedef unsigned short GM_PProto;

/*
 *  Request ID's identify adaptive-pipe connections.  A Request ID is
 *  returned immediately by the async GetDocument call, and passed later
 *  to a callback proc to identify which document is arriving.
 */

typedef unsigned short GM_ReqId;

/*
 *  Binary large objects and chunks of a document
 */
typedef void * BlobPtr;

/*
 *  QOS parameters requested per-document by the client.  The Network
 *  Mgmt layer reconciles these with the per-NI models for latency,
 *  reliability, eff. bandwidth, power consumption, and cost.
 */

typedef struct _qos {
    GM_Priority qos_prio;       /* on some abstract scale */
    unsigned qos_reliability;   /* on a "scale" from TCP to UDP */
    unsigned qos_latency;       /* max tolerable wall-clock latency */
    unsigned qos_cost;          /* max tolerable (monetary) cost */
    unsigned qos_power;         /* max tolerable power consumption */
} GM_QosPrefs;

/*
 *  A DocLocator tells the proxy how to retrieve a document from the
 *  "wired network".  The doc_path field is interpreted relative to the
 *  protocol.  Some example pairs might be:
 *     doc_protocol                     doc_path
 *     HTTP                             URL of document
 *     FTP                              ftp host, path
 *     Local file                       pathname on local disk
 *  The ClientData field can hold additional info needed for retrieval
 *  (e.g., username & password for non-anonymous FTP).
 *  Some additional fields are filled in for proxy-supplied embedded
 *  document references: doc_reqid (the ID by which the object can be
 *  requested), doc_size (size in bytes), doc_type.  The idea is that
 *  the embedded reference can be passed directly to CGetDoc.
 */

typedef struct _DocLocator {
    GM_ReqId doc_reqid;         /* proxy is prefetching for us */
    GM_PProto doc_protocol;     /* protocol number */
    char doc_path[];            /* protocol-specific */
    char doc_clientdata[];      /* additional protocol-specific stuff */
    size_t doc_size;            /* size of object in bytes */
    GM_DocType doc_type;        /* document's type */
} GM_DocLocator;

/*
 *  A list of types acceptable to the client.
 */

typedef struct _TypeList {
    int numTypes;
    GM_DocType types[];          /* addressable as an array */
}


/*
 *  Each chunk of a document begins with a Document Reference Table,
 *  which gives ReqID's for other documents embedded in this chunk.
 *  (EXAMPLE: A MIME-encoded mail message is a chunk consisting of plain
 *  text whose Document Reference Table points to the MIME objects, each
 *  of which must be fetched with a separate CGetDoc
 *  connection.  EXAMPLE: A Web page is a chunk consisting of HTML whose
 *  Document Reference Table contains references to inlined graphics,
 *  and  possibly also references to other HTML pages accessed as
 *  links.)  Since each reference in the DRT is a GM_DocLocator, it can
 *  be passed directly to CGetDoc to fetch the corresponding document on
 *  a new pipe.
 */

typedef struct _DocChunk {
    int chunk_nrefs;            /* number of references in DRT */
    GM_DocLocator chunk_refs[]; /* and here they are */
    BlobPtr chunk_obj;          /* this is the actual chunk data */
} GM_DocChunk;

/*
 *  Callback procedure for when a document is arriving.  Reqid
 *  identifies the document; type gives the datatype; numChunksAvail
 *  specifies the number of (possibly noncontiguous) chunks arriving;
 *  theChunks is an array of pointers to these chunks (actually all
 *  pointers into a contiguous buffer); clientOwnsBuffer is nonzero iff
 *  it is the application's responsibility to manage and eventually free
 *  the buffer (i.e. the network manager guarantees that this buffer
 *  space will not be overwritten unless the client explicitly passes
 *  the pointer in a future Get call).
 */

typedef void \
(GM_DocCallbackProcPtr *)(GM_ReqID reqid,
                          GM_DocType type,
                          unsigned numChunksAvail,
                          unsigned totalChunks,
                          GM_DocChunk theChunks[],
                          Boolean clientOwnsBuffer);

/*
 *  chunk deltas: Used when setting the threshold at which the next
 *  callback should occur.  Semantics are similar to lseek();
 *  GM_RELATIVE and GM_ABSOLUTE are followed by another argument, the
 *  numeric value.
 */

#define GM_ALL_CHUNKS  1        /* don't call me till all chunks are in */
#define GM_AS_MANY_AS_POSSIBLE 2
#define GM_NEXT_CHUNK  2        /* call when next chunk is ready */
#define GM_RELATIVE    3        /* call after next N chunks arrive */
#define GM_ABSOLUTE    4        /* call when the N'th chunk arrives */
#define GM_ANYTIME     5        /* call whenever it's convenient */
                                /* (e.g. from load balancing standpoint) */

/*
 *  Callback procedure for when a network property of interest changes.
 */

typedef void \
(GM_EventCallbackProcPtr *)(GM_EventType event,
                            ClientData *clientdata);

/*
 *  A declaration for the error code set by some procedures
 */

extern GM_Error cErrno;
extern char cErrString[];
