/**
** JPG_ARGS.h for jpg_munge distiller.
**
** The Args structure can contain the following:
**   id   JPG_MAX_X (int):  maximum X for output JPEG.  If this argument
                            is missing, X will have no maximum.
**   id   JPG_MAX_Y (int):  maximum Y for output JPEG.  If this argument
**                          is missing, Y will have no maximum.
**   id   JPG_MIN_X (int):  minimum X for output JPEG.  This is ignored
**                          if the JPEG is already smaller than the minimum.
**                          If missing, no minimum.
**   id   JPG_MIN_Y (int):  minimum Y for output JPEG.  This is ignored
**                          if the JPEG is already smaller than the minimum.
**                          If missing, no minimum.
**   id   JPG_SCALE (dbl):  dimension scale factor for JPG.  If missing,
**                          .5 (each axis).  Scale factor will be adjusted
**                          to satisfy {min|max}{x|y} constraints.  Currently
**                          only 1, .5, .25, and .125 are supported.
**   id   JPG_QUAL (int):   quality for JPEG (1-100).  If missing, 55.
**
** Note: it is possible to specify impossible constraints to solve.  The
**       distiller will give highest priority to minimum x, then to min y,
**       then to max x, then to max y.  The distiller will never enlarge
**       an image.
**/

#define JPG_SIG 2000

#define JPG_MAX_X       JPG_SIG+1 /* int */
#define JPG_MAX_Y       JPG_SIG+2 /* int */
#define JPG_MIN_X       JPG_SIG+3 /* int */
#define JPG_MIN_Y       JPG_SIG+4 /* int */
#define JPG_SCALE       JPG_SIG+5 /* float */
#define JPG_QUAL        JPG_SIG+6 /* int */



