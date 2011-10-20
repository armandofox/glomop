/* idea.h */
#ifndef _IDEA_DOT_H
#define _IDEA_DOT_H

#define IDEA_AVAILABLE

#define IDEAKEYSIZE   16
#define IDEABLOCKSIZE 8
#define ROUNDS	      8
#define IDEAKEYLEN    (6*ROUNDS+4)
#define low16(x) ((x) & 0xffff)
#define MUL(x,y) (x=mul(low16(x),y))


typedef unsigned short int uint16;
typedef unsigned long int word32;
typedef uint16 idea_key[IDEAKEYLEN];

void en_key_idea(uint16 *,uint16 *);
void de_key_idea(uint16 *,uint16 *);
void idea_encrypt_block(idea_key,char *,char *,int);

#endif
