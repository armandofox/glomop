/*
 * Arguments for html_munger
 *
 * HTML_NODIST_LINKTOIMAGE - WHAT A LAME NAME.  if true this means that a 
 * link to an image only (such as an <A href="foo.jpg> FOO </A>) should 
 * me magified so the href gets magified to turn of distillation.
 */

#ifndef HTML_SIG

#define HTML_SIG 5000
#define HTML_NODIST_LINKTOIMAGE HTML_SIG + 1 /* int */

#endif
