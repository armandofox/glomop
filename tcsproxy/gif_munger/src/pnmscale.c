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
*/

#include <math.h>
#include <string.h>
#include "pnm.h"
#include "gif_munge.h"

#define SCALE 4096
#define HALFSCALE 2048

int
pnmscale_main(int newrows, int newcols, int newpixels, ppmbuf *gif,
              ppmbuf *pnm)
    {
    xel* xelrow;
    xel* tempxelrow;
    xel* newxelrow;
    register xel* xP;
    register xel* nxP;

    int rows, cols, format, newformat, rowsread;
    register int row, col, needtoreadrow;
    xelval maxval;
    float xscale, yscale;
    long sxscale, syscale;
    register long fracrowtofill, fracrowleft;
    long* rs;
    long* gs;
    long* bs;
    xel **xelrowptr;
    xel **scaledpnmrowptr;
    int specxscale=0, specyscale=0, specxsize = 0;
    int specysize=0, specxysize=0;

    if (newrows < 1) {
        specxscale = specyscale = 1;
        xscale = yscale = -1.0/newrows;
    } else if (newpixels == 0)
      specxsize = specysize = specxysize = 1;

    pnm_pbmmaxval = PNM_MAXMAXVAL;  /* use larger value for better results */

    /*
     *  gifmunch: Following call is replaced by importing global values.
     */
    /* pnm_readpnminit( ifp, &cols, &rows, &maxval, &format ); */

    cols = gif->cols;
    rows = gif->rows;
    maxval = (pixval)255;
    format = gif->format;

    /* Promote PBM files to PGM. */
    if ( PNM_FORMAT_TYPE(format) == PBM_TYPE )
	{
        newformat = PGM_TYPE;
        /*	fprintf(stderr, "promoting from PBM to PGM" ); */
	}
    else
        newformat = format;

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

    
    xelrow = pnm_allocrow( cols );

    /*
     *  gifmunch: assign params of scaled pnm to global vars, and alloc
     *  a new array to hold the scaled pnm.
     */

    pnm->rows = newrows;
    pnm->cols = newcols;
    pnm->format = newformat;
    munge_init_ppmarray(pnm, newcols, newrows);

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

    /*
     *  gifmunch: writepnminit eliminated
     */
    /* pnm_writepnminit( stdout, newcols, newrows, maxval, newformat, 0); */

    newxelrow = pnm_allocrow( newcols ); 

    xelrowptr = gif->rowarray;
    scaledpnmrowptr = pnm->rowarray;

    for ( row = 0; row < newrows; ++row )
      {
          
	/* First scale Y from xelrow into tempxelrow. */
	if ( newrows == rows )	/* shortcut Y scaling if possible */
          {
              /*
               *  gifmunch: pnm_readpnmrow replaced by memcpy.
               */
              /* pnm_readpnmrow( ifp, xelrow, cols, maxval, format );*/

              memcpy(xelrow, *xelrowptr++, (size_t)(cols * sizeof(xel)));
              
	    }
	else
	    {
	    while ( fracrowleft < fracrowtofill )
		{
		if ( needtoreadrow )
		    if ( rowsread < rows )
			{
/*                            pnm_readpnmrow( ifp, xelrow, cols, maxval, */
/*                            format ); */
                            memcpy(xelrow, *xelrowptr++, (size_t)(cols * sizeof(xel)));

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
/*                        pnm_readpnmrow( ifp, xelrow, cols, maxval, */
/*                        format ); */
                        memcpy(xelrow, *xelrowptr++, (size_t)(cols * sizeof(xel)));

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
/*          pnm_writepnmrow( stdout, tempxelrow, newcols, maxval, */
/*          newformat, 0 ); */
          memcpy(*(scaledpnmrowptr++), tempxelrow,
                 (size_t)(newcols * sizeof(xel)));
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
/*	    pnm_writepnmrow( stdout, newxelrow, newcols, maxval, */
/*newformat, 0 ); */
            memcpy(*scaledpnmrowptr++, newxelrow,
                   (size_t)(newcols * sizeof(xel)));
	    }
	}

    pnm_freerow(xelrow);
    if (xelrow != tempxelrow)
      pnm_freerow(tempxelrow);
    pnm_freerow(rs);
    pnm_freerow(gs);
    pnm_freerow(bs);
    pnm_freerow(newxelrow);
    return 1;
}
