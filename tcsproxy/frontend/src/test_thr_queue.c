#include <config_tr.h>
#include <stdio.h>
#include <unistd.h>

#define NTASKS 4*60

int
main(ac,av)
     int ac;
     char *av[];
{
  int res;
  int j;
  task_t done[NTASKS];

  if ((res = make_threads(NTASKS>>2)))
    exit(1);

  for (j=0; j<NTASKS; j++) {
    done[j].task_id = j;
    done[j].task_result_ptr = (void *)j;
    fprintf(stderr, "Dispatching task %d...\n",j);
    dispatch(&done[j]);
  }
  while (1) {
    sleep(1);
    for (j=0; j < NTASKS; j++) {
      if ((int)(done[j].task_result_ptr) > 0) {
        fprintf(stderr,"task %d = %d\n", j, (int)(done[j].task_result_ptr));
        done[j].task_result_ptr = (void *)(-1);
      }
    }
  }
}
