/*
 * cderror.h
 *
 * Copyright (C) 1994, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file defines the error and message codes for the cjpeg/djpeg
 * applications.  These strings are not needed as part of the JPEG library
 * proper.
 * Edit this file to add new codes, or to translate the message strings to
 * some other language.
 */

/*
 * To define the enum list of message codes, include this file without
 * defining macro JMESSAGE.  To create a message string table, include it
 * again with a suitable JMESSAGE definition (see jerror.c for an example).
 */

#ifndef CDERROR_H_INCL
#define CDERROR_H_INCL

#ifndef JMESSAGE

typedef enum {

#define JMESSAGE(code,string)	code ,

JMESSAGE(JMSG2_FIRSTADDONCODE=1000, NULL) /* Must be first entry! */

JMESSAGE(JERR2_BMP_BADCMAP, "Unsupported BMP colormap format")
JMESSAGE(JERR2_BMP_BADDEPTH, "Only 8- and 24-bit BMP files are supported")
JMESSAGE(JERR2_BMP_BADHEADER, "Invalid BMP file: bad header length")
JMESSAGE(JERR2_BMP_BADPLANES, "Invalid BMP file: biPlanes not equal to 1")
JMESSAGE(JERR2_BMP_COLORSPACE, "BMP output must be grayscale or RGB")
JMESSAGE(JERR2_BMP_COMPRESSED, "Sorry, compressed BMPs not yet supported")
JMESSAGE(JERR2_BMP_NOT, "Not a BMP file - does not start with BM")
JMESSAGE(JTRC2_BMP, "%ux%u 24-bit BMP image")
JMESSAGE(JTRC2_BMP_MAPPED, "%ux%u 8-bit colormapped BMP image")
JMESSAGE(JTRC2_BMP_OS2, "%ux%u 24-bit OS2 BMP image")
JMESSAGE(JTRC2_BMP_OS2_MAPPED, "%ux%u 8-bit colormapped OS2 BMP image")

JMESSAGE(JERR2_GIF_BUG, "GIF output got confused")
JMESSAGE(JERR2_GIF_CODESIZE, "Bogus GIF codesize %d")
JMESSAGE(JERR2_GIF_COLORSPACE, "GIF output must be grayscale or RGB")
JMESSAGE(JERR2_GIF_IMAGENOTFOUND, "Too few images in GIF file")
JMESSAGE(JERR2_GIF_NOT, "Not a GIF file")
JMESSAGE(JTRC2_GIF, "%ux%ux%d GIF image")
JMESSAGE(JTRC2_GIF_BADVERSION,
	 "Warning: unexpected GIF version number '%c%c%c'")
JMESSAGE(JTRC2_GIF_EXTENSION, "Ignoring GIF extension block of type 0x%02x")
JMESSAGE(JTRC2_GIF_NONSQUARE, "Caution: nonsquare pixels in input")
JMESSAGE(JWRN2_GIF_BADDATA, "Corrupt data in GIF file")
JMESSAGE(JWRN2_GIF_CHAR, "Bogus char 0x%02x in GIF file, ignoring")
JMESSAGE(JWRN2_GIF_ENDCODE, "Premature end of GIF image")
JMESSAGE(JWRN2_GIF_NOMOREDATA, "Ran out of GIF bits")

JMESSAGE(JERR2_PPM_COLORSPACE, "PPM output must be grayscale or RGB")
JMESSAGE(JERR2_PPM_NONNUMERIC, "Nonnumeric data in PPM file")
JMESSAGE(JERR2_PPM_NOT, "Not a PPM file")
JMESSAGE(JTRC2_PGM, "%ux%u PGM image")
JMESSAGE(JTRC2_PGM_TEXT, "%ux%u text PGM image")
JMESSAGE(JTRC2_PPM, "%ux%u PPM image")
JMESSAGE(JTRC2_PPM_TEXT, "%ux%u text PPM image")

JMESSAGE(JERR2_RLE_BADERROR, "Bogus error code from RLE library")
JMESSAGE(JERR2_RLE_COLORSPACE, "RLE output must be grayscale or RGB")
JMESSAGE(JERR2_RLE_DIMENSIONS, "Image dimensions (%ux%u) too large for RLE")
JMESSAGE(JERR2_RLE_EMPTY, "Empty RLE file")
JMESSAGE(JERR2_RLE_EOF, "Premature EOF in RLE header")
JMESSAGE(JERR2_RLE_MEM, "Insufficient memory for RLE header")
JMESSAGE(JERR2_RLE_NOT, "Not an RLE file")
JMESSAGE(JERR2_RLE_TOOMANYCHANNELS, "Cannot handle %d output channels for RLE")
JMESSAGE(JERR2_RLE_UNSUPPORTED, "Cannot handle this RLE setup")
JMESSAGE(JTRC2_RLE, "%ux%u full-color RLE file")
JMESSAGE(JTRC2_RLE_FULLMAP, "%ux%u full-color RLE file with map of length %d")
JMESSAGE(JTRC2_RLE_GRAY, "%ux%u grayscale RLE file")
JMESSAGE(JTRC2_RLE_MAPGRAY, "%ux%u grayscale RLE file with map of length %d")
JMESSAGE(JTRC2_RLE_MAPPED, "%ux%u colormapped RLE file with map of length %d")

JMESSAGE(JERR2_TGA_BADCMAP, "Unsupported Targa colormap format")
JMESSAGE(JERR2_TGA_BADPARMS, "Invalid or unsupported Targa file")
JMESSAGE(JERR2_TGA_COLORSPACE, "Targa output must be grayscale or RGB")
JMESSAGE(JTRC2_TGA, "%ux%u RGB Targa image")
JMESSAGE(JTRC2_TGA_GRAY, "%ux%u grayscale Targa image")
JMESSAGE(JTRC2_TGA_MAPPED, "%ux%u colormapped Targa image")

  JMSG2_LASTADDONCODE
} ADDON_MESSAGE_CODE_2;

#undef JMESSAGE

#endif
#endif

