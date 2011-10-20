/*
 *  accept_upref.c
 *  Set up a separate TCP socket for receiving client
 *  prefs requests.  Note that the request may arrive via the proxy's
 *  main socket, or directly if the user has configured "no proxies for"
 *  to point to the user prefs address (the preferred setup).  This
 *  runs as a single thread that is created at initialization time and
 *  repeatedly handles requests from the socket.
 *
 *   Actual parsing of the prefs data is handled in file parse_upref.c.
 */

#include "userpref.h"
#include "config_tr.h"
#include "ALLARGS.h"
#include "utils.h"
#include <string.h>
#include <errno.h>

extern Monitor *gMon;

static int upref_sock;
static pthread_t thr_parse_upref;
static ThrFunc accept_upref_request;

gm_Bool
init_parse_uprefs(UINT16 port)
{
  char portnum[8];
  int i;

  sprintf(portnum, "%d", port);
  if ((upref_sock = slisten(portnum)) < 1) {
    char error[200];
    MON_SEND_3(MON_ERR,"Listen on %d failed: %s\n", (int)port, strerror(errno));
    return gm_False;
  }

  THR_OP("Thread create listen for prefs",
         pthread_create(&thr_parse_upref, (pthread_attr_t *)NULL,
                        accept_upref_request, (void *)NULL));

  MON_SEND_2(MON_LOGGING,"Listening for prefs changes on port %d\n", port);
  return gm_True;
}

/*
 *  accept_upref_request
 *  Waits in a loop accepting user pref requests on socket; tries to
 *  process request, if impossible, returns an error through http_error
 *  function.  If request looks good, tries to change the prefs database
 *  to reflect new prefs.
 */

static int newconn = 0;

static void *
accept_upref_request(void *arg)
{
  MON_SEND(MON_LOGGING,"Accepting user prefs changes");
  while (1) {
    newconn = saccept(upref_sock);
    
    if (newconn < 0) {
      MON_SEND_2(MON_ERR,"Accepting new connection: %s", strerror(errno));
      continue;
    }

    (void)parse_prefs_request(newconn);
    shutdown(newconn, 2);
    close(newconn);
  }

}

/*
 *  Collecting incoming prefs request from open file descriptor, parse
 *  them, and call userpref routines to do the work.  It should not
 *  close or shutdown the file descriptor - that is done by the caller.
 *  
 *
 *  ARGS: file descriptor of open socket
 *  REENTRANT: No - only one thread is doing userprefs on a single
 *  socket
 *  RETURNS: true/false (success/failure);
 *  SIDE EFFECTS: calls http_error_return to return a result to the
 *  client
 */
gm_Bool
parse_prefs_request(int sock)
{
  Request hd;
  Request *hp = &hd;
  size_t headerlen = sizeof(HeaderData);
  ts_strtok_state *ts_st;
  char *method;
  char *tmp;
  gm_Bool result = gm_False;
  
  hp->len = readline_or_timeout(hp, READ_ALL_HEADERS, NULL);

  if (hp->len == -1) {
    /* request timed out */
    goto PARSE_PREFS_RETURN;
    /* NOTREACHED */
  }

  ts_st = ts_strtok_init(hp->header_data);

  /*
   *  Currently, user prefs are delivered via the "GET" method and an
   *  HTML form.  Form data is parsed out of the URL, which has the form
   *  "name=val&name=val&...."
   */
  if (((method = ts_strtok(" ",ts_st)) == NULL)
      || (strncasecmp(method, "get", (size_t)3) != 0)) {
    /* no tokens on line! */
    http_error_return(sock, HTTP_VERSION_UNKNOWN,
                      HTTP_ERR_MALFORMED_REQUEST, hp->header_data);
    ts_strtok_finish(ts_st);
    goto PARSE_PREFS_RETURN;
    /* NOTREACHED */
  }
  if ((hp->url = ts_strtok(" ", ts_st)) == NULL) {
    /* No URL! */
    http_error_return(sock, HTTP_VERSION_UNKNOWN,
                      HTTP_ERR_MALFORMED_REQUEST, hp->header_data);
    ts_strtok_finish(ts_st);
    goto PARSE_PREFS_RETURN;
    /* NOTREACHED */
  }
  /* note, we don't really care about the HTTP version, but we need to know
     where the URL terminates, so.... */

  if ((tmp = ts_strtok(" ", ts_st)) != NULL) {
    *(tmp-1) = '\0';            /* terminator for hp->url */
  }
  /* now analyze the URL.  */

  result = parse_and_change_prefs(hp->url, sock);
  ts_strtok_finish(ts_st);

PARSE_PREFS_RETURN:
  return result;
}

/*
 *  upref_shutdown
 *  Close socket in an orderly way when exiting.
 */

void
upref_shutdown(void)
{
  shutdown(newconn, 2);
  close(newconn);
  shutdown(upref_sock, 2);
  close(upref_sock);
}
