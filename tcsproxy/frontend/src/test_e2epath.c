/*
 *  httpgo.c
 *  The code to handle and dispatch an incoming HTTP request is here.
 *  This file defines a function called http_go_proc that is called when
 *  a thread grabs a task off the queue.  The argument is the task structure.
 */

#include <config_tr.h>
#include <frontend.h>
#include <proxyinterface.h>
#include <unistd.h>
#include <stdio.h>

/*
 *  http_go_proc: thread entry point to handle an incoming HTTP request 
 *  The request will be parsed, validated, and handed off to a remote
 *  distiller (or else rejected).  This is a long blocking operation.
 *
 *  ARGS: the task_t structure; its task_data field contains the file
 *      number of the socket on which the request is arriving.
 *  RETURNS: a void* which is ignored; errors are reported directly on
 *      socket using HTTP error protocol (500 Error, etc)
 *  REENTRANT: yes
 *  
 */

void *
http_go_proc(task_t *t)
{
  int sock = (int)(TASK_DATA(t));
  C_DistillerType type;
  Argument ar;
  userpref u;
  userkey uk;
  FILE *htmlfile;
  char filename[80];
  size_t inbufsize;
  UINT32 outbufsize;
  DistillerStatus status;
  ssize_t filenamelen;
  char inbuf[64000];
  char *outbuf;

  
  SET_DISTILLER_TYPE(type, "text/html");
  SET_ARG_ID(ar, 1);
  SET_ARG_STRING(ar, "foobar");

  /* read filename from the socket */
  filenamelen = 0;
  while (filenamelen < 80) {
    filenamelen += read(sock, (void *)(filename+filenamelen), 1);
    if ((filename[filenamelen-1] == '\n')
        || (filename[filenamelen-1] == '\r'))
      break;
  }
  filename[filenamelen-1] = '\0';
  proxy_debug_3(DBG_HTTP, "Filename is <%s>", filename);
  assert(htmlfile = fopen(filename, "r"));
  inbufsize = fread((void *)inbuf, sizeof(char), (size_t)64000, htmlfile);
  fclose(htmlfile);
  /* call html distiller */

  status = Distill(&type, &ar, 1, (void *)inbuf, (UINT32)inbufsize,
                   (void *)&outbuf, &outbufsize);

  proxy_debug_3(DBG_HTTP, "Distiller status = %d\n", (int)status);

  /* dump outbuf to a file */

  if (status == distOk) {
    assert(htmlfile = fopen("/tmp/foo", "w"));
    fwrite((void *)outbuf, sizeof(char), (size_t)outbufsize, htmlfile);
    fclose(htmlfile);
  }

  /* get user preferences of this user, if possible. */

#if 0
  uk = userkey_from_sock_ipaddr(sock);
  u = get_userprefs(uk);

  fprintf(stderr, "User prefs bpp=%d\n", (int)(u->bpp));
#endif

  return (void *)0;
}
