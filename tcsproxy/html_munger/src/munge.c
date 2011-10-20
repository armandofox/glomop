/*
 *  munge.c
 *
 *  DistillerMain for HTML munger.  The DistillerMain function accepts a
 *  single distiller argument: a string that should be spliced at the
 *  end of every URL whose extension (.gif, etc) suggests that it is a
 *  "recognized type".  (I know you aren't supposed to guess MIME types
 *  from extensions, but hey, sue me.)  The function known_extensions
 *  returns a boolean indicating whether the given URL has an extension
 *  that should be munged.
 *
 *  The HTML lexer is actually Dan Connolly's (W3C) reference SGML
 *  lexer, which is fast but has an evil callback convention (see
 *  comments in the callback() function).
 */

#include "munge.h"
#include "sgml_lex.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../../frontend/include/ARGS.h"
#include <stdlib.h>

#ifdef INST
FILE *Log;
#endif
MonitorClientID mon;

/* this is messy, it gets around the lexer callback stupidity */
static Argument * args;
static int nargs;

/* value defines where ICON_STRING should be placed */
/* this is one of icon_string_pos                   */
/* if no_icon dont place at all                     */
/* if at_beginning place at the very beginning      */
/* if after_head place after </HEAD>                */
/* if in_body place after <BODY>                    */
/* set to no_icon after placing                     */
static int where_string;

void callback(void *,SGML_Lexer *, int, SGML_TokType *, SGML_String *,
              size_t *);

void clean_HtmlTagDescr(struct HtmlTagDescr *, int);

/*
 *  BUG::Why are strncasecmp and strcasecmp defined here????  There is
 *  an AC_REPLACE_FUNCS in the toplevel Makefile, but there is no
 *  strncasecmp.c or strcasecmp.c.
 */

int
strncasecmp(const char *s1, const char *s2, size_t s)
{
  register int i = 0;
  register signed char z,c1,c2;
  while (i++ < s && (c1 = *s1++) && (c2 = *s2++))
    if ((z = tolower(c1) - tolower(c2)) != 0)
      return (z < 0? -1: 1);
  return 0;
}

#define strcasecmp_(s1,s2) strncasecmp((s1),(s2),strlen(s1))

int find_where_string(DistillerInput * di) {
  int head_end = 0, frameset = 0, noframes = 0;
  char * data = (char *) DataPtr(di);
  UINT32 i;

  for (i = 0; i < DataLength(di); i++) {
    if (data[i] == '<') {
      if (!strcasecmp_("</HEAD", data+i)) {
	head_end = i;
      } else if (!strcasecmp_("<BODY", data+i)) {
	return in_body;
      } else if (!strcasecmp_("<NOFRAMES", data+i)) {
	noframes = 1;
      } else if (!strcasecmp_("<FRAMESET", data+i)) {
	frameset = 1;
      }
    }    
  }

  if (noframes) return in_noframes;
  if (frameset) return no_icon;
  if (head_end) return after_head;
  return at_beginning;
}

DistillerStatus
ComputeDistillationCost(Argument *args, int numArgs,
                        DistillerInput *in,
                        DistillationCost *cost)
{
  cost->estimatedTime_ms = DataLength(in);
  return distOk;
}

DistillerStatus
DistillerInit(C_DistillerType distType, int argc, const char * const *argv)
{
#ifdef INST
  char file[80];
  extern FILE *Log;
  extern int errno;
#endif
  
  mon = DistillerGetMonitorClientID();

#ifdef INST
  sprintf(file, "/tmp/html.%d", getpid());
  Log = fopen(file, "w");
  if (Log == NULL) {
    sprintf(file, "Opening /tmp/html.%d: errno = %d\n", getpid(), errno);
    MonitorClientSend(mon, "Errors", file, "");
    return distFatalError;
  }
#endif
  return distOk;
}

DistillerStatus
DistillerMain(Argument *args_, int nargs_,
              DistillerInput *din,
              DistillerOutput *dout)
{
  SGML_Lexer *l;
  OutputInfo output_info;
  int show_icon = 1; 
  int i;

  DataNeedsFree(dout,gm_False);
  MetadataNeedsFree(dout,gm_False);

  args = args_;
  nargs = nargs_;

  /* initilize the tag processor */
  process_tag(NULL, NULL, 0, NULL, NULL, 0, NULL);
  
  where_string = no_icon;

  /* add the ICON_STRING of we should */
  for (i=0; i<nargs; i++) {
    if ((ARG_ID(args[i]) == FRONT_SHOW_ICON) && ARG_INT(args[i]) == 0) {
      show_icon = 0;
      break;
    }
  }

  if (show_icon) where_string = find_where_string(din);
  
  assert(l = SGML_newLexer());
  /*
   *  fill in the output structure that will be passed to the callback
   *  routine.  Allocate an output buffer that is initially the size
   *  of the input buffer * 2.
   */
  output_info.html_bufsize = (DataLength(din) << 1) + 1000;
  output_info.html_buf = DistillerMalloc((size_t)(output_info.html_bufsize));
  assert(output_info.html_buf);
  output_info.html_buflen = 0;

  if (where_string == at_beginning) {
    append_string(&output_info, ICON_STRING, -1);
    where_string = no_icon;
  }

  SGML_lex(l, (const char *)DataPtr(din), (size_t)DataLength(din),
           callback, (void *)&output_info,
           callback, (void *)&output_info,
           callback, NULL);
  SGML_lex(l, (const char *)NULL, (size_t)0,
           callback, (void *)&output_info,
           callback, (void *)&output_info,
           callback, NULL);
  SGML_delLexer(l);

  /*
   *  Return the output buffer and size.
   */
  SetDataLength(dout,output_info.html_buflen);
  SetData(dout, output_info.html_buf);
  DataNeedsFree(dout, gm_True);
  strncpy(dout->mimeType, "text/html", MAX_MIMETYPE);

  
#ifdef INST
  fprintf(Log, "\n");
#endif
  return(distOk);
}

int
known_extension(char *str, int len)
{
  static char *known[] = {"gif", "jpeg", "jpg", "pgif", "pjpeg", "pjpg"};
  static int knownlen[] = { 3, 4, 3, 4, 5, 4 };
  int i;

  if (len == 0)
    return 0;
  for (i=0; i<(sizeof(known)/sizeof(char *)); i++)
    if (strncasecmp(known[i], str+len-knownlen[i], knownlen[i]) == 0)
      return 1;
  return 0;
}

/*
 *  HTML lexer callback; gets called when a tag or HTML content is
 *  parsed.
 *
 *  ARGS:
 *  distinfo - struct containing output buf pointer, "magic dust" to add
 *     to URL if this is a tag that has a URL link, etc.
 *  l - lexer object
 *  qty - number of <tokentype,string> tuples to follow
 *  types,strings,lengths - something that really oughta be an array of structs
 *      Each types[] is an SGML_TokType:
 *      SGML_ERROR: error token; corresponding string is error message
 *      SGML_ATTRNAME: tag was parsed; corresponding string is either an
 *              attr or a value, alternating.  E.g. we would get:
 *              SGML_ATTR "SRC"
 *              SGML_ATTR "http://www.com"
 *              SGML_ATTR "WIDTH"
 *              SGML_ATTR "3"
 *         The auto variable next_str_is_attrval remembers this state. Ugh.
 *         Note the strings aren't null-terminated; hence lengths[].  Ugh.
 */
void
callback(void *_distinfo,
         SGML_Lexer *l,
         int qty,
         SGML_TokType types[], SGML_String strings[], size_t lengths[])
{
  int i;
  int sgml_error;
  int next_str_is_attrval;
  int next_str_is_attrname;
  int in_html_tag;
  OutputInfo *di = (OutputInfo *)_distinfo;
  struct HtmlTagDescr* tag_attribs;
  int nattribs;
  char *tagname = NULL;

  if (_distinfo == NULL)
    return;

  tag_attribs = (struct HtmlTagDescr*) 
    calloc(sizeof(struct HtmlTagDescr) * qty, 1);

  /*
   *  In case we're going to parse a tag, initialize....
   */
  nattribs = 0;
  next_str_is_attrval = 0;
  next_str_is_attrname = 0;
  sgml_error = 0;
  in_html_tag = 0;
  /* memset(tag_attribs, sizeof(tag_attribs), '*');*/
  
  for (i=0; i< qty && !sgml_error; i++) {
    if (next_str_is_attrval) {
      int stringlen;
      char *ls, *ts;
      int lslen, tslen;
      /*
       *  Handle an attribute value
       */
      next_str_is_attrval = 0;
      if (lengths[i] == 0) {
        /* null attribute */
	tag_attribs[nattribs].attr_origval = (char *) DistillerMalloc(1);
	tag_attribs[nattribs].attr_val = (char *) DistillerMalloc(1);	  
        tag_attribs[nattribs].attr_origval[0] = '\0';
        tag_attribs[nattribs].attr_val[0] = '\0';
        nattribs++;
        continue;
      }
      /*
       *  This is evil: the attribute value is returned as a single
       *  string that INCLUDES both leading and trailing whitespace,
       *  EVEN IF the attribute value itself is double quoted.  I.e. the
       *  text  '<SRC= "foo"  >' will return the attribute value
       *  ' "foo"  '.  Truly evil.  To fix it, we separate it into the
       *  leading whitespace, attr value optionally in double quotes,
       *  and trailing whitespace.  The char pointers ls and ts point to
       *  the leading and trailing strings, and lslen and tslen are
       *  their lengths.  Note that we special-case ts to "" if tslen is
       *  found to be zero, to avoid a possible bad pointer dereference
       *  off the end of the original string.  This lexer interface sucks.
       */

      tag_attribs[nattribs].attr_origval = (char *) DistillerMalloc(lengths[i]+1);
      bogon_strncpy(tag_attribs[nattribs].attr_origval, strings[i], lengths[i]);

      lslen = 0;
      ls = (char *)strings[i];
      while (lslen < lengths[i] && isspace(*ls)) {
        ls++; lslen++;
      }
      if (*ls == '"') {
        ls++; lslen++;
      }
      tslen = 1;
      ts = (char *)(&strings[i][lengths[i]-1]);
      while (tslen < lengths[i]-lslen  && isspace(*ts)) {
        ts--; tslen++;
      }
      if (*ts != '"') {
        ts++; tslen--;
      }
      if (tslen == 0)
        ts = "";
      stringlen = lengths[i]-tslen-lslen;
      
      tag_attribs[nattribs].attr_val = (char *) DistillerMalloc(stringlen+1);
      bogon_strncpy(tag_attribs[nattribs].attr_val, ls, stringlen);

      nattribs++;
    } else {
      /*
       *  Not an attrib name: consider other token types
       */
      switch(types[i]) {
      case SGML_ATTRNAME: 
        /* the strings are attribute/value pairs.  WHen parsing such a
         *  pair, the lexer always alternates attrib names with values.
         *  Even if we have something like <IMG SRC="foo" ISMAP HREF="...">,
         *  when it gets to the ISMAP, we'll get an attribute name of ''
         *  (null) and a value of "ISMAP".  (If we had said
         *  ISMAP="TRUE", we'd get an attribute name of "ISMAP=" and a
         *  value of "TRUE".)  So if we do see a null attribute name,
         *  assume the *real* attribute name is the *next* token.  Ugh.
         */
        if (lengths[i] > 0) {
          int real_len = lengths[i];
          /* get rid of trailing '=' and whitespace */
          while(isspace(strings[i][real_len-1])
                || strings[i][real_len-1] == '=')
            real_len--;
	  
	  tag_attribs[nattribs].attr_name = (char *) DistillerMalloc(real_len+1);
          bogon_strncpy(tag_attribs[nattribs].attr_name, strings[i], real_len);
          next_str_is_attrval = 1;
          next_str_is_attrname = 0;
        } else {
          /* this is a "null attribute" */
          next_str_is_attrname = 1;
        }
        break;
        
      case SGML_NAME:
      case SGML_NMTOKEN:
      case SGML_LITERAL:
        /*
         *  If we are inside parsing an HTML tag, *and*
         *  next_str_is_attrname is true, then this string is the
         *  attribute name of some attribute that doesn't have a value,
         *  like ISMAP.  THe corresponding attribute value should be set
         *  to the null string.
         */
        if (next_str_is_attrname) {
          next_str_is_attrname = 0;
	  tag_attribs[nattribs].attr_name = (char *) DistillerMalloc(lengths[i]+1);
          bogon_strncpy(tag_attribs[nattribs].attr_name, strings[i], lengths[i]);
	  tag_attribs[nattribs].attr_val = (char *) DistillerMalloc(1);
	  tag_attribs[nattribs].attr_origval = (char *) DistillerMalloc(1);
          tag_attribs[nattribs].attr_val[0] = '\0';
          tag_attribs[nattribs].attr_origval[0] = '\0';
          nattribs++;
        } else {
          /*
           *  Otherwise, just copy this stuff to the output.
           */
          append_string(di, strings[i], lengths[i]);
        }
        break;
      case SGML_WARNING:
      case SGML_ERROR:
      case SGML_LIMITATION:
        {
          char error[1024];

	  append_string(di, "LAME", -1);
          sprintf(error, "Warning/error: %*.*s\n", (int)lengths[i],
                  (int)lengths[i], strings[i]);
          MonitorClientSend(mon, "Errors", error, "");
        }
        sgml_error = 1;
        break;
      case SGML_TAGC:
        /*
         *  We've reached a tag close, so process the entire tag at once.
         */
        if (in_html_tag) {
	  process_tag(tagname, tag_attribs, nattribs, di, args, nargs, 
		      &where_string);
        } else {
          append_string(di, strings[i], lengths[i]);
        }
        in_html_tag = 0;
        break;
      case SGML_START:
      case SGML_END:
        /*
         *  Open tag (start tag or end tag, i.e. "<A" or "</A"): copy verbatim
         *  to  tagname.
         */
	tagname = (char *) DistillerMalloc (lengths[i]+1);
        bogon_strncpy(tagname, strings[i], lengths[i]);
        in_html_tag = 1;
        nattribs = 0;
        break;
      case SGML_COMMENT:
	/*	append_string(di, "TEST", -1);*/
      case SGML_DATA:
      case SGML_GEREF:
      case SGML_REFC:
      case SGML_MARKUP_DECL:
        append_string(di, strings[i], lengths[i]);
        break;
      default:
#if 0
        fprintf(stderr, "Type=%d str=%*.*s\n", (int)types[i],
                lengths[i],lengths[i],strings[i]);
#endif
        break;
      }
    }
  }
  clean_HtmlTagDescr(tag_attribs, qty);
  DistillerFree (tag_attribs);
  if (tagname) {
    DistillerFree (tagname);
  }
}

void clean_HtmlTagDescr(struct HtmlTagDescr *td, int qty) {
  int i;
  
  for (i = 0; i < qty; i++) {
    if (td[i].attr_name) {
      DistillerFree (td[i].attr_name); 
    }
    if (td[i].attr_val) {
      DistillerFree (td[i].attr_val); 
    }
    if (td[i].attr_origval) {
      DistillerFree (td[i].attr_origval); 
    }
  }
}

