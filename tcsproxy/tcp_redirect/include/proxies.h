#ifndef PROXY_TCPREDIRECT_H
#define PROXY_TCPREDIRECT_H

#define TR_REENABLE_SECS 30   /* How long until check to see if a proxy has
                                 come back up */

/* Make a new proxy known to the system.  Will re-add an existing proxy
   if you pass it in again.  Returns 1 on successful add, 0 if failure
   (such as DNS lookup failure).  */
int add_proxy(char *hostname, char *port);

/* Remove a proxy from the system.  Will only remove first such-named proxy,
   so if there are multiple instances of same proxy/port that TCP_REDIRECT
   knows about, you need to do multiple removes.  Returns 1 if successfully
   removed proxy, 0 if no such proxy could be found. */
int remove_proxy(char *hostname, char *port);

/* Temporary disable proxy (connections failing to it, for instance).
   Returns 1 if successful, 0 if it doesn't know about the proxy, and
   -1 if the proxy is already disabled. */
int disable_proxy(struct sockaddr_in *proxy_addr);

/* This function should be periodically called - it will reenable proxies
   that have been temporarily disabled for more than TR_ REENABLE_SECS secs.
   No errors could possibly occur on this.  */
void try_enable(void);

/* Clients use this to get a fd to a proxy to communicate with.  Returns NULL
   in case of failure.  (Client should probably return a clever HTTP error
   code.)  */
fe_host *get_proxy(void);

#endif
