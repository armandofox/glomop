/*
 *  munge_magic.c
 *
 *  Support for HTML munger to insert magic into/remove magic from URL's
 *  as it's munging HTML. 
 */

#include "munge.h"
#include "url_magic.h"
#include "distinterface.h"
#include "../../frontend/include/ARGS.h"
#include <ctype.h>
#include <stdio.h>

/*#define DistillerRealloc(x,y) realloc(x,y)*/

char * check_size(int, int *, char *);
static int is_http_get(char *url);

/*
 *  returns 1 if the url in question is a partial url or if it is http://
 */
int is_http_get(char *url) {
  if (strstr(url, "://")) {
    if (!strncasecmp(url, "http://", 6)) {return 1;}
    else {return 0;}
  }
  return 1;
}

/*
 *  Return the index for a given tag in the attribs array, or -1 if
 *  the attribute isn't specified. 
 */

static int attrib_index(char *attrib_name, struct HtmlTagDescr attribs[],
                        int nattribs)

{
  int i;

  for (i=0; i<nattribs; i++) {
    if (strcasecmp(attrib_name, attribs[i].attr_name) == 0) {
      return i;
    }
  }
  return -1;
}

/*
 * Process a <INPUT declaration.  Recieves arguments just like proccess_tag.
 * Basically a sub function of process_tag made to handle the special cases
 * of form inputs used in the changeprefs page.  It looks for the keywords
 * TRANSENDID and TRANSENDVAL.  If you have a button and if you have an
 * arguement of type TRANSENDID with value TRANSENDVAL then make it checked.
 * If you have text box of value TRANSENDID then add a VALUE="x" tag where
 * x equals the value of the argument TRANSENDID.
 */

void
handle_form_field(char *tagname, struct HtmlTagDescr attribs[],
            int nattribs, OutputInfo *di, Argument *args, int nargs)
{
  /*
   * dump form information and at the same time scan for TRANSENDVAL
   * TRANSENDID tags.
   */ 
  
  char *val = NULL;
  char *id = NULL;
  char *tmp;
  char type;
  int argid;

  int i, ivalue;
  double dvalue;
  int button = 0;
  int tmp_size=100;

  append_string(di, tagname, -1);

  tmp = DistillerMalloc(tmp_size);
  for (i = 0; i<nattribs; i++) {
    /* dump the info */
    if (attribs[i].attr_val[0] != '\0') {
      tmp = check_size ((strlen(attribs[i].attr_name) + 
			 strlen(attribs[i].attr_val)) + 5, &tmp_size, tmp);
      if (attribs[i].attr_val[0] != '\'') {
	sprintf(tmp, "%s%s=\"%s\"", (i==0 ? "" : " "),
		attribs[i].attr_name, attribs[i].attr_val);
      }
      else {
	sprintf(tmp, "%s%s=%s", (i==0 ? "" : " "),
		attribs[i].attr_name, attribs[i].attr_val);
      }
    } else {
      tmp = check_size (strlen(attribs[i].attr_name) + 7, &tmp_size, tmp);
      sprintf(tmp, "%s%s", (i==0 ? "" : " "),
	      attribs[i].attr_name);
    }
    append_string(di, tmp, -1);

    /* do we have a TYPE tag?  If so see if it is a button or not */
    if ((strlen (attribs[i].attr_name) >= 4) && 
	!strcasecmp(attribs[i].attr_name, "TYPE")) {
       button = strcasecmp(attribs[i].attr_val, "text");
    } else if ((strlen (attribs[i].attr_name) >= 11) && 
	    !strcasecmp(attribs[i].attr_name, "TRANSENDVAL")) {
      /* or do we have a TRANSENDVAL tag? */
      if  (attribs[i].attr_val[0] != '\0') {
	val = attribs[i].attr_val;
      }
      
    } else if ((strlen (attribs[i].attr_name) >= 10) && 
	    !strcasecmp(attribs[i].attr_name, "TRANSENDID")) {
      /*   or do we have a TRANSENDID tag?  */
      /* (needs to be at least 2 chars long) */
      if (strlen(attribs[i].attr_val) >= 2) {
	id = attribs[i].attr_val;
      }
    }
  }

  /* 
   * now if we found TRANSENDID tags, put in the CHECKED or VALUE 
   * fields 
   *
   */
  if (id) {

    /* find out if we have the arguement in question */
    
    type = id[0];
    argid = atoi(id+1);

    for (i = 0; i < nargs; i++) {
      if (ARG_ID(args[i]) == argid) {
	/* if we have that argument */
	append_string(di, &type, 1);
	if (!button) {
	  /* if we have an ID tag and we are a text form */
	  append_string(di, " VALUE=\"", 8);
	  switch (type) {
	  case 'i':
	    sprintf(tmp, "%ld", ARG_INT(args[i]));
	    break;
	  case 's':
	    sprintf(tmp, "%s", ARG_STRING(args[i]));
	    break;
	  case 'd':
	    sprintf(tmp, "%f", ARG_DOUBLE(args[i]));
	    break;
	  default:
	    tmp[0]=0;
	    break;
	  }
	  append_string(di, tmp, -1);
	  append_string(di, "\"", 1);
	} else if (val) {
	  /* if we have an ID tag and a val tag and are a button */
	  switch(type) {
	  case 'i':

	    ivalue = atoi(val);
	    if (ivalue == ARG_INT(args[i])) {
	      append_string(di, " CHECKED", 8);
	    }
	    break;
	  case 's':
	    if (strlen(ARG_STRING(args[i])) == strlen(val) && 
		 !strcmp(ARG_STRING(args[i]), val)) {
	      append_string(di, " CHECKED", 8);	      
	    }
	    break;
	  case 'd':
	    sscanf(val, "%lf", &dvalue);
	    if (dvalue == ARG_DOUBLE(args[i])) {
	      append_string(di, " CHECKED", 8);
	    }
	    break;
	  } 
	}
	/* handled, don't keep doing it */
	break;	
      }
    }
  }
  append_string(di, ">", 1);    /* tag close! */
  DistillerFree(tmp);
}
      
/*
 *  Process a tag declaration.  We receive the tag name (actually includes
 *  the opening "<" or "</"), an array of  HtmlTagDescr with the attribute
 *  names, values, and original verbatim values already parsed, and the
 *  distiller output buffer info so we can write the tag to the output
 *  the opening "<" or "</"), an array of  HtmlTagDescr with the attribute
 *  names, (possibly having modified it, etc.)
 *
 * If you send it a tagname of NULL then it treats that as an init call
 * telling it a new apge was started.  (damn state)
 *
 * The calls to checksize tend to add a semi-arbitrary number to the 
 * actual size needed.  (For instance... 
 *  tmp = check_size((strlen (attribs[i].attr_name) + 
 *     strlen(attribs[i].attr_val) + 10), &tmp_size, tmp);
 *  is done before 
 *  sprintf(tmp, "%s%s=\"%s\"", (i==0 ? "" : " "),
 *     attribs[i].attr_name, attribs[i].attr_val);
 *  This was basically done so I could change one or two characters 
 *  and everything would work.  In one case it could be a problem
 *  
 */

void
process_tag(char *tagname, struct HtmlTagDescr attribs[],
            int nattribs, OutputInfo *di, Argument *args, int nargs,
	    int *where_string)
{
  int i;
  static int in_link = 0;
  static int queue_size = 0, queue_max_size = 0;
  ArgumentList al;
  char *tmp;
  char *src_url = NULL;
  int tmp_size = MAX_URL_SIZE;
  static char **image_queue;
  int is_map = 0;
  char *magic_src_url;
  int s;
  
#ifdef INST
  extern FILE *Log;
#endif
  
  if (tagname == NULL) {
    in_link = 0;
    return;
  }

  if ((strncasecmp (tagname, "<INPUT", 2) == 0) && 
      (strlen(tagname) >= 6) && (!isalpha(tagname[6]))) {
    handle_form_field(tagname, attribs, nattribs, di, args, nargs);
    return;
  }

  tmp = (char *) DistillerMalloc(tmp_size);
  if ((strncasecmp (tagname, "<A", 2) == 0) && 
      (strlen(tagname) >= 2) && (!isalpha(tagname[2]))) {
    for (i=0; i <  nattribs; i++) {
      if (!strcasecmp (attribs[i].attr_name, "HREF")) {
	in_link = 1;
      }
    }
  }
  if ((strncasecmp (tagname, "</A", 3) == 0) && 
      (strlen(tagname) >= 3) && (!isalpha(tagname[3]))) {
    in_link = 0;
  }
  s = attrib_index("src",attribs,nattribs);
  if ((strncasecmp (tagname, "<IMG", 4) == 0) &&
      (strlen(tagname) >= 4) && !isalpha(tagname[4]) && 
      is_http_get(attribs[s].attr_val)) {
    
#ifdef INST
    int q = attrib_index("src",attribs,nattribs);
    if (q != -1) {
      fprintf(Log, "%s ", attribs[q].attr_val);
    }
#endif

    /* magify the src tag if it exists */
    if ((s != -1) && is_http_get(attribs[s].attr_val)) {
      /* are we an image map of some sort? */
      if ((attrib_index("ismap",attribs,nattribs) != -1) || 
	  (attrib_index("usemap",attribs,nattribs) != -1) ) {
	
	is_map = 1;
	magic_src_url = DistillerMalloc
	  (strlen(attribs[s].attr_val) + 50);
	al.nargs = 1;
	SET_ARG_ID(al.arg[0], FRONT_ISMAP);
	SET_ARG_INT(al.arg[0], 1);
	to_magic(magic_src_url, attribs[s].attr_val, &al);
	DistillerFree(attribs[s].attr_val);
	attribs[s].attr_val = magic_src_url;
      }
      
      /* do we not have width or height attributes? */
      if ( (attrib_index("width",attribs,nattribs) == -1) &&
	   (attrib_index("height",attribs,nattribs) == -1) ) {
	magic_src_url = DistillerMalloc
	  (strlen(attribs[s].attr_val) + 50);
	al.nargs = 1;
	SET_ARG_ID(al.arg[0], FRONT_RESIZE);
	SET_ARG_INT(al.arg[0], 1);
	to_magic(magic_src_url, attribs[s].attr_val, &al);
	DistillerFree(attribs[s].attr_val);
	attribs[s].attr_val = magic_src_url;
 
      }
    }

    if (in_link) {
      queue_size++;
      if (queue_size > queue_max_size) {
	image_queue = (char **) DistillerRealloc(image_queue, 
						queue_size * sizeof (char *));
	image_queue[queue_size-1] = NULL;
	queue_max_size = queue_size;
      }

      append_string(di, tagname, -1);
      for (i = 0; i<nattribs; i++) {
	if (attribs[i].attr_val[0] != '\0') {
	  tmp = check_size((strlen (attribs[i].attr_name) + 
		      strlen(attribs[i].attr_val) + 10), &tmp_size, tmp);
	  if (attribs[i].attr_val[0] != '\'') {
	    sprintf(tmp, "%s%s=\"%s\"", (i==0 ? "" : " "),
		    attribs[i].attr_name, attribs[i].attr_val);
	  }
	  else {	    
	    sprintf(tmp, "%s%s=%s", (i==0 ? "" : " "),
		    attribs[i].attr_name, attribs[i].attr_val);
	  }
	}
	else {
	  tmp = check_size(strlen(attribs[i].attr_name) + 3, &tmp_size, tmp);
	  sprintf(tmp, "%s%s", (i==0 ? "" : " "),
		  attribs[i].attr_name);
	}
	if (strcasecmp (attribs[i].attr_name, "src") == 0)
	  src_url = attribs[i].attr_val;
	append_string(di, tmp, -1);
      }
      append_string(di, ">", 1); /* tag close */
      if (src_url) {
	/***********************************************************/
	/* Make sure there is enough space in tmp to allow for the */
	/* magic url because there exist 5k urls out there         */
	/* THIS COULD BE A PROBLEM IF MAGIC CHANGES OR SOMETHING   */
	/* 50 CHARACTERS COULD BE TOO LITTLE                       */
	/***********************************************************/
	image_queue[queue_size-1] = 
	  (char *) DistillerMalloc(strlen(src_url) + 50);
	/* get rid of any magic in the url, cleaner that way */
	tmp = check_size(strlen(src_url) + 50, &tmp_size, tmp);
	from_magic(src_url, tmp, &al);
	al.nargs = 1;
	SET_ARG_ID(al.arg[0], FRONT_NO_DISTILL);
	SET_ARG_INT(al.arg[0], 1);	
	to_magic(image_queue[queue_size-1], tmp, &al);	
      }
      else {
	queue_size--;
      }
    } else {
      /* first find the url and put in the <A href=magicurl>*/
      for (i = 0; i<nattribs; i++) {
	if (strcasecmp (attribs[i].attr_name, "src") == 0)
	  src_url = attribs[i].attr_val;
      }  
      if (src_url) {
	char * t;
	tmp = check_size(strlen(src_url)+50, &tmp_size, tmp);
	t = DistillerMalloc(strlen(src_url)+50);
	from_magic(src_url, t, &al);
	al.nargs = 1;
	SET_ARG_ID(al.arg[0], FRONT_NO_DISTILL);
	SET_ARG_INT(al.arg[0], 1);
	to_magic(tmp, t, &al);
	append_string (di, "<A href=\"",9);
	append_string (di, tmp,-1);
	append_string (di, "\">",2);
	append_string(di, tagname, -1);
	append_string(di, " border=0 ", 10);
	DistillerFree(t);
      } else {
	append_string(di, tagname, -1);
      }
      for (i = 0; i<nattribs; i++) {
	if (strcmp (attribs[i].attr_val, "")) {
	  tmp = check_size ((strlen(attribs[i].attr_name) + 
		      strlen(attribs[i].attr_val) + 5), &tmp_size, tmp);
	  if (attribs[i].attr_val[0] != '\'') {
	    sprintf(tmp, "%s%s=\"%s\"", (i==0 ? "" : " "),
		    attribs[i].attr_name, attribs[i].attr_val);
	  }
	  else {
	    sprintf(tmp, "%s%s=%s", (i==0 ? "" : " "),
		    attribs[i].attr_name, attribs[i].attr_val);
	  }
	}
	else {
	  tmp = check_size (strlen(attribs[i].attr_name) + 3, &tmp_size, tmp);
	  sprintf(tmp, "%s%s", (i==0 ? "" : " "),
		  attribs[i].attr_name);
	}
	append_string(di, tmp, -1);
      }
      append_string(di, "></a>", 5);
    }
  } else {
    /* not an image */
    append_string(di, tagname, -1);
    for (i = 0; i<nattribs; i++) {
      if (attribs[i].attr_val[0] != '\0') {
	tmp = check_size ((strlen(attribs[i].attr_name) + 
			   strlen(attribs[i].attr_val)) + 5, &tmp_size, tmp);
	if (attribs[i].attr_val[0] != '\'') {
	  sprintf(tmp, "%s%s=\"%s\"", (i==0 ? "" : " "),
		  attribs[i].attr_name, attribs[i].attr_val);
	}
	else {
	  sprintf(tmp, "%s%s=%s", (i==0 ? "" : " "),
		  attribs[i].attr_name, attribs[i].attr_val);
	}
      } else {
 	tmp = check_size (strlen(attribs[i].attr_name) + 7, &tmp_size, tmp);
	sprintf(tmp, "%s%s", (i==0 ? "" : " "),
		attribs[i].attr_name);
      }
      append_string(di, tmp, -1);
    }
    append_string(di, ">", 1);    /* tag close! */
  }
  if (queue_size && !in_link
      && (attrib_index("norefine", attribs,nattribs) == -1)) {
    for (i = 0; i < queue_size; i++) {
      append_string(di, "<A href=\"", 9);
      append_string(di, image_queue[i], -1);
      append_string(di, "\">&#164;</A>",12);
      DistillerFree(image_queue[i]);
      image_queue[i] = NULL;
    }
    queue_size = 0;
  }
  DistillerFree (tmp);

  /* handle adding the ICON_STRING if needed */
  if ((*where_string == after_head) && !(strncasecmp(tagname, "</HEAD", 6))) {
    *where_string = no_icon;
    append_string(di, ICON_STRING, -1);
  } else if ((*where_string == in_body) && !(strncasecmp(tagname,"<BODY",5))){
    *where_string = no_icon;
    append_string(di, ICON_STRING, -1);
  } else if ((*where_string == in_noframes) && \
	     !(strncasecmp(tagname, "<NOFRAMES", 9))) {
    *where_string = no_icon;
    append_string(di, ICON_STRING, -1);
  }
}


void
append_string(OutputInfo *di, const char *str, int len)
{
  if (len <= 0) {
    len = strlen(str);
  
    if (di->html_buflen + len >= di->html_bufsize) {
      di->html_buf = DistillerRealloc(di->html_buf, 
			     di->html_buflen + (di->html_buflen >> 2));
    }
    strcpy(di->html_buf + di->html_buflen, str);
  }
  else {
    if (di->html_buflen + len >= di->html_bufsize) {
      di->html_buf = DistillerRealloc(di->html_buf, 
			     di->html_buflen + (di->html_buflen >> 2));
    }
    bogon_strncpy(di->html_buf + di->html_buflen, str, len);
  }
  di->html_buflen += len;
}


/* 
 * checks to see if new_size is > buf_size and if so reallocs buf to
 * to new_size and sets buf_size to new_size.  It returns the return of
 * the realooc or the original buf depending on wether it realloc or not
 */
char * check_size(int new_size, int *buf_size, char * buf) {
  if (*buf_size < new_size) {
    *buf_size = new_size;
    return ((char *) DistillerRealloc(buf, new_size));
  } else {
    return buf;
  }
}


/*
 *  Here is a strncpy that correctly adds a terminating null even if the
 *  source string doesn't have one in the right place, provided we know
 *  the length of the source string.
 */

char *
bogon_strncpy(char *dst, const char *src, size_t n)
{
  strncpy(dst, src, n);
  dst[n] = '\0';
  return dst;
}
