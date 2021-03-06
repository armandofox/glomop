#define INITIAL 0
/* $Id: sgml_act.c,v 1.3 1997/10/26 19:31:13 tkimball Exp $ */
/* sgml.l -- a lexical analyzer for Basic+/- SGML Documents
 * See: "A Tractible Lexical Analyzer for SGML"
 */
/*
 * NOTE: We assume the locale used by lex and the C compiler
 * agrees with ISO-646-IRV; for example: '1' == 0x31.
 */
/* Figure 1 -- Character Classes: Abstract Syntax */
/* Figure 2 -- Character Classes: Concrete Syntax */
/* LCNMSTRT	[] */
/* UCNMSTRT	[] */
/* @# hmmm. sgml spec says \015 */
/* @# hmmm. sgml spec says \012 */
/* Figure 3 -- Reference Delimiter Set: General */
/* 9.2.1 SGML Character */
/*name_start_character	{LCLetter}|{UCLetter}|{LCNMSTRT}|{UCNMSTRT}*/
/* 9.3 Name */
/* 6.2.1 Space */
/* trailing white space */
/* 9.4.5 Reference End */
/*
 * 10.1.2 Parameter Literal
 * 7.9.3  Attribute Value Literal
 * (we leave recognition of character references and entity references,
 *  and whitespace compression to further processing)
 *
 * @# should split this into minimum literal, parameter literal,
 * @# and attribute value literal.
 */
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
#define CDATA 1

#define TAG 2

#define ATTR 3

#define ATTRVAL 4

#define MD 5

#define COM 6

#define DS 7

case 1:
YY_RULE_SETUP
#line 158 "sgml.l"
{
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
	YY_BREAK
/* &#60xyz. -- syntax error */
case 2:
YY_RULE_SETUP
#line 172 "sgml.l"
{
                                 ERROR(SGML_ERROR,
				       "bad character in character reference",
				       yytext, yyleng);
			       }
	YY_BREAK
/* &#SPACE; -- named character reference. Not supported. */
case 3:
YY_RULE_SETUP
#line 180 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "named character references are not supported",
			       yytext, yyleng);
		       }
	YY_BREAK
/* &amp; -- general entity reference */
case 4:
YY_RULE_SETUP
#line 188 "sgml.l"
{
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
	YY_BREAK
/* </title> -- end tag */
case 5:
YY_RULE_SETUP
#line 202 "sgml.l"
{
                                  ADDCASE(SGML_END, yytext, yyleng);
				  BEGIN(TAG);
				}
	YY_BREAK
/* @# HACK for XMP, LISTING?
  Date: Fri, 19 Jan 1996 23:13:43 -0800
  Message-Id: <v01530502ad25cc1a251b@[206.86.76.80]>
  To: www-html@w3.org
  From: chris@walkaboutsoft.com (Chris Lovett)
  Subject: Re: Daniel Connolly's SGML Lex Specification
  */
/* </> -- empty end tag */
case 6:
YY_RULE_SETUP
#line 216 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "empty end tag not supported",
			       yytext, yyleng);
		       }
	YY_BREAK
/* <!DOCTYPE -- markup declaration */
case 7:
YY_RULE_SETUP
#line 223 "sgml.l"
{
                                  ADDCASE(SGML_MARKUP_DECL, yytext, yyleng);
				  BEGIN(MD);
				}
	YY_BREAK
/* <!> -- empty comment */
case 8:
YY_RULE_SETUP
#line 229 "sgml.l"
{ 
                               TOK(auxF, auxObj, SGML_MARKUP_DECL,
				   yytext, yyleng);
			     }
	YY_BREAK
/* <!--  -- comment declaration */
case 9:
yy_c_buf_p = yy_cp = yy_bp + 2;
YY_DO_BEFORE_ACTION; /* set up yytext again */
YY_RULE_SETUP
#line 235 "sgml.l"
{
                                 ADD(SGML_MARKUP_DECL, yytext, yyleng);
				 BEGIN(COM);
				}
	YY_BREAK
/* <![ -- marked section */
case 10:
YY_RULE_SETUP
#line 241 "sgml.l"
{
                        ERROR(SGML_LIMITATION,
			      "marked sections not supported",
			       yytext, yyleng);
			BEGIN(DS); /* @# skip past some stuff */
		      }
	YY_BREAK
/* ]]> -- marked section end */
case 11:
YY_RULE_SETUP
#line 249 "sgml.l"
{
                        ERROR(SGML_ERROR,
			      "unmatched marked sections end",
			       yytext, yyleng);
		      }
	YY_BREAK
/* <? ...> -- processing instruction */
case 12:
YY_RULE_SETUP
#line 256 "sgml.l"
{
                                  TOK(auxF, auxObj, SGML_PI, yytext, yyleng);
				}
	YY_BREAK
/* <name -- start tag */
case 13:
YY_RULE_SETUP
#line 260 "sgml.l"
{
                                  ADDCASE(SGML_START, yytext, yyleng);
				  BEGIN(ATTR);
				}
	YY_BREAK
/* <> -- empty start tag */
case 14:
YY_RULE_SETUP
#line 267 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "empty start tag not supported",
			       yytext, yyleng);
		       }
	YY_BREAK
/* abcd -- data characters */
case 15:
YY_RULE_SETUP
#line 274 "sgml.l"
{
                                  TOK(tokF, tokObj, SGML_DATA, yytext, yyleng);
				}
	YY_BREAK
/* abcd -- data characters */
case 16:
YY_RULE_SETUP
#line 279 "sgml.l"
{
                                  TOK(tokF, tokObj, SGML_DATA, yytext, yyleng);
				}
	YY_BREAK
/* 7.4 Start Tag */
/* Actually, the generic identifier specification is consumed
  * along with the STAGO delimiter ("<"). So we're only looking
  * for tokens that appear in an attribute specification list,
  * plus TAGC (">"). NET ("/") and STAGO ("<") signal limitations.
  */
/* 7.5 End Tag */
/* Just looking for TAGC. NET, STAGO as above */
/* <a ^href = "xxx"> -- attribute name */
case 17:
YY_RULE_SETUP
#line 294 "sgml.l"
{

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
	YY_BREAK
/* <img src="xxx" ^ismap> -- name */
case 18:
YY_RULE_SETUP
#line 310 "sgml.l"
{
                                  ADD(SGML_ATTRNAME, NULL, 0);
                                  ADDCASE(SGML_NAME, yytext, yyleng);
				}
	YY_BREAK
/* <a name = ^xyz> -- name token */
case 19:
YY_RULE_SETUP
#line 316 "sgml.l"
{
                                  ADD(SGML_NMTOKEN, yytext, yyleng);
				  BEGIN(ATTR);
				}
	YY_BREAK
/* <a href = ^"a b c"> -- literal */
case 20:
YY_RULE_SETUP
#line 322 "sgml.l"
{
                                  if(yyleng > 2 && yytext[yyleng-2] == '='
				     && memchr(yytext, '>', yyleng)){
				    ERROR(SGML_WARNING,
					  "missing attribute end-quote?",
					  yytext, yyleng);
				  }
                                  ADD(SGML_LITERAL, yytext, yyleng);
				  BEGIN(ATTR);
				}
	YY_BREAK
/* <a href = ^http://foo/> -- unquoted literal HACK */
case 21:
YY_RULE_SETUP
#line 334 "sgml.l"
{
                                  ERROR(SGML_ERROR,
					"attribute value needs quotes",
					yytext, yyleng);
                                  ADD(SGML_LITERAL, yytext, yyleng);
				  BEGIN(ATTR);
				}
	YY_BREAK
/* <a name= ^> -- illegal tag close */
case 22:
YY_RULE_SETUP
#line 343 "sgml.l"
{
                                  ERROR(SGML_ERROR,
		       "Tag close found where attribute value expected",
			       yytext, yyleng);

                                  CALLBACK(tokF,tokObj);
				  BEGIN(INITIAL);
				}
	YY_BREAK
/* <a name=foo ^>,</foo^> -- tag close */
case 23:
YY_RULE_SETUP
#line 353 "sgml.l"
{
                                  ADD(SGML_TAGC, yytext, yyleng);
                                  CALLBACK(tokF,tokObj);
				  BEGIN(INITIAL);
				}
	YY_BREAK
/* <em^/ -- NET tag */
case 24:
YY_RULE_SETUP
#line 360 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "NET tags not supported",
			       yytext, yyleng);

			 CALLBACK(tokF, tokObj);
			 BEGIN(INITIAL);
		       }
	YY_BREAK
/* <foo^<bar> -- unclosed start tag */
case 25:
YY_RULE_SETUP
#line 370 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "Unclosed tags not supported",
			       yytext, yyleng);

                         /* report pending tag */
			 CALLBACK(tokF, tokObj);
			 BEGIN(INITIAL);
		       }
	YY_BREAK
case 26:
YY_RULE_SETUP
#line 380 "sgml.l"
{
                         ERROR(SGML_ERROR,
			       "bad character in tag",
			       yytext, yyleng);
		       }
	YY_BREAK
/* 10 Markup Declarations: General */
/* <!^--...-->   -- comment */
case 27:
YY_RULE_SETUP
#line 389 "sgml.l"
{
                                  ADD(SGML_COMMENT, yytext, yyleng);
				}
	YY_BREAK
/* <!doctype ^%foo;> -- parameter entity reference */
case 28:
YY_RULE_SETUP
#line 394 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "parameter entity reference not supported",
			       yytext, yyleng);
		       }
	YY_BREAK
/* The limited set of markup delcarations we're interested in
  * use only numbers, names, and literals.
  */
case 29:
YY_RULE_SETUP
#line 403 "sgml.l"
{
                                  ADD(SGML_NUMBER, yytext, yyleng);
				}
	YY_BREAK
case 30:
YY_RULE_SETUP
#line 407 "sgml.l"
{
                                  ADDCASE(SGML_NAME, yytext, yyleng);
				}
	YY_BREAK
case 31:
YY_RULE_SETUP
#line 411 "sgml.l"
{
                                  ADD(SGML_LITERAL, yytext, yyleng);
				}
	YY_BREAK
case 32:
YY_RULE_SETUP
#line 415 "sgml.l"
{
                                  ADD(SGML_TAGC, yytext, yyleng);
                                  CALLBACK(auxF, auxObj);
				  BEGIN(INITIAL);
			        }
	YY_BREAK
/* other constructs are errors. */
/* <!doctype foo ^[  -- declaration subset */
case 33:
YY_RULE_SETUP
#line 423 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "declaration subset not supported",
			       yytext, yyleng);
			 CALLBACK(auxF, auxObj);
			 BEGIN(DS);
		       }
	YY_BREAK
case 34:
YY_RULE_SETUP
#line 431 "sgml.l"
{
                         ERROR(SGML_ERROR,
			       "illegal character in markup declaration",
			       yytext, yyleng);
		       }
	YY_BREAK
/* 10.4 Marked Section Declaration */
/* 11.1 Document Type Declaration Subset */
/* Our parsing of declaration subsets is just an error recovery technique:
  * we attempt to skip them, but we may be fooled by "]"s
  * inside comments, etc.
  */
/* ]]> -- marked section end */
case 35:
YY_RULE_SETUP
#line 447 "sgml.l"
{
				 BEGIN(INITIAL);
				}
	YY_BREAK
/* ] -- declaration subset close */
case 36:
YY_RULE_SETUP
#line 451 "sgml.l"
{ BEGIN(COM); }
	YY_BREAK
case 37:
YY_RULE_SETUP
#line 453 "sgml.l"
{
                         ERROR(SGML_LIMITATION,
			       "declaration subset: skipping",
			       yytext, yyleng);
		       }
	YY_BREAK
