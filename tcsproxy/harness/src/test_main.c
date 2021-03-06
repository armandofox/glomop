#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "distinterface.h"
#include "link.h"

#define INPUT_MIME_TYPE  "image/ppm"

unsigned char buf[65535];
void *out;

void usage(void) {
  fprintf(stderr, "Usage: test <inputFile> <count>\n");
}
          

int
main(int argc, char *argv[])
{
  DistillerStatus st;
  UINT32 len;
  int i;
  char nextfile[MAX_FILENAME];
  char logFile[MAX_FILENAME];
  char *timeStr;
  TestRun runs[MAX_TESTRUNS];
  FILE *f;
  time_t t;
  
  int numRuns;
  int repeatCount;
  DistillerInput in;
  DistillerOutput out;
  C_DistillerType distType;


  sprintf(distType.string, "test " INPUT_MIME_TYPE);
  if (argc < 3) {
    usage();
    exit(1);
  }
  
  if (!(numRuns = readInputFile(argv[1],runs))) {
    fprintf(stderr, "Error reading input file %s.\n", argv[1]);
    exit(1);
  }
  repeatCount = atoi(argv[2]);
  if (repeatCount <=0) {
    fprintf(stderr, "Invaild count value.\n");
    exit(1);
  }

  if ((st = DistillerInit(distType, 0, NULL)) != distOk) {
    fprintf(stderr, "DistillerInit failed: error %d\n", (int)st);
    exit(1);
  }

  sprintf(logFile,"harness.log");

  t = time(NULL);
  timeStr = ctime(&t);

  printf("%s\n",logFile);
  if ((f = fopen(logFile, "a")) == NULL) {
    fprintf(stderr, "Can't open log file %s\n", logFile);
    return 0;
  }

  SetMimeType(&in, INPUT_MIME_TYPE);
  
  for (i=0;i<numRuns;i++) {
    int fd;
    int count;
    int ii;

    fprintf(stdout,"Test run for case %s:\n",runs[i].inputFileName);
    fprintf(f,"Test run for case %s:\n",runs[i].inputFileName);
    printArguments(f,*(runs[i].argList));

    fd = open(runs[i].inputFileName, O_RDONLY);


    if (fd == -1) {
      fprintf(stderr, "Can't read %s, skipping\n", runs[i].inputFileName);
      fprintf(f, "Can't read %s, skipping\n", runs[i].inputFileName);
      continue;
    }
    for (len = 0;
         (count = read(fd, (void*)(buf+len), (sizeof(buf)-len))) > 0;
         len += count)
      ;

          
    fprintf(stderr, "Read %lu bytes from %s\n", len, runs[i].inputFileName);
    fprintf(f, "Read %lu bytes from %s\n", len, runs[i].inputFileName);
    SetData(&in, (void *)buf);
    SetDataLength(&in, len);
    SetMetadata(&in, NULL);
    SetMetadataLength(&in, 0);


    for (ii= 0; ii<repeatCount; ii++) {

      fprintf(stderr,"Calling distiller main\n");
      fprintf(f,"Calling distiller main\n");
     /* Distiller status */
      st = DistillerMain(runs[i].argList->arg,runs[i].argList->nargs,&in,&out); 
      if (st != distOk) {
        fprintf(stderr, "DistillerMain failed: error %d\n", (int)st);
        fprintf(f, "DistillerMain failed: error %d\n", (int)st);
      }
      close(fd);
      strcpy(nextfile,runs[i].inputFileName);
      strcat(nextfile, ".OUT");
      fd = open(nextfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
      if (fd == -1) {
        fprintf(stderr, "Can't write %s, using stdout\n", nextfile);
        fprintf(f, "Can't write %s, using stdout\n", nextfile);
        fd = fileno(stdout);
      }
      len = write(fd, (const void *)DataPtr(&out), (size_t)(DataLength(&out)));
      if (fd != fileno(stdout))
        close(fd);
      fprintf(stderr, "Wrote %lu of %lu bytes to %s\n", len, DataLength(&out), nextfile);
      fprintf(f, "Wrote %lu of %lu bytes to %s\n", len, DataLength(&out), nextfile);
      if (out.data.freeMe == gm_True) 
        DistillerFree(DataPtr(&out));
      if (out.metadata.freeMe == gm_True)
        DistillerFree(MetadataPtr(&out));
    }
  }
  fclose(f);
  return(1);
}

MonitorClientID DistillerGetMonitorClientID(void) { return(NULL); }
gm_Bool MonitorClientSend(MonitorClientID client, const char *fieldID, 
                          const char *value, const char *script)
{ return gm_True; }

