#include "log.h"
#include <errno.h>
#include "defines.h"
#include "proxyinterface.h"
#include <stdio.h>
#include <iostream.h>
#include <string.h>
#include "threads.h"


/*int main()
{
  int idx, numberOfArgs;
  char input[256], *output;
  UINT32 outputLen;
  DistillerStatus status;
  C_DistillerType type;
  Argument *args;
  double d;

  InitializeDistillerCache(DefaultMulticastAddress, DefaultMulticastPort, 
			   "", 0);

  while (1) {
    printf("Enter distiller type (\"\" to exit): ");
    gets(type.string);
    if (*type.string=='\0') break;

    printf("Enter number of arguments: ");
    scanf("%d", &numberOfArgs); fflush(stdin);

    args = new Argument[numberOfArgs];
    for (idx=0; idx<numberOfArgs; idx++) {
      printf("Type (0=int, 1=string, 2=double): ");
      scanf("%d", &args[idx].type); fflush(stdin);
      args[idx].id = idx+1;
      printf("Argument value: ");
      switch(args[idx].type) {
      case typeInt: scanf("%lu", &_ARG_INT(args[idx])); fflush(stdin);break;
      case typeString: gets(_ARG_STRING(args[idx])); fflush(stdin); break;
      case typeDouble: 
	_ARG_DOUBLE(args[idx]) = 0.0; 
	scanf("%lf", &d);
	args[idx].a.arg_double = d; 
	DEBUG("Got " << ARG_DOUBLE(args[idx]) << "\n"); 
	fflush(stdin); 
	break;
      }
    }

    printf("The input string: ");
    gets(input); fflush(stdin);
    status = Distill(type, args, numberOfArgs, input, strlen(input)+1, 
		     (void**)&output, &outputLen);
    printf("--------------------------------------------------------\n");
    printf("Status: %d\n", status);
    printf("Output: %s (%lu)\n", output, outputLen);
    FreeOutputBuffer(output);
    delete [] args;
    printf("--------------------------------------------------------\n");
  }
}*/



#if 0
char input1[256], input2[256], input3[256];
#define INBUFSIZE 1024*768

int Child(char *input, char *mtype, char *outfile)
{
  DistillerInput in;
  DistillerOutput out;
  DistillerStatus status;
  C_DistillerType type;
  Argument arg;
  FILE *infile;

  strcpy(type.string, mtype);
  in.data = (char *) malloc(INBUFSIZE*sizeof(char));
  if (inbuf == NULL) {
    printf("Child ran out of memory. Zot\n");
    return 0;
  }
  infile = fopen(input, "r");
  if (infile == NULL) {
    printf("Child couldn't open input file %s\n", input);
    free(inbuf);
    return 0;
  }
  inbuflen = fread((void *) inbuf, 1, INBUFSIZE, infile);
  fclose(infile);
  if (inbuflen <= 0) {
    printf("read of gif %s failed.\n", input);
    free(inbuf);
    return 0;
  }

  if (strcmp(mtype, "text/html") == 0) {
    printf("Child sent file %s (%d bytes)\n", input, inbuflen);
    arg.id = 1;
    SET_ARG_STRING(arg, "steves_magicdust!");
    status = Distill(&type, &arg, 1, &in, &out, &freeOutputBuffer);
  } else {
    printf("Child sent file %s (%d bytes)\n", input, inbuflen);
    status = Distill(&type, 0, 0, inbuf, inbuflen, 
		   (void**)&output, &outputLen);
  }
  printf("Child received len %ld (status = %d)\n", (status==distOk) ? 
	 outputLen : 0, status);
  infile = fopen(outfile, "w");
  if (infile == NULL) {
    printf("couldn't open output file '%s'\n", outfile);
    free(inbuf);
    FreeOutputBuffer(output);
    return 0;
  }
  if (fwrite(output, 1, outputLen, infile) != outputLen) {
    printf("couldn't write output file output.gif\n");
    fclose(infile);
    free(inbuf);
    FreeOutputBuffer(output);
    return 0;
  }

  fclose(infile);
  FreeOutputBuffer(output);
  free(inbuf);
  return 0;
}


int old_main()
{
  if (InitializeDistillerCache(".gm_options.sample", "", 0)==gm_False) {
    gm_Log("Aborting program: " << "\n\n");
    Error::Print();
    exit(-1);
  }
  gm_Log("Waiting for PTM\n");
  WaitForPTM();

  while(1) {
    printf("Enter input filename: ");
    gets(input1);
    if (strcmp(input1, "") == 0)
      break;
    printf("Enter input mimetype: ");
    gets(input2);
    printf("Enter output filename: ");
    gets(input3);

    Child(input1, input2, input3);
  }
}
#endif




void *Endless1(void *arg)
{
  int cnt = (int)arg, m, retVal;
  DistillerInput in;
  DistillerOutput out;
  gm_Bool freeOutputBuffer;
  char str[100];
  C_DistillerType type;

  strcpy(type.string, "all/passthru");
  in.data = str;
  while(1) {
    m = rand();
    sprintf(str, "%d", m);
    fprintf(stdout, "Thread %d sent %s\n", cnt, str);
    in.length = strlen(str)+1;
    if ((retVal=Distill(&type, NULL, 0, &in, &out, &freeOutputBuffer))==distOk) {
      fprintf(stdout, "Thread %d returned %s\n", cnt, (char*)out.data);
    }
    else {
      fprintf(stdout, "Thread %d returned error (%d) !!!!!!!!!!!!!!!\n", 
	      cnt, retVal);
    }
    if (freeOutputBuffer==gm_True) {
      FreeOutputBuffer(&out);
    }
  }
}


/*void *Endless2(void *arg)
{
  int cnt = (int)arg, m, retVal;
  UINT32 outLen=0;
  char *out, str[100];
  C_DistillerType type;

  strcpy(type.string, "all/passthru");
  while(1) {
    m = rand();
    sprintf(str, "%d", m);
    fprintf(stdout, "******  Thread %d sent %s\n", cnt, str);
    if ((retVal=Distill(&type, NULL, 0, str, strlen(str)+1,(void**)&out, &outLen))==distOk) {
      fprintf(stdout, "******  Thread %d returned %s\n", cnt, out);
      FreeOutputBuffer(out);
    }
    else {
      fprintf(stdout, "******  Thread %d returned error (%d) !!!!!!!!!!!!!!!\n", 
	      cnt, retVal);
    }
  }
}*/


int main()
{
  SetStderrLogging("Proxy: ");
  if (InitializeDistillerCache("gm_options.yatin", "", 0)==gm_False) {
    gm_Log("Aborting program: " << "\n\n");
    Error::Print();
    exit(-1);
  }
  gm_Log("Waiting for PTM\n");
  WaitForPTM();
  gm_Log("Found the PTM\n");

#define MAXT 5
  Thread t[MAXT];
  for (int i=0; i<MAXT; i++) {
    if ( i % 2 ) t[i].Fork(Endless1, (void*)i);
    else t[i].Fork(Endless1, (void*)i);
  }
  while(1) {
    sleep(10);
  }

  //Endless1(10000);
}
