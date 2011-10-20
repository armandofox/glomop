#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "md5.h"
#include "idea.h"

typedef unsigned long long u64;
typedef unsigned long u32;

#define MD5SALT     "p6s|U_p@83s"
#define MD5SALTLEN   11
uint16   ideakey[8] = {0x4526, 0x8743, 0x3341, 0x9932,
		       0x7865, 0x2093, 0x1996, 0x2321};
u64      iv = 0x41334938;
idea_key en_key, de_key;
unsigned long size;

static char asc[256][20];
static int asclen[256];

int version;

char *stream_ntoa(unsigned long addr);
void  md5_plus_salt_text(char *text, char *buf);
int   do_idea_encrypt(unsigned char *text, unsigned char *buf, int inlen);
void  do_idea_decrypt(unsigned char *text, unsigned char *buf, int inlen);
void  do_read_or_die(int fd, char *buf, size_t length);
void  asc_init(void);

void do_encrypt_stuff(void);
void do_decrypt_stuff(void);

int main(int argc, char **argv)
{
  
  if (argc != 2) {
    fprintf(stderr, "Usage: httpde_univ vers < stdin > stdout\n", argv[0]);
    exit(1);
  }
  
  sscanf(*(argv+1), "%d", &version);
  if ((version <= 0) || (version > 3)) {
    fprintf(stderr, "bad version\n");
    exit(1);
  }

  asc_init();

  do_decrypt_stuff();

  exit(0);
}

void do_encrypt_stuff(void)
{
  fprintf(stderr, "Encryption not available.\n");
}

void die_painful_death(char *dm)
{
  fprintf(stderr, "aaaaaaaaaugugugghgghhhh (%s).\n", dm);
  exit(1);
}

void do_decrypt_stuff(void)
{
  unsigned char inbuf[38192], cryptbuf[38192], cbuf2[38192], outbuf[34096];
  unsigned char  *nexttok;
  unsigned long  addr;
  unsigned short port;
  int urllen, paddedurllen, i;
  unsigned long  crs, cru, sfs, sfu, sls, slu, cip, sip, cims, sep, slm,
                 rhl, rdl, longF = 0xFFFFFFFF;
  unsigned short cpt, spt, url, shortF = 0xFFFF;
  unsigned char  cpr, spr, charF = 0xFF;
  unsigned int   tmpu;

  while(1) {
    if (fgets(inbuf, 38191, stdin) == NULL)
      break;
    if (inbuf[strlen(inbuf)-1] == '\n')
      inbuf[strlen(inbuf)-1] = '\0';

    /* Time of client request (seconds) */
    nexttok = strtok(inbuf, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &crs) != 1)
      die_painful_death("crs scanf");

    /* Time of client request (microseconds) */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &cru) != 1)
      die_painful_death("cru scanf");

    /* Source address */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    sscanf(nexttok, "%16Lx", (u64 *) cbuf2);
    do_idea_decrypt(cbuf2, cryptbuf, 8);
    cip = *((unsigned long *) cryptbuf);

    /* Source port */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%hu", &cpt) != 1)
      die_painful_death("cpt scanf");

    /* Destination address */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    sscanf(nexttok, "%16Lx", (u64 *) cbuf2);
    do_idea_decrypt(cbuf2, cryptbuf, 8);
    sip = *((unsigned long *) cryptbuf);

    /* Destination port */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%hu", &spt) != 1)
      die_painful_death("spt scanf");

    /* Time of server response (seconds) */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &sfs) != 1)
      die_painful_death("sfs scanf");

    /* Time of server response (microseconds) */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &sfu) != 1)
      die_painful_death("sfu scanf");

    /* Response headerlength */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &rhl) != 1)
      die_painful_death("rhl scanf");

    /* Response datalength */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &rdl) != 1)
      die_painful_death("rdl scanf");

    /* Time of server response end (seconds) */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &sls) != 1)
      die_painful_death("sls scanf");

    /* Time of server response end (microseconds) */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    if (sscanf(nexttok, "%lu", &slu) != 1)
      die_painful_death("slu scanf");

    if (version > 1) {
      /* Client pragmas */
      nexttok = strtok(NULL, " ");
      if (nexttok == NULL)
	break;
      if (sscanf(nexttok, "%u", &tmpu) != 1)
	die_painful_death("tmpu scanf");
      cpr = (unsigned char) tmpu;
    } else
      cpr = (unsigned char) 0xFF;

    if (version > 2) {
      nexttok = strtok(NULL, " ");
      if (nexttok == NULL)
	break;
      if (sscanf(nexttok, "%lu", &cims) != 1)
	die_painful_death("cims scanf");

      if (cpr & 0x08) {
	if (cims == (unsigned long) 0xFFFFFFFF)
	  cims = (unsigned long) 0;
      } else
	cims = (unsigned long) 0xFFFFFFFF;
    } else
      cims = (unsigned long) 0xFFFFFFFF;

    /* Server pragmas */
    if (version > 1) {
      nexttok = strtok(NULL, " ");
      if (nexttok == NULL)
	break;
      if (sscanf(nexttok, "%u", &tmpu) != 1)
	die_painful_death("tmpu scanf");
      spr = (unsigned char) tmpu;
    } else
      spr = (unsigned char) 0xFF;

    if (version > 2) {
      nexttok = strtok(NULL, " ");
      if (nexttok == NULL)
	break;
      if (sscanf(nexttok, "%lu", &sep) != 1)
	die_painful_death("sep scanf");
      nexttok = strtok(NULL, " ");
      if (nexttok == NULL)
	break;
      if (sscanf(nexttok, "%lu", &slm) != 1)
	die_painful_death("slm scanf");

      if (spr & 0x04) {
	if (sep == (unsigned long) 0xFFFFFFFF)
	  sep = (unsigned long) 0;
      } else
	sep = (unsigned long) 0xFFFFFFFF;
      if (spr & 0x08) {
	if (slm == (unsigned long) 0xFFFFFFFF)
	  slm = (unsigned long) 0;
      } else
	slm = (unsigned long) 0xFFFFFFFF;

    } else {
      sep = slm = (unsigned long) 0xFFFFFFFF;
    }

    /* URL len */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    sscanf(nexttok, "%hu", &url);

    urllen = url;
    paddedurllen = urllen+1;
    if (paddedurllen % 8 != 0) {
      paddedurllen += 8 - (paddedurllen % 8);
    }

    /* URL itself */
    nexttok = strtok(NULL, " :");
    if (nexttok == NULL)
      break;
    
    /* convert URL text to binary and decrypt*/
    for (i=0; i<2*paddedurllen; i+=2) {
      sscanf(nexttok+(unsigned long) i,
	     "%02x",
	     &(cryptbuf[i/2]));
    }
    do_idea_decrypt(cryptbuf, cbuf2, paddedurllen);
    
    /* Let's start writing stuff out */
    if (crs == 0) { 
      write(1, &longF, 4); write(1, &longF, 4);
    } else { 
      crs = htonl(crs); write(1, &crs, 4);
      cru = htonl(cru); write(1, &cru, 4);
    }
    if (sfs == 0) {
      write(1, &longF, 4); write(1, &longF, 4);
    } else {
      sfs = htonl(sfs); write(1, &sfs, 4);
      sfu = htonl(sfu); write(1, &sfu, 4);
    }
    if (sls == 0) {
      write(1, &longF, 4); write(1, &longF, 4);
    } else {
      sls = htonl(sls); write(1, &sls, 4);
      slu = htonl(slu); write(1, &slu, 4);
    }
    if (cip == 0) {
      write(1, &longF, 4); write(1, &shortF, 2);
    } else {
      cip = htonl(cip); write(1, &cip, 4);  /* already net order */
      cpt = htons(cpt); write(1, &cpt, 2);  /* already net order */
    }
    if (sip == 0) {
      write(1, &longF, 4); write(1, &shortF, 2);
    } else {
      sip = htonl(sip); write(1, &sip, 4);  /* already net order */
      spt = htons(spt); write(1, &spt, 2);  /* already net order */
    }
    if (version == 1) {
      write(1, &charF, 1);  write(1, &charF, 1);
    } else {
      write(1, &cpr, 1);
      write(1, &spr, 1);
    }
    cims = htonl(cims); write(1, &cims, 4);
    sep = htonl(sep);   write(1, &sep, 4);
    slm = htonl(slm);   write(1, &slm, 4);
    rhl = htonl(rhl);   write(1, &rhl, 4);
    rdl = htonl(rdl);   write(1, &rdl, 4);
    url -= 2;  /* get rid of pesky quotes around URL */
    url = htons(url);   write(1, &url, 2);
    if (ntohs(url) != strlen(cbuf2)-2) {
      fprintf(stderr, "%d %d %d\n", url, ntohs(url), strlen(cbuf2)-2);
      fprintf(stderr, "%s!!!!\n", cbuf2);
      /*die_painful_death();*/
    }
    write(1, cbuf2+1, ntohs(url));
  }
   
}

char *stream_ntoa(unsigned long addr)
{
    static char buf[100];
    sprintf(buf, "%lu.%lu.%lu.%lu",
        (addr >> 24),
        (addr >> 16) % 256,
        (addr >> 8) % 256,
        (addr) % 256);
    return buf;
}

void  do_read_or_die(int fd, char *buf, size_t length)
{
  int sofar = 0, next;

  while (1) {
    next = read(fd, buf + (unsigned long int) sofar, length-sofar);
    if (next <= 0) {
      exit(1);
    }
    sofar += next;
    if (sofar >= length)
      break;
  }
}

void  md5_plus_salt_text(char *text, char *buf)
{
  MD5_CTX mdcontext;

  MD5Init(&mdcontext);
  MD5Update(&mdcontext, MD5SALT, MD5SALTLEN);
  MD5Update(&mdcontext, text, strlen(text));
  MD5Final(&mdcontext);
  
  memcpy((void *) buf, (const void *) &(mdcontext.buf[0]), 
	 4*sizeof(unsigned int));
}

void  asc_init(void)
{
    int i;

    for(i=128;i<=255;++i)
    {
        sprintf(asc[i], "\x03%02x", i);
        asclen[i] = 3;
    }
    for(i=0;i<=31;++i)
    {
        sprintf(asc[i], "\x03^%c", i+64);
        asclen[i] = 3;
    }
    for(i=32;i<=126;++i)
    {
        sprintf(asc[i], "%c", i);
        asclen[i] = 1;
    }
    strcpy(asc[10], "\n"); asclen[10] = 1;
    strcpy(asc[13], "\x03\\r");
    strcpy(asc[27], "\x03\\e");
    strcpy(asc[8], "\x03\\b");
    strcpy(asc[7], "\x03\\a");
    strcpy(asc[9], "\x03\\t");
    strcpy(asc[12], "\x03\\f");
    strcpy(asc[127], "\x03^?");
}

int do_idea_encrypt(unsigned char *text, unsigned char *buf, int inlen)
{
    int      padded_url_len, i;
    unsigned char  *tmpo;

    iv = 0x12345678;
    en_key_idea(ideakey, en_key);
    de_key_idea(en_key, de_key);

    memcpy(buf, text, inlen);
    tmpo = buf;

    /* Now encrypt the URL */
    if (inlen % 8 != 0) {
      padded_url_len = 8 - (inlen % 8);
      tmpo = &(buf[inlen]);
      for (i=0; i<padded_url_len; i++) {
	*tmpo = '\0';
	tmpo++;
      }
      padded_url_len = inlen+padded_url_len;
    } else {
      padded_url_len = inlen;
    }

    size = padded_url_len / 8;
    tmpo = buf;
    while (size) {
      *(u64 *)tmpo ^= iv;
      idea_encrypt_block(en_key, tmpo, tmpo, 8);
      iv = *(u64 *) tmpo;
      tmpo += 8;
      size--;
    }
    tmpo = buf;
    return padded_url_len;
}

void  do_idea_decrypt(unsigned char *text, unsigned char *buf, int inlen)
{
    int      padded_url_len, i;
    unsigned char  *tmpo;

    iv = 0x12345678;
    en_key_idea(ideakey, en_key);
    de_key_idea(en_key, de_key);

    memcpy(buf, text, inlen);

    /* Now decrypt the URL */
    size = inlen / 8;
    tmpo = buf;
    while (size) {
      u64 tmp = *(u64 *)tmpo;
      idea_encrypt_block(de_key, tmpo, tmpo, 8);
      *(u64 *)tmpo ^= iv;
      iv = tmp;
      tmpo += 8;
      size--;
    }
    tmpo = buf;
}
