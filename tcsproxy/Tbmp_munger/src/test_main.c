#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include "distinterface.h"

#define INPUT_MIME_TYPE  "image/gif"
#define REPEAT_COUNT 1

unsigned char buf[65535];
void *out;

void usage(void) {
  fprintf(stderr, "Usage: test inputfilelist argId1 arg1 ... argIdN argN outDir > output\n");
  fprintf(stderr, "  argId is unsigned long; argtype is taken to be an int if\n");
  fprintf(stderr, "   arg begins with decimal digit, string otherwise\n");
  fprintf(stderr, "Inputfilelist is a file containing test input filenames,");
  fprintf(stderr, " one per line\n");
  fprintf(stderr, "outDir is directory for output files\n");
}
          

int
main(int argc, char *argv[])
{
  DistillerStatus st;
  UINT32 len;
  Argument args[12];
  int nargs = 0;
  int i;
  char *k;
  FILE *f;
  char nextfile[255];
  DistillerInput in;
  DistillerOutput out;
  C_DistillerType distType;

  sprintf(distType.string, "test " INPUT_MIME_TYPE);
  
  if ((argc < 2) || (argc >= 2  &&  strncmp(argv[1], "-h", 2) == 0)) {
    usage();
    exit(1);
  }

  if ((f = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Can't open input file %s\n", argv[1]);
    exit(1);
  }


  for (i=2; i<argc-1; i += 2, nargs++) {
    SET_ARG_ID(args[nargs], strtoul(&argv[i][1], (char**)NULL, 0));
    switch (argv[i][0]) {
    case 'i':
      SET_ARG_INT(args[nargs], strtol(argv[i+1], (char**)NULL, 0));
      fprintf(stderr, "Arg id %lu is %ld\n", ARG_ID(args[nargs]),
              ARG_INT(args[nargs]));
      break;
    case 'f':
      SET_ARG_DOUBLE(args[nargs], strtod(argv[i+1], (char**)NULL));
      fprintf(stderr, "Arg id %lu is %f\n", ARG_ID(args[nargs]),
              (float)ARG_DOUBLE(args[nargs]));
      break;
    case 's':
    default:
      SET_ARG_STRING(args[nargs], argv[i+1]);
      fprintf(stderr, "Arg id %lu is \"%s\"\n", ARG_ID(args[nargs]),
              ARG_STRING(args[nargs]));
    }
  }

  if ((st = DistillerInit(distType, 0, NULL)) != distOk) {
    fprintf(stderr, "DistillerInit failed: error %d\n", (int)st);
    exit(1);
  }

  SetMimeType(&in, INPUT_MIME_TYPE);
  while (fgets(nextfile, 254, f) != NULL) {
    char nextfile2[255];
    int fd;
    int count;
    int ii;
      
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
    SetData(&in, (void *)buf);
    SetDataLength(&in, len);
    SetMetadata(&in, NULL);
    SetMetadataLength(&in, 0);

    for (ii= 0; ii<REPEAT_COUNT; ii++) {

      fprintf(stderr,"Calling distiller main\n");
      st = DistillerMain(args,nargs,&in,&out);
      if (st != distOk) {
        fprintf(stderr, "DistillerMain failed: error %d\n", (int)st);
      }
      close(fd);
      strcpy(nextfile2, argv[argc-1]);
      if (nextfile2[strlen(nextfile2)-1] != '/')
        strcat(nextfile2,"/");
      k = strrchr(nextfile, '/');
      if (k)
        strcat(nextfile2, k+1);
      else
        strcat(nextfile2, nextfile);
      strcat(nextfile2, ".OUT");
      fd = open(nextfile2, O_CREAT | O_WRONLY | O_TRUNC, 0666);
      if (fd == -1) {
        fprintf(stderr, "Can't write %s, using stdout\n", nextfile2);
        fd = fileno(stdout);
      }
      len = write(fd, (const void *)DataPtr(&out), (size_t)(DataLength(&out)));
      if (fd != fileno(stdout))
        close(fd);
      fprintf(stderr, "Wrote %lu of %lu bytes to %s\n", len, DataLength(&out), nextfile2);
      if (out.data.freeMe == gm_True) 
        DistillerFree(DataPtr(&out));
      if (out.metadata.freeMe == gm_True)
        DistillerFree(MetadataPtr(&out));
    }
  }
  return(1);
}

MonitorClientID DistillerGetMonitorClientID(void) { return(NULL); }
gm_Bool MonitorClientSend(MonitorClientID client, const char *fieldID, 
                          const char *value, const char *script)
{
  fprintf(stderr, "[%-20.20s] %s\n", fieldID, value);
  return gm_True;
}

