/*
 *  Tbmp_munge: a "shell" around Ian Goldberg's ppmtoTbmp program, to
 *  allow it to be used as a distiller.
 *  A. Fox 9/97
 */

#include "../include/ARGS.h"
#include "distinterface.h" 
#include "ppmbufs.h"
#include <string.h>

/* ppmtoTbmp.c - read a portable pixmap and write a pilot Tbmp file
 *
 * Program by Ian Goldberg <iang@cs.berkeley.edu>
 *
 * Based on ppmtopuzz.c by Jef Paskanzer, from the netpbm-1mar1994 package.
 */

DistillerStatus ppm_to_tbmp(DistillerInput *din, DistillerOutput *dout,
                            int bitsperpixel);
MonitorClientID mon;

typedef DistillerStatus DistillerInitFunc(C_DistillerType, int, const char * const *);
typedef DistillerStatus DistillerMainFunc(Argument *args, int nargs,
                                            DistillerInput *din, DistillerOutput *dout);

#define MSGSIZE 256


DistillerStatus
DistillerInit(C_DistillerType dType, int argc, const char * const *argv)
{
  DistillerStatus result;
  extern DistillerInitFunc giftoppm_DistillerInit;
  extern DistillerInitFunc jpgtoppm_DistillerInit;
  extern DistillerInitFunc pnmscale_DistillerInit;
  
  mon = DistillerGetMonitorClientID();
  if ((result = giftoppm_DistillerInit(dType,argc,argv)) != distOk) {
    Message("giftoppm init failed!");
    return result;
  }
  if ((result = jpgtoppm_DistillerInit(dType,argc,argv)) != distOk) {
    Message("jpgtoppm init failed!");
    return result;
  }
  if ((result = pnmscale_DistillerInit(dType,argc,argv)) != distOk) {
    Message("pnmbufscale init failed!");
    return result;
  }
  Message("ppmtoTbmp munger started");
  return distOk;
}

DistillerStatus
DistillerMain(Argument *args, int nargs,
              DistillerInput *din, DistillerOutput *dout)
{
  int bitsperpixel = 1;
  DistillerStatus status = distOk;
  DistillerOutput d_tmp[3];
  int i;

  extern DistillerMainFunc ppmquant_DistillerMain;
  extern DistillerMainFunc pnmscale_DistillerMain;
  extern DistillerMainFunc giftoppm_DistillerMain;
  extern DistillerMainFunc jpgtoppm_DistillerMain;

  for (i = 0; i < nargs; i++) {
    switch(ARG_ID(args[i])) {
    case TBMP_2BIT:
      if (ARG_INT(args[i]) == 1)
        bitsperpixel = 2;
      break;
    default:
      break;
    }
  }

  memset(d_tmp,  0, sizeof(d_tmp));

  /*
   *  Convert gif or jpg to ppm if necessary
   */
  if ( (strcasecmp(MimeType(din), "image/jpeg") == 0)
       || (strcasecmp(MimeType(din), "image/jpg") == 0) ) {
    status = jpgtoppm_DistillerMain(args, nargs, din, &d_tmp[0]);
  } else if (strcasecmp(MimeType(din), "image/gif") == 0) {
    status = giftoppm_DistillerMain(args, nargs, din, &d_tmp[0]);
  } else if (strcasecmp(MimeType(din), "image/ppm") != 0) {
    Message("Bad input: not JPEG, GIF, or ppm");
    return distBadInput;
  }
  if (status != distOk) {
    Message("giftoppm or jpgtoppm failed");
    return status;
  }

  /*
   *  Scale down the image
   */
  status = pnmscale_DistillerMain(args, nargs, &d_tmp[0], &d_tmp[1]);
  if (status != distOk) {
    Message("pnmscale failed");
    goto FINISH;
  }

  /*
   *  Quantize to 4 colors
   */
  status = ppmquant_DistillerMain(args, nargs, &d_tmp[1], &d_tmp[2]);
  if (status != distOk) {
    Message("ppmquant failed");
    goto FINISH;
  }
  /*
   *  Convert to Tbmp
   */
  status = ppm_to_tbmp(&d_tmp[2], dout, bitsperpixel);
  if (status != distOk) {
    Message("ppmtotbmp failed");
    goto FINISH;
  }
  
  FINISH:
  /*
   *  all done...copy d_tmp to dout for real
   */
  for (i=0; i<sizeof(d_tmp)/sizeof(DistillerData); i++) {
    if (d_tmp[i].data.freeMe == gm_True) 
      DistillerFree(DataPtr(&d_tmp[i]));
    if (d_tmp[i].metadata.freeMe == gm_True)
      DistillerFree(MetadataPtr(&d_tmp[i]));
  }

  return status;
}

/*
 *  actual ppmtoTbmp code starts here.
 */

static int colcompare(const void *a, const void *b)
{
    colorhist_vector ch1 = (colorhist_vector)a;
    colorhist_vector ch2 = (colorhist_vector)b;
    return
	(int) PPM_GETR(ch1->color) - (int) PPM_GETR(ch2->color) +
	(int) PPM_GETG(ch1->color) - (int) PPM_GETG(ch2->color) +
	(int) PPM_GETB(ch1->color) - (int) PPM_GETB(ch2->color);
}

DistillerStatus
ppm_to_tbmp(DistillerInput *din, DistillerOutput *dout, int bitsperpixel)
{
  pixel** pixels;
  register pixel* pP;
  colorhist_vector chv;
  colorhash_table cht;
  int rows, cols, row, colors;
  register int col;
  pixval maxval;
  unsigned char outbyte, outbit;
  int chars_per_row;
  DistillerStatus retcode = distFatalError;   /* failsafe */
#ifdef DEBUGGING
  char msg[MSGSIZE];
#endif /* DEBUGGING */
  struct ppmbuf outbuf;  
  
  pixels = ppmbuf_readppm( DataPtr(din), &cols, &rows, &maxval );

  DebugMsg( "computing colormap..." );
  chv = ppm_computecolorhist( pixels, cols, rows, 1<<bitsperpixel, &colors );
  if ( chv == (colorhist_vector) 0 )
    {
      Message( "too many colors - run ppmquant first");
      goto FINISH;
    }
  qsort((char *)chv, colors, sizeof(struct colorhist_item), colcompare);
#ifdef DEBUGGING
  sprintf(msg, "%d colors found", colors );
  DebugMsg(msg);
#endif /*DEBUGGING*/

  /* alloc the output buffer. */

  chars_per_row = 2 * (bitsperpixel == 2 ? ((cols+7)/8) : ((cols+15)/16));
  ppmbuf_alloc(&outbuf, chars_per_row*rows + 24); /* for headers etc */

    /* Write Tbmp header. */
  ppmbuf_putshort(&outbuf, cols*bitsperpixel);   /* width */
  ppmbuf_putshort(&outbuf, rows );                    /* height */
  if (bitsperpixel == 2) {
    ppmbuf_putshort(&outbuf, chars_per_row );      /* chars/row */
  } else {
    ppmbuf_putshort(&outbuf, chars_per_row);
  }
  ppmbuf_putshort(&outbuf, 0 );                       /* flags */
  ppmbuf_putlong(&outbuf,  0 );                        /* pad */
  ppmbuf_putlong(&outbuf,  0 );                        /* pad */

    /* Convert color vector to color hash table, for fast lookup. */
  cht = ppm_colorhisttocolorhash( chv, colors );
  ppm_freecolorhist( chv );

    /* And write out the data. */
  for ( row = 0; row < rows; ++row ) {
    outbyte = 0x00;
    outbit = (bitsperpixel == 2) ? 6 : 7;
    for ( col = 0, pP = pixels[row]; col < cols; ++col, ++pP ) {
      register int color;

      color = ppm_lookupcolor( cht, pP );
      if ( color == -1 ) {
        Message("Color not found! Bleah!");
        ppmbuf_free(&outbuf);
        goto FINISH;
      }
      /*
        pm_error("color not found?!?  row=%d col=%d  r=%d g=%d b=%d",
        row, col, PPM_GETR(*pP), PPM_GETG(*pP),
        PPM_GETB(*pP) );
        */
      if (bitsperpixel==2) {
        if (color == 0) {
          outbyte |= (3 << outbit);
        } else if (color == 1 && colors != 2) {
          outbyte |= (2 << outbit);
        } else if (color == 2 && colors == 4) {
          outbyte |= (1 << outbit);
        }
      } else {
        if (color == 0) {
          outbyte |= (1 << outbit);
        }
      }
      if (!outbit) {
        ppmbuf_putc(&outbuf,outbyte);
        outbyte = 0x00;
        outbit = (bitsperpixel==2 ? 6 : 7);
      } else {
        outbit -= (bitsperpixel==2 ? 2 : 1);
      }
    }
    if (bitsperpixel==2) {
      /* Output any partial byte remaining */
      if (cols & 0x03) {
        ppmbuf_putc(&outbuf,outbyte);
      }
      if (!((cols+7) & 0x04)) {
        ppmbuf_putc(&outbuf,0x00);
      }
    } else {                /* 1 bit per pixel */
      /* Output any partial byte remaining */
      if (cols & 0x07) {
        ppmbuf_putc(&outbuf,outbyte);
      }
      if (!((cols+15) & 0x08)) {
        ppmbuf_putc(&outbuf,0x00);
      }
    }
  }

  /* all done. */

  if (bitsperpixel == 2) {
    SetMimeType(dout, "image/tbmp2");
  } else {
    SetMimeType(dout, "image/tbmp");
  }
  SetMetadataLength(dout, 0);
  MetadataNeedsFree(dout, gm_False);

  SetDataLength(dout, ppmbuf_length(&outbuf));
  SetData(dout, ppmbuf_base(&outbuf));
  DataNeedsFree(dout, gm_True);

  retcode = distOk;

FINISH:
  if (cht)
    ppm_freecolorhash(cht);
  ppm_freearray(pixels, rows);
  return retcode;
}
