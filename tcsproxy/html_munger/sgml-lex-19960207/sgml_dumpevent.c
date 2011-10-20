/* $Id: sgml_dumpevent.c,v 1.1.1.1 1996/10/25 02:37:16 fox Exp $ */

/* exports... */
#include "sgml_lex.h"

/* imports... */
#include <stdio.h>
#include <assert.h>

/*
 * This is an example of an SGML_LexFunc.
 * rock should be a null-terminated string
 */

void
SGML_dumpEvent(void *rock,
	       SGML_Lexer *l,
	       int qty,
	       SGML_TokType *types, SGML_String *strings, size_t *lengths)
{
  int i;
  const char *indent = "";
  const char *token_names[] = {
    NULL,
    "Data", "Start Tag", "End Tag", "Tag Close",
    "Attr Name", "Name", "Number", "Name Token", "Literal",
    "Entity", "CharRef", "RefC",
    "Processing Instruction",
    "Markup Decl", "Comment",
    "!!Limitation!!", "!!Error!!", "!!Warning!!"
  };

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

  assert(ARRAY_SIZE(token_names) == SGML_MAXTOK);

  for(i = 0; i < qty; i++){

    if(i == 0){
      printf("\nline %d: [%s]\n", SGML_lexLine(l), (const char*)rock);
    }

    printf("%s%s: `", indent, token_names[types[i]]);

    fwrite(strings[i], 1, lengths[i], stdout);

    printf("'\n");
    indent = "  ";
  }
}


