#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#include "config_tr.h"
#include "optdb.h"
#include "monitor.h"
#include "utils.h"
#include "tcp_redirect.h"
#include "connections.h"
#include "proxies.h"

/* Local (private) functions */
static void init(int ac, char *av[]);
static void usage(void);
static void missing_field_error(const char *fld);
static int  build_fe_list(const char *frontends);

/* Ickky nastie global variables */
static Options         runtime_options;
MonitorClientID        monID;
int                    listen_fd = -1, errno;
fd_set                 read_fds, write_fds, exception_fds;

int main(int ac, char *av[])
{
  char monbuf[2048];

  /* Parse command line options, open up a connection to the monitor,
     and create a non-blocking accept port. */
  init(ac,av);

  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&exception_fds);

  /* Add the accept fd to the sets */
  FD_SET(listen_fd, &read_fds);
/*  FD_SET(listen_fd, &exception_fds); */

  /* Main event loop! */
  while(1) {
    fd_set rfd, wfd, efd;
    int    res, i;

    /* Copy in descriptor sets */
    rfd = read_fds;
    wfd = write_fds;
    efd = exception_fds;

    /* Do select */
    res = select(FD_SETSIZE, &rfd, &wfd, &efd, NULL);

    if (res == -1) {
      sprintf(monbuf, "select error: %s\n", strerror(errno));
      MonitorClientSend(monID, "Messages", monbuf, "Log");
    } else if (res > 0) {
      
      /* Incoming connections? */
      handle_incoming(listen_fd, &rfd, &efd);

      /* Deal with others - check to see if local and global are both
         set, as functions may close sockets and mark it off on global */
      for (i=0; i<FD_SETSIZE; i++) {
	if (FD_ISSET(i, &rfd) && FD_ISSET(i, &read_fds)) {
	  conn_read(i);
	}
	if (FD_ISSET(i, &wfd) && FD_ISSET(i, &write_fds)) {
	  conn_write(i);
	}
/*	if (FD_ISSET(i, &efd) && FD_ISSET(i, &exception_fds)) {
	  conn_except(i);
	}*/
      }
    }
    try_enable();
  }
  
  exit(0);
}


/* Initialize all of the state of tcp_redirect */
static void init(int ac, char *av[])
{
  char *portptr;
  const char *monaddr, *frontends;
  char monaddr_copy[256];
  int result;
  
  if (ac < 2)
    usage();
  
  if ((result = Options_New(&runtime_options, av[1])) != 0) {
    fprintf(stderr, "Error %d opening configuration file '%s', bye bye!\n",
            result, av[1]);
    exit(1);
  }

  /*
   *  Initialize monitor client if possible
   */

  if ((monaddr = Options_Find(runtime_options, "Monitor.listen"))
      == NULL) {
    missing_field_error("Monitor.listen");
    /* NOTREACHED */
  }
  strncpy(monaddr_copy, monaddr, sizeof(monaddr_copy)-1);
  if ((portptr = strchr(monaddr_copy, '/')) == NULL) {
    usage();
    /* NOTREACHED */
  }
  *portptr = 0;

   /* BUG:: should parse out TTL, not assume it's 16! */
   if ((monID = MonitorClientCreate("TCP Multiplexor",
                                    monaddr_copy, /* monitor IP addr */
                                    strtoul(portptr+1, NULL, 0), /*  port */
                                    16)) /* ttl */
       == NULL) {
     fprintf(stderr, "Warning: couldn't create monitor client\n");
   } 

   refresh_listen_port();

  /* Build the list of known front ends */
  MonitorClientSend(monID, "Messages", "Initializing tcp_redirect\n", "Log");
  if ((frontends = Options_Find(runtime_options, "tcp_redirect.fe_hosts"))
      == NULL) {
    missing_field_error("tcp_redirect.fe_hosts");
    /* NOTREACHED */
  }
  build_fe_list(frontends);

  /* All done! */
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: tcp_redirect configfilename\n");
    exit(1);
}

static void missing_field_error(const char *fld)
{
  fprintf(stderr,
          "Reading config file: property '%s' missing or illegal value\n",
	  fld);
  exit(1);
}

static int  build_fe_list(const char *frontends)
{
  char       *next_tok;
  char       *frontends_bup;
  int         num = 0;
  char        mon_msg[2048];

  frontends_bup = (char *) calloc(sizeof(char), strlen(frontends)+1);
  if (frontends_bup == NULL) {
    fprintf(stderr, "Out of memory in build_fe_list.\n");
    exit(1);
  }
  strcpy(frontends_bup, frontends);
  next_tok = strtok(frontends_bup, " ");

  while (next_tok != NULL) {
    char *colon;

    colon = strrchr(next_tok, (int) ':');
    if ((colon == NULL) || (colon - next_tok >= strlen(next_tok)-1)) { 
      sprintf(mon_msg, "Bogus FE entry in config file %s - ignoring\n",
	      next_tok);
      fprintf(stderr, mon_msg);
      MonitorClientSend(monID, "Messages", mon_msg, "Log");
      next_tok = strtok(NULL, " ");
      continue;
    }
    *colon = '\0';
    colon++;
    add_proxy(next_tok, colon);
    next_tok = strtok(NULL, " ");
    num++;
  }

  free(frontends_bup);
  sprintf(mon_msg, "Found %d hosts in total.\n", num);
  MonitorClientSend(monID, "Messages", mon_msg, "Log");
  return num;
}

void refresh_listen_port(void)
{
  char *lport;

  if (listen_fd != -1) {
    close(listen_fd);
    listen_fd = -1;
  }

   lport = (char *) Options_Find(runtime_options, "tcp_redirect.listen_port");
   if (lport == NULL) {
     missing_field_error("tcp_redirect.listen_port");
     /* NOTREACHED */
   }

  /* Create the accept/listen port */
  listen_fd = slisten(lport);
  if (listen_fd == -1) {
    fprintf(stderr, "Couldn't open listening port.\n");
    exit(1);
  }
  if (make_nonblocking(listen_fd) != 1) {
    fprintf(stderr, "Couldn't make listen port non-blocking.\n");
    exit(1);
  }
}
