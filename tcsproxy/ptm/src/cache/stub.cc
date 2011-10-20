#include "config_tr.h"
#include "defines.h"
#include "distinterface.h"
#include "proxyinterface.h"
#include <stdio.h>
#include <string.h>


void Usage(int arg)
{
  fprintf(stderr, 
	  "Usage: stub -d distiller-type -i inputfile -o outputfile \n"
	  "      [-l monitor-ip-address/port]\n(Errorcode=%d)\n", arg);
  exit(-1);
}


int main(int argc, char **argv)
{
  char filename[256], outputFilename[256], monitorAddr[25];
  C_DistillerType type;
  int optCh;
  Port monitorPort=0;
  char     *options = "d:i:o:l:";
  
  strcpy(filename, "");
  strcpy(outputFilename, "");
  strcpy(monitorAddr, "");
  optind = 1;
  while ( (optCh = getopt(argc, argv, options))!=-1) {
    switch (optCh) {
    case 'd':
      strcpy(type.string, optarg);
      break;

    case 'i':
      strcpy(filename, optarg);
      break;

    case 'o':
      strcpy(outputFilename, optarg);
      break;

    case 'l':
      if (sscanf(optarg, "%[^/]/%d", monitorAddr, &monitorPort)!=2) Usage(1);
      break;

    case ':':
    case '?':
    default:
      Usage(2);
      break;
    }
  }

  if (*filename=='\0' || *outputFilename=='\0' || *type.string=='\0') Usage(3);


  InitializeDistillerCache(".gm_options.sample", "", 0);
  fprintf(stderr, "Waiting for PTM...\n");
  WaitForPTM();
  fprintf(stderr, "Found PTM.\n");

  Argument *args = new Argument[argc - optind];
  if (args==NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(-1);
  }
  for (int idx=optind; idx < argc; idx++) {
    SET_ARG_ID(args[idx-optind], idx);
    SET_ARG_STRING(args[idx-optind], argv[idx]);
  }


  FILE *fp = fopen(filename, "r");
  if (fp==NULL) {
    fprintf(stderr, "Could not open file: %s\n", filename);
    exit(-1);
  }

  fseek(fp, 0, SEEK_END);
  UINT32 length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *input = new char[length];
  if (input==NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(-1);
  }
  if (fread(input, sizeof(char), length, fp) != length) {
    fprintf(stderr, "File read error\n");
    exit(-1);
  }
  fclose(fp);

  char *output;
  UINT32 outputLength;
  DistillerStatus status;
  status = Distill(&type, args, argc-optind, input, length, 
		   (void**)&output, &outputLength);
  fprintf(stderr, "Distiller status: %d\n", status);
  if (status!=distOk) {
    exit(-1);
  }

  if (output==NULL || outputLength==0) {
    fprintf(stderr, "No output generated\n");
    exit(-1);
  }

  fp = fopen(outputFilename, "w");
  if (fp==NULL) {
    fprintf(stderr, "Error opening output file\n");
    exit(-1);
  }

  if (fwrite(output, sizeof(char), outputLength, fp)
      !=outputLength) {
    fprintf(stderr, "Error writing output file\n");
    exit(-1);
  }

  fclose(fp);
  delete [] args;
  delete [] input;
  FreeOutputBuffer(output);
  fprintf(stderr, "Succeeded...\n");
  return 0;
}



#if 0
#include "communication.h"

int main()
{
  MulticastSocket sock;
  char buffer[500];
  int len;

  if (sock.CreateSender("224.6.6.6", 6666, 1)==gm_False) {
    printf("Couldn't create multicast socket\n");
    return 1;
  }

  while(1) {
    printf("--> ");
    gets(buffer);
    if (strcmp(buffer, "quit")==0 || strcmp(buffer, "exit")==0) break;

    len = strlen(buffer);
    while(len > 0 && (buffer[len-1]=='\n' || buffer[len-1]=='\r')) 
      buffer[--len] = '\0';

    int read=0, write = 0;
    while(read <= len) {
      if (buffer[read]=='\\' && buffer[read+1]=='n') { 
	buffer[write] = '\n';
	read += 2;
      }
      else {
	buffer[write] = buffer[read];
	read++;
      }
      write++;
    }

    if (sock.Write(buffer, strlen(buffer)+1)==gm_False) {
      printf("Write error\n");
    }
  }

  return 0;
}

#endif
