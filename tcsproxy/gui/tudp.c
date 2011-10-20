#include "tcl.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define TUDP_MAXPROCNAMELEN 80
#define TUDP_DEFAULT_TTL    8
#define TUDP_MAXMSGSIZE     1024 /* max size of a single message */

typedef struct {
  Tcl_Interp *cb_interp;
  int cb_sock;
  struct sockaddr_in cb_inaddr;

#if TCL_MAJOR_VERSION < 8
  Tcl_File cb_tclsock;
#else
  int cb_tclsock;
#endif

  char cb_procname[TUDP_MAXPROCNAMELEN];
} CB;

/*
 *  Forward prototypes
 */

Tcl_PackageInitProc Tudp_Init;
static int   Tudp_UdpListenCmd  _ANSI_ARGS_((ClientData cd, Tcl_Interp *i,
                                         int argc, char *argv[]));
static int   Tudp_UdpForgetCmd  _ANSI_ARGS_((ClientData cd, Tcl_Interp *i,
                                         int argc, char *argv[]));
static int   Tudp_UdpSendCmd    _ANSI_ARGS_((ClientData cd, Tcl_Interp *i,
                                         int argc, char *argv[]));
static int   Tudp_CreateUdpSocket _ANSI_ARGS_((Tcl_Interp *interp, int port,
                                               CB *cb));
static int   Tudp_CreateMulticastSocket _ANSI_ARGS_((Tcl_Interp *interp, 
						     unsigned long multicast,
						     int port, CB *cb,
                                                     int ttl));

static Tcl_FileProc Tudp_ReceiveDgram;

/*
 *  Tudp_Init adds the udp_listen and udp_cancel commands to the interpreter.
 */

int
Tudp_Init(interp)
     Tcl_Interp *interp;
{

  if (! Tcl_CreateCommand(interp, "udp_listen", 
			Tudp_UdpListenCmd, (ClientData)NULL,
                          (Tcl_CmdDeleteProc *)NULL)) {
    Tcl_AppendResult(interp, "Can't create udp_listen command", (char *)NULL);
    return TCL_ERROR;
  }
  if (! Tcl_CreateCommand(interp, "udp_cancel", Tudp_UdpForgetCmd,
                          (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL)) {
    Tcl_AppendResult(interp, "Can't create udp_cancel command", (char *)NULL);
    return TCL_ERROR;
  }
  if (! Tcl_CreateCommand(interp, "udp_send", Tudp_UdpSendCmd,
                        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL)) {
    Tcl_AppendResult(interp, "Can't create udp_send command", (char *)NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}

void
Tudp_UdpListenUsage(interp, procname, message)
     Tcl_Interp *interp;
     const char *procname;
     const char *message;
{
  Tcl_AppendResult(interp, message, "Usage: ", procname,
		   " -multicast multicast_addr/port_number\n -callback proc",
                   "\n or: ", procname,
                   " -unicast port_number -callback proc\n",
		   (char *) NULL);
}
      

/*
 *  Tudp_UdpListenCmd
 *
 *  This procedure is invoked to handle the "udp_listen" command.  It
 *  expects a UDP port number and the name of a callback function.  It
 *  sets up the event-based callback to Tudp_ReceiveDgram, and sets
 *  interp->result to the filehandle of the socket.
 *
 *  Format: udp_listen -multicast mcast_addr/port[/ttl] -callback proc
 *          udp_listen -unicast port
 */

static int
Tudp_UdpListenCmd(dummy, interp, argc, argv)
     ClientData dummy;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  int port, isMulticast=0;
  int sock;
  CB *callback_info;
  unsigned long ipAddr;
  char *callbackProc;
  int i;
  int ttl = TUDP_DEFAULT_TTL;
  
  port = -1;
  ipAddr = 0xFFFFFFFF;
  callbackProc = NULL;

  for (i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-callback")==0) {
      /*
       *  Consume callback proc name
       */
      if (++i >= argc) {
	Tudp_UdpListenUsage(interp, argv[0], 
			    "No callback procedure specified\n");
	return TCL_ERROR;
      }
      
      if (strlen(argv[i]) > TUDP_MAXPROCNAMELEN-1) {
	Tudp_UdpListenUsage(interp, argv[0], "Callback proc name too long\n");
	return TCL_ERROR;
      }

      callbackProc = argv[i];
      continue;
    }

    
    if (strcmp(argv[i], "-address") == 0) {
      /*
       * consume -address plus address/port[/ttl]
       */

      char *port_p, *ttl_p;
    
      if (ipAddr!=0xFFFFFFFF) {
        /* address has already been specified! */
	Tudp_UdpListenUsage(interp,argv[0],
			    "Duplicate network address option specified\n");
	return TCL_ERROR;
      }

      /* Check for address/port number.  */
      
      if (++i == argc) {        /* this is the last arg */
        Tudp_UdpListenUsage(interp,argv[0], "No address/port specified\n");
	return TCL_ERROR;
      }

      /* consume address/port specification */
      if ((port_p = strchr(argv[i], '/'))==NULL) {
        Tudp_UdpListenUsage(interp,argv[0],
                            "Must specify both address and port number\n");
        return TCL_ERROR;
      }
      *port_p++ = '\0';

      if ((ttl_p = strchr(port_p, '/')) != NULL) {
        *ttl_p++ = '\0';
        if (Tcl_GetInt(interp, ttl_p, &ttl) != TCL_OK 
            || ttl < 0 || ttl > 255) {
          Tudp_UdpListenUsage(interp, argv[0],
                              "Invalid TTL (must be between 0 and 255)");
          return TCL_ERROR;
        }
      }
      
      ipAddr = inet_addr(argv[i]);

      /*
       * ipAddr may be a valid multicast addr, a valid unicast addr or 
       * a hostname
       */

      if ((Tcl_GetInt(interp, port_p, &port) != TCL_OK)
          || port <= 0 || port > 65535) {
        Tudp_UdpListenUsage(interp,argv[0],"Invalid port number ",
                            "(must be between 1 and 65535)\n");
        return TCL_ERROR;
      }

      if (ipAddr==0xFFFFFFFF || IN_CLASSD(ntohl(ipAddr))==0) {
	/* this is a unicast address */
	isMulticast = 0;
      }
      else {
	/* this is a multicast address */
	isMulticast = 1;
      }
      continue;
    }

#if 0
    if (strcmp(argv[i], "-multicast") == 0) {
      char *port_p, *ttl_p;
      /*
       *  consume -multicast plus address/port.
       */
      isMulticast = 1;
    
      if (ipAddr!=0xFFFFFFFF) {
        /* address has already been specified! */
	Tudp_UdpListenUsage(interp,argv[0],
			    "Duplicate network address option specified\n");
	return TCL_ERROR;
      }

      /* Check for address/port number.  */
      
      if (++i == argc) {        /* this is the last arg */
        Tudp_UdpListenUsage(interp,argv[0], "No address/port specified\n");
	return TCL_ERROR;
      }

      /* consume address/port specification */
      if ((port_p = strchr(argv[i], '/'))==NULL) {
        Tudp_UdpListenUsage(interp,argv[0],
                            "Must specify both address and port number\n");
        return TCL_ERROR;
      }
      *port_p++ = '\0';

      if ((ttl_p = strchr(port_p, '/')) != NULL) {
        *ttl_p++ = '\0';
        if (Tcl_GetInt(interp, ttl_p, &ttl) != TCL_OK 
            || ttl < 0 || ttl > 255) {
          Tudp_UdpListenUsage(interp, argv[0],
                              "Invalid TTL (must be between 0 and 255)");
          return TCL_ERROR;
        }
      }
      
      ipAddr = inet_addr(argv[i]);
      if (ipAddr==0xFFFFFFFF) {
        Tudp_UdpListenUsage(interp, argv[0], "Invalid address\n");
        return TCL_ERROR;
      }
      if ((Tcl_GetInt(interp, port_p, &port) != TCL_OK)
          || port <= 0 || port > 65535) {
        Tudp_UdpListenUsage(interp,argv[0],"Invalid port number ",
                            "(must be between 1 and 65535)\n");
        return TCL_ERROR;
      }
      continue;
    }

    if (strcmp(argv[i], "-unicast") == 0) {

      char *port_p;

      ++i;
      port_p = strchr(argv[i], '/');
      if (port_p != 0)
        port_p++;
      else
        port_p = argv[i];

      isMulticast = 0;
      
      if ((Tcl_GetInt(interp, port_p, &port) != TCL_OK)
          || port<=0 || port > 65535) {
        Tudp_UdpListenUsage(interp,argv[0],"Invalid port number ",
                            "(must be between 1 and 65535)\n");
        return TCL_ERROR;
      }
      continue;
    }
#endif
    
    /* here if no argument keyword was matched */

    Tcl_AppendResult(interp, "Incorrect argument: ", argv[i], "\n", 
                     (char *)NULL);
    Tudp_UdpListenUsage(interp, argv[0], "");
    return TCL_ERROR;
  }

  /* sanity check arguments */
  if (port==-1 || callbackProc==NULL) {
    Tudp_UdpListenUsage(interp, argv[0], "Incorrect number of arguments\n");
    return TCL_ERROR;
  }

  assert(callback_info = (CB *)ckalloc(sizeof(CB)));

  if (isMulticast==0) {
    /* we are using unicast UDP */
    sock = Tudp_CreateUdpSocket(interp, port, callback_info);
  } else {
    sock = Tudp_CreateMulticastSocket(interp, ipAddr, port, callback_info, ttl);
  }

  if (sock==-1) return TCL_ERROR;
  
  /*
   *  now register the Tcl callback
   */
  strcpy(callback_info->cb_procname, callbackProc);

#if TCL_MAJOR_VERSION < 8
  callback_info->cb_tclsock = Tcl_GetFile((ClientData)sock, TCL_UNIX_FD);
  if (callback_info->cb_tclsock == (Tcl_File)NULL) {
    Tcl_AppendResult(interp, "Tcl_GetFile call failed for socket", 
		     (char *)NULL);
    return TCL_ERROR;
  }
#else
  callback_info->cb_tclsock = sock;
#endif

  callback_info->cb_interp = interp;
  callback_info->cb_sock = sock;
  Tcl_CreateFileHandler(callback_info->cb_tclsock, TCL_READABLE|TCL_EXCEPTION,
                        Tudp_ReceiveDgram, (ClientData)callback_info);
  sprintf(interp->result, "%lu", (unsigned long)callback_info);
  return TCL_OK;
}

static int
Tudp_UdpSendCmd(cdata, interp, argc, argv)
     ClientData cdata;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  CB *cb;
  
  if (argc != 3) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                     " callback_cookie string\"", (char *)NULL);
    return TCL_ERROR;
  }

  cb = (CB *)strtoul(argv[1], (char **)NULL, 0);

  if (sendto(cb->cb_sock, (const char *)argv[2], strlen(argv[2]),
             0, (const struct sockaddr *)&(cb->cb_inaddr),
             sizeof(struct sockaddr_in))
      == -1) {
    Tcl_AppendResult(interp, argv[0], ": ", strerror(errno), (char *)NULL);
    return TCL_ERROR;
  } else {
    return TCL_OK;
  }
}


static int
Tudp_UdpForgetCmd(dummy, interp, argc, argv)
     ClientData dummy;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  CB *cb;
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                     " callback_cookie\"", (char *) NULL);
    return TCL_ERROR;
  }

  cb = (CB *)strtoul(argv[1], (char **)NULL, 0);
  close(cb->cb_sock);
  Tcl_DeleteFileHandler(cb->cb_tclsock);
#if TCL_MAJOR_VERSION < 8
  Tcl_FreeFile(cb->cb_tclsock);
#endif
  ckfree((char *)cb);
  return TCL_OK;
}



/*
 * This function creates a (unicast) UDP socket listening on "port"
 * Returns the socket id on success; -1 on failure
 */
static int
Tudp_CreateUdpSocket(interp, port, cb)
     Tcl_Interp *interp;
     int port;
     CB *cb;
{

  if ((cb->cb_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    Tcl_AppendResult(interp, "socket: ", strerror(errno), (char *)NULL);
    return -1;
  }

  cb->cb_inaddr.sin_addr.s_addr = INADDR_ANY;
  cb->cb_inaddr.sin_family = AF_INET;
  cb->cb_inaddr.sin_port = htons(port);
  if (bind(cb->cb_sock, (struct sockaddr *)&(cb->cb_inaddr),
           sizeof(struct sockaddr_in)) < 0) {
    close(cb->cb_sock);
    Tcl_AppendResult(interp, "bind: ", strerror(errno), (char *)NULL);
    return -1;
  }
  return cb->cb_sock;
}


/*
 * This function creates a multicast socket listening on "multicastAddr/port"
 * Returns the socket descriptor on success; -1 on failure
 */
static int
Tudp_CreateMulticastSocket(interp, multicastAddr, port, cb, ttl)
     Tcl_Interp *interp;
     unsigned long multicastAddr;
     int port;
     CB *cb;
     int ttl;
{
  int on;
  struct sockaddr_in *sin = &(cb->cb_inaddr);
  struct ip_mreq mr;
  unsigned char u_ttl = (unsigned char)ttl;

  if ((cb->cb_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    Tcl_AppendResult(interp, "socket: ", strerror(errno), (char *)NULL);
    return -1;
  }

  on = 1;
  if (setsockopt(cb->cb_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&on,sizeof(on))<0) {
    Tcl_AppendResult(interp, "reuseaddr: ", strerror(errno), (char *)NULL);
    return -1;
  }
  if (setsockopt(cb->cb_sock,IPPROTO_IP,IP_MULTICAST_TTL, (char *)&u_ttl,
                 sizeof(u_ttl)) < 0) {
    Tcl_AppendResult(interp, "setting ttl: ", strerror(errno), (char *)NULL);
    return -1;
  }

  memset((char*)sin, 0, sizeof(struct sockaddr_in));
  sin->sin_family = AF_INET;
  sin->sin_port   = htons((u_short)port);
  sin->sin_addr.s_addr = multicastAddr;

  if (IN_CLASSD(ntohl(sin->sin_addr.s_addr))==0) {
    close(cb->cb_sock);
    Tcl_AppendResult(interp,"multicast: not a Class-D address\n",(char *)NULL);
    return -1;
  }

  if (bind(cb->cb_sock, (struct sockaddr*)sin, sizeof(struct sockaddr_in)) < 0) {
    close(cb->cb_sock);
    Tcl_AppendResult(interp, "bind: ", strerror(errno), (char *)NULL);
    return -1;
  }

  /* This multicast setup shouldn't really be required */
  /* That's what the vic code says - Yatin */
  mr.imr_multiaddr.s_addr = sin->sin_addr.s_addr;
  mr.imr_interface.s_addr = INADDR_ANY;
  if (setsockopt(cb->cb_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mr, 
		 sizeof(mr)) < 0) {
    close(cb->cb_sock);
    Tcl_AppendResult(interp, "add-membership: ",strerror(errno), (char *)NULL);
    return -1;
  }

  return cb->cb_sock;
}


/*
 *  Callback routine.
 *  Receive a packet, and call the Tcl callback proc with the sending host
 *  IP address, sending port number, and the string.
 */

static void
Tudp_ReceiveDgram(cd, mask)
     ClientData cd;
     int mask;
{

  CB *cb;
  int fromlen = sizeof(struct sockaddr);
  int msglen;
  char sender_ipaddr[20], sender_port[8];
  unsigned long ipaddr;
  struct sockaddr from;
  struct sockaddr_in *fromp = (struct sockaddr_in *)&from;
  char buf[TUDP_MAXMSGSIZE];

  cb = (CB *)cd;
  msglen = recvfrom(cb->cb_sock, buf, TUDP_MAXMSGSIZE-1, 0,
                    &from, &fromlen);
  assert(cb);
  if (msglen <= 0) {
    Tcl_AppendResult(cb->cb_interp, "recvfrom returned ", strerror(errno),
                     (char *)NULL);
    Tcl_BackgroundError(cb->cb_interp);
    return;
  }
  buf[msglen] = 0;
  /*
   *  Parse out IP address and port of sender from the sockaddr_in.
   */

  sprintf(sender_port, "%d", (int)(ntohs(fromp->sin_port)));
  ipaddr = (unsigned long)(fromp->sin_addr.s_addr);
  sprintf(sender_ipaddr, "%d.%d.%d.%d", (int)(ipaddr >> 24),
          (int)((ipaddr >> 16) & 0xff), (int)((ipaddr >> 8) & 0xff),
          (int)(ipaddr & 0xff));
  
  /*
   *  If the datagram contains any zeros, replace them with '@'.
   */
          
  /*buf[msglen] = '\0';
  for (index = 0; index < msglen; index++)
    if (buf[index] == '\0')
      buf[index] = '@';*/

  /*
   *  deliver it....
   */

  if (Tcl_VarEval(cb->cb_interp, cb->cb_procname, " ",
                  sender_ipaddr, " ", sender_port, 
                  " {",
                  buf, "}", (char *)NULL)
      != TCL_OK) 
    Tcl_BackgroundError(cb->cb_interp);
}

    
    
