/* gif2grey.c: convert a GIF to a greyscale array.  Ian Goldberg 19971006
   Based on giftopnm.c: */
/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, 1991, 1993, David Koblas.  (koblas@netcom.com)    | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include <stdio.h>
#include "distinterface.h"
#include "gif2grey.h"
#include "../include/ARGS.h"

#define        MAXCOLORMAPSIZE         256

#define        TRUE    1
#define        FALSE   0

#define CM_RED         0
#define CM_GREEN       1
#define CM_BLUE                2

#define        MAX_LWZ_BITS            12

#define INTERLACE              0x40
#define LOCALCOLORMAP  0x80
#define BitSet(byte, bit)      (((byte) & (bit)) == (bit))

#define LM_to_uint(a,b)                        (((b)<<8)|(a))

static struct {
       unsigned int    Width;
       unsigned int    Height;
       unsigned char   ColorMap[3][MAXCOLORMAPSIZE];
       unsigned int    BitPixel;
       unsigned int    ColorResolution;
       unsigned int    Background;
       unsigned int    AspectRatio;
} GifScreen;

static struct {
       int     transparent;
       int     delayTime;
       int     inputFlag;
       int     disposal;
} Gif89;

typedef struct {
    unsigned char *buf;
    unsigned int size, read;
} mFILE;

#define verbose 0
#define ARGS(x) x

static DistillerStatus ReadGIF ARGS(( mFILE *fd, int imageNumber, unsigned char **outgrey, int *owidth, int *oheight ));
static DistillerStatus ReadColorMap ARGS(( mFILE *fd, int number, unsigned char buffer[3][MAXCOLORMAPSIZE] ));
static int DoExtension ARGS(( mFILE *fd, int label ));
static int GetDataBlock ARGS(( mFILE *fd, unsigned char  *buf ));
static int GetCode ARGS(( mFILE *fd, int code_size, int flag ));
static int LWZReadByte ARGS(( mFILE *fd, int flag, int input_code_size ));
static DistillerStatus ReadImage ARGS(( mFILE *fd, int len, int height, unsigned char cmap[3][MAXCOLORMAPSIZE], int interlace, int ignore, unsigned char **outgrey ));

static MonitorClientID mon;
static char msgbuf[5192];

#define pm_message0(s) MonitorClientSend(mon,"Distiller Messages",(s),"Log")
#define pm_error0(s) do { \
    MonitorClientSend(mon,"Distiller Messages",(s),"Log"); \
    return distFatalError; \
} while(0)
#define pm_message(s) do { \
    strcpy(msgbuf, s); \
    strcat(msgbuf, "\n"); \
    pm_message0(msgbuf); \
} while(0)
#define pm_message2(s,a1) do { \
    sprintf(msgbuf, s, a1); \
    strcat(msgbuf, "\n"); \
    pm_message0(msgbuf); \
} while(0)
#define pm_message3(s,a1,a2) do { \
    sprintf(msgbuf, s, a1, a2); \
    strcat(msgbuf, "\n"); \
    pm_message0(msgbuf); \
} while(0)
#define pm_message4(s,a1,a2,a3) do { \
    sprintf(msgbuf, s, a1, a2, a3); \
    strcat(msgbuf, "\n"); \
    pm_message0(msgbuf); \
} while(0)
#define pm_error(s) do { \
    strcpy(msgbuf, s); \
    strcat(msgbuf, "\n"); \
    pm_error0(msgbuf); \
} while(0)
#define pm_error2(s,a1) do { \
    sprintf(msgbuf, s, a1); \
    strcat(msgbuf, "\n"); \
    pm_error0(msgbuf); \
} while(0)
#define pm_error3(s,a1,a2) do { \
    sprintf(msgbuf, s, a1, a2); \
    strcat(msgbuf, "\n"); \
    pm_error0(msgbuf); \
} while(0)
#define pm_error4(s,a1,a2,a3) do { \
    sprintf(msgbuf, s, a1, a2, a3); \
    strcat(msgbuf, "\n"); \
    pm_error0(msgbuf); \
} while(0)

DistillerStatus
gif2grey_init(C_DistillerType distType, int argc, const char * const *argv)
{
    mon = DistillerGetMonitorClientID();
    return distOk;
}

void
gif2grey_clean(void)
{
}

DistillerStatus
gif2grey_main(unsigned char *ingif, unsigned int inlen,
		unsigned char **outgrey, int *owidth, int *oheight)
{
    mFILE gfile;

    gfile.buf = ingif;
    gfile.size = inlen;
    gfile.read = 0;

    Gif89.transparent = -1;
    Gif89.delayTime = -1;
    Gif89.inputFlag = -1;
    Gif89.disposal = 0;

    return ReadGIF(&gfile, 1, outgrey, owidth, oheight);
}

static int ReadOK(mFILE *file, unsigned char *buf, int len)
{
    if (file->size - file->read < len) return 0;
    memmove(buf, file->buf+file->read, len);
    file->read += len;
    return 1;
}

static DistillerStatus
ReadGIF(mFILE *fd, int imageNumber, unsigned char **outgrey, int *owidth,
	int *oheight)
{
       unsigned char   buf[16];
       unsigned char   c;
       unsigned char   localColorMap[3][MAXCOLORMAPSIZE];
       int             useGlobalColormap;
       int             bitPixel;
       int             imageCount = 0;
       char            version[4];
       DistillerStatus retval = distFatalError;

       if (! ReadOK(fd,buf,6))
               pm_error("error reading magic number" );

       if (strncmp((char *)buf,"GIF",3) != 0)
               pm_error("not a GIF file" );

       strncpy(version, (char *)buf + 3, 3);
       version[3] = '\0';

       if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0))
               pm_error("bad version number, not '87a' or '89a'" );

       if (! ReadOK(fd,buf,7))
               pm_error("failed to read screen descriptor" );

       GifScreen.Width           = LM_to_uint(buf[0],buf[1]);
       GifScreen.Height          = LM_to_uint(buf[2],buf[3]);
       GifScreen.BitPixel        = 2<<(buf[4]&0x07);
       GifScreen.ColorResolution = (((buf[4]&0x70)>>3)+1);
       GifScreen.Background      = buf[5];
       GifScreen.AspectRatio     = buf[6];

       if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
               if (ReadColorMap(fd,GifScreen.BitPixel,GifScreen.ColorMap))
                       pm_error("error reading global colormap" );
       }

       if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49) {
               float   r;
               r = ( (float) GifScreen.AspectRatio + 15.0 ) / 64.0;
               pm_message3("warning - non-square pixels; to fix do a 'pnmscale -%cscale %g'",
                   r < 1.0 ? 'x' : 'y',
                   r < 1.0 ? 1.0 / r : r );
       }

       for (;;) {
               if (! ReadOK(fd,&c,1))
                       pm_error("EOF / read error on image data" );

               if (c == ';') {         /* GIF terminator */
                       if (imageCount < imageNumber)
                               pm_error3("only %d image%s found in file",
                                        imageCount, imageCount>1?"s":"" );
                       break;
               }

               if (c == '!') {         /* Extension */
                       if (! ReadOK(fd,&c,1))
                               pm_error("OF / read error on extention function code");
                       DoExtension(fd, c);
                       continue;
               }

               if (c != ',') {         /* Not a valid start character */
                       pm_message2("bogus character 0x%02x, ignoring", (int) c );
                       continue;
               }

               ++imageCount;

               if (! ReadOK(fd,buf,9))
                       pm_error("couldn't read left/top/width/height");

               useGlobalColormap = ! BitSet(buf[8], LOCALCOLORMAP);

               bitPixel = 1<<((buf[8]&0x07)+1);

	       *owidth = LM_to_uint(buf[4],buf[5]);
	       *oheight = LM_to_uint(buf[6],buf[7]);
               if (! useGlobalColormap) {
                       if (ReadColorMap(fd, bitPixel, localColorMap))
                               pm_error("error reading local colormap" );
                       retval = ReadImage(fd, *owidth, *oheight,
                                 localColorMap,
                                 BitSet(buf[8], INTERLACE),
				 imageCount != imageNumber, outgrey);
               } else {
                       retval = ReadImage(fd, *owidth, *oheight,
                                 GifScreen.ColorMap,
                                 BitSet(buf[8], INTERLACE),
				 imageCount != imageNumber, outgrey);
               }
		if (imageCount == imageNumber) return retval;
       }
       return retval;
}

static DistillerStatus
ReadColorMap(fd,number,buffer)
mFILE           *fd;
int            number;
unsigned char  buffer[3][MAXCOLORMAPSIZE];
{
       int             i;
       unsigned char   rgb[3];
       int             flag;

       flag = TRUE;

       for (i = 0; i < number; ++i) {
               if (! ReadOK(fd, rgb, sizeof(rgb)))
                       pm_error("bad colormap" );

               buffer[CM_RED][i] = rgb[0] ;
               buffer[CM_GREEN][i] = rgb[1] ;
               buffer[CM_BLUE][i] = rgb[2] ;

               flag &= (rgb[0] == rgb[1] && rgb[1] == rgb[2]);
       }

       return FALSE;
}

static int
DoExtension(fd, label)
mFILE   *fd;
int    label;
{
       static char     buf[256];
       char            *str;

       switch (label) {
       case 0x01:              /* Plain Text Extension */
               str = "Plain Text Extension";
#ifdef notdef
               if (GetDataBlock(fd, (unsigned char*) buf) == 0)
                       ;

               lpos   = LM_to_uint(buf[0], buf[1]);
               tpos   = LM_to_uint(buf[2], buf[3]);
               width  = LM_to_uint(buf[4], buf[5]);
               height = LM_to_uint(buf[6], buf[7]);
               cellw  = buf[8];
               cellh  = buf[9];
               foreground = buf[10];
               background = buf[11];

               while (GetDataBlock(fd, (unsigned char*) buf) != 0) {
                       PPM_ASSIGN(image[ypos][xpos],
                                       cmap[CM_RED][v],
                                       cmap[CM_GREEN][v],
                                       cmap[CM_BLUE][v]);
                       ++index;
               }

               return FALSE;
#else
               break;
#endif
       case 0xff:              /* Application Extension */
               str = "Application Extension";
               break;
       case 0xfe:              /* Comment Extension */
               str = "Comment Extension";
               while (GetDataBlock(fd, (unsigned char*) buf) != 0) ;
               return FALSE;
       case 0xf9:              /* Graphic Control Extension */
               str = "Graphic Control Extension";
               (void) GetDataBlock(fd, (unsigned char*) buf);
               Gif89.disposal    = (buf[0] >> 2) & 0x7;
               Gif89.inputFlag   = (buf[0] >> 1) & 0x1;
               Gif89.delayTime   = LM_to_uint(buf[1],buf[2]);
               if ((buf[0] & 0x1) != 0)
                       Gif89.transparent = buf[3];

               while (GetDataBlock(fd, (unsigned char*) buf) != 0)
                       ;
               return FALSE;
       default:
               str = buf;
               sprintf(buf, "UNKNOWN (0x%02x)", label);
               break;
       }

       pm_message2("got a '%s' extension", str );

       while (GetDataBlock(fd, (unsigned char*) buf) != 0)
               ;

       return FALSE;
}

int    ZeroDataBlock = FALSE;

static int
GetDataBlock(fd, buf)
mFILE           *fd;
unsigned char  *buf;
{
       unsigned char   count;

       if (! ReadOK(fd,&count,1)) {
               pm_message("error in getting DataBlock size" );
               return -1;
       }

       ZeroDataBlock = count == 0;

       if ((count != 0) && (! ReadOK(fd, buf, count))) {
               pm_message("error in reading DataBlock" );
               return -1;
       }

       return count;
}

static int
GetCode(fd, code_size, flag)
mFILE   *fd;
int    code_size;
int    flag;
{
       static unsigned char    buf[280];
       static int              curbit, lastbit, done, last_byte;
       int                     i, j, ret;
       unsigned char           count;

       if (flag) {
               curbit = 0;
               lastbit = 0;
               done = FALSE;
               return 0;
       }

       if ( (curbit+code_size) >= lastbit) {
               if (done) {
                       if (curbit >= lastbit)
                               pm_error("ran off the end of my bits" );
                       return -1;
               }
               buf[0] = buf[last_byte-2];
               buf[1] = buf[last_byte-1];

               if ((count = GetDataBlock(fd, &buf[2])) == 0)
                       done = TRUE;

               last_byte = 2 + count;
               curbit = (curbit - lastbit) + 16;
               lastbit = (2+count)*8 ;
       }

       ret = 0;
       for (i = curbit, j = 0; j < code_size; ++i, ++j)
               ret |= ((buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;

       curbit += code_size;

       return ret;
}

static int
LWZReadByte(fd, flag, input_code_size)
mFILE   *fd;
int    flag;
int    input_code_size;
{
       static int      fresh = FALSE;
       int             code, incode;
       static int      code_size, set_code_size;
       static int      max_code, max_code_size;
       static int      firstcode, oldcode;
       static int      clear_code, end_code;
       static int      table[2][(1<< MAX_LWZ_BITS)];
       static int      stack[(1<<(MAX_LWZ_BITS))*2], *sp;
       register int    i;

       if (flag) {
               set_code_size = input_code_size;
               code_size = set_code_size+1;
               clear_code = 1 << set_code_size ;
               end_code = clear_code + 1;
               max_code_size = 2*clear_code;
               max_code = clear_code+2;

               GetCode(fd, 0, TRUE);
               
               fresh = TRUE;

               for (i = 0; i < clear_code; ++i) {
                       table[0][i] = 0;
                       table[1][i] = i;
               }
               for (; i < (1<<MAX_LWZ_BITS); ++i)
                       table[0][i] = table[1][0] = 0;

               sp = stack;

               return 0;
       } else if (fresh) {
               fresh = FALSE;
               do {
                       firstcode = oldcode =
                               GetCode(fd, code_size, FALSE);
               } while (firstcode == clear_code);
               return firstcode;
       }

       if (sp > stack)
               return *--sp;

       while ((code = GetCode(fd, code_size, FALSE)) >= 0) {
               if (code == clear_code) {
                       for (i = 0; i < clear_code; ++i) {
                               table[0][i] = 0;
                               table[1][i] = i;
                       }
                       for (; i < (1<<MAX_LWZ_BITS); ++i)
                               table[0][i] = table[1][i] = 0;
                       code_size = set_code_size+1;
                       max_code_size = 2*clear_code;
                       max_code = clear_code+2;
                       sp = stack;
                       firstcode = oldcode =
                                       GetCode(fd, code_size, FALSE);
                       return firstcode;
               } else if (code == end_code) {
                       int             count;
                       unsigned char   buf[260];

                       if (ZeroDataBlock)
                               return -2;

                       while ((count = GetDataBlock(fd, buf)) > 0)
                               ;

                       if (count != 0)
                               pm_message("missing EOD in data stream (common occurence)");
                       return -2;
               }

               incode = code;

               if (code >= max_code) {
                       *sp++ = firstcode;
                       code = oldcode;
               }

               while (code >= clear_code) {
                       *sp++ = table[1][code];
                       if (code == table[0][code])
                               pm_error("circular table entry BIG ERROR");
                       code = table[0][code];
               }

               *sp++ = firstcode = table[1][code];

               if ((code = max_code) <(1<<MAX_LWZ_BITS)) {
                       table[0][code] = oldcode;
                       table[1][code] = firstcode;
                       ++max_code;
                       if ((max_code >= max_code_size) &&
                               (max_code_size < (1<<MAX_LWZ_BITS))) {
                               max_code_size *= 2;
                               ++code_size;
                       }
               }

               oldcode = incode;

               if (sp > stack)
                       return *--sp;
       }
       return code;
}

static DistillerStatus
ReadImage(fd, len, height, cmap, interlace, ignore, outgrey)
mFILE   *fd;
int    len, height;
unsigned char  cmap[3][MAXCOLORMAPSIZE];
int    interlace, ignore;
unsigned char **outgrey;
{
       unsigned char   c;      
       int             v;
       int             xpos = 0, ypos = 0, pass = 0;

       /*
       **  Initialize the Compression routines
       */
       if (! ReadOK(fd,&c,1))
               pm_error("EOF / read error on image data" );

       if (LWZReadByte(fd, TRUE, c) < 0)
               pm_error("error reading image" );

       /*
       **  If this is an "uninteresting picture" ignore it.
       */
       if (ignore) {
               if (verbose)
                       pm_message("skipping image..." );

               while (LWZReadByte(fd, FALSE, c) >= 0)
                       ;
               return distOk;
       }

       if ((*outgrey = DistillerMalloc(len * height)) == NULL)
               pm_error("couldn't alloc space for image" );

       if (verbose)
               pm_message4("reading %d by %d%s GIF image",
                       len, height, interlace ? " interlaced" : "" );

       while ((v = LWZReadByte(fd,FALSE,c)) >= 0 ) {
	   (*outgrey)[ypos*len+xpos] =
	       (Gif89.transparent == v) ? 255 : (unsigned char)
					   ((cmap[CM_RED][v] +
					   cmap[CM_GREEN][v] +
					   cmap[CM_BLUE][v])/3);
               ++xpos;
               if (xpos == len) {
                       xpos = 0;
                       if (interlace) {
                               switch (pass) {
                               case 0:
                               case 1:
                                       ypos += 8; break;
                               case 2:
                                       ypos += 4; break;
                               case 3:
                                       ypos += 2; break;
                               }

                               if (ypos >= height) {
                                       ++pass;
                                       switch (pass) {
                                       case 1:
                                               ypos = 4; break;
                                       case 2:
                                               ypos = 2; break;
                                       case 3:
                                               ypos = 1; break;
                                       default:
                                               goto fini;
                                       }
                               }
                       } else {
                               ++ypos;
                       }
               }
               if (ypos >= height)
                       break;
       }

fini:
       if (LWZReadByte(fd,FALSE,c)>=0)
               pm_message("too much input data, ignoring extra...");

    return distOk;
}
