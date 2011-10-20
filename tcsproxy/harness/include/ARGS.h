/*
 *  Arguments understood by Tbmp converter
 */

#define TBMP_SIG 1500
#define TBMP_2BIT  TBMP_SIG+1

#define PPMQUANT_COLORS  TBMP_SIG+11

/* emulate the command line switches of pnmscale */
#define PNMSCALE_XSCALE   TBMP_SIG+21 /* float */
#define PNMSCALE_YSCALE   TBMP_SIG+22
#define PNMSCALE_XSIZE    TBMP_SIG+23 /* int; negative means xcoord of -xysize */
#define PNMSCALE_YSIZE    TBMP_SIG+24 /* int; negative means ycoord of -xysize */
#define PNMSCALE_PIXELS   TBMP_SIG+25 /* scale to this total # of pixels */

/*
 *  Other stuff used by all parts of the converter, for now
 */


#ifdef DEBUGGING
#define DebugMsg(x) MonitorClientSend(mon,"Messages",(x),"")
#else  /* not DEBUGGING */
#define DebugMsg(x)
#endif /* DEBUGGING */

#define Message(x) MonitorClientSend(mon,"Messages",(x),"")

#include "distinterface.h"
extern MonitorClientID mon;

