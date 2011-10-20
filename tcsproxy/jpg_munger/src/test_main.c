#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "distinterface.h"
#include "jpg_munge.h"
#include <sys/time.h>
#include <unistd.h>

#include "../include/ARGS.h"

unsigned char buf[512000];

int
main(int argc, char *argv[])
{
  DistillerStatus st;
  UINT32 len = read(fileno(stdin), (char*)buf, (size_t)512000);
  Argument args[6];
  int nargs = 0, i;
  gm_Bool bool;
  FILE *f;
  char outf[100];
  struct timeval before, after;
  DistillerInput din, dout;
  C_DistillerType distType;

  sprintf(distType.string, "test image/jpeg");

  fprintf(stderr, "Read %lu bytes\n", len);
  if (DistillerInit(distType) != distOk) {
    fprintf(stderr, "Foo!\n");
    exit(1);
  }

  argc--;
  if ((argc / 2)*2 != argc) {
    fprintf(stderr, "Usage:   test [option val]* < infile\n");
    fprintf(stderr, "options: maxx (int)\n");
    fprintf(stderr, "         maxy (int)\n");
    fprintf(stderr, "         minx (int)\n");
    fprintf(stderr, "         miny (int)\n");
    fprintf(stderr, "         qual (int)\n");
    fprintf(stderr, "         scale (float)\n");
  }

  for (i=0; i<argc; i+=2) {
    if (strcmp(argv[i+1], "maxx") == 0) {
      args[nargs].type = typeInt;
      args[nargs].id = JPG_MAX_X;
      sscanf(argv[i+2], "%d", (int *) &(args[nargs].a.arg_int));
      fprintf(stderr, "maxx = %d\n", (int) args[nargs].a.arg_int);
      nargs++;
    }
    if (strcmp(argv[i+1], "maxy") == 0) {
      args[nargs].type = typeInt;
      args[nargs].id = JPG_MAX_Y;
      sscanf(argv[i+2], "%d", (int *) &(args[nargs].a.arg_int));
      fprintf(stderr, "maxy = %d\n", (int) args[nargs].a.arg_int);
      nargs++;
    }
    if (strcmp(argv[i+1], "minx") == 0) {
      args[nargs].type = typeInt;
      args[nargs].id = JPG_MIN_X;
      sscanf(argv[i+2], "%d", (int *) &(args[nargs].a.arg_int));
      fprintf(stderr, "minx = %d\n", (int) args[nargs].a.arg_int);
      nargs++;
    }
    if (strcmp(argv[i+1], "miny") == 0) {
      args[nargs].type = typeInt;
      args[nargs].id = JPG_MIN_Y;
      sscanf(argv[i+2], "%d", (int *) &(args[nargs].a.arg_int));
      fprintf(stderr, "miny = %d\n", (int) args[nargs].a.arg_int);
      nargs++;
    }
    if (strcmp(argv[i+1], "qual") == 0) {
      args[nargs].type = typeInt;
      args[nargs].id = JPG_QUAL;
      sscanf(argv[i+2], "%d", (int *) &(args[nargs].a.arg_int));
      fprintf(stderr, "qual = %d\n", (int) args[nargs].a.arg_int);
      nargs++;
    }
    if (strcmp(argv[i+1], "scale") == 0) {
      float scalet;

      args[nargs].type = typeDouble;
      args[nargs].id = JPG_SCALE;
      sscanf(argv[i+2], "%f", &scalet);
      fprintf(stderr, "scale = %f\n", scalet);
      args[nargs].a.arg_double = (double) scalet;
      nargs++;
    }
  }

  gettimeofday(&before, NULL);
  DEBUG("Calling distiller main\n");
  din.length = len;  din.data = buf;
  dout.length = 0;   dout.data = NULL;
  st = DistillerMain(args,nargs, &din, &dout, &bool);
  if (st != distOk) {
    fprintf(stderr, "Foo!\n");
    exit(1);
  }
  sprintf(outf, "out.jpg");
  f = fopen(outf, "w");
  fwrite((const void *) dout.data, (size_t) dout.length, 1, f);
  fclose(f);
  if (bool)
    free(dout.data);

  gettimeofday(&after, NULL);
  fprintf(stdout, "before: %ld.%ld  after: %ld.%ld\n",
	  (long int) before.tv_sec, (long int) before.tv_usec,
	  (long int) after.tv_sec, (long int) after.tv_usec);
  exit(0);
}

