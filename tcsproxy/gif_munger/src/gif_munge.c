/*
 *  gif_munge.c
 *
 *  $Id: gif_munge.c,v 1.21 1997/08/30 21:34:35 fox Exp $
 *  Code stolen from various p{ng}m*.c programs by J. Poskanzer et al.
 *
 *  This code acts as a GIF distillation server for tcsproxy.
 *  Since multiple passes over the image may be required, but only one
 *  distillation will be in progress at any given time, double-buffering
 *  is used.  A "typical maximum" buffer is allocated and
 *  used for most distillation requests; the occasional request that
 *  would overflow this buffer allocates and then frees its own buffer,
 *  but in the common case we don't do any memory allocation.
 *
 *  See ARGS.h for the arguments to this distiller.
 */

#include "ppm.h"
#include "pnm.h"
#include "ppmcmap.h"
#include "gif_munge.h"
#include "../include/ARGS.h"
#include "../../frontend/include/ARGS.h"            /* to get frontend "quality" args */
#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "distinterface.h"
#include <stdio.h>

/*
 *  for setjmp
 */

jmp_buf jumpbuffer;

MonitorClientID mon;

/*
 *  Lookup table to map beginner interface "quality" arg
 *  Each tuple is GIFMUNGE_MODE, GIFMUNGE_NEWX, GIFMUNGE_BPP,
 *  for quality values 0...4.  Note that GIFMUNGE_RESCALE is
 *  passed with each request: since it's only needed if original HTML didn't
 *  have size hints, HTML munger inserts it when needed.  Also note that
 *  GIFMUNGE_ABSMAX doesn't make sense for "quality" -- it's a client-specific
 *  thing. 
 */

static struct {
  INT32 q_mode;
  INT32 q_newsize;
  INT32 q_bpp;
} quality_table[] = {
  { -2, 0, 1 },                 /* lowest quality - does some scaling */
  { 0, 0, 1 },                  /* no scaling, 1 bit/pxl */
  { 0, 0, 2 },                  /* no scaling, 2 bit/pxl */
  { 0, 0, 4 },                  /* no scaling, 4 bit/pxl */
  { 0, 0, 8 },                  /* no scaling, 8 bit/pxl */
};


/* Lookup tables for fast RGB -> luminance calculation. */

static int times77[256] = {
	    0,    77,   154,   231,   308,   385,   462,   539,
	  616,   693,   770,   847,   924,  1001,  1078,  1155,
	 1232,  1309,  1386,  1463,  1540,  1617,  1694,  1771,
	 1848,  1925,  2002,  2079,  2156,  2233,  2310,  2387,
	 2464,  2541,  2618,  2695,  2772,  2849,  2926,  3003,
	 3080,  3157,  3234,  3311,  3388,  3465,  3542,  3619,
	 3696,  3773,  3850,  3927,  4004,  4081,  4158,  4235,
	 4312,  4389,  4466,  4543,  4620,  4697,  4774,  4851,
	 4928,  5005,  5082,  5159,  5236,  5313,  5390,  5467,
	 5544,  5621,  5698,  5775,  5852,  5929,  6006,  6083,
	 6160,  6237,  6314,  6391,  6468,  6545,  6622,  6699,
	 6776,  6853,  6930,  7007,  7084,  7161,  7238,  7315,
	 7392,  7469,  7546,  7623,  7700,  7777,  7854,  7931,
	 8008,  8085,  8162,  8239,  8316,  8393,  8470,  8547,
	 8624,  8701,  8778,  8855,  8932,  9009,  9086,  9163,
	 9240,  9317,  9394,  9471,  9548,  9625,  9702,  9779,
	 9856,  9933, 10010, 10087, 10164, 10241, 10318, 10395,
	10472, 10549, 10626, 10703, 10780, 10857, 10934, 11011,
	11088, 11165, 11242, 11319, 11396, 11473, 11550, 11627,
	11704, 11781, 11858, 11935, 12012, 12089, 12166, 12243,
	12320, 12397, 12474, 12551, 12628, 12705, 12782, 12859,
	12936, 13013, 13090, 13167, 13244, 13321, 13398, 13475,
	13552, 13629, 13706, 13783, 13860, 13937, 14014, 14091,
	14168, 14245, 14322, 14399, 14476, 14553, 14630, 14707,
	14784, 14861, 14938, 15015, 15092, 15169, 15246, 15323,
	15400, 15477, 15554, 15631, 15708, 15785, 15862, 15939,
	16016, 16093, 16170, 16247, 16324, 16401, 16478, 16555,
	16632, 16709, 16786, 16863, 16940, 17017, 17094, 17171,
	17248, 17325, 17402, 17479, 17556, 17633, 17710, 17787,
	17864, 17941, 18018, 18095, 18172, 18249, 18326, 18403,
	18480, 18557, 18634, 18711, 18788, 18865, 18942, 19019,
	19096, 19173, 19250, 19327, 19404, 19481, 19558, 19635 };
static int times150[256] = {
	    0,   150,   300,   450,   600,   750,   900,  1050,
	 1200,  1350,  1500,  1650,  1800,  1950,  2100,  2250,
	 2400,  2550,  2700,  2850,  3000,  3150,  3300,  3450,
	 3600,  3750,  3900,  4050,  4200,  4350,  4500,  4650,
	 4800,  4950,  5100,  5250,  5400,  5550,  5700,  5850,
	 6000,  6150,  6300,  6450,  6600,  6750,  6900,  7050,
	 7200,  7350,  7500,  7650,  7800,  7950,  8100,  8250,
	 8400,  8550,  8700,  8850,  9000,  9150,  9300,  9450,
	 9600,  9750,  9900, 10050, 10200, 10350, 10500, 10650,
	10800, 10950, 11100, 11250, 11400, 11550, 11700, 11850,
	12000, 12150, 12300, 12450, 12600, 12750, 12900, 13050,
	13200, 13350, 13500, 13650, 13800, 13950, 14100, 14250,
	14400, 14550, 14700, 14850, 15000, 15150, 15300, 15450,
	15600, 15750, 15900, 16050, 16200, 16350, 16500, 16650,
	16800, 16950, 17100, 17250, 17400, 17550, 17700, 17850,
	18000, 18150, 18300, 18450, 18600, 18750, 18900, 19050,
	19200, 19350, 19500, 19650, 19800, 19950, 20100, 20250,
	20400, 20550, 20700, 20850, 21000, 21150, 21300, 21450,
	21600, 21750, 21900, 22050, 22200, 22350, 22500, 22650,
	22800, 22950, 23100, 23250, 23400, 23550, 23700, 23850,
	24000, 24150, 24300, 24450, 24600, 24750, 24900, 25050,
	25200, 25350, 25500, 25650, 25800, 25950, 26100, 26250,
	26400, 26550, 26700, 26850, 27000, 27150, 27300, 27450,
	27600, 27750, 27900, 28050, 28200, 28350, 28500, 28650,
	28800, 28950, 29100, 29250, 29400, 29550, 29700, 29850,
	30000, 30150, 30300, 30450, 30600, 30750, 30900, 31050,
	31200, 31350, 31500, 31650, 31800, 31950, 32100, 32250,
	32400, 32550, 32700, 32850, 33000, 33150, 33300, 33450,
	33600, 33750, 33900, 34050, 34200, 34350, 34500, 34650,
	34800, 34950, 35100, 35250, 35400, 35550, 35700, 35850,
	36000, 36150, 36300, 36450, 36600, 36750, 36900, 37050,
	37200, 37350, 37500, 37650, 37800, 37950, 38100, 38250 };
static int times29[256] = {
	    0,    29,    58,    87,   116,   145,   174,   203,
	  232,   261,   290,   319,   348,   377,   406,   435,
	  464,   493,   522,   551,   580,   609,   638,   667,
	  696,   725,   754,   783,   812,   841,   870,   899,
	  928,   957,   986,  1015,  1044,  1073,  1102,  1131,
	 1160,  1189,  1218,  1247,  1276,  1305,  1334,  1363,
	 1392,  1421,  1450,  1479,  1508,  1537,  1566,  1595,
	 1624,  1653,  1682,  1711,  1740,  1769,  1798,  1827,
	 1856,  1885,  1914,  1943,  1972,  2001,  2030,  2059,
	 2088,  2117,  2146,  2175,  2204,  2233,  2262,  2291,
	 2320,  2349,  2378,  2407,  2436,  2465,  2494,  2523,
	 2552,  2581,  2610,  2639,  2668,  2697,  2726,  2755,
	 2784,  2813,  2842,  2871,  2900,  2929,  2958,  2987,
	 3016,  3045,  3074,  3103,  3132,  3161,  3190,  3219,
	 3248,  3277,  3306,  3335,  3364,  3393,  3422,  3451,
	 3480,  3509,  3538,  3567,  3596,  3625,  3654,  3683,
	 3712,  3741,  3770,  3799,  3828,  3857,  3886,  3915,
	 3944,  3973,  4002,  4031,  4060,  4089,  4118,  4147,
	 4176,  4205,  4234,  4263,  4292,  4321,  4350,  4379,
	 4408,  4437,  4466,  4495,  4524,  4553,  4582,  4611,
	 4640,  4669,  4698,  4727,  4756,  4785,  4814,  4843,
	 4872,  4901,  4930,  4959,  4988,  5017,  5046,  5075,
	 5104,  5133,  5162,  5191,  5220,  5249,  5278,  5307,
	 5336,  5365,  5394,  5423,  5452,  5481,  5510,  5539,
	 5568,  5597,  5626,  5655,  5684,  5713,  5742,  5771,
	 5800,  5829,  5858,  5887,  5916,  5945,  5974,  6003,
	 6032,  6061,  6090,  6119,  6148,  6177,  6206,  6235,
	 6264,  6293,  6322,  6351,  6380,  6409,  6438,  6467,
	 6496,  6525,  6554,  6583,  6612,  6641,  6670,  6699,
	 6728,  6757,  6786,  6815,  6844,  6873,  6902,  6931,
	 6960,  6989,  7018,  7047,  7076,  7105,  7134,  7163,
	 7192,  7221,  7250,  7279,  7308,  7337,  7366,  7395 };

unsigned char hex[256];

static ppmbuf B1,B2;
static int current_max_x = GIFMUNGE_TYP_MAX_X;
static int current_max_y = GIFMUNGE_TYP_MAX_Y;

int
alloc_pnmbuf(ppmbuf *b, int cols, int rows)
{
  b->buf = (pixel *)DistillerMalloc(cols * rows * sizeof(pixel));
  b->rowarray = (pixel **)DistillerMalloc(rows * sizeof(pixel *));
  return ((b->buf == NULL || b->rowarray == NULL)
          ? 0
          : 1);
}

void
free_pnmbuf(ppmbuf *b) {
  DistillerFree(b->rowarray);
  DistillerFree(b->buf);
}

void
munge_init_ppmarray(ppmbuf *b, int cols, int rows)
{
  int i;

  for (i = 0; i < rows; i++) {
    b->rowarray[i] = &(b->buf[i*cols]);
  }
  b->rows = rows;
  b->cols = cols;
}

DistillerStatus
ComputeDistillationCost(Argument *args, int numArgs,
                        DistillerInput *din,
                        DistillationCost *cost)
{
  cost->estimatedTime_ms = DataLength(din);
  return distOk;
}

DistillerStatus
DistillerInit(C_DistillerType distType, int argc, const char * const *argv)
{
  /*
   *  we have to set up some memory.  a pm_array is actually an array of
   *  pointers (rows) which point into a single large block.  so for each
   *  buffer, we setup the array of pointers and the single large block,
   *  and for each distillation request, we "initialize" the space by
   *  rewriting the array of pointers so the indices are right.  this is
   *  an ugly hack required by pbm's representation of arrays.
   */
  int x;

  mon = DistillerGetMonitorClientID();

  if (alloc_pnmbuf(&B1, GIFMUNGE_TYP_MAX_X, GIFMUNGE_TYP_MAX_Y) == 0
      ||  alloc_pnmbuf(&B2, GIFMUNGE_TYP_MAX_X, GIFMUNGE_TYP_MAX_Y) == 0) {
    char p[80];
    (void)sprintf(p, "Cannot allocate %dx%d array\n",
                  GIFMUNGE_TYP_MAX_X, GIFMUNGE_TYP_MAX_Y);
    MonitorClientSend(mon, "Errors", p, "");
    return (distFatalError);
  }
  
  for (x=0; x<256; x++)
    hex[x] = 0;
  for (x='0'; x<='9'; x++)
    hex[x] = x-'0';
  for (x='a'; x<='f'; x++)
    hex[x] = x-'a'+10;
  for (x='A'; x<='F'; x++)
    hex[x] = x-'A'+10;

  /* m = MonitorClient_Create(*/

  DEBUG("distiller initialized ok\n");
  return distOk;
}

void
DistillerExit(void)
{
  free_pnmbuf(&B1);
  free_pnmbuf(&B2);
}
  

extern pixel **ppmpad_main(int, int, int, int, pixel, pixel**);
extern pixel parsecolor(char *);

DistillerStatus
DistillerMain(Argument *args, int nargs,
              DistillerInput *din,
              DistillerOutput *dout)
{
    int colors;
    int x,y;
    int origcolors;
    double newarea,area,scale;
    int newsize;
    int make_gray;
    int mode;
    int rescale;
    int bpp;
    INT32 absmax;
    int is_transparent = 0;
    pixel transcolorpixel;
    
    DataNeedsFree(dout, gm_False);
    PPM_ASSIGN(transcolorpixel, 0,0,0);
    if (setjmp(jumpbuffer) != 0) {
      /*
       *  fatal error occurred, so return immediately.
       */
      MonitorClientSend(mon, "Errors",
                        "Fatal error: exiting through longjmp()", "");
      return distFatalError;
    }

    /*
     *  Set a length of zero for the output, in case an error is encountered.
     */
    SetDataLength(dout, 0);
    SetData(dout, NULL);

    /*
     *  parse distillation arguments.  set default values for some
     *  things, then override them if args are specified.  Default will
     *  be to quantize only.
     */

    mode = 0;
    newsize = 0;
    bpp = 2;
    rescale = 0;
    absmax = LONG_MAX;

    for (x = 0; x < nargs; x++) {
      INT32 argval = ARG_INT(args[x]);
      switch(ARG_ID(args[x])) {
      case FRONT_QUALITY:
        /* set from table, but allow individual args to be overridden too. */
        if (argval >= 1 && argval <= 5) {
          argval--;             /* index into array */
          mode = quality_table[argval].q_mode;
          newsize = quality_table[argval].q_newsize;
          bpp = quality_table[argval].q_bpp;
        }
        break;
      case GIFMUNGE_MODE:
        mode = argval;
        break;
      case GIFMUNGE_BPP:
        bpp = argval;
        break;
      case GIFMUNGE_RESCALE:
        rescale = 1;
        break;
      case GIFMUNGE_NEWX:
        newsize = argval;
        break;
      case GIFMUNGE_ABSMAX:
        absmax = argval;
        break;
      default:
        break;
      }
    }

    if (bpp < 0) {
      make_gray = 1;
      bpp = -bpp;
    } else {
      make_gray = 0;
    }
    colors = 1 << bpp;
    /*
     *  Actual image processing happens here.  First read the source GIF
     *  and compute new x and y sizes based on bpp (compression ratio).
     *  If the new target size is zero, then just scale the image to the
     *  given pixel dimensions, rather than treating the dimensions as
     *  minimums.
     */

    /*
     *  If this image is larger than the current buffers, reallocate
     *  them to be large enough.
     */

    {
      int tmpx, tmpy;
      unsigned char *p = (unsigned char *)(DataPtr(din));
      tmpx = *(p+6) + (*(p+7) << 8);
      tmpy = *(p+8) + (*(p+9) << 8);

      if (tmpx > current_max_x || tmpy > current_max_y) {
        current_max_x = tmpx;
        current_max_y = tmpy;
        free_pnmbuf(&B1);
        free_pnmbuf(&B2);
        if (alloc_pnmbuf(&B1, current_max_x, current_max_y) == 0
            || alloc_pnmbuf(&B2, current_max_x, current_max_y) == 0) {
          SetData(dout,p); /* make output buffer same as input buffer */
          sprintf((char *)p,
                  "Could not realloc memory for %dx%d array", tmpx, tmpy);
          (void)MonitorClientSend(mon, "Errors", (char *)p, "");
          return (distFatalError);
        }
      }
    }

    /*
     *  Read the GIF from the input into buffer 1.
     */
    ReadGIF(DataPtr(din), 1, &is_transparent,
            &transcolorpixel, &B1);
    DEBUG("Finished reading gif\n");

    if (mode > 0) {
      /* target a particular output size in bytes */
      area = B1.rows * B1.cols;
      newarea = (double)mode * (area/(DataLength(din)));
      scale = sqrt(newarea/area);
      x = B1.cols*scale;
      y = B1.rows*scale;
      if (x < 10) x = 10;
      if (y < 10) y = 10;         /* pin to harmless values */
    } else if (mode < 0) {
      /* scale each axis by reciprocal of this amount */
      scale = -1.0/mode;
      x = B1.cols * scale;
      y = B1.rows * scale;
      if (x < 10) x = 10;
      if (y < 10) y = 10;         /* pin to harmless values */
    } else {  /* mode == 0 */
      /* scale to this x size (negative: to this y size) */
      if (newsize < 0) {
        y = -newsize;
        x = B1.cols * ((double)y / B1.rows);
      } else {
        x = newsize;
        y = B1.rows * ((double)x / B1.cols);
      }
    }

    /*
     *  Pin to absolute maximum X or Y, if given.
     */

    /* if necessary, convert to grayscale, in place. */

    if (make_gray) {
      unsigned long pxl;
      int i;
      pixel *pp;
      gray grayval;

      for (i=0; i<B1.rows; i++) {
        for (pxl = 0, pp = B1.rowarray[i];
             pxl < B1.cols;
             pxl++, pp++) {
          grayval = ( ( times77[PPM_GETR( *pp )] + times150[PPM_GETG( *pp )] +
                        times29[PPM_GETB( *pp )] ) >> 8 );
          PPM_ASSIGN(*pp, grayval, grayval, grayval);
        }
      }
      DEBUG("Finished gray\n");
    }
    /*
     *  Count colors in original image by constructing histogram.  If
     *  there are "too many", assume it's 255, since that's the GIF
     *  allowed max.
     */
    origcolors = 0;
    {
      colorhist_vector temp_chv;
      if ((temp_chv = ppm_computecolorhist(B1.rowarray, B1.cols, B1.rows, 32767, 
                                           &origcolors)) == (colorhist_vector)0)
        origcolors = 255;
      ppm_freecolorhist(temp_chv);
    }
    DEBUG("finished colorhist\n");
    
    /*
     *  Scale image to new dimensions. Store scaled image in buffer 2.
     */

    if (x==0) {
      /* no scaling desired */ 
      x = B1.cols;
      y = B1.rows;              /* dumb...the pnmscale call becomes a memcpy */
    }

    pnmscale_main(y, x, 0, &B1, &B2);

    DEBUG("finished scale\n");

    /*
     *  Quantize to new colors, optionally using Floyd-Sternberg
     *  correction.  Quantized image in place in buffer 2.
     */
    if (colors > 0) {
      if (colors > 255)
        colors = 255;
      ppmquant_main(colors, "", 1, &transcolorpixel, &B2);
    } else {
      ppmquant_main(origcolors, "", 1, &transcolorpixel, &B2);
    }
    DEBUG("finished quant\n");
    
    /*
     *  Output will be stored into the pixel buffer that was used to decompress
     *  the GIF originally.  (This assumes that an encoded GIF will be smaller
     *  than its raw-pixel equivalent at 3 bytes/pixel, probably a safe
     *  assumption.  This is safer than assuming it will be smaller than the
     *  original GIF, a condition that turns out not to be true for GIFs that
     *  are already well optimized.)
     *
     *  Args for ppmtogif_main:
     *    int interlace (nonzero=yes)
     *    int sorted_colormap (nonzero=yes)
     *    int transparent (nonzero=yes)
     *    pixel *transcolorpixel (color to use for trans)
     *    ppmbuf *pbuf - source buffer containing ppm
     *    void *outbuf - dest buffer for writing GIF data
     */

    SetData(dout,(B1.buf));

    if (is_transparent > 0) {
      SetDataLength(dout,
                    ppmtogif_main(1,0,1,&transcolorpixel,&B2, DataPtr(dout)));
    } else {
      SetDataLength(dout,
                    ppmtogif_main(1, 0, 0, NULL, &B2, DataPtr(dout))); 
    }
    strncpy(dout->mimeType, "image/gif", MAX_MIMETYPE);

    /*
     *  Return the new version only if it's smaller.
     *  BUG::for things like Pilot browser, will probably want to return the
     *  distilled version even if it's bigger, because it's been grayified,
     *  etc. or has other properties that client wants.
     */
    if (DataLength(dout) < DataLength(din)) {
      return distOk;
    } else {
      /* original was smaller, whaddaya know */
      return distBadInput;      /* let frontend bypass original. */
    }
}

pixel parsecolor (char *color)
{
    pixel p;

    PPM_ASSIGN(p, ((HEX(color[1])<<4) + HEX(color[2])),
               ((HEX(color[3])<<4) + HEX(color[4])),
               ((HEX(color[5])<<4) + HEX(color[6])));
    return(p);
}
    
