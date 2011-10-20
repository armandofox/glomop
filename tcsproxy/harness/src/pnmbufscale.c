

/* pnmscale.c - read a portable anymap and scale it
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
* Modified to work with ppmbuf routines by Steve Gribble 9/97
*/

#include "pnm.h"
#include "ppmbufs.h"
#include "distinterface.h"
#include "../include/ARGS.h"
#include <assert.h>
#include <math.h>

#define SCALE 4096
#define HALFSCALE 2048

DistillerStatus
pnmscale(Argument *args, int nargs,
         DistillerInput *din, DistillerOutput *dout)
{
  xel* xelrow;
  xel* tempxelrow;
  xel* newxelrow;
  register xel* xP;
  register xel* nxP;
  int argn, specxscale, specyscale, specxsize, specysize, specxysize;
  int rows, cols, format, newformat, rowsread, newrows, newcols, newpixels;
  register int row, col, needtoreadrow;
  xelval maxval;
  float xscale, yscale;
  long sxscale, syscale;
  register long fracrowtofill, fracrowleft;
  long* rs;
  long* gs;
  long* bs;
  DistillerStatus retcode = distFatalError;
  struct ppmbuf inbuf;
  struct ppmbuf outbuf;

  SetData(dout, NULL);
  DataNeedsFree(dout, gm_False);
  SetDataLength(dout, 0);

  argn = 1;
  specxscale = specyscale = specxsize = specysize = specxysize = newpixels = 0;

  for (argn = 0; argn < nargs; argn++) {
    switch(ARG_ID(args[argn])) {
    case PNMSCALE_XSCALE:
      if ( specxscale ) {
	DebugMsg("already specified an x scale" );
	return distBadInput;
      }
      if ( specxsize ) {
	DebugMsg("only one of -xsize/-width and -xscale may be specified" );
	return distBadInput;
      }
      xscale = (float) ARG_DOUBLE(args[argn]);
      if ( xscale <= 0.0 ) {
	DebugMsg("x scale must be greater than 0" );
	return distBadInput;
      }
      specxscale = 1;
      break;
    case PNMSCALE_YSCALE:
      if ( specyscale ) {
	DebugMsg( "already specified a y scale" );
	return distBadInput;
      }
      if ( specysize ) {
	DebugMsg("only one of -ysize/-height and -yscale may be specified" );
	return distBadInput;
      }
      yscale = (float) ARG_DOUBLE(args[argn]);
      if ( yscale <= 0.0 ) {
	DebugMsg( "y scale must be greater than 0" );
	return distBadInput;
      }
      specyscale = 1;
      break;
    case PNMSCALE_XSIZE:
      if ( specxsize ) {
	DebugMsg( "already specified a width" );
	return distBadInput;
      }
      if ( specxscale ) {
	DebugMsg("only one of -xscale and -xsize/-width may be specified" );
	return distBadInput;
      }
      newcols = ARG_INT(args[argn]);
      if ( newcols <= 0 ) {
	DebugMsg( "new width must be greater than 0" );
	return distBadInput;
      }
      specxsize = 1;
      break;
    case PNMSCALE_YSIZE:
      if ( specysize ) {
	DebugMsg( "already specified a height" );
	return distBadInput;
      }
      if ( specyscale ) {
	DebugMsg("only one of -yscale and -ysize/-height may be specified" );
	return distBadInput;
      }
      newrows = ARG_INT(args[argn]);
      if ( newrows <= 0 ) {
	DebugMsg( "new height must be greater than 0" );
	return distBadInput;
      }
      specysize = 1;
      break;
    default:
      break;
    } /* switch */
  } /* for loop */

  if ( ! ( specxscale || specyscale || specxsize || specysize || newpixels ) ) {
      DebugMsg("Death!  No scale specified by arguments.");
      return distBadInput;
  }

  /*** OK - We've parsed arguments.  Let's read the input and get busy ***/

  pnm_pbmmaxval = PNM_MAXMAXVAL;  /* use larger value for better results */
  ppmbuf_alloc(&inbuf, Datalength(din)+100);  /* 100 for safety */
  ppmbuf_readppminit(&inbuf, &cols, &rows, &maxval, &format);

  /* Compute all sizes and scales. */
  if ( newpixels )
    if ( rows * cols <= newpixels )
      {
        newrows = rows;
        newcols = cols;
        xscale = yscale = 1.0;
      }
    else
      {
        xscale = yscale =
          sqrt( (float) newpixels / (float) cols / (float) rows );
        specxscale = specyscale = 1;
      }

  if ( specxysize )
    if ( (float) newcols / (float) newrows > (float) cols / (float) rows )
      specxsize = 0;
    else
      specysize = 0;
    
  if ( specxsize )
    xscale = (float) newcols / (float) cols;
  else if ( specxscale )
    newcols = cols * xscale + 0.999;

  if ( specysize )
    yscale = (float) newrows / (float) rows;
  else if ( specyscale )
    newrows = rows * yscale + 0.999;
  else
    if ( specxsize )
      {
        yscale = xscale;
        newrows = rows * yscale + 0.999;
      }
    else
      {
        yscale = 1.0;
        newrows = rows;
      }
    
  if ( ! ( specxsize || specxscale ) )
    if ( specysize )
      {
        xscale = yscale;
        newcols = cols * xscale + 0.999;
      }
    else
      {
        xscale = 1.0;
        newcols = cols;
      }

  sxscale = xscale * SCALE;
  syscale = yscale * SCALE;


  /***************** XXX THIS IS HOW FAR I GOT XXX *****************/
  xelrow = pnm_allocrow( cols );
  if ( newrows == rows )	/* shortcut Y scaling if possible */
    tempxelrow = xelrow;
  else
    tempxelrow = pnm_allocrow( cols );
  rs = (long*) pm_allocrow( cols, sizeof(long) );
  gs = (long*) pm_allocrow( cols, sizeof(long) );
  bs = (long*) pm_allocrow( cols, sizeof(long) );
  rowsread = 0;
  fracrowleft = syscale;
  needtoreadrow = 1;
  for ( col = 0; col < cols; ++col )
    rs[col] = gs[col] = bs[col] = HALFSCALE;
  fracrowtofill = SCALE;

  pnm_writepnminit( stdout, newcols, newrows, maxval, newformat, 0 );
  newxelrow = pnm_allocrow( newcols );

  for ( row = 0; row < newrows; ++row )
    {
      /* First scale Y from xelrow into tempxelrow. */
      if ( newrows == rows )	/* shortcut Y scaling if possible */
        {
          pnm_readpnmrow( ifp, xelrow, cols, maxval, format );
        }
      else
        {
          while ( fracrowleft < fracrowtofill )
            {
              if ( needtoreadrow )
                if ( rowsread < rows )
                  {
                    pnm_readpnmrow( ifp, xelrow, cols, maxval, format );
                    ++rowsread;
                    /* needtoreadrow = 0; */
                  }
              switch ( PNM_FORMAT_TYPE(format) )
                {
                case PPM_TYPE:
                  for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
                    {
                      rs[col] += fracrowleft * PPM_GETR( *xP );
                      gs[col] += fracrowleft * PPM_GETG( *xP );
                      bs[col] += fracrowleft * PPM_GETB( *xP );
                    }
                  break;

                default:
                  for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
                    gs[col] += fracrowleft * PNM_GET1( *xP );
                  break;
                }
              fracrowtofill -= fracrowleft;
              fracrowleft = syscale;
              needtoreadrow = 1;
            }
          /* Now fracrowleft is >= fracrowtofill, so we can produce a row. */
          if ( needtoreadrow )
            if ( rowsread < rows )
              {
                pnm_readpnmrow( ifp, xelrow, cols, maxval, format );
                ++rowsread;
                needtoreadrow = 0;
              }
          switch ( PNM_FORMAT_TYPE(format) )
            {
            case PPM_TYPE:
              for ( col = 0, xP = xelrow, nxP = tempxelrow;
                    col < cols; ++col, ++xP, ++nxP )
                {
                  register long r, g, b;

                  r = rs[col] + fracrowtofill * PPM_GETR( *xP );
                  g = gs[col] + fracrowtofill * PPM_GETG( *xP );
                  b = bs[col] + fracrowtofill * PPM_GETB( *xP );
                  r /= SCALE;
                  if ( r > maxval ) r = maxval;
                  g /= SCALE;
                  if ( g > maxval ) g = maxval;
                  b /= SCALE;
                  if ( b > maxval ) b = maxval;
                  PPM_ASSIGN( *nxP, r, g, b );
                  rs[col] = gs[col] = bs[col] = HALFSCALE;
                }
              break;

            default:
              for ( col = 0, xP = xelrow, nxP = tempxelrow;
                    col < cols; ++col, ++xP, ++nxP )
                {
                  register long g;

                  g = gs[col] + fracrowtofill * PNM_GET1( *xP );
                  g /= SCALE;
                  if ( g > maxval ) g = maxval;
                  PNM_ASSIGN1( *nxP, g );
                  gs[col] = HALFSCALE;
                }
              break;
            }
          fracrowleft -= fracrowtofill;
          if ( fracrowleft == 0 )
            {
              fracrowleft = syscale;
              needtoreadrow = 1;
            }
          fracrowtofill = SCALE;
        }

      /* Now scale X from tempxelrow into newxelrow and write it out. */
      if ( newcols == cols )	/* shortcut X scaling if possible */
        pnm_writepnmrow( stdout, tempxelrow, newcols, maxval, newformat, 0 );
      else
        {
          register long r, g, b;
          register long fraccoltofill, fraccolleft;
          register int needcol;

          nxP = newxelrow;
          fraccoltofill = SCALE;
          r = g = b = HALFSCALE;
          needcol = 0;
          for ( col = 0, xP = tempxelrow; col < cols; ++col, ++xP )
            {
              fraccolleft = sxscale;
              while ( fraccolleft >= fraccoltofill )
                {
                  if ( needcol )
                    {
                      ++nxP;
                      r = g = b = HALFSCALE;
                    }
                  switch ( PNM_FORMAT_TYPE(format) )
                    {
                    case PPM_TYPE:
                      r += fraccoltofill * PPM_GETR( *xP );
                      g += fraccoltofill * PPM_GETG( *xP );
                      b += fraccoltofill * PPM_GETB( *xP );
                      r /= SCALE;
                      if ( r > maxval ) r = maxval;
                      g /= SCALE;
                      if ( g > maxval ) g = maxval;
                      b /= SCALE;
                      if ( b > maxval ) b = maxval;
                      PPM_ASSIGN( *nxP, r, g, b );
                      break;

                    default:
                      g += fraccoltofill * PNM_GET1( *xP );
                      g /= SCALE;
                      if ( g > maxval ) g = maxval;
                      PNM_ASSIGN1( *nxP, g );
                      break;
                    }
                  fraccolleft -= fraccoltofill;
                  fraccoltofill = SCALE;
                  needcol = 1;
                }
              if ( fraccolleft > 0 )
                {
                  if ( needcol )
                    {
                      ++nxP;
                      r = g = b = HALFSCALE;
                      needcol = 0;
                    }
                  switch ( PNM_FORMAT_TYPE(format) )
                    {
                    case PPM_TYPE:
                      r += fraccolleft * PPM_GETR( *xP );
                      g += fraccolleft * PPM_GETG( *xP );
                      b += fraccolleft * PPM_GETB( *xP );
                      break;

                    default:
                      g += fraccolleft * PNM_GET1( *xP );
                      break;
                    }
                  fraccoltofill -= fraccolleft;
                }
            }
          if ( fraccoltofill > 0 )
            {
              --xP;
              switch ( PNM_FORMAT_TYPE(format) )
                {
                case PPM_TYPE:
                  r += fraccoltofill * PPM_GETR( *xP );
                  g += fraccoltofill * PPM_GETG( *xP );
                  b += fraccoltofill * PPM_GETB( *xP );
                  break;

                default:
                  g += fraccoltofill * PNM_GET1( *xP );
                  break;
                }
            }
          if ( ! needcol )
            {
              switch ( PNM_FORMAT_TYPE(format) )
                {
                case PPM_TYPE:
                  r /= SCALE;
                  if ( r > maxval ) r = maxval;
                  g /= SCALE;
                  if ( g > maxval ) g = maxval;
                  b /= SCALE;
                  if ( b > maxval ) b = maxval;
                  PPM_ASSIGN( *nxP, r, g, b );
                  break;

                default:
                  g /= SCALE;
                  if ( g > maxval ) g = maxval;
                  PNM_ASSIGN1( *nxP, g );
                  break;
                }
            }
          /*          pnm_writepnmrow( stdout, newxelrow, newcols, maxval, newformat, 0 ); */
          ppmbuf_writeppmrow(&outbuf, newxelrow, newcols, maxval, newformat, 1);
        }
    }



}
