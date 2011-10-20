/*
 * File:          part.c
 * Purpose:       Partition management for the Harvest cache substrate
 * Author:        Steve Gribble
 * Creation Date: October 23rd, 1996
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include "config_tr.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "clib.h"
#include "icp.h"
#include "utils.h"
#include "part.h"
#include "configure.h"
#include "md5.h"
#include "optdb.h"

#include "libmon.h"

extern Monitor *gMon;

/* Various globals used for configuration information */
pthread_mutex_t config_list_mutex;
extern ll       config_list;

extern Options part_opts;

/* The structure describing the currently up partitions */
pthread_mutex_t partition_mutex;
int             num_partitions = 0;
ll              partition_list = NULL;

/* Monitor information */
Monitor        *pMon;

/* Cached TCP information */
struct sockaddr *localaddr;
int              localaddrlen;
struct protoent  tcpproto;
int              attempt_to_regrow=0;

int  part_compare(void *d1, void *d2);
void part_nuke(void *nukeMe);
int  get_part_number(int num_partitions, char *url);
void regrow_partitionlist(int thesignal);
void do_regrow(void);
clib_response do_suck_from_configfile(Options opts);
void stream_ntoa(unsigned long addr, char *buf);

clib_response Part_Initialize(Options opts, Monitor *defMon)
{
  clib_response    retval = CLIB_OK;
  int              pretval;
  struct protoent  *foo;
  cfg_nv           *match;
  char             *ip, def_ip[20] = "228.9.10.7";
  char             tmp_ip[20];
  int              port, def_port = 8884, ttl, def_ttl = 4;
  ll               tmp;

  signal(SIGHUP, regrow_partitionlist);

  foo = getprotobyname("tcp");
  memcpy(&tcpproto, foo, sizeof(struct protoent));

  if ((pretval = pthread_mutex_init(&partition_mutex, NULL)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }

  if ((pretval = pthread_mutex_init(&config_list_mutex, NULL)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }

  /* Make a local address */
  localaddr = (struct sockaddr *) malloc(sizeof(struct sockaddr_in));
  localaddrlen = sizeof(struct sockaddr_in);
  make_inetaddr(NULL, NULL, (struct sockaddr_in *) localaddr);
  if ((retval = config_parse(opts)) != CLIB_OK)
    return retval;

  tmp = config_list; match = get_next_matching("Monitor.listen", &tmp);
  if (match == NULL) {
    ip = def_ip;
    port = def_port;
    ttl = def_ttl;
    fprintf(stderr, "Using default IP (%s/%d/%d).\n", ip,port,ttl);
  }   else {
    /* format of string is 99.99.99.99/9999/9.  Need the ip addr as string and
       remaining components as ints.  */
    int dummy;
    char *tmp2;
    strncpy(tmp_ip, match->value, 20);
    if ((sscanf(tmp_ip, "%d.%d.%d.%d/%d/%d", &dummy,&dummy,&dummy,&dummy,
                &port, &ttl) == 6)
        &&  ((tmp2 = strchr(tmp_ip, '/')) != NULL)) {
      ip = tmp_ip;
      *tmp2 = '\0';
    } else {
      ip = def_ip;
      port = def_port;
      ttl = def_ttl;
    }
  }

  if (defMon == NULL)
    gMon = pMon = MonitorClient_Create("client-side harvest library", ip, port, ttl);
  else
    pMon = defMon;
  MonitorClient_SetAsDefault(pMon);
  MonitorClient_UnitSend(pMon, pMon->unitname, "Messages",
			 "Harvest client library initializing.\n", MON_LOG);

  return do_suck_from_configfile(opts);
}

clib_response do_suck_from_configfile(Options opts)
{
  clib_response    retval = CLIB_OK, pretval;
  part_element    *part_el;
  ll               tmp;
  cfg_nv          *cfg_el;
  char            *tok = NULL;
  ts_strtok_state *ts_st;
  char             ipb[100], msgb[200];

  /* CRITICAL SECTION..... */
  if ((pretval = pthread_mutex_lock(&config_list_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  if ((retval = config_parse(opts)) != CLIB_OK)
    return retval;

  tmp = config_list;
  /* Loop through each partition defined in the configuration file
      and build the correct part_element structure for it */
  do {
    cfg_el = get_next_matching("cache.partition", &tmp);
    if (cfg_el == NULL)
      break;
    if (cfg_el->value == NULL)
      continue;
    tmp = tmp->next;
    ts_st = ts_strtok_init(cfg_el->value);
    tok = ts_strtok(" ", ts_st);

    /* Loop through each token in the name/values tuple */
    while (tok != NULL) {
      part_el = (part_element *) malloc(sizeof(part_element));
      if (part_el == NULL) {
	fprintf(stderr, "Out of memory in Part_Initialize(1)\n");
	exit(1);
      }
      part_el->addr = (struct sockaddr *) malloc(sizeof(struct sockaddr_in));
      if (part_el->addr == NULL) {
	fprintf(stderr, "Out of memory in Part_Initialize(2)\n");
	exit(1);
      }
      part_el->addrlen = sizeof(struct sockaddr_in);

      /* Build a struct sockaddr_in for this harvest partition host */
      if (make_inetaddr(tok, "3128", (struct sockaddr_in *) part_el->addr)
	  != 0) {
	free(part_el->addr);
	free(part_el);
	fprintf(stderr, "Bogus partition name in Part_Initialize: %s\n", tok);
      } else {
	/* add the structure to our partition list */
	stream_ntoa(((struct sockaddr_in *) part_el->addr)->sin_addr.s_addr,
		    ipb);
	sprintf(msgb, "Adding partition at %s..\n",ipb);
	MonitorClient_UnitSend(pMon, pMon->unitname, "Messages", msgb,
			       MON_LOG);

	if (!ll_add_to_end(&partition_list, (void *) part_el)) {
	  fprintf(stderr, "ll_add_to_end failed in Part_Initialize\n");
	  exit(1);
	}
	num_partitions++;
      }
      tok = ts_strtok(" ", ts_st);
    }
    ts_strtok_finish(ts_st);
  } while (cfg_el != NULL);

  if ((pretval = pthread_mutex_unlock(&config_list_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  /* END CRITICAL SECTION..... */

  return retval;
}

clib_response Part_Add(struct sockaddr *addr, int addrlen)
{
  part_element *add;
  int           pretval;

  do_regrow();

  /* Build a part_element structure and add it to our list */
  add = (part_element *) malloc(sizeof(part_element *));
  if (add == NULL) {
    fprintf(stderr, "Out of memory in Part_Add (1)\n");
    exit(1);
  }

  add->addr = (struct sockaddr *) malloc(addrlen);
  if (add->addr == NULL) {
    fprintf(stderr, "Out of memory in Part_Add (2)\n");
    free(add);
    exit(1);
  }

  memcpy(add->addr, addr, addrlen);
  add->addrlen = addrlen;

  /* CRITICAL SECTION..... */
  if ((pretval = pthread_mutex_lock(&partition_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  if (!ll_add_to_end(&partition_list, (void *) add)) {
    fprintf(stderr, "ll_add_to_end failed in Part_Add\n");
    exit(1);
  }
  num_partitions++;
  if ((pretval = pthread_mutex_unlock(&partition_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  /* END CRITICAL SECTION..... */

  return CLIB_OK;
}

clib_response Part_Delete(struct sockaddr *addr, int addrlen)
{
  part_element    el, *e2;
  int             pretval;
  clib_response   rv;
  char            ipb[100], mon_msg[200];
  ll              tmp;
  struct sockaddr *localaddrtmp;

  do_regrow();

  /* Attempt to find a partition that matches, and delete it from list */
  el.addr = addr;
  el.addrlen = addrlen;

  /* CRITICAL SECTION..... */
  if ((pretval = pthread_mutex_lock(&partition_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }

  tmp = ll_find(&partition_list, &el, part_compare);
  if (tmp != NULL) {
    e2 = (part_element *) tmp->data;
    localaddrtmp = (struct sockaddr *) e2->addr;
    stream_ntoa(((struct sockaddr_in *) localaddrtmp)->sin_addr.s_addr,
		ipb);
  } else {
    sprintf(ipb, "(failed)");
  }
  
  if (ll_del(&partition_list, &el, part_compare, part_nuke)) {
    num_partitions--;
    rv = CLIB_OK;
  } else
    rv = CLIB_NO_SUCH_PART;
  if ((pretval = pthread_mutex_unlock(&partition_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  /* END CRITICAL SECTION..... */

  sprintf(mon_msg, "Deleted partition at %s.\n", ipb);
  MonitorClient_UnitSend(pMon, pMon->unitname, "Messages", mon_msg, MON_LOG);
  return rv;
}

clib_response Part_GetTCPsock(char *url, int *fd)
{
  int              part_num, i, pretval, len=0;
  struct sockaddr_in addr;
  ll               tmp;
  part_element     *el;
  clib_response    rv = CLIB_OK;
  struct protoent  *proto;

  do_regrow();

  /* CRITICAL SECTION..... */
  if ((pretval = pthread_mutex_lock(&partition_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  /* Find the right structure in the partition list */
  tmp = partition_list;
  part_num = get_part_number(num_partitions, url);
  if (part_num == -1)
    rv = CLIB_PTHREAD_FAIL;
  else {
    for (i=0; i<part_num; i++) {
      assert(tmp != NULL);
      tmp = tmp->next;
    }
    el = (part_element *) tmp->data;
    addr = *((struct sockaddr_in *) (el->addr));
    len = el->addrlen;
  }

  if ((pretval = pthread_mutex_unlock(&partition_mutex)) != 0) {
    strerror(pretval);
    rv = CLIB_PTHREAD_FAIL;
  }
  /* END CRITICAL SECTION..... */

  if (rv != CLIB_OK)
    return rv;

  /* we have the structure, now make the socket and connect it */
  proto = &tcpproto;
  if (proto == NULL) {
    perror("getprotobyname failed: ");
    rv = CLIB_NO_FD;
  } else {
    *fd = socket(PF_INET, SOCK_STREAM, proto->p_proto);
    if (*fd < 0) {
      perror("socket failed: ");
      rv = CLIB_NO_FD;
    } else {
      if (bind(*fd, localaddr, localaddrlen) < 0) {
	perror("bind failed: ");
	rv = CLIB_NO_FD;
      } else {
	/* set the port to 3128 */
	addr.sin_port = htons(3128);
	if (connect(*fd, (struct sockaddr *) &addr, len) < 0) {
	  perror("connect failed: ");
          Part_HandleFailureS((struct sockaddr *) &addr, len);
	  if (num_partitions == 0)
	    rv = CLIB_NO_FD;
	  else
	    return Part_GetTCPsock(url, fd);
	}
      }
    }
  }

  return rv;
}

clib_response Part_GetUDPsock(char *url, int *fd)
{
  int              part_num, i, pretval, len;
  struct sockaddr_in addr;
  ll               tmp;
  part_element     *el;
  struct protoent *proto;
  clib_response    rv = CLIB_OK;

  do_regrow();

  /* CRITICAL SECTION..... */
  if ((pretval = pthread_mutex_lock(&partition_mutex)) != 0) {
    strerror(pretval);
    return CLIB_PTHREAD_FAIL;
  }
  tmp = partition_list;
  /* Find the right structure in the partition list */
  part_num = get_part_number(num_partitions, url);
  for (i=0; i<part_num; i++) {
    assert(tmp != NULL);
    tmp = tmp->next;
  }
  el = (part_element *) tmp->data;
  addr = *((struct sockaddr_in *) (el->addr));
  len = el->addrlen;
  
  if ((pretval = pthread_mutex_unlock(&partition_mutex)) != 0) {
    strerror(pretval);
    rv = CLIB_PTHREAD_FAIL;
  }
  /* END CRITICAL SECTION..... */

  /* we have the structure, now make the socket and connect it */
  proto = getprotobyname("udp");
  if (proto == NULL) {
    perror("getprotobyname failed: ");
    rv = CLIB_NO_FD;
  } else {
    *fd = socket(PF_INET, SOCK_DGRAM, proto->p_proto);
    if (*fd < 0) {
      perror("socket failed: ");
      rv = CLIB_NO_FD;
    } else {
      if (bind(*fd, (struct sockaddr *) localaddr, localaddrlen) < 0) {
	perror("bind failed: ");
	rv = CLIB_NO_FD;
      } else {
	/* set the port to 3130 */
	addr.sin_port = htons(3130);
	if (connect(*fd, (struct sockaddr *) &addr, len) < 0) {
	  perror("connect failed: ");
	  rv = CLIB_NO_FD;
	}
      }
    }
  }

  return rv;
}

clib_response Part_HandleFailureS(struct sockaddr *addr, int addrlen)
{
  clib_response dresp;

  MonitorClient_UnitSend(pMon, pMon->unitname, "Messages",
			 "Failure detected on partition!\n", MON_LOG);
  dresp = Part_Delete(addr, addrlen);

  /* Should do clever EPOCH handling routine here */

  return dresp;
}

clib_response Part_HandleFailure(int fd)
{
  int res, namelen;
  struct sockaddr name;
  clib_response dresp;

  MonitorClient_UnitSend(pMon, pMon->unitname, "Messages", 
		     "Failure detected on partition!\n", MON_LOG);
  res = getsockname(fd, &name, &namelen);
  if ((res < 0) || (namelen = 0))
    return CLIB_SOCKETOP_FAIL;

  dresp = Part_Delete(&name, namelen);

  /* Should do clever EPOCH handling routine here */

  return dresp;
}

/* Some utility functions used by the linked list routines to find and
   delete stuff */
int  part_compare(void *d1, void *d2)
{
  part_element    *el1 = (part_element *) d1;
  part_element    *el2 = (part_element *) d2;

  assert(d1 != NULL);
  assert(d2 != NULL);

  if (el1->addrlen > el2->addrlen)
    return 1;
  if (el1->addrlen < el2->addrlen)
    return -1;

  if ( ((struct sockaddr_in *) el1->addr)->sin_addr.s_addr >
       ((struct sockaddr_in *) el2->addr)->sin_addr.s_addr)
    return 1;

  if ( ((struct sockaddr_in *) el1->addr)->sin_addr.s_addr <
       ((struct sockaddr_in *) el2->addr)->sin_addr.s_addr)
    return -1;

  if ( ((struct sockaddr_in *) el1->addr)->sin_port > 
       ((struct sockaddr_in *) el2->addr)->sin_port)
    return 1;

  if ( ((struct sockaddr_in *) el1->addr)->sin_port <
       ((struct sockaddr_in *) el2->addr)->sin_port)
    return -1;

  return 0;

}
void part_nuke(void *nukeMe)
{
  part_element    *el = (part_element *) nukeMe;

  assert(el != NULL);
  if (el->addr)
    free(el->addr);
  free(el);
}

/* This function does the MD5 hash of the URL, and computes a partition
   number from that hash and the number of partitions available. */
int  get_part_number(int num_partitions, char *url)
{
  MD5_CTX  ctx;
  long int bits;
  int      got = 0;
  char     hostnamebuf[1024];

  if (num_partitions == 0)
    return -1;

  /* Try to MD5 on hostname, not on full URL.  This way, all traffic for
     a particular host will flow through a single partition. */
  if (*url == 'h') {
    char *fslash, *nslash;
    int   diff;

    fslash = strchr(url, '/');
    if (fslash == NULL)
      goto DONE_HTTP_TEST;
    if (*(fslash+1) != '/')
      goto DONE_HTTP_TEST;
    nslash = strchr(fslash+2, '/');
    if (nslash == NULL) {
      got = 1;
      MD5Init(&ctx);
      MD5Update(&ctx, fslash+2, strlen(fslash+2));
      MD5Final(&ctx);
    } else {
      diff = nslash-fslash-2;
      if (diff > 0) {
	if (diff > 1020)
	  diff = 1020;
	memcpy(hostnamebuf, fslash+2, diff);
	hostnamebuf[diff] = '\0';
	MD5Init(&ctx);
	MD5Update(&ctx, hostnamebuf, strlen(hostnamebuf));
	MD5Final(&ctx);
	got = 1;
      }
    }
  }
DONE_HTTP_TEST:
  if (got == 0) {
    MD5Init(&ctx);
    MD5Update(&ctx, url, strlen(url));
    MD5Final(&ctx);
  }

  memcpy(&bits, &(ctx.digest[0]), sizeof(long int));
  bits = ntohl(bits);
  if (bits < 0)
    bits *= -1;

/*  fprintf(stderr, "Partition number is %d of (0-%d)\n",
	  (int) (bits % num_partitions), num_partitions-1);*/
  return ((int) (bits % num_partitions));
}

void regrow_partitionlist(int thesignal)
{
  signal(SIGHUP, regrow_partitionlist);
  attempt_to_regrow = 1;
}

void do_regrow(void)
{
  int pretval;
  char mon_msg[75];

  if (attempt_to_regrow == 0)
    return;

  MonitorClient_UnitSend(pMon, pMon->unitname, "Messages",
			 "Regrowing partition list (SIG_HUP)\n", MON_LOG);
  attempt_to_regrow = 0;

  /* Let's reinitialize... */

  /* CRITICAL SECTION..... */
  if ((pretval = pthread_mutex_lock(&partition_mutex)) != 0) {
    strerror(pretval);
    return;
  }
  /* delete all partitions */
  ll_destroy(&partition_list, part_nuke);
  partition_list = NULL;
  num_partitions = 0;

  /* add all partitions */
  do_suck_from_configfile(part_opts);

  if ((pretval = pthread_mutex_unlock(&partition_mutex)) != 0) {
    strerror(pretval);
    return;
  }
  /* END CRITICAL SECTION..... */
  sprintf(mon_msg, "Regrow succeeded, %d partitions loaded\n",
	  num_partitions);
  MonitorClient_UnitSend(pMon, pMon->unitname, "Messages", mon_msg, MON_LOG);

}

void stream_ntoa(unsigned long addr, char *buf)
{
  sprintf(buf, "%lu.%lu.%lu.%lu",
	  (addr >> 24),
	  (addr >> 16) % 256,
	  (addr >> 8) % 256,
	  (addr) % 256);
}
