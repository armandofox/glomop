#include <stdlib.h>
#include "distinterface.h"


#define MAX_FILENAME 256
#define MAX_LINE_SIZE 1024
#define MAX_TOKEN_LEN 1

#define MAX_TESTRUNS 1000
#define ARG_IS_INT 'i'
#define ARG_IS_DOUBLE 'f'
#define ARG_IS_STRING 's'


typedef struct TestRun {
      ArgumentList *argList;
      char inputFileName[MAX_FILENAME]; 
      struct TestRun *next;
   } TestRun;


ArgumentList *duplicateList(ArgumentList Original); 
int addArgument(ArgumentList *argList,Argument argToAdd); 
int readInArguments(char *line,ArgumentList *baseArgs); 
void printArguments(FILE *f,ArgumentList toPrint);
int readInputFile(char *filename,TestRun *runs);

