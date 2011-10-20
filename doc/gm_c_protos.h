/*
 *  Establish connection to proxy
 */

GM_ConnectId  CConnectProxy(char *proxyAddr,
                            char *returnPath,
                            char *idString,
                            GM_EventCallbackProcPtr connReadyProc);
GM_Error    CDisconnectProxy(GM_ConnectId connId);
GM_Error    CAuth(...);

/*
 *  ISSUE: Is there value in talking to more than one proxy at a time?
 */

/*
 *  Register the list of types known.  Called once after connection, may
 *  be called again later to update the list.
 */

void        CRegisterTypes(TypeList *t);

/*
 *  Initiate a "get document" request.
 *
 *  Args:
 *  
 *  DocLocator structure: tells proxy how to locate the document in a
 *  protocol-dependent way.  E.g. for HTTP, DocLocator may contain a
 *  URL; for GMail, it may contain the name of the mailspool directory
 *  and host; etc.
 *
 *  QosPrefs structure: specifies desired QOS params for this document,
 *  details TBD.
 *
 *  AcceptTypes list: a list specifying which types are acceptable as
 *  return.  If null, any type previously registered with proxy is OK.
 *
 *  annotation: a descriptive comment about the request, stored locally
 *  so client can review what requests are outstanding.  not used by the
 *  proxy. 
 *
 *  callbackThresh: the number of document "chunks" that must arrive
 *  before the callback procedure that receives the document is able to
 *  do anything useful.  "Chunks" are defined relative to each datatype.
 *
 *  callbackProc: the procedure that should be called as soon as
 *  CallbackThresh "chunks" of the document have arrived.  The
 *  CallbackProc will continue to be called as new chunks arrive.  The
 *  callback procedure and chunk threshold between calls can be changed
 *  after the first  callback.
 *
 *  maxChunks: the maximum number of chunks to prefetch initially.  By
 *  convention the first chunk is the document table of contents.  Some
 *  symbolic constants are available to specify things like "all
 *  chunks", "as many as will fit in some existing buffer", etc.
 *
 *  buffer: if non-NULL, specifies where to store incoming chunks; if
 *  NULL, GM will allocate the buffer space.  User-supplied buffer space
 *  should be large enough to hold maxChunks.
 *
 *  Returns request ID that will be passed to the callback proc later to
 *  identify the document.  If the request ID is null, an error
 *  occurred.  (TBD: an "errno"-like mechanism in which information
 *  about the error is stored.)
 *
 */

GM_ReqID  CGetDoc(GM_DocLocator *loc,
                  GM_TypeList *types,
                  GM_QosPrefs *qos,
                  char *annotation,
                  GM_DocCallbackProcPtr callback,
                  unsigned callbackThreshold,
                  unsigned maxChunks,
                  BlobPtr buffer);

/*
 *  Get a chunk from an open document connection.  This may require
 *  fetching the chunk from the proxy if the chunk isn't already there.
 *  (ISSUE: If chunk isn't available, the call should return
 *  asynchronously & perform a callback when the chunk arrives.  Is
 *  it redundant to implement that behavior both here and in CGetDoc?
 *  Maybe CGetDoc should only return the TOC (though it may prefetch up
 *  to maxChunks chunks)?
 */
GM_Chunk  CGetChunk(GM_ReqId reqid,
                    int chunkIndex);
                   
/*
 *  Close an open document connection.  All client-allocated buffers for
 *  this document are freed, and the document is "forgotten about"; any
 *  prefetches in progress for this document are cancelled; etc.
 */
GM_Error  CDisposeDoc(GM_ReqId reqid);
                   
/*
 *  Set/change the callback proc for an existing request; any future
 *  notifications on the request will go to the new proc.  Returns the
 *  pointer to the old callback procedure (like signal(1)).
 *  newThreshold and threshRelative have similar semantics as the
 *  parameters to lseek().
 */

GM_DocCallbackProcPtr CSetCallback(GM_ReqID reqid,
                                   GM_DocCallbackProcPtr newProc,
                                   unsigned newMaxChunks,
                                   unsigned newThreshold,
                                   BlobPtr buffer);



