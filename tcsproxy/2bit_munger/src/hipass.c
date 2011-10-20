/*
 *	hipass -
 *		Hi pass filter rows using a 3x3 kernel.
 *
 *				Paul Haeberli - 1989
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hipass.h"
#include "izoom.h"

static void xblur();
static void extrap();
static void addsrow();
static void subsrow();
static void copyrow();

hipass *newhipass(getfunc,xsize,ysize,mag)
int (*getfunc)();
int xsize, ysize;
float mag;
{
    hipass *hp; 
    int i;

    hp = (hipass *)malloc(sizeof(hipass));
    hp->getfunc = getfunc;
    hp->xsize = xsize;
    hp->ysize = ysize;
    hp->y = 0;
    hp->extrapval = 1024*mag;
    for(i=0; i<3; i++)
        hp->blurrows[i] = (short *)malloc(xsize*sizeof(short));
    for(i=0; i<2; i++)
        hp->pastrows[i] = (short *)malloc(xsize*sizeof(short));
    hp->acc = (short *)malloc(xsize*sizeof(short));
    if(xsize<3 || ysize<3 || hp->extrapval == 0) 
	hp->active = 0;
    else {
	hp->active = 1;
    }
    return hp;
}

void freehipass(hp)
hipass *hp; 
{
    int i;

    for(i=0; i<3; i++)
        free(hp->blurrows[i]);
    for(i=0; i<2; i++)
        free(hp->pastrows[i]);
    free(hp->acc);
    free(hp);
}

void gethipassrow(hp,buf,y)
hipass *hp;
short *buf;
int y;
{
    short *temp;
    int iy;

    if(y == 0)
	hp->y = 0;
    if(hp->y != y) {
	fprintf(stderr,"hpgetrow: y error\n");
	exit(1);
    }
    if(hp->active) {
	if (y == 0) {
	    memset(hp->acc,0,hp->xsize*sizeof(short));
	    for(iy=0; iy<2; iy++) {
		temp = hp->pastrows[0];
		hp->pastrows[0] = hp->pastrows[1];
		hp->pastrows[1] = temp;
		hp->getfunc(hp->pastrows[1],iy);
		xblur(hp->blurrows[iy],hp->pastrows[1],hp->xsize);
		addsrow(hp->acc,hp->blurrows[iy],hp->xsize);
	    }
	    copyrow(hp->pastrows[0],buf,hp->xsize);
	    extrap(buf,hp->acc,6,hp->xsize,hp->extrapval);
	} else if (y == hp->ysize-1) {
	    copyrow(hp->pastrows[1],buf,hp->xsize);
	    extrap(buf,hp->acc,6,hp->xsize,hp->extrapval);
	} else {
	    temp = hp->pastrows[0];
	    hp->pastrows[0] = hp->pastrows[1];
	    hp->pastrows[1] = temp;
	    hp->getfunc(hp->pastrows[1],y+1);
	    xblur(hp->blurrows[2],hp->pastrows[1],hp->xsize);
	    addsrow(hp->acc,hp->blurrows[2],hp->xsize);
	    copyrow(hp->pastrows[0],buf,hp->xsize);
	    extrap(buf,hp->acc,9,hp->xsize,hp->extrapval);
	    subsrow(hp->acc,hp->blurrows[0],hp->xsize);
	    temp = hp->blurrows[0];
	    hp->blurrows[0] = hp->blurrows[1];
	    hp->blurrows[1] = hp->blurrows[2];
	    hp->blurrows[2] = temp;
	}
    } else {
	hp->getfunc(buf,y);
    }
    hp->y++;
}

/*
 *	extrap -
 *		Extrapolate from the blurred image beyond the original
 *
 */
static void extrap(pix,acc,div,n,extrapval)
unsigned short *pix;
short *acc;
int div, n;
int extrapval;
{
    int delta, val;
    int mul, ext;

    mul = div;
    div = div*256;
    ext = extrapval;
    while(n--) {
	delta = (*pix * mul)-*acc++;
	val = *pix+(ext*delta)/div;
	if(val<0)
	    val = 0;
	if(val>255)
	    val = 255;
	*pix++ = val;
    }
}

/*
 *	xblur -
 *		Blur a row in the x direction
 *
 */
static void xblur(sptr,cptr,n)
unsigned short *sptr, *cptr;
int n;
{
    short acc;

    if (n<3) {
	fprintf(stderr,"xblur: n may not be less than 3\n");
	exit(1);
    }
    acc = cptr[0] + cptr[1];
    *sptr++ = (3*acc+1)>>1;
    acc += cptr[2];
    n-=3;
    while(n--) {
	*sptr++ = acc;
	acc -= cptr[0];
	acc += cptr[3];
	cptr++;
    }
    *sptr++ = acc;
    acc -= cptr[0];
    *sptr++ = (3*acc+1)>>1;
}

/*
 *      addsrow -
 *              Add two rows together
 *
 */
void addsrow(dptr,sptr,n)
short *dptr, *sptr;
int n;
{
    while(n>=8) {
        dptr[0] += sptr[0];
        dptr[1] += sptr[1];
        dptr[2] += sptr[2];
        dptr[3] += sptr[3];
        dptr[4] += sptr[4];
        dptr[5] += sptr[5];
        dptr[6] += sptr[6];
        dptr[7] += sptr[7];
        dptr += 8;
        sptr += 8;
        n -= 8;
    }
    while(n--)
        *dptr++ += *sptr++;
}

/*
 *      subsrow -
 *              Subtract two rows
 *
 */
void subsrow(dptr,sptr,n)
short *dptr, *sptr;
int n;
{
    while(n>=8) {
        dptr[0] -= sptr[0];
        dptr[1] -= sptr[1];
        dptr[2] -= sptr[2];
        dptr[3] -= sptr[3];
        dptr[4] -= sptr[4];
        dptr[5] -= sptr[5];
        dptr[6] -= sptr[6];
        dptr[7] -= sptr[7];
        dptr += 8;
        sptr += 8;
        n -= 8;
    }
    while(n--)
        *dptr++ -= *sptr++;
}

void copyrow(s,d,n)
short *s, *d;
int n;
{
    memmove(d,s,n*sizeof(short));
}

