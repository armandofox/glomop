/**
** GIFJPG_ARGS.h for gifjpg_munge distiller.
**
** The Args structure can contain the following:
**   id  GJPG_MAX_X (int):  maximum X for output JPEG.  If this argument
                            is missing, X will have no maximum.
**   id  GJPG_MAX_Y (int):  maximum Y for output JPEG.  If this argument
**                          is missing, Y will have no maximum.
**   id  GJPG_MIN_X (int):  minimum X for output JPEG.  This is ignored
**                          if the JPEG is already smaller than the minimum.
**                          If missing, no minimum.
**   id  GJPG_MIN_Y (int):  minimum Y for output JPEG.  This is ignored
**                          if the JPEG is already smaller than the minimum.
**                          If missing, no minimum.
**   id  GJPG_SCALE (dbl):  dimension scale factor for JPG.  If missing,
**                          .5 (each axis).  Scale factor will be adjusted
**                          to satisfy {min|max}{x|y} constraints.  Currently
**                          only 1, .5, .25, and .125 are supported.
**   id  GJPG_QUAL (int):   quality for JPEG (1-100).  If missing, 55.
**
** Note: it is possible to specify impossible constraints to solve.  The
**       distiller will give highest priority to minimum x, then to min y,
**       then to max x, then to max y.  The distiller will never enlarge
**       an image.
**/

#define GJPG_SIG 3000

#define GJPG_MAX_X       GJPG_SIG+1 /* int */
#define GJPG_MAX_Y       GJPG_SIG+2 /* int */
#define GJPG_MIN_X       GJPG_SIG+3 /* int */
#define GJPG_MIN_Y       GJPG_SIG+4 /* int */
#define GJPG_SCALE       GJPG_SIG+5 /* float */
#define GJPG_QUAL        GJPG_SIG+6 /* int */

