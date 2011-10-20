/*
 *  Dataflow support for front end
 */

#include <stdio.h>
#include <string.h>
#include "config_tr.h"
#include "thr_cntl.h"
#include "dataflow.h"
#include "clib.h"

/*
 *  Private functions and vars
 */
static char *url_from_parent_id(UINT32 id, char *buf);

#define CONTINUATION_URL_LEN  (40+strlen(fe_server_name))

extern ThrFunc http_go_proc;

/*
 *  handle_child_retiring: one of an outstanding set of async fetches has
 *      completed.   If this is the last child of this set, queue a new task
 *      that will redispatch the parent request to the distiller, with an
 *      additional "X-State:" header containing the distiller state that it
 *      originally passed back.
 */
void
handle_child_retiring(task_t *tsk)
{
  task_t *tn;
  char *tmp;
  
  new_task(&tn);

  *tn = *tsk;
  
  SET_TASK_ID(tn,1);
  SET_TASK_CHILD_INDEX(tn,0);

  assert(tmp = (char *)MALLOC(CONTINUATION_URL_LEN + 100));
  sprintf(tmp, "GET ");
  (void)url_from_parent_id(TASK_PARENT(tsk), tmp+4);
  strcat(tmp, " HTTP/1.0\r\n\r\n");
  SET_TASK_METADATA(tn, tmp);

  fprintf(stderr, "Queueing PARENT task meta = %s\n", tmp);
  dispatch(tn);
}

/*
 *  queue_async_requests: Distiller has returned a status that indicates it's
 *      asking for a bunch of thigns to be fetched asynchronously (dataflow
 *      style) and be woken up again later.  Place returned metadata (headers)
 *      and data in cache as a pseudo-continuation; also parse data to
 *      determine which thing(s) need to be fetched, and queue a new task for
 *      each of them.   Return True if everything succeeded, False if anything
 *      fails. 
 */

gm_Bool
queue_async_requests(DistillerOutput *dout, task_t *t)
{
  task_t *tn;
  char *tmp;
  size_t len = DataLength(dout);
  clib_data contin;
  UINT32 par = TASK_ID(t);
  char *contin_url = ALLOCA(CONTINUATION_URL_LEN);

  /* create pseudo-continuation */
  contin.mime_headers = MetadataPtr(dout);
  contin.mime_size = MetadataLength(dout);
  contin.data = DataPtr(dout);
  contin.data_size = DataLength(dout);
  (void)url_from_parent_id(par, contin_url);

  if (Clib_Push(contin_url, contin) != CLIB_OK) {
    return gm_False;

  } else {

    /* create and queue new task. */
    new_task(&tn);
    *tn = *t;
    SET_TASK_CHILD_INDEX(tn, 1);
    SET_TASK_PARENT(tn, par);
    tmp = (char *)MALLOC(1+len);
    memcpy(tmp, DataPtr(dout), len);
    tmp[len] = '\0';
    SET_TASK_METADATA(tn, tmp);
    fprintf(stderr, "Queueing task meta='%s'\n", tmp);
    dispatch(tn);
    return gm_True;
  }
}

static char *
url_from_parent_id(UINT32 id, char *buf)
{
  sprintf(buf, "http://%s/id%lu", fe_server_name, id);
  return buf;
}
  
