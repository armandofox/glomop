/* $Id: filter.c,v 1.1.1.1 1996/10/25 02:37:15 fox Exp $ */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "sgml_lex.h"

#define BIG 16000

int main(int argc, char **argv)
{
  char buf[BIG];
  const char *p = buf;
  int qty;
  SGML_Lexer *lexer = SGML_newLexer();

  assert(lexer); /*@@ malloc() -> 0? */

  /* Use: sgml_lex -n <xxx to change identifiers to lower-case
   */
  if(argc > 1 && strcmp(argv[1], "-n") == 0){
    SGML_lexNorm(lexer, 1);
  }

  while(1){
    qty = fread(buf, 1, sizeof(buf)-1, stdin);

    if (qty <= 0) break;

    while(qty > 0){
      /* pick wierd-sized chunks to stress-test lexer.*/
      int chunk = *p % 23;
      
      if(chunk == 0) chunk = 1;
      if(chunk > qty) chunk = qty;
      
      SGML_lex(lexer,
	       p, chunk,
	       SGML_dumpEvent, "Tag/Data",
	       SGML_dumpEvent, "Aux Markup",
	       SGML_dumpEvent, "Err/Lim");
      
      qty -= chunk;
      p += chunk;
    }

    p = buf;
  };

  /* final step... */
  SGML_lex(lexer,
	   NULL, 0,
	   SGML_dumpEvent, "Tag/Data",
	   SGML_dumpEvent, "Aux Markup",
	   SGML_dumpEvent, "Err/Lim");

  SGML_delLexer(lexer);

  return 0;
}
