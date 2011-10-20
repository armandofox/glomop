/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, 1991, 1993, David Koblas.  (koblas@netcom.com)    | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */



#include <sys/types.h>
#include <string.h>
#include       "pnm.h"
#include "gif_munge.h"

/*
 *  Number of transparent gifs seen so far -- something we'd like to monitor
 */

static int ntrans = 0;

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
       /*
       **
       */
       int             GrayScale;
} GifScreen;

static struct {
       int     transparent;
       int     delayTime;
       int     inputFlag;
       int     disposal;
} Gif89 = { -1, -1, -1, 0 };

pixel  *Image = NULL;
int    verbose = 1;
int    showComment = 0;


static int ReadColorMap ARGS(( int number, unsigned char buffer[3][MAXCOLORMAPSIZE], int *flag ));
static int DoExtension ARGS(( int label ));
static int GetDataBlock ARGS(( unsigned char  *buf ));
static int GetCode ARGS(( int code_size, int flag ));
static int LWZReadByte ARGS(( int flag, int input_code_size ));
static void ReadImage ARGS(( int len, int height, unsigned
                             char cmap[3][MAXCOLORMAPSIZE], int gray,
                             int interlace, int ignore, ppmbuf *buf ));

static signed char *fd;

void
ReadGIF(bufferptr, imageNumber, transparent_p, transcolorpixel, pnmbufp)
unsigned char *bufferptr;
int    imageNumber;
int *transparent_p;
pixel *transcolorpixel;
ppmbuf *pnmbufp;
{
       unsigned char   buf[16];
       unsigned char   c;
       unsigned char   localColorMap[3][MAXCOLORMAPSIZE];
       int             grayScale;
       int             useGlobalColormap;
       int             bitPixel;
       int             imageCount = 0;
       char            version[4];

       fd = (signed char *)bufferptr;
       memcpy((void*)buf, (void*)fd, (size_t)6);
       fd+=6;
       if (strncmp((char *)buf,"GIF",3) != 0) {
         /*         fprintf(stderr, "Not a GIF! begins with %s\n", (char*)buf); */
         longjmp(jumpbuffer, GIFMUNGE_FATAL_ERROR);
       }

       strncpy(version, (char *)buf + 3, 3);
       version[3] = '\0';

       if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0)) {
         /*         fprintf(stderr,"bad version number, not '87a' or '89a'" ); */
         longjmp(jumpbuffer, GIFMUNGE_FATAL_ERROR);
       }

       memcpy((void*)buf,(void*)fd,(size_t)7);
       fd+=7;

       GifScreen.Width           = LM_to_uint(buf[0],buf[1]);
       GifScreen.Height          = LM_to_uint(buf[2],buf[3]);
       GifScreen.BitPixel        = 2<<(buf[4]&0x07);
       GifScreen.ColorResolution = (((buf[4]&0x70)>>3)+1);
       GifScreen.Background      = buf[5];
       GifScreen.AspectRatio     = buf[6];

       if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
         DEBUG("reading global cmap\n");
         if (ReadColorMap(GifScreen.BitPixel,GifScreen.ColorMap,
                          &GifScreen.GrayScale)) {
           /*           fprintf(stderr,"error reading global colormap" ); */
           longjmp(jumpbuffer, GIFMUNGE_FATAL_ERROR);
         }
       }

       if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49)
         DEBUG("warning - non-square pixels; to fix do 'pnmscale'");
         
       for (;;) {
               c= *(fd++);
               if (c == ';') {         /* GIF terminator */
                       if (imageCount < imageNumber)
                               pm_error("only %d image%s found in file",
                                        imageCount, imageCount>1?"s":"" );
                       if (Gif89.transparent != -1) {
                         unsigned char tr = Gif89.transparent;
                         char numstr[12];
                         PPM_ASSIGN(*transcolorpixel,
                                    GifScreen.ColorMap[CM_RED][tr],
                                    GifScreen.ColorMap[CM_GREEN][tr],
                                    GifScreen.ColorMap[CM_BLUE][tr]);
                         *transparent_p = 1;
                         ntrans++;
                         sprintf(numstr,"%d",ntrans);
                         MonitorClientSend(mon, "Transparent GIFs", numstr, "");
                       }
                       return;
               }

               if (c == '!') {         /* Extension */
                       c= *(fd++);
                       DoExtension(c);
                       continue;
               }

               if (c != ',') {         /* Not a valid start character */
                       DEBUG2("bogus character 0x%02x, ignoring", (int) c );
                       continue;
               }

               ++imageCount;

               memcpy((void*)buf,(void*)fd,(size_t)9);
               fd+=9;

               useGlobalColormap = ! BitSet(buf[8], LOCALCOLORMAP);

               bitPixel = 1<<((buf[8]&0x07)+1);

               if (! useGlobalColormap) {
                       if (ReadColorMap(bitPixel, localColorMap, &grayScale))
                               pm_error("error reading local colormap" );
                       DEBUG("reading with local colormap\n");
                       ReadImage( LM_to_uint(buf[4],buf[5]),
                                 LM_to_uint(buf[6],buf[7]), 
                                 localColorMap, grayScale,
                                 BitSet(buf[8], INTERLACE), imageCount
                                 != imageNumber, pnmbufp);
               } else {
                       DEBUG("reading with global colormap\n");
                       ReadImage( LM_to_uint(buf[4],buf[5]),
                                 LM_to_uint(buf[6],buf[7]), 
                                 GifScreen.ColorMap, GifScreen.GrayScale,
                                 BitSet(buf[8], INTERLACE), imageCount
                                 != imageNumber, pnmbufp);
               }

       }
}

static int
ReadColorMap(number,buffer,pbm_format)
int            number;
unsigned char  buffer[3][MAXCOLORMAPSIZE];
int            *pbm_format;
{
       int             i;
       unsigned char   rgb[3];
       int             flag;

       flag = TRUE;

       for (i = 0; i < number; ++i) {
               memcpy((void*)rgb,(void*)fd,sizeof(rgb));
               fd+=sizeof(rgb);
               buffer[CM_RED][i] = rgb[0] ;
               buffer[CM_GREEN][i] = rgb[1] ;
               buffer[CM_BLUE][i] = rgb[2] ;

               flag &= (rgb[0] == rgb[1] && rgb[1] == rgb[2]);
       }

       if (flag)
               *pbm_format = (number == 2) ? PBM_TYPE : PGM_TYPE;
       else
               *pbm_format = PPM_TYPE;

       return FALSE;
}

static int
DoExtension(label)
int    label;
{
       static char     buf[256];
       char            *str;

       switch (label) {
       case 0x01:              /* Plain Text Extension */
               str = "Plain Text Extension";
#ifdef notdef
               if (GetDataBlock((unsigned char*) buf) == 0)
                       ;

               lpos   = LM_to_uint(buf[0], buf[1]);
               tpos   = LM_to_uint(buf[2], buf[3]);
               width  = LM_to_uint(buf[4], buf[5]);
               height = LM_to_uint(buf[6], buf[7]);
               cellw  = buf[8];
               cellh  = buf[9];
               foreground = buf[10];
               background = buf[11];

               while (GetDataBlock((unsigned char*) buf) != 0) {
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
               /*               fprintf(stderr, "Application ext code %d\n",label); */
               break;
       case 0xfe:              /* Comment Extension */
               str = "Comment Extension";
               while (GetDataBlock((unsigned char*) buf) != 0) {
                       if (showComment)
                               DEBUG2("gif comment: %s", buf );
               }
               return FALSE;
       case 0xf9:              /* Graphic Control Extension */
               str = "Graphic Control Extension";
               (void) GetDataBlock((unsigned char*) buf);
               Gif89.disposal    = (buf[0] >> 2) & 0x7;
               Gif89.inputFlag   = (buf[0] >> 1) & 0x1;
               Gif89.delayTime   = LM_to_uint(buf[1],buf[2]);
               if ((buf[0] & 0x1) != 0)
                       Gif89.transparent = buf[3];

               while (GetDataBlock((unsigned char*) buf) != 0)
                       ;
               return FALSE;
       default:
               str = buf;
               sprintf(buf, "UNKNOWN (0x%02x)", label);
               break;
       }

       {
         int ct;
         while ((ct = GetDataBlock((unsigned char*) buf)) != 0) {
           /*           fprintf(stderr, "Discarded %d bytes\n", ct); */
         }
       }

       return FALSE;
}

int    ZeroDataBlock = FALSE;

static int
GetDataBlock( buf)
unsigned char  *buf;
{
       unsigned char   count;

       count=*(fd++);
       ZeroDataBlock = count == 0;
       if (count>0)
         memcpy((void*)buf,(void*)fd,(size_t)count);
       fd += count;
       return count;
}

static int
GetCode( code_size, flag)
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

               if ((count = GetDataBlock( &buf[2])) == 0)
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
LWZReadByte( flag, input_code_size)
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

               GetCode( 0, TRUE);
               
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
                               GetCode( code_size, FALSE);
               } while (firstcode == clear_code);
               return firstcode;
       }

       if (sp > stack) {
                code = *--sp;
               return code;
       }
       while ((code = GetCode( code_size, FALSE)) >= 0) {
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
                                       GetCode( code_size, FALSE);
                       return firstcode;
               } else if (code == end_code) {
                       int             count;
                       unsigned char   buf[260];

                       if (ZeroDataBlock) {
                         return -2;
                       }
                       while ((count = GetDataBlock( buf)) > 0)
                               ;

                       if (count != 0)
                         DEBUG("missing EOD in data stream (common occurence)");
                       return -2;
               }

               incode = code;

               if (code >= max_code) {
                       *sp++ = firstcode;
                       code = oldcode;
               }

               while (code >= clear_code) {
                       *sp++ = table[1][code];
                       if (code == table[0][code]) {
                         /*                         fprintf(stderr,"circular table entry BIG ERROR"); */
                         longjmp(jumpbuffer, GIFMUNGE_FATAL_ERROR);
                       }
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

               if (sp > stack) {
                 code = *--sp;
                 return code;
               }
       }
       return code;
}

static void
ReadImage( len, height, cmap, pbm_format, interlace, ignore, pnmbufp)
int    len, height;
unsigned char  cmap[3][MAXCOLORMAPSIZE];
int    pbm_format, interlace, ignore;
ppmbuf *pnmbufp;
{
       unsigned char   c;      
       int             v;
       int             xpos = 0, ypos = 0, pass = 0;
       pixel           **image;

       /*
       **  Initialize the Compression routines
       */
       c=*(fd++);
       if (LWZReadByte( TRUE, c) < 0)
               pm_error("error reading image" );

       /*
       **  If this is an "uninteresting picture" ignore it.
       */
       if (ignore) {
               while (LWZReadByte( FALSE, c) >= 0)
                       ;
               return;
       }

       /*
        * initialize image buffer pointers
        */

       image = pnmbufp->rowarray;
       munge_init_ppmarray(pnmbufp, len, height);

       DEBUG2("len=%d ",len); DEBUG2("ht=%d", height);
       /* fprintf(stderr,"interlace=%d",interlace); */

       while ((v = LWZReadByte(FALSE,c)) >= 0 ) {
         PPM_ASSIGN(image[ypos][xpos],
                    cmap[CM_RED][v],
                    cmap[CM_GREEN][v], cmap[CM_BLUE][v]);
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
             default:
               /*               fprintf(stderr, "Panic! %d\n", __LINE__); */
               longjmp(jumpbuffer, GIFMUNGE_FATAL_ERROR);
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
             /* not interlaced */
             ++ypos;
           }
         }
         if (ypos >= height)
           break;
       }

fini:
       if (LWZReadByte(FALSE,c)>=0)
         DEBUG("too much input data, ignoring extra...");

       pnmbufp->cols = len;
       pnmbufp->rows = height;
       pnmbufp->format = pbm_format;

       /*       ppm_writeppm(stdout, pnmbufp->rowarray, len,height,255,0); */
}



