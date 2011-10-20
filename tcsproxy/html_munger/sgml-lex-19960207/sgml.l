/* $Id: sgml.l,v 1.1.1.1 1996/10/25 02:37:16 fox Exp $ */
/* sgml.l -- a lexical analyzer for Basic+/- SGML Documents
 * See: "A Tractible Lexical Analyzer for SGML"
 */

/*
 * NOTE: We assume the locale used by lex and the C compiler
 * agrees with ISO-646-IRV; for example: '1' == 0x31.
 */


/* Figure 1 -- Character Classes: Abstract Syntax */

Digit		[0-9]
LCLetter	[a-z]
Special		['()_,\-\./:=?]
UCLetter	[A-Z]

/* Figure 2 -- Character Classes: Concrete Syntax */

LCNMCHAR	[\.-]
/* LCNMSTRT	[] */
UCNMCHAR	[\.-]
/* UCNMSTRT	[] */
 /* @# hmmm. sgml spec says \015 */
RE		\n
 /* @# hmmm. sgml spec says \012 */
RS		\r
SEPCHAR		\011
SPACE		\040

/* Figure 3 -- Reference Delimiter Set: General */

COM	"--"
CRO	"&#"
DSC	"]"
DSO	"["
ERO	"&"
ETAGO	"</"
LIT	\"
LITA	"'"
MDC	">"
MDO	"<!"
MSC	"]]"
NET     "/"
PERO    "%"
PIC	">"
PIO	"<?"
REFC	";"
STAGO	"<"
TAGC	">"

/* 9.2.1 SGML Character */

/*name_start_character	{LCLetter}|{UCLetter}|{LCNMSTRT}|{UCNMSTRT}*/
name_start_character	{LCLetter}|{UCLetter}
name_character		{name_start_character}|{Digit}|{LCNMCHAR}|{UCNMCHAR}

/* 9.3 Name */

name		{name_start_character}{name_character}*
number		{Digit}+
number_token	{Digit}{name_character}*
name_token	{name_character}+

/* 6.2.1 Space */
s		{SPACE}|{RE}|{RS}|{SEPCHAR}
ps		({SPACE}|{RE}|{RS}|{SEPCHAR})+

/* trailing white space */
ws		({SPACE}|{RE}|{RS}|{SEPCHAR})*

/* 9.4.5 Reference End */
reference_end	{REFC}|{RE}

/*
 * 10.1.2 Parameter Literal
 * 7.9.3  Attribute Value Literal
 * (we leave recognition of character references and entity references,
 *  and whitespace compression to further processing)
 *
 * @# should split this into minimum literal, parameter literal,
 * @# and attribute value literal.
 */
literal		({LIT}[^\"]*{LIT})|({LITA}[^\']*{LITA})



/* 9.6.1 Recognition modes */

/*
 * Recognition modes are represented here by start conditions.
 * The default start condition, INITIAL, represents the
 * CON recognition mode. This condition is used to detect markup
 * while parsing normal data charcters (mixed content).
 *
 * The CDATA start condition represents the CON recognition
 * mode with the restriction that only end-tags are recognized,
 * as in elements with CDATA declared content.
 * (@# no way to activate it yet: need hook to parser.)
 *
 * The TAG recognition mode is split into two start conditions:
 * ATTR, for recognizing attribute value list sub-tokens in
 * start-tags, and TAG for recognizing the TAGC (">") delimiter
 * in end-tags.
 *
 * The MD start condition is used in markup declarations. The COM
 * start condition is used for comment declarations.
 *
 * The DS condition is an approximation of the declaration subset
 * recognition mode in SGML. As we only use this condition after signalling
 * an error, it is merely a recovery device.
 *
 * The CXT, LIT, PI, and REF recognition modes are not separated out
 * as start conditions, but handled within the rules of other start
 * conditions. The GRP mode is not represented here.
 */

 /* EXCERPT ACTIONS: START */

/* %x CON == INITIAL */
%x CDATA

%x TAG
%x ATTR
%x ATTRVAL

%x MD
%x COM
%x DS

 /* EXCERPT ACTIONS: STOP */

%%

  int *types = NULL;
  char **strings = NULL;
  size_t *lengths = NULL;
  int qty = 0;

      /*
       * See sgml_lex.c for description of
       *   ADD, CALLBACK, ERROR, TOK macros.
       */


 /*
  * 9.6 Delimiter Recognition and
  * Figure 3 -- Reference Delimiter Set: General
  *
  * This is organized by recognition mode: first CON, then TAG,
  * MD, and DS. Within a mode, the rules are ordered alphabetically
  * by delimiter name.
  */


  /* &#60; -- numeric character reference */
{CRO}{number}{reference_end}?	 {
                                   /* explicit REFC? */
                                   if(yytext[yyleng-1] == '\n'
				      || yytext[yyleng-1] == ';'){
				     TOK2(tokF, tokObj,
					  SGML_NUMCHARREF, yytext, yyleng-1,
					  SGML_REFC, yytext + yyleng - 1, 1);
				   }else{
				     TOK(tokF, tokObj, SGML_NUMCHARREF,
					 yytext, yyleng);
				   }
				 }

  /* &#60xyz. -- syntax error */
{CRO}{number_token}{reference_end}?	{
                                 ERROR(SGML_ERROR,
				       "bad character in character reference",
				       yytext, yyleng);
			       }


  /* &#SPACE; -- named character reference. Not supported. */
{CRO}{name}{reference_end}?		{
                         ERROR(SGML_LIMITATION,
			       "named character references are not supported",
			       yytext, yyleng);
		       }


  /* &amp; -- general entity reference */
{ERO}{name}{reference_end}?	{
                                  /* explicit REFC? */
                                  if(yytext[yyleng-1] == '\n'
				     || yytext[yyleng-1] == ';'){
				    TOK2(tokF, tokObj,
					 SGML_GEREF, yytext, yyleng-1,
					 SGML_REFC, yytext + yyleng - 1, 1);
				  }else{
				    TOK(tokF, tokObj, SGML_GEREF,
					yytext, yyleng);
				  }
				}

  /* </title> -- end tag */
<INITIAL,CDATA>{ETAGO}{name}{ws} {
                                  ADDCASE(SGML_END, yytext, yyleng);
				  BEGIN(TAG);
				}

  /* @# HACK for XMP, LISTING?
  Date: Fri, 19 Jan 1996 23:13:43 -0800
  Message-Id: <v01530502ad25cc1a251b@[206.86.76.80]>
  To: www-html@w3.org
  From: chris@walkaboutsoft.com (Chris Lovett)
  Subject: Re: Daniel Connolly's SGML Lex Specification
  */

  /* </> -- empty end tag */
{ETAGO}{TAGC}			{
                         ERROR(SGML_LIMITATION,
			       "empty end tag not supported",
			       yytext, yyleng);
		       }

  /* <!DOCTYPE -- markup declaration */
{MDO}{name}{ws}			{
                                  ADDCASE(SGML_MARKUP_DECL, yytext, yyleng);
				  BEGIN(MD);
				}

  /* <!> -- empty comment */
{MDO}{MDC}			{ 
                               TOK(auxF, auxObj, SGML_MARKUP_DECL,
				   yytext, yyleng);
			     }

  /* <!--  -- comment declaration */
{MDO}/{COM}			{
                                 ADD(SGML_MARKUP_DECL, yytext, yyleng);
				 BEGIN(COM);
				}

  /* <![ -- marked section */
{MDO}{DSO}{ws}			{
                        ERROR(SGML_LIMITATION,
			      "marked sections not supported",
			       yytext, yyleng);
			BEGIN(DS); /* @# skip past some stuff */
		      }

  /* ]]> -- marked section end */
{MSC}{MDC}			{
                        ERROR(SGML_ERROR,
			      "unmatched marked sections end",
			       yytext, yyleng);
		      }

  /* <? ...> -- processing instruction */
{PIO}[^>]*{PIC}			{
                                  TOK(auxF, auxObj, SGML_PI, yytext, yyleng);
				}
  /* <name -- start tag */
{STAGO}{name}{ws}		{
                                  ADDCASE(SGML_START, yytext, yyleng);
				  BEGIN(ATTR);
				}


  /* <> -- empty start tag */
{STAGO}{TAGC}			{
                         ERROR(SGML_LIMITATION,
			       "empty start tag not supported",
			       yytext, yyleng);
		       }

  /* abcd -- data characters */
([^<&]|(<[^<&a-zA-Z!->?])|(&[^<&#a-zA-Z]))+|.	{
                                  TOK(tokF, tokObj, SGML_DATA, yytext, yyleng);
				}

  /* abcd -- data characters */
<CDATA>[^<]+|.			{
                                  TOK(tokF, tokObj, SGML_DATA, yytext, yyleng);
				}

 /* 7.4 Start Tag */
 /* Actually, the generic identifier specification is consumed
  * along with the STAGO delimiter ("<"). So we're only looking
  * for tokens that appear in an attribute specification list,
  * plus TAGC (">"). NET ("/") and STAGO ("<") signal limitations.
  */

 /* 7.5 End Tag */
 /* Just looking for TAGC. NET, STAGO as above */

  /* <a ^href = "xxx"> -- attribute name */
<ATTR>{name}{s}*={ws}		{

                                  if(l->normalize){

                                    /* strip trailing space and = */
                                    while(yytext[yyleng-1] == '='
					  || isspace(yytext[yyleng-1])){
				      --yyleng;
				    }
                                  }

                                  ADDCASE(SGML_ATTRNAME, yytext, yyleng);
				  BEGIN(ATTRVAL);
				}

  /* <img src="xxx" ^ismap> -- name */
<ATTR>{name}{ws}		{
                                  ADD(SGML_ATTRNAME, NULL, 0);
                                  ADDCASE(SGML_NAME, yytext, yyleng);
				}

  /* <a name = ^xyz> -- name token */
<ATTRVAL>{name_token}{ws}	{
                                  ADD(SGML_NMTOKEN, yytext, yyleng);
				  BEGIN(ATTR);
				}

  /* <a href = ^"a b c"> -- literal */
<ATTRVAL>{literal}{ws}		{
                                  if(yyleng > 2 && yytext[yyleng-2] == '='
				     && memchr(yytext, '>', yyleng)){
				    ERROR(SGML_WARNING,
					  "missing attribute end-quote?",
					  yytext, yyleng);
				  }
                                  ADD(SGML_LITERAL, yytext, yyleng);
				  BEGIN(ATTR);
				}

  /* <a href = ^http://foo/> -- unquoted literal HACK */
<ATTRVAL>[^ "\t\n>]+{ws}	{
                                  ERROR(SGML_ERROR,
					"attribute value needs quotes",
					yytext, yyleng);
                                  ADD(SGML_LITERAL, yytext, yyleng);
				  BEGIN(ATTR);
				}

  /* <a name= ^> -- illegal tag close */
<ATTRVAL>{TAGC}			{
                                  ERROR(SGML_ERROR,
		       "Tag close found where attribute value expected",
			       yytext, yyleng);

                                  CALLBACK(tokF,tokObj);
				  BEGIN(INITIAL);
				}

  /* <a name=foo ^>,</foo^> -- tag close */
<ATTR,TAG>{TAGC}		{
                                  ADD(SGML_TAGC, yytext, yyleng);
                                  CALLBACK(tokF,tokObj);
				  BEGIN(INITIAL);
				}

  /* <em^/ -- NET tag */
<ATTR,ATTRVAL>{NET}	{
                         ERROR(SGML_LIMITATION,
			       "NET tags not supported",
			       yytext, yyleng);

			 CALLBACK(tokF, tokObj);
			 BEGIN(INITIAL);
		       }

  /* <foo^<bar> -- unclosed start tag */
<ATTR,ATTRVAL,TAG>{STAGO}	{
                         ERROR(SGML_LIMITATION,
			       "Unclosed tags not supported",
			       yytext, yyleng);

                         /* report pending tag */
			 CALLBACK(tokF, tokObj);
			 BEGIN(INITIAL);
		       }

<ATTR,ATTRVAL,TAG>.	{
                         ERROR(SGML_ERROR,
			       "bad character in tag",
			       yytext, yyleng);
		       }

 /* 10 Markup Declarations: General */

 /* <!^--...-->   -- comment */
<MD,COM>{COM}([^-]|-[^-])*{COM}{ws}	{
                                  ADD(SGML_COMMENT, yytext, yyleng);
				}

 /* <!doctype ^%foo;> -- parameter entity reference */
<MD>{PERO}{name}{reference_end}?{ws}		{
                         ERROR(SGML_LIMITATION,
			       "parameter entity reference not supported",
			       yytext, yyleng);
		       }

 /* The limited set of markup delcarations we're interested in
  * use only numbers, names, and literals.
  */
<MD>{number}{ws}		{
                                  ADD(SGML_NUMBER, yytext, yyleng);
				}

<MD>{name}{ws}			{
                                  ADDCASE(SGML_NAME, yytext, yyleng);
				}

<MD>{literal}{ws}	        {
                                  ADD(SGML_LITERAL, yytext, yyleng);
				}

<MD,COM>{MDC}			{
                                  ADD(SGML_TAGC, yytext, yyleng);
                                  CALLBACK(auxF, auxObj);
				  BEGIN(INITIAL);
			        }

 /* other constructs are errors. */
   /* <!doctype foo ^[  -- declaration subset */
<MD>{DSO}			{
                         ERROR(SGML_LIMITATION,
			       "declaration subset not supported",
			       yytext, yyleng);
			 CALLBACK(auxF, auxObj);
			 BEGIN(DS);
		       }

<MD,COM>.				{
                         ERROR(SGML_ERROR,
			       "illegal character in markup declaration",
			       yytext, yyleng);
		       }


 /* 10.4 Marked Section Declaration */
 /* 11.1 Document Type Declaration Subset */

 /* Our parsing of declaration subsets is just an error recovery technique:
  * we attempt to skip them, but we may be fooled by "]"s
  * inside comments, etc.
  */

  /* ]]> -- marked section end */
<DS>{MSC}{MDC}			{
				 BEGIN(INITIAL);
				}
  /* ] -- declaration subset close */
<DS>{DSC}			{ BEGIN(COM); }

<DS>[^\]]+			{
                         ERROR(SGML_LIMITATION,
			       "declaration subset: skipping",
			       yytext, yyleng);
		       }

 /* EXCERPT ACTIONS: STOP */

%%
