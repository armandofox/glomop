/*
 *  Forbid access to certain addresses.  Ugh.
 *  This list comes from Shuli Roth, with the WebNet group.
 */

#include "config_tr.h"

typedef struct {
  UINT32 ip;
  UINT32 mask;
} NETID;

static UINT32 forbidden[] = {
  0x8020b7d7,                   /* 128.32.123.215 depot */
  0x80207b63,                   /* 128.32.123.99 cornucopia */
  0x80207bac,                   /* 128.32.123.172 tuna */
  0x80207bca                   /* 128.32.123.202 cobweb */
};

static NETID allowed[] = {
  { 0x80200000, 0xffff0000 },   /* 128.32.0.0/255.255.0.0 */
  { 0x88980000, 0xffff0000 },   /* 136.152.0.0/255.255.0.0 */
  { 0xd0015900, 0xffffff00 },   /* 208.1.89.0/255.255.255.0 */
  { 0xd0015800, 0xffffff00 },   /* 208.1.88.0/255.255.255.0 */
  { 0xa9e51400, 0xffffff80 },   /* 169.229.20.128/255.255.255.128 */
  { 0xc0650000, 0xffff0000 }    /* 192.101.0.0/255.255.0.0 */
};

#define NUM_FORBIDDEN  (sizeof(forbidden)/sizeof(UINT32))
#define NUM_ALLOWED    (sizeof(allowed)/sizeof(NETID))

gm_Bool
is_allowed(int socket, userkey k)
{
  struct sockaddr_in a;
  UINT32 rem_addr;
  int alen = sizeof(struct sockaddr_in);

  memset(&a, 0, alen);
  if (getpeername(, (struct sockaddr *)&a, &alen) != 0) {
    return gm_True;             /* BUG::not failsafe! But who cares. */
  }
  /* compare to allowed list */
}


  
