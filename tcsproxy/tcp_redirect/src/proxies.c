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
#include "utils.h"
#include "tcp_redirect.h"
#include "connections.h"
#include "proxies.h"
#include "monitor.h"

/* More nastie(TM) global variables */
static ll                fe_list = NULL;
static int               num_fe = 0, num_enabled = 0;

extern MonitorClientID   monID;

int add_proxy(char *hostname, char *port)
{
  int                 res;
  struct sockaddr_in  naddr;
  fe_host            *nexthost;
  char                mon_msg[2048];

  /* Do the DNS dance */
  res = make_inetaddr(hostname, port, &naddr);
  if (res == -1) {
    sprintf(mon_msg, "Couldn't resolve host/port %s:%s - ignoring\n",
	    hostname, port);
    fprintf(stderr, mon_msg);
    MonitorClientSend(monID, "Messages", mon_msg, "Log");
    return 0;
  }

  /* Build the appropriate fe_host entry */
  nexthost = (fe_host *) calloc(sizeof(fe_host), 1);
  if (nexthost == NULL) {
    fprintf(stderr, "Out of memory in build_fe_list(2)\n");
    exit(1);
  }
  strncpy(nexthost->hostname, hostname, HOST_MAXLEN-1);
  nexthost->hostname[HOST_MAXLEN-1] = '\0';
  strncpy(nexthost->port, port, PORT_MAXLEN-1);
  nexthost->port[PORT_MAXLEN-1] = '\0';
  memcpy((void *) &(nexthost->host_addr), 
	 (void *) &naddr,
	 sizeof(struct sockaddr_in));
  nexthost->available = 1;   /* Assume up until find out otherwise */

    /* Add to list of fe_hosts */
  if (ll_add_to_end(&fe_list, (void *) nexthost) != 1) {
    sprintf(mon_msg, "Couldn't add %s:%s to linked list (!?) - ignoring\n",
	    hostname, port);
    fprintf(stderr, mon_msg);
    MonitorClientSend(monID, "Messages", mon_msg, "Log");
    free(nexthost);
    return 0;
  }
  num_fe++;
  num_enabled++;

  sprintf(mon_msg, "Added FE host/port %s:%s\n", hostname, port);
  MonitorClientSend(monID, "Messages", mon_msg, "Log");
  return 1;
}

int remove_proxy(char *hostname, char *port)
{
  return 1;
}

int disable_proxy(struct sockaddr_in *proxy_addr)
{
  ll       tmp;
  fe_host *data;
  char     mon_msg[2048];
  int      disabled = 0;
  struct timeval tv;

  tmp = fe_list;
  while (tmp != NULL) {

    data = (fe_host *) tmp->data;
    if (memcmp(&(data->host_addr), proxy_addr, sizeof(struct sockaddr_in)) 
	== 0) {
      data->available = 0;
      num_enabled--;
      disabled++;
      sprintf(mon_msg, "Disabled proxy %s:%s;  %d left\n",
	      data->hostname, data->port, num_enabled);
      fprintf(stderr, "%s", mon_msg);
      MonitorClientSend(monID, "Messages", mon_msg, "Log");
      gettimeofday(&tv, NULL);
      data->lastcheck = tv.tv_sec;
    }
    tmp = tmp->next;
  }
  return (disabled > 0) ? 1 : 0;
}

void try_enable(void)
{
  ll tmp;
  struct timeval tv;
  fe_host *data;
  int      reenabled = 0;
  char     mon_msg[2048];

  gettimeofday(&tv, NULL);
  tmp = fe_list;
  while (tmp != NULL) {
    data = (fe_host *) tmp->data;

    if ((data->available == 0) && 
	(tv.tv_sec - data->lastcheck > TR_REENABLE_SECS)) {
      data->available = 1;
      reenabled++;
      num_enabled++;
      sprintf(mon_msg, "Reenabled proxy %s:%s;  %d left\n",
	      data->hostname, data->port, num_enabled);
      fprintf(stderr, "%s", mon_msg);
      MonitorClientSend(monID, "Messages", mon_msg, "Log");
    }
    tmp = tmp->next;
  }
}

/* Proxy selection function */
fe_host *get_proxy(void)
{
  static int round = 0;
  int        i;
  ll         tmpo;
  char       mon_msg[2048];
  fe_host   *hst;

  /* Make sure at least one proxy is alive. */
  if (num_enabled == 0) {
    try_enable();
    if (num_enabled == 0)
      return NULL;
  }

  i = round % num_enabled;
  round = ++round % num_fe;

/*   fprintf(stderr, "Grabbing num %d\n", i);  */
  /* Grab the i'th available proxy */
  
  tmpo = fe_list;
  while(1) {
    if (tmpo == NULL) {
      sprintf(mon_msg, "In inconsistent state in get_proxy()\n");
      fprintf(stderr, mon_msg);
      MonitorClientSend(monID, "Messages", mon_msg, "Log");
      return NULL;
    }
    hst = (fe_host *) tmpo->data;
    if (hst->available) {
      if (i == 0) {
/*	fprintf(stderr, "returning host/port %s/%s\n",
		hst->hostname, hst->port); */
	return hst;
      }
      i--;
    }
    tmpo = tmpo->next;
  }
  return hst;
}
