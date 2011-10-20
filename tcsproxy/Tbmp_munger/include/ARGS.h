/*
 *  Arguments understood by Tbmp converter
 */

#define TBMP_SIG 1500
#define TBMP_2BIT  TBMP_SIG+1

#define PPMQUANT_COLORS  TBMP_SIG+11

/* emulate the command line switches of pnmscale */
#define PNMSCALE_XSCALE   TBMP_SIG+21 /* float */
#define PNMSCALE_YSCALE   TBMP_SIG+22 /* float */
#define PNMSCALE_XSIZE    TBMP_SIG+23 /* int */
#define PNMSCALE_YSIZE    TBMP_SIG+24 /* int */

/*
 *  Other stuff used by all parts of the converter, for now
 */


#ifdef DEBUGGING
#define DebugMsg(x) MonitorClientSend(mon,"Distiller Debugging",(x),"Log")
#else  /* not DEBUGGING */
#define DebugMsg(x)
#endif /* DEBUGGING */

#define Message(x) MonitorClientSend(mon,"Distiller Errors",(x),"Log")

#include "distinterface.h"
extern MonitorClientID mon;

