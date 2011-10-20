/*
 *  munge.h
 *
 *  Compile-time definitions that control various aspects of the HTML munger.
 */

#ifndef _MUNGE_H
#define _MUNGE_H

#include "config_tr.h"
#include "distinterface.h"

typedef struct {
  char *html_buf;
  UINT32 html_buflen;
  UINT32 html_bufsize;
} OutputInfo;

/*
 *  Compile time maxima for HTML constructs
 */

struct HtmlTagDescr {
  char *attr_name;
  char *attr_val;
  char *attr_origval;
};

#define MAX_TAG_ATTRS 20

/*
 *  Arg ID's used by HTML munger
 */

#define HTMLMUNGE_ARG_BASE  27000

/*
 *  Function protos in support file munge_magic.c
 */

char *bogon_strncpy(char *dst, const char *src, size_t n);
void append_string(OutputInfo *di, const char *str, int len);
const char *make_link(const char *linktext, const char *linktarget);

/* calling this with a NULL tagname is an initilization call */
void process_tag(char *tagname, struct HtmlTagDescr attribs[],
                 int nattribs, OutputInfo *out, Argument *args, int nargs,
		 int *where_string);

void handle_form_field(char *tagname, struct HtmlTagDescr attribs[],
                 int nattribs, OutputInfo *out, Argument *args, int nargs);


enum icon_string_pos {
  no_icon,
  at_beginning,
  after_head,
  in_body,
  in_noframes
};

static char *ICON_STRING = "<A href=\"http://transend.CS.Berkeley.EDU/prefs.html\"><IMG ALIGN=\"right\" BORDER=0 SRC=\"http://transend.CS.Berkeley.EDU/logo2.gif*^i1=1^\" WIDTH=\"45\" HEIGHT=\"25\"></A><P><BR CLEAR=\"BOTH\">";

#endif /* ifndef _MUNGE_H */

