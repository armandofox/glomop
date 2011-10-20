#include "munge.h"
#include "distinterface.h"
#include <stdio.h>
#include <string.h>

/*
 *  test the HTML munger by giving it a filename and a magic-dust string
 *  on the command line.  it will call the distiller using the standard
 *  distiller front-end interface.
 */

int
main(int ac, char *av[])
{
  char buf[50000L];
  FILE *f;
  Argument distargs;
  size_t size;
  DistillerStatus st;
  void *outbuf;
  UINT32 outsize;
  
  if (ac != 3) {
    fprintf(stderr, "Bad usage\n");
    exit(0);
  }

  assert(f = fopen(av[1],"r"));
  size = fread((void *)buf, (size_t)1, (size_t)50000, f);
  fclose(f);
  fprintf(stderr,"Read %d bytes\n", (int)size);

  SET_ARG_STRING(distargs, av[2]);
  
  st = DistillerMain(&distargs, 1, (void *)buf, (UINT32)size,
                     &outbuf, &outsize);
  fprintf(stderr, "Distiller status: %d\n", (int)st);
  printf("%s", outbuf);

  fwrite(outbuf, 1, outsize, stdout);
  exit(0);
}
