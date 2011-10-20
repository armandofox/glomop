#ifndef TCP_REDIRECT_H
#define TCP_REDIRECT_H

#define HOST_MAXLEN 128
#define PORT_MAXLEN 12
typedef struct fe_host_st {
  char               hostname[HOST_MAXLEN], port[PORT_MAXLEN];
  struct sockaddr_in host_addr;
  int                available;   /* 1 if up, 0 if down */
  long int           lastcheck;   /* last time we checked if it was up */
} fe_host;

void refresh_listen_port(void);

#endif


