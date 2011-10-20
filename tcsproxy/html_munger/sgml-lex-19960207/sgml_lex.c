/* $Id: sgml_lex.c,v 1.1.1.1 1996/10/25 02:37:16 fox Exp $ */

/*
 * Implementation of Basic SGML lexer.
 * For theory of operation, see W3C tech report:
 *
 *   "A Lexical Analyzer for HTML and Basic SGML"
 *   http://www.w3.org/team/WWW/MarkUp/SGML/sgml-lex/sgml-lex.html
 *
 * Uses tables output by flex, but driver code is adapted to
 * "message passing" style.
 *
 * RESTRICTIONS ON FLEX INPUT:
 * -- yytext is immutable
 * -- no EOF actions
 * -- @# others???
 */

/* virtual memory exhaustion is treated as a FATAL ERROR! @@*/

/* exports... */
#include "sgml_lex.h"

/* imports... */
#include <ctype.h> /* for atoi, tolower */
#include <stdlib.h> /* for size_t, realloc, ... */
#include <stdio.h>
#include <assert.h>
#include <string.h> /* for memcpy, memmove */


/*****************************************
 *
 * Code borrowed wholesale from flex scanner...
 *
 */

/* Promotes a possibly negative, possibly signed char to an unsigned
 * integer for use as an array index.  If the signed char is negative,
 * we want to instead treat it as an 8-bit unsigned char, hence the
 * double cast.
 */
#define YY_SC_TO_UI(c) ((unsigned int) (unsigned char) c)

/* Done after the current pattern has been matched and before the
 * corresponding action - sets up yytext.
 */
#define YY_DO_BEFORE_ACTION \
	yytext = yy_bp; \
	yyleng = yy_cp - yy_bp;

#define YY_USER_ACTION /* nothing */
#define YY_RULE_SETUP \
	YY_USER_ACTION
#define YY_BREAK break;
#define YY_FATAL_ERROR(str) assert((str,0))

/* Enter a start condition. */
#define BEGIN(x) l->yy_start = 1 + 2 * (x)

#define yyconst const
#define YY_PROTO(proto) proto

/*
 *
 *****************************************/


/*
 * include flex-generated tables...
 */
#include "sgml_tab.c"

/* Secret knowledge from flex... */
#define FLEX_START_STATE 1


/*
 * Utility functions used in lexer actions
 */

/*
 * Add one token-string to current event.
 */
#define ADD(ty, str, len) \
		  token_add(l, (ty), (str), (len), 0)

/*
 * Add one case-insensitive token-string to current event.
 */
#define ADDCASE(ty, str, len) \
		  token_add(l, (ty), (str), (len), 1)

/*
 * Pass current event to f, with rock r
 */
#define CALLBACK(f,r) \
		  call(l, (f), (r))

/*
 * Report error "msg" via errF/errObj.
 */
#define ERROR(ty, msg, ytext, ylen) \
          call_err(errF, errObj, l, (ty), (msg), (ytext), (ylen))

/*
 * Pass single-string event in yytext to callback function f
 */
#define TOK(f, r, ty, ytext, ylen) \
          call_one(f, r, l, (ty), (ytext), (ylen))

/*
 * Pass double-string event in yytext to callback function f
 */
#define TOK2(f, r, ty1, ytext1, ylen1, ty2, ytext2, ylen2) \
          call_two(f, r, l, (ty1), (ytext1), (ylen1), (ty2), (ytext2), (ylen2))


static void
token_add(SGML_Lexer *l,
	  SGML_TokType ty, const char *string, size_t length, int downcase);

static void
call(SGML_Lexer *l,
     SGML_LexFunc f, void *rock);

static void
freeEvent(SGML_Lexer *l);

static void
call_err(SGML_LexFunc f, void *rock,
	 SGML_Lexer *l,
	 SGML_TokType ty, const char *msg, const char* ytext, size_t ylen);

static void
call_one(SGML_LexFunc f, void *rock,
	 SGML_Lexer *l,
	 SGML_TokType ty, const char* ytext, size_t ylen);

static void
call_two(SGML_LexFunc f, void *rock,
	 SGML_Lexer *l,
	 SGML_TokType ty1, const char* ytext1, size_t ylen1,
	 SGML_TokType ty2, const char* ytext2, size_t ylen2);





/*
 * Lexer private data structure...
 */
struct _SGML_Lexer{
  /* space to build an event... */
  int              qty;       /* how many strings pending in this event? */
  SGML_TokType    *types;     /* type of each string */
  SGML_String     *strings;   /* strings */
  size_t          *lengths;   /* lengths */

  char            *scratch;   /* scratch space */
  size_t           scratchQ;

  /* scanner state (from lex code) */
  size_t pending;             /* pending bytes from previous call */
  int yy_start;               /* current start-condition state index */
  int yy_current_state;       /* lexer state index */

  int yy_last_accepting_state;         /* last seen accepting state */
  const char *yy_last_accepting_cpos;  /* corresponding character */
  const char *yy_bp;          /* point back to beginning of current token */

  /* Normalize identifiers to lower-case? */
  int normalize;

  /* Error reporting info */
  int line;
};


/*
 * Allocate, initialize an SGML_Lexer
 */
SGML_Lexer*
SGML_newLexer(void)
{
  SGML_Lexer *ret;

  ret = malloc(sizeof(*ret));
  assert(ret); /*@@*/

  ret->types = NULL;
  ret->strings = NULL;
  ret->lengths = NULL;
  ret->qty = 0;

  ret->pending = 0;
  ret->yy_bp = NULL;
  ret->yy_start = FLEX_START_STATE;

  ret->scratch = NULL;
  ret->scratchQ = 0;

  ret->normalize = 0;

  ret->line = 1;

  return ret;
}


/*
 * Deallocate SGML_Lexer
 */
void
SGML_delLexer(SGML_Lexer *l)
{
  if(l->scratch) free(l->scratch);

  freeEvent(l);
  free(l);
}



int SGML_lexLine(SGML_Lexer *l)
{
  return l->line;
}


int SGML_lexNorm(SGML_Lexer *l, int normalize)
{
  int old = l->normalize;
  l->normalize = normalize;
  return old;
}


static char *
need_scratch(SGML_Lexer *l, size_t needQ)
{
  if(needQ > l->scratchQ){
    l->scratch = realloc(l->scratch, needQ);
    assert(l->scratch);

    l->scratchQ = needQ;
  }

  return l->scratch;
}


static size_t
copy_pending(SGML_Lexer *l, SGML_String *here, size_t hereQ)
     /*
      * In:          <abc             def=ghi>
      *              ^               ^
      *              |               |
      *   l->yy_bp---/         here--/
      *   l->pending = 3       hereQ = 9
      *
      * Out:         <abc def=ghi>
      *              ^
      *              |
      *   l->yy_bp---/
      *   return  12
      */
{
  need_scratch(l, l->pending + hereQ);

  if(hereQ > 0) memmove(l->scratch + l->pending, *here, hereQ);

  l->yy_bp = *here = l->scratch;

  return l->pending + hereQ; /* @# what if size_t overflows? */
}


static void
save_pending(SGML_Lexer *l,
	     SGML_String tosave, size_t saveQ)
{
  need_scratch(l, saveQ);
  memmove(l->scratch, tosave, saveQ);
  l->yy_bp = l->scratch;
  l->pending = saveQ;
}


void
SGML_lex(SGML_Lexer *l,
	 const char *encoded_chars, size_t len,
	 /* @# character encoding args? */
	 SGML_LexFunc tokF, void *tokObj,
	 SGML_LexFunc auxF, void *auxObj,
	 SGML_LexFunc errF, void *errObj)
{
  const char *yy_cp;  /* pointer to current char */
  const char *yy_bp;  /* local copy of l->yy_bp: pointer to beginning
		       * of token.
		       */
  const char *yy_c_buf_p; /* dummy, referenced in generated code */
  int yy_act;             /* action corresponding to recognized token */
  const char* yytext;     /* text made available to action
			   * !! NOT NULL-TERMINATED !!
			   */
  int yyleng;             /* #bytes in text... */
  int final = len == 0;   /* final chunk of characters? */
  int line_bp;            /* line to back-up to on incomplete token */

  /* left-over stuff from last time? */
  if(l->pending){
    assert(l->yy_bp);

    len = copy_pending(l, &encoded_chars, len);
    yy_cp = l->yy_bp;

  }else{ /* ... nope. Start fresh with new token. */
    assert(l->yy_bp == NULL);

    l->yy_bp = yy_cp = encoded_chars;
  }

  /* Start state-machine at beginning of token
   * @# This is something of a kludge: we shouldn't start over at
   * the beginning of the token on each call.
   */
  l->yy_current_state = l->yy_start;
  yy_bp = l->yy_bp;


  /* Iterate over encoded_chars */
  while(yy_cp - encoded_chars < len){

    line_bp = l->line;

    /* do-loop iterates over characters in a token */
    do
      {
	unsigned char yy_c;

	if ( yy_accept[l->yy_current_state] )
	  {
	    l->yy_last_accepting_state = l->yy_current_state;
	    l->yy_last_accepting_cpos = yy_cp;
	  }

	/* out of chars in the middle of a token? */
	if(yy_cp - encoded_chars == len){
	  if(final){
	    break; /* @@ hmmm.... what if not in an accepting state? */
	  }else{
	    save_pending(l, yy_bp, yy_cp - yy_bp);
	    l->line = line_bp;
	    return;
	  }
	}

	yy_c = yy_ec[YY_SC_TO_UI(*yy_cp)];


	/*
	 * The following code interprets the state machine
	 * tables created by flex
	 */
	while ( yy_chk[yy_base[l->yy_current_state] + yy_c]
	       != l->yy_current_state )
	  {
	    l->yy_current_state = (int) yy_def[l->yy_current_state];
	    if ( l->yy_current_state >= FLEX_MAGIC_STATE )
	      yy_c = yy_meta[(unsigned int) yy_c];
	  }

	l->yy_current_state = yy_nxt[yy_base[l->yy_current_state] + (unsigned int) yy_c];

	/* count lines */
	if(*yy_cp == '\n' /* @@ what about Macs? */){
	  l->line++;
	}

	++yy_cp;
      }
    while ( yy_base[l->yy_current_state] != FLEX_MAGIC_STATE2 );


    /*
     * Now we've found a complete token. Choose an action...
     */
yy_find_action:
    yy_act = yy_accept[l->yy_current_state];

    YY_DO_BEFORE_ACTION;

    switch ( yy_act )
      { /* beginning of action switch */
      case 0: /* must back up */
	yy_cp = l->yy_last_accepting_cpos;
	l->yy_current_state = l->yy_last_accepting_state;
	goto yy_find_action;

#include "sgml_act.c"

      }

    l->yy_bp = yy_bp = yy_cp;
    l->yy_current_state = l->yy_start;
  }

  l->pending = 0;
  l->yy_bp = NULL;

  return;
}



static void
token_add(SGML_Lexer *l,
	  SGML_TokType ty, const char *string, size_t length,
	  int downcase)
{

  int qty = l->qty;
  char *s;

  if(qty % 10 == 0){ /* resize arrays every 10th time */
    l->types = realloc(l->types, (qty+10) * sizeof(l->types));
    assert(l->types);

    l->strings = realloc(l->strings, (qty + 10) * sizeof(l->strings));
    assert(l->strings);

    l->lengths = realloc(l->lengths, (qty + 10) * sizeof(l->lengths));
    assert(l->lengths);
  }

  if(string){
    (l->strings)[qty] = s = malloc(length);
    assert(s); /*@@*/
    memcpy(s, string, length);

    if(l->normalize){
      int i;
      int last_non_white = 0;

      for(i = 0; i < length; i++){
	if(!isspace(s[i])) last_non_white = i;

	if(downcase && isalpha(s[i])) s[i] = tolower(s[i]);
      }

      length = last_non_white + 1;
    }
  }else{
    (l->strings)[qty] = NULL;
  }

  (l->types)[qty] = ty;
  (l->lengths)[qty] = length;

  l->qty = qty + 1;
}


static void
call_err(SGML_LexFunc f, void *rock,
	 SGML_Lexer *l,
	 SGML_TokType ty, const char *msg, const char* ytext, size_t ylen)
{
  SGML_TokType types[2];
  SGML_String strings[2];
  size_t lengths[2];

  types[0] = ty;        strings[0] = msg;   lengths[0] = strlen(msg);
  types[1] = SGML_DATA; strings[1] = ytext; lengths[1] = ylen;

  f(rock, l, 2, types, strings, lengths);
}


static void
call_one(SGML_LexFunc f, void *rock,
	 SGML_Lexer *l,
	 SGML_TokType ty, const char* ytext, size_t ylen)
{
  SGML_TokType types[1];
  SGML_String strings[1];
  size_t lengths[1];

  types[0] = ty; strings[0] = ytext; lengths[0] = ylen;

  f(rock, l, 1, types, strings, lengths);
}


static void
call_two(SGML_LexFunc f, void *rock,
	 SGML_Lexer *l,
	 SGML_TokType ty1, const char* ytext1, size_t ylen1,
	 SGML_TokType ty2, const char* ytext2, size_t ylen2)
{
  SGML_TokType types[2];
  SGML_String strings[2];
  size_t lengths[2];

  types[0] = ty1; strings[0] = ytext1; lengths[0] = ylen1;
  types[1] = ty2; strings[1] = ytext2; lengths[1] = ylen2;

  f(rock, l, 2, types, strings, lengths);
}


static void
freeEvent(SGML_Lexer *l)
{
  int i;

  for(i = 0; i < l->qty; i++){
    free((char*) (l->strings)[i]);
  }
  free(l->strings); l->strings = NULL;
  free(l->lengths); l->lengths = NULL;
  free(l->types); l->types = NULL;

  l->qty = 0;
}

static void
call(SGML_Lexer *l,
     SGML_LexFunc f, void *rock)
{
  f(rock, l, l->qty, l->types, l->strings, l->lengths);

  freeEvent(l);
}

