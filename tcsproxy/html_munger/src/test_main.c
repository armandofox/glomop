#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include "distinterface.h"

unsigned char buf[65535];
void *out;

void usage(void) {
  fprintf(stderr, "Usage: test inputfilelist argId1 arg1 ... argIdN argN  > output\n");
  fprintf(stderr, "  argId is unsigned long; argtype is taken to be an int if\n");
  fprintf(stderr, "   arg begins with decimal digit, string otherwise\n");
  fprintf(stderr, "Inputfilelist is a file containing test input filenames,");
  fprintf(stderr, " one per line\n");
}
          

int
main(int argc, char *argv[])
{
  DistillerStatus st;
  UINT32 len;
  Argument args[12];
  int nargs = 0;
  gm_Bool bool;
  int i;
  FILE *f;
  char nextfile[255];
  DistillerInput in;
  DistillerOutput out;
  C_DistillerType dType;

  sprintf(dType.string, "test text/html");
  
  if (argc >= 2  &&  strncmp(argv[1], "-h", 2) == 0) {
    usage();
    exit(1);
  }

  if ((f = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Can't open input file %s\n", argv[1]);
    exit(1);
  }


  for (i=2; i<argc; i += 2, nargs++) {
    SET_ARG_ID(args[nargs], strtoul(argv[i], (char**)NULL, 0));
    if (isdigit(*argv[i+1])) {
      SET_ARG_INT(args[nargs], strtol(argv[i+1], (char**)NULL, 0));
      fprintf(stderr, "Arg id %lu is %ld\n", ARG_ID(args[nargs]),
              ARG_INT(args[nargs]));
    } else {
      SET_ARG_STRING(args[nargs], argv[i+1]);
      fprintf(stderr, "Arg id %lu is \"%s\"\n", ARG_ID(args[nargs]),
              ARG_STRING(args[nargs]));
    }
  }

  if ((st = DistillerInit(dType)) != distOk) {
    fprintf(stderr, "DistillerInit failed: error %d\n", (int)st);
    exit(1);
  }

  strcpy(in.mimeType, "image/gif");
  while (fgets(nextfile, 254, f) != NULL) {
    char nextfile2[255];
    int fd;
    int count;

    nextfile[strlen(nextfile)-1] = 0;
    fd = open(nextfile, O_RDONLY);
    if (fd == -1) {
      fprintf(stderr, "Can't read %s, skipping\n", nextfile);
      continue;
    }
    for (len = 0;
         (count = read(fd, (void*)(buf+len), (sizeof(buf)-len))) > 0;
         len += count)
      ;
          
    fprintf(stderr, "Read %lu bytes from %s\n", len, nextfile);
    in.data = (void *)buf;
    in.length = len;
    fprintf(stderr,"Calling distiller main\n");
    st = DistillerMain(args,nargs,&in,&out,&bool);
    if (st != distOk) {
      fprintf(stderr, "DistillerMain failed: error %d\n", (int)st);
      exit(1);
    }
    close(fd);
    strcpy(nextfile2, "/tmp/");
    strcat(nextfile2, nextfile);
    strcat(nextfile2, ".OUT");
    fd = open(nextfile2, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) {
      fprintf(stderr, "Can't write %s, using stdout\n", nextfile2);
      fd = fileno(stdout);
    }
    len = write(fd, (const void *)out.data, (size_t)out.length);
    if (fd != fileno(stdout))
      close(fd);
    fprintf(stderr, "Wrote %lu of %lu bytes to %s\n", len, out.length, nextfile);
    fprintf(stderr, "%s", out.data);
    if (bool)
      DistillerFree(out.data);
  }
}

void *DistillerMalloc(size_t size) { return (void*)malloc(size); }
void DistillerFree(void *ptr) { free(ptr); }
void *DistillerRealloc(void * ptr, size_t size) { return (void*)
						    realloc(ptr, size);}

MonitorClientID DistillerGetMonitorClientID(void) { return(NULL); }
gm_Bool MonitorClientSend(MonitorClientID client, const char *fieldID, 
                          const char *value, const char *script)
{ return gm_True; }
Argument *
getArgumentFromId(Argument *args, int numberOfArgs, UINT32 id)
{
  Argument *ptr;
  if (args==NULL) return NULL;
  for(ptr=args; ptr < args+numberOfArgs; ptr++) {
    if (ARG_ID(*ptr)==id) return ptr;
  }
  return NULL;
}

  
