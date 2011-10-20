/*
 *  Dataflow support defs for front end
 */

#include "config_tr.h"
#include "proxyinterface.h"

/*
 *  Functions called from distillation dispatch logic
 */
void handle_child_retiring(task_t *tsk);
gm_Bool queue_async_requests(DistillerOutput *dout, task_t *tsk);

