#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#define INCOMING 0    /* client to tcp_redirect */
#define OUTGOING 1    /* tcp_redirect to proxy */

#define READ_BLOCKSIZE 8192

typedef unsigned char fd_read_buf[READ_BLOCKSIZE];

typedef struct conn_struct {
  int direction, other_fd;
  int forged_outgoing;    /* 1 if connect succeeded, 0 otherwise */
  int otherdir_open;      /* 1 if other fd is still open */

  struct sockaddr_in remote_host;
  
  int data_len;
  fd_read_buf data;
} conn_st;

void initialize_incoming(void);
void handle_incoming(int listen_fd, fd_set *rfd, fd_set *efd);
void conn_read(int fd);
void conn_write(int fd);
void conn_except(int fd);

#endif
