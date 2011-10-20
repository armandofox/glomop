/*
 *  ARGS.h for gif_munge distiller
 *
 *  The Args structure can contain the following:
 *  arg id GIFMUNGE_MODE (int): distillation mode
 *     =0  output should target particular image dimensions (to follow)
 *     >0  output should target this output byte size
 *     <0  this is the (negative) reciprocal of the scale factor
 *  arg id GIFMUNGE_NEWX (int): new pixel dimension for x (negative: for
 *     y); zero means don't do any scaling at all
 *  arg id GIFMUNGE_BPP (int):
 *      Max bits per pixel for output; negative means grayscale
 *  arg id GIFMUNGE_RESCALE (int):
 *      If present, rescale image (using pnmscale) back to original
 *     pixel size before returning it.  (result is "fuzzy" image)
 *  arg id GIFMUNGE_ABSMAX (int):
 *      No matter what, make sure resulting image is no larger than this
 *      X dimension (negative: this Y dimension)
 */

/* the "signature" of the gif_munge distiller: */
/* #define GIFMUNGE_SIG  (('G' << 24) | ('I' << 16) | ('F' << 8) | 'M') */
#define GIFMUNGE_SIG     1000

#define GIFMUNGE_MODE    GIFMUNGE_SIG+1 /* int */
#define GIFMUNGE_NEWX    GIFMUNGE_SIG+2 /* int */
#define GIFMUNGE_BPP     GIFMUNGE_SIG+3 /* int */
#define GIFMUNGE_RESCALE GIFMUNGE_SIG+4 /* int */
#define GIFMUNGE_ABSMAX  GIFMUNGE_SIG+5 /* int */

