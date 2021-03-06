/*
 * File:          icp.h
 * Purpose:       Header for the client-library ICP implementation.
 * Author:        Steve Gribble
 * Creation Date: September 25th, 1996
*/        

#include "clib.h"
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef CLIB_ICP_H
#define CLIB_ICP_H


/******* 
******** The following are straight out of the "proto.h" header file from
******** the Harvest v1.4r3 implementation.
********/

#define ICP_AUTH_SIZE (2)
struct icp_common_s {
    unsigned char opcode;          /* opcode */
    unsigned char version;         /* version number */
    unsigned short length;         /* total length (bytes) */
    clib_u32 reqnum;               /* req number (req'd for UDP) */
    clib_u32 auth[ICP_AUTH_SIZE];  /* authenticator (future) */
    clib_u32 shostid;              /* sender host id */
};

typedef struct icp_common_s icp_common_t;
#define ICP_COMMON_SZ (sizeof(icp_common_t))
#define ICP_HDR_SZ (sizeof(icp_common_t)+sizeof(u_num32))
#define ICP_MAX_URL (4096)

typedef enum {
    ICP_OP_INVALID,             /* ensure 0 not accidently interpreted. */
    ICP_OP_QUERY,               /* query opcode (cl->sv) */
    ICP_OP_HIT,                 /* hit (cl<-sv) */
    ICP_OP_MISS,                /* miss (cl<-sv) */
    ICP_OP_ERR,                 /* error (cl<-sv) */
    ICP_OP_SEND,                /* send object non-auth (cl->sv) */
    ICP_OP_SENDA,               /* send object authoritative (cl->sv) */
    ICP_OP_DATABEG,             /* first data, but not last (sv<-cl) */
    ICP_OP_DATA,                /* data middle of stream (sv<-cl) */
    ICP_OP_DATAEND,             /* last data (sv<-cl) */
    ICP_OP_SECHO,               /* echo from source (sv<-os) */
    ICP_OP_DECHO,               /* echo from dumb cached (sv<-dc) */
    ICP_OP_END                  /* marks end of opcodes */
} icp_opcode;
#define ICP_OP_HIGHEST (ICP_OP_END - 1)         /* highest valid opcode */

/* Header for QUERY packet */
struct icp_query_s {
    clib_u32 q_rhostid;         /* requestor host id */
    char *q_url;                /* variable sized, null-term URL data */
};
typedef struct icp_query_s icp_query_t;
#define ICP_QUERY_SZ (sizeof(icp_query_t))

/* Header for HIT packet */
struct icp_hit_s {
    clib_u32 h_size;            /* size if known */
    char *h_url;                /* variable sized URL data */
};
typedef struct icp_hit_s icp_hit_t;
#define ICP_HIT_SZ (sizeof(icp_hit_t))

/* Header for MISS packet */
struct icp_miss_s {
    char *m_url;                /* variable sized URL data */
};
typedef struct icp_miss_s icp_miss_t;
#define ICP_MISS_SZ (sizeof(icp_miss_t))

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Header for ERROR packet */
struct icp_error_s {
    unsigned short e_code;      /* error code */
    char *e_msg;                /* variable sized string message */
};
typedef struct icp_error_s icp_error_t;
#define ICP_ERROR_SZ (sizeof(icp_error_t))
#define ICP_ERROR_MSGLEN        256     /* max size for string, incl '\0' */

/* Error Codes - These can come back in the response packet */
typedef enum {
    ICP_ERROR_INVALID,          /* invalid (not used) */
    ICP_ERROR_BADVERS,          /* version error */
    ICP_ERROR_BADURL,           /* bad URL */
    ICP_ERROR_BADFLAGS,         /* bad flags */
    ICP_ERROR_TIMEDOUT,         /* couldn't get data */
    ICP_ERROR_ACCESS,           /* authorization problem */
    ICP_ERROR_INTERNAL          /* cache server internal err */
} icp_error_code;

/* ICP message type. */
struct icp_message_s {
    icp_common_t header;
    union {
        icp_query_t query;
        icp_hit_t hit;
        icp_miss_t miss;
        icp_error_t error;
    } op;
};
typedef struct icp_message_s icp_message_t;
#define ICP_MESSAGE_SZ (sizeof(icp_message_t))

/* Version */
#define ICP_VERSION_1           1       /* version 1 */
#define ICP_VERSION_2           2       /* version 2 */
#define ICP_VERSION_CURRENT     ICP_VERSION_2   /* current version */

typedef struct obj {
    icp_common_t header;
    char *url;
    unsigned long ttl;
    unsigned long timestamp;
    unsigned long object_size;
    unsigned long buf_len;
    unsigned long offset;
    char *data;
} icp_object;

/* The following defines are for the do_http_send request_type argument */
#define ICP_GET  0
#define ICP_POST 1
#define ICP_HEAD 2
#define ICP_NONE -1

/** ICP functions that other routines can use **/
int do_icp_query(int udp_fd, char *url, struct in_addr source_addr,
		 int reqnum);
int do_icp_udp_receive(int udp_fd, icp_message_t *ret_obj);
int do_http_send(int tcp_fd, char *url, char *mime_headers,
		 char *data, int data_len,
		 int send_http, int request_type);
int do_http_client_receive(int tcp_fd, unsigned *len, char **data);
int do_http_receive(int tcp_fd, unsigned *len, char **data);
int do_http_put(int tcp_fd, char *url, char *mime_headers, int mh_len,
		char *data, int data_len);

#endif
