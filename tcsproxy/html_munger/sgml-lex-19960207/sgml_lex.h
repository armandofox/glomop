/* $Id: sgml_lex.h,v 1.1.1.1 1996/10/25 02:37:16 fox Exp $ */

#ifndef __sgml_lex_h
#define __sgml_lex_h

/*
 * Interface to Basic SGML lexer.
 * For theory of operation, see W3C tech report:
 *
 *   "A Lexical Analyzer for HTML and Basic SGML"
 *   http://www.w3.org/team/WWW/MarkUp/SGML/sgml-lex/sgml-lex.html
 */

#include <stdlib.h>

typedef struct _SGML_Lexer SGML_Lexer;

typedef enum {
  SGML_EOF,
  SGML_DATA, SGML_START, SGML_END, SGML_TAGC,
  SGML_ATTRNAME, SGML_NAME, SGML_NUMBER, SGML_NMTOKEN, SGML_LITERAL,
  SGML_GEREF, SGML_NUMCHARREF, SGML_REFC,
  SGML_PI,
  SGML_MARKUP_DECL, SGML_COMMENT,
  SGML_LIMITATION, SGML_ERROR, SGML_WARNING,
  SGML_MAXTOK
}SGML_TokType;

typedef const char* SGML_String; /* anybody know another
				    way to declare a pointer
				    to a const char*? */

/* Callback function */
typedef void (*SGML_LexFunc)(void *rock,
			     SGML_Lexer *lexer,
			     int qty, 
			     SGML_TokType types[],
			     SGML_String strings[],
			     size_t lengths[]);


/* Create an SGML_Lexer */
SGML_Lexer *SGML_newLexer(void);

/* Destroy an SGML_Lexer */
void SGML_delLexer(SGML_Lexer *l);

/* Main lexer function */
/* On the last call to SGML_lex, pass len=0.
 * @# any unclosed markup isn't reported (e.g. <xxx)
 */
void SGML_lex(SGML_Lexer *lexer,
	      const char *encoded_chars, size_t len,
	      /* @# character encoding args? */
	      SGML_LexFunc elementF, void *eltObj,
	      SGML_LexFunc auxF, void *auxObj,
	      SGML_LexFunc errF, void *errObj);


/* Query current line number (e.g. for error reporting) */
int SGML_lexLine(SGML_Lexer *l);

/* Tell lexer to normalize all identifiers to lower-case,
 * and eliminate trailing whitespace.
 * When set to 0, lexer passes all bytes through unchanged.
 */
int SGML_lexNorm(SGML_Lexer *l, int normalize);

/* Sample callback function */
void SGML_dumpEvent(void *rock,
		    SGML_Lexer *lexer,
		    int qty,
		    SGML_TokType types[],
		    SGML_String strings[],
		    size_t lengths[]);


#endif /* __sgml_lex_h */
