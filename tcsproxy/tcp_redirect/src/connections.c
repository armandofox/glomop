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

#include "config_tr.h"
#include "optdb.h"
#include "monitor.h"
#include "utils.h"
#include "tcp_redirect.h"
#include "connections.h"
#include "proxies.h"

/* Nasty globals */
extern MonitorClientID  monID;
extern int              errno;
extern fd_set           read_fds, write_fds, exception_fds;

conn_st                 connections[FD_SETSIZE];
int                     tcp_pnum;
struct sockaddr_in      local;

void cleanup_fd(int fd);
void handle_unforged(int fd);
void forge_proxy_connect(int stalk);

void initialize_incoming(void)
{
  bzero((void *) &(connections[0]), FD_SETSIZE*sizeof(conn_st));
  tcp_pnum = protonumber("tcp");
  make_inetaddr((char *) 0, (char *) 0, &local);
}

void handle_incoming(int listen_fd, fd_set *rfd, fd_set *efd) {
  char               msgbuf[2048];
  int                stalk, fromlen;
  struct sockaddr_in from;

/*  fprintf(stderr, "handle_incoming on %d\n", listen_fd);  */
  if (FD_ISSET(listen_fd, efd)) {
    FD_CLR(listen_fd, efd);
    sprintf(msgbuf, "Exception on listening fd - %s\n", strerror(errno));
    fprintf(stderr, "%s", msgbuf);
    MonitorClientSend(monID, "Messages", msgbuf, "Log");
    refresh_listen_port();
    return;
  }

  if (!FD_ISSET(listen_fd, rfd))
    return;

  /* We have a new connection */
  FD_CLR(listen_fd, rfd);

  /* Start the accept */
  fromlen = sizeof(struct sockaddr_in);
  stalk = accept(listen_fd, (void *) &from, &fromlen);
  if (stalk < 0) {
    sprintf(msgbuf, "Exception while accepting - %s\n", strerror(errno));
    fprintf(stderr, "%s", msgbuf);
    MonitorClientSend(monID, "Messages", msgbuf, "Log");
    return;
  }
  make_nonblocking(stalk);

  /* Set up the connection structure, forge connect to proxy */
  forge_proxy_connect(stalk);
  return;
}

/* A file descriptor has been marked as readable */
void conn_read(int fd)
{
  conn_st *ptr, *ptr2;
  char     msgbuf[2048];

  /* A file descriptor is ready to read. */

  ptr = &(connections[fd]);
/*   fprintf(stderr, "Got %d (other %d) in read\n", fd, ptr->other_fd);  */
  if (ptr->forged_outgoing == 0) {
    handle_unforged(fd);
    return;
  }

  if (ptr->otherdir_open == 0) {
    /* Shouldn't be here */
    fprintf(stderr, "Shouldn't be here either...\n");
    cleanup_fd(fd);
    return;
  }

  /* The connection is fully in place, and we have data from the client. */
  ptr2 = &(connections[ptr->other_fd]);
  if (ptr2->data_len != 0) {
    fprintf(stderr, "Shouldn't be here..\n");
    FD_SET(ptr->other_fd, &write_fds);
    return;
  }
  ptr2->data_len = read(fd, ptr2->data, READ_BLOCKSIZE);

  if (ptr2->data_len < 0) {
    if ((errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EINTR))
      return;

    sprintf(msgbuf, "Exception on read in read_conn- %s\n", strerror(errno));
    fprintf(stderr, "%s", msgbuf);
    MonitorClientSend(monID, "Messages", msgbuf, "Log");
    if (ptr->otherdir_open != 0) {
      cleanup_fd(ptr->other_fd);
    }
    cleanup_fd(fd);
    return;
  } else if (ptr2->data_len == 0) {

    /* This fd is dead, so we'll close it.  If the other direction has
       nothing to write, close it as well. */
    if (ptr->otherdir_open != 0) {
      if (ptr2->data_len == 0) {
/*	fprintf(stderr, "Cleaning %d (other %d) due to other read 0\n",
		ptr->other_fd, fd);  */
	cleanup_fd(ptr->other_fd);
      }
    }
/*  fprintf(stderr, "Cleaning %d (other %d) in read\n", fd, ptr->other_fd); */
    cleanup_fd(fd);
    return;
  }

  /* We got some data! */
/*  fprintf(stderr, "Done %d (other %d) in read\n", fd, ptr->other_fd); */
  FD_CLR(fd, &read_fds);
  FD_SET(ptr->other_fd, &write_fds);
}

void conn_write(int fd)
{
  conn_st *ptr;
  char     msgbuf[2048];
  int      res;

  /* A file descriptor is ready to write.  Remember to special case the
     connect. */

  ptr = &(connections[fd]);
  if (ptr->forged_outgoing == 0) {
    handle_unforged(fd);
    return;
  }
/*   fprintf(stderr, "Got %d (other %d) in write\n", fd, ptr->other_fd); */

  /* we've forged the connection, and a previously blocked socket is ready
     to receive data.  let's send it. */
  if (ptr->data_len == 0) {
    fprintf(stderr, "Shouldn't be here (2)\n");
    return;
  }

  res = write(fd, ptr->data, ptr->data_len);
  if (res < 0) {
    if ((errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EINTR))
      return;

    sprintf(msgbuf, "Exception on write in write_conn- %s\n", strerror(errno));
    fprintf(stderr, "%s", msgbuf);
    MonitorClientSend(monID, "Messages", msgbuf, "Log");
    if (ptr->otherdir_open != 0) {
      cleanup_fd(ptr->other_fd);
    }
    cleanup_fd(fd);
    return;
  } else if (res == 0) {
/*  fprintf(stderr, "Cleaning %d (other %d) in write\n", fd, ptr->other_fd);*/
    cleanup_fd(fd);
    return;
  }

  /* We wrote some data! */
  if (res != ptr->data_len) {
    memmove(ptr->data, ptr->data + res, ptr->data_len - res);
    ptr->data_len -= res;
/* fprintf(stderr, "More write %d (other %d) in write\n", fd, ptr->other_fd); */
    return;
  } else {
    ptr->data_len = 0;
    if (ptr->otherdir_open == 0) {
      /* If other direction is dead, and we've finished our write, go
	 away. */
      cleanup_fd(fd);
      return;
    }
    FD_CLR(fd, &write_fds);
    FD_SET(ptr->other_fd, &read_fds);
/* fprintf(stderr, "Done write %d (other %d) in write\n", fd, ptr->other_fd); */
    return;
  }
}

void conn_except(int fd)
{
  conn_st *ptr;
  char     msgbuf[2048];

  sprintf(msgbuf, "Exception on fd %d - closing\n", fd);
  fprintf(stderr, "%s", msgbuf);
  MonitorClientSend(monID, "Messages", msgbuf, "Log");
  ptr = &(connections[fd]);
  if (ptr->otherdir_open == 1) {
    sprintf(msgbuf, "Also closing other fd - %d\n", ptr->other_fd);
    fprintf(stderr, "%s", msgbuf);
    MonitorClientSend(monID, "Messages", msgbuf, "Log");
    cleanup_fd(ptr->other_fd);
  }
  cleanup_fd(fd);
}

void cleanup_fd(int fd)
{
  int fd2;
  conn_st *ptr;

  if (fd == 0) {
    fprintf(stderr, "Hey!  not supposed to get fd 0 in cleanup_fd\n");
    return;
  }

  ptr = &connections[fd];
  if (ptr->otherdir_open) {
    fd2 = ptr->other_fd;
    connections[fd2].otherdir_open = 0;
  }
  FD_CLR(fd, &read_fds);
  FD_CLR(fd, &write_fds);
/*  FD_CLR(fd, &exception_fds); */
  close(fd);
  bzero((void *) ptr, sizeof(conn_st));
}

void handle_unforged(int fd)
{
  conn_st *ptr, *ptr2;
  char     msgbuf[2048];
  int      fd2;

  ptr = &(connections[fd]);

  if (ptr->direction == OUTGOING) {
    /* Are we connected now? */
    int res;

    res = read(fd, msgbuf, 1);

    if (res < 0) {
      if ((errno != EINPROGRESS) && (errno != EAGAIN) && (errno != EINTR))
      {
	/* some bad error happened. */
	disable_proxy(&(ptr->remote_host));
	if (ptr->otherdir_open) {
	  fd2 = ptr->other_fd;
	} else {
	  fd2 = -1;
	}
	cleanup_fd(fd);
	sprintf(msgbuf, "Exception on read test - %s\n", strerror(errno));
	fprintf(stderr, "%s", msgbuf);
	MonitorClientSend(monID, "Messages", msgbuf, "Log");

	if (fd2 >= 0)
	  /* try again ... */
	  forge_proxy_connect(fd2);
	return;
      } else if (errno == EINTR)
	/* try again */
	return;
    } else {
      if (res == 0) {
	fprintf(stderr, "Got 0 on read in handle_unforged.\n");
	if (ptr->otherdir_open) {
	  fd2 = ptr->other_fd;
	  cleanup_fd(fd2);
	}
	cleanup_fd(fd);
	return;
      }
      /* we got a byte.  oops. */
      if (ptr->otherdir_open) {
	fd2 = ptr->other_fd;
	cleanup_fd(fd2);
      }
      cleanup_fd(fd);
      sprintf(msgbuf, "Read SUCCEEDED???  That never happens..\n");
      fprintf(stderr, "%s", msgbuf);
      MonitorClientSend(monID, "Messages", msgbuf, "Log");
      return;
    }

    /* connect has indeed succeeded */
    ptr->forged_outgoing = 1;
    ptr2 = &(connections[ptr->other_fd]);
    ptr2->forged_outgoing = 1;
    FD_CLR(fd, &write_fds);

    FD_SET(fd, &read_fds);
    FD_SET(ptr->other_fd, &read_fds);
    return;
  } else {
    /* ignore data from/to the client for now, until connect succeeds */
    return;
  }
}

void forge_proxy_connect(int stalk)
{
  int done = 0, outgoing_fd, res;
  fe_host    *hoste;
  char        msgbuf[2048];

  done = 0;
  while (!done) {
    done = 1;          /* Assume success until something goes wrong */
    connections[stalk].direction = INCOMING;
    connections[stalk].forged_outgoing = 0;
    connections[stalk].data_len = 0;
    connections[stalk].otherdir_open = 1;
    hoste = get_proxy();
    if (hoste == NULL) {
      sprintf(msgbuf, "No proxies are alive!\n");
      fprintf(stderr, "%s", msgbuf);
      MonitorClientSend(monID, "Messages", msgbuf, "Log");
      close(stalk);
      return;
    }

    outgoing_fd = socket(PF_INET, SOCK_STREAM, tcp_pnum);
    make_nonblocking(outgoing_fd);
    if (outgoing_fd < 0) {
      sprintf(msgbuf, "Exception on socket - %s\n", strerror(errno));
      fprintf(stderr, "%s", msgbuf);
      MonitorClientSend(monID, "Messages", msgbuf, "Log");
      close(stalk);
      return;
    }
    res = bind(outgoing_fd, (struct sockaddr *) &local, 
	       sizeof(struct sockaddr_in));
    if (res < 0) {
      close(outgoing_fd);
      sprintf(msgbuf, "Exception on bind - %s\n", strerror(errno));
      fprintf(stderr, "%s", msgbuf);
      MonitorClientSend(monID, "Messages", msgbuf, "Log");
      close(stalk);
      return;
    }
    res = connect(outgoing_fd, (struct sockaddr *) &(hoste->host_addr),
		  sizeof(struct sockaddr_in));
    if (res == -1) {
      if (errno != EINPROGRESS) {
	close(outgoing_fd);
	sprintf(msgbuf, "Exception on connect - %s\n", strerror(errno));
	fprintf(stderr, "%s", msgbuf);
	MonitorClientSend(monID, "Messages", msgbuf, "Log");
	disable_proxy(&(hoste->host_addr));
	done = 0;  /* Try connect again. */
      }
    }
  }
  connections[stalk].other_fd = outgoing_fd;
  connections[outgoing_fd].other_fd = stalk;
  connections[outgoing_fd].direction = OUTGOING;
  connections[outgoing_fd].otherdir_open = 1;
  connections[outgoing_fd].forged_outgoing = (res == 0 ? 1 : 0);
  connections[outgoing_fd].remote_host = hoste->host_addr;
  connections[stalk].remote_host = hoste->host_addr;
  connections[outgoing_fd].data_len = 0;

  /* Add these two file descriptors to the read/exception masks */
  if (res == 0) {
    FD_SET(stalk, &read_fds);
    FD_SET(outgoing_fd, &read_fds);
  } else {
    FD_SET(outgoing_fd, &write_fds);
  }
/*  FD_SET(stalk, &exception_fds);
  FD_SET(outgoing_fd, &exception_fds); */
  
}
