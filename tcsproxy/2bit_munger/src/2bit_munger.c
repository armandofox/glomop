/*
 *  Munge common image formats down to 2 bpp
 *
 *  Ian Goldberg
 */

#include <netinet/in.h>
#include "../include/ARGS.h" /* Definition of your argument types.. */
#include "distinterface.h" 
#include "gif2grey.h"
#include "jpg2grey.h"
#include "ditpic.h"

#define Message(x) MonitorClientSend(mon,"Distiller Errors",(x),"Log")

MonitorClientID mon;

#include <stdio.h>
static void dump(u_char *bytes, int len)
{
	static FILE *fp=NULL;
	if (fp==NULL) {
		fp = fopen("tgmb.2bit.dump", "w");
	}
	fwrite(bytes, 1, len, fp);
	fflush(fp);
}



DistillerStatus
DistillerInit(C_DistillerType dType, int argc, const char * const *argv)
{
    DistillerStatus result;

    mon = DistillerGetMonitorClientID();

    if ((result = gif2grey_init(dType,argc,argv)) != distOk) {
	Message("gif2grey init failed!");
	return result;
    }
    if ((result = jpg2grey_init(dType,argc,argv)) != distOk) {
	Message("jpg2grey init failed!");
	return result;
    }

    Message("2bit munger started");

    return distOk;
}

DistillerStatus
DistillerMain(Argument *args, int nargs,
              DistillerInput *din, DistillerOutput *dout)
{
    DistillerStatus status;
    unsigned char *grey, *tbmp;
    int rows, cols;
    int orows, ocols, orsize;
    int argn, zoom;

    dump(DataPtr(din), DataLength(din));
    /* Convert various input image formats to a greyscale array */
    if ( (!strcasecmp(MimeType(din), "image/jpeg"))
       || (!strcasecmp(MimeType(din), "image/jpg")) ) {
	status = jpg2grey_main(DataPtr(din), DataLength(din), &grey,
				&cols, &rows);
    } else if (strcasecmp(MimeType(din), "image/gif") == 0) {
	status = gif2grey_main(DataPtr(din), DataLength(din), &grey,
				&cols, &rows);
    } else {
	Message("Bad input: not JPEG or GIF");
	return distBadInput;
    }
    if (status != distOk) {
	Message("image conversion failed");
	return status;
    }

    /* Now that we have the greyscale, convert it to a 2-bit Pilot bitmap */

    /* First get the requested output size; default is full size */
    ocols = cols;
    orows = rows;
    for (argn = 0; argn < nargs; argn++) {
	switch(ARG_ID(args[argn])) {
	case TWOBIT_XSIZE:
	    ocols = ARG_INT(args[argn]);
	    break;
	case TWOBIT_YSIZE:
	    orows = ARG_INT(args[argn]);
	    break;
	case TWOBIT_SCALE:
	    zoom = ARG_INT(args[argn]);
	    ocols = (cols * zoom)/100;
	    orows = (rows * zoom)/100;
	    break;
	}
    }

    /* The number of bytes used for each row (must be even) */
    orsize = ((ocols+7)/8)*2;

    /* Allocate space and write the preamble */
    tbmp = DistillerMalloc(16 + orows * orsize);
    if (!tbmp) {
        return distOutOfLocalMemory;
    }
    *(unsigned short *)(tbmp) = htons(ocols*2);
    *(unsigned short *)(tbmp+2) = htons(orows);
    *(unsigned short *)(tbmp+4) = htons(orsize);
    *(unsigned short *)(tbmp+6) = 0;
    *(unsigned long *)(tbmp+8) = 0;
    *(unsigned long *)(tbmp+12) = 0;

    ditpic(grey, cols, rows, tbmp+16, ocols, orows, 0.75);

    /* Finish up */
    DistillerFree(grey);
    SetMimeType(dout, "image/x-Tbmp2");
    SetDataLength(dout, 16 + orows * orsize);
    SetData(dout, tbmp);
    DataNeedsFree(dout, gm_True);

    return distOk;
}
