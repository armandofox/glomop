/* hey emacs, this is a -C- file */
#include "clib.h"
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "proxyinterface.h"

static MonitorClientID mon = NULL;

/* Re-ifdef the following lines to allow "standalone" testing with perl -de 0 */
#if 0
gMon = NULL; 
const char *Options_Find(Options w,const char *x) { return NULL; } 
#endif

static int
not_here(s)
char *s;
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

static double
constant(name, arg)
char *name;
int arg;
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	break;
    case 'C':
	if (strEQ(name, "CLIB_ACCESS_DENIED"))
#ifdef CLIB_ACCESS_DENIED
	    return CLIB_ACCESS_DENIED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_BAD_URL"))
#ifdef CLIB_BAD_URL
	    return CLIB_BAD_URL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_BAD_VERSION"))
#ifdef CLIB_BAD_VERSION
	    return CLIB_BAD_VERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_CACHE_HIT"))
#ifdef CLIB_CACHE_HIT
	    return CLIB_CACHE_HIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_CACHE_MISS"))
#ifdef CLIB_CACHE_MISS
	    return CLIB_CACHE_MISS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_CONFIG_ERROR"))
#ifdef CLIB_CONFIG_ERROR
	    return CLIB_CONFIG_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_H"))
#ifdef CLIB_H
	    return CLIB_H;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_NO_CONFIGFILE"))
#ifdef CLIB_NO_CONFIGFILE
	    return CLIB_NO_CONFIGFILE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_NO_FD"))
#ifdef CLIB_NO_FD
	    return CLIB_NO_FD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_NO_SERVERS"))
#ifdef CLIB_NO_SERVERS
	    return CLIB_NO_SERVERS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_NO_SUCH_PART"))
#ifdef CLIB_NO_SUCH_PART
	    return CLIB_NO_SUCH_PART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_OK"))
#ifdef CLIB_OK
	    return CLIB_OK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_PTHREAD_FAIL"))
#ifdef CLIB_PTHREAD_FAIL
	    return CLIB_PTHREAD_FAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_SERVER_DOWN"))
#ifdef CLIB_SERVER_DOWN
	    return CLIB_SERVER_DOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_SERVER_INTERNAL"))
#ifdef CLIB_SERVER_INTERNAL
	    return CLIB_SERVER_INTERNAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_SERVER_TIMEOUT"))
#ifdef CLIB_SERVER_TIMEOUT
	    return CLIB_SERVER_TIMEOUT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_SERVICE_UNAVAIL"))
#ifdef CLIB_SERVICE_UNAVAIL
	    return CLIB_SERVICE_UNAVAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_SOCKETOP_FAIL"))
#ifdef CLIB_SOCKETOP_FAIL
	    return CLIB_SOCKETOP_FAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLIB_TIMEOUT"))
#ifdef CLIB_TIMEOUT
	    return CLIB_TIMEOUT;
#else
	    goto not_there;
#endif
	break;
    case 'D':
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}



MODULE = clib       PACKAGE = clib

double
constant(name,arg)
	char *		name
	int		arg

Argument
make_argument(type,id,val)
        char            type
        unsigned long   id
        SV*             val
        CODE:
        Zero(&RETVAL,1,Argument);
        RETVAL.id = id;
        if (type == 's' || type == 'S') {
          RETVAL.type = typeString;
          SET_ARG_STRING(RETVAL, SvPV(val,na));
        } else if (type == 'i' || type == 'I') {
          RETVAL.type = typeInt;
          SET_ARG_INT(RETVAL, SvIV(val));
        } else if (type == 'f' || type == 'F') {
          RETVAL.type = typeDouble;
          SET_ARG_DOUBLE(RETVAL, SvNV(val));
        } else {
          /* bleah! */
          croak("make_argument: Legal argument types are s,i,f (you said: '%c')",
                type);
        }
        OUTPUT:
        RETVAL

int
InitializeDistillerCache()
        CODE:
        RETVAL = (int)InitializeDistillerCache(Options_DefaultDb(),
                                               MonitorClientUnitID(DistillerGetMonitorClientID()));
        if (RETVAL == gm_True) {
          RETVAL = 1;
          WaitForPTM();
        } else {
          RETVAL = 0;
        }
        OUTPUT:
        RETVAL
        
int
Distill(mimeType,distType,inputHdrs,inputData,...)
	char *	mimeType
	char *	distType
	SV *	inputHdrs
	SV *	inputData
	PROTOTYPE: $$$$@
        PREINIT:
        Argument arglist[MAX_ARGS];
        const int REQUIRED_ARGS = 4; /* must match #of args in XS decl above */
        int nargs = 0;
        C_DistillerType dType;
        DistillerInput din;
        DistillerOutput dout;
        DistillerStatus result;
        int len;
        PPCODE:
        Zero(&din,1,DistillerInput);
        Zero(&dout,1,DistillerOutput);
        Zero(&dType,1,C_DistillerType);
        Zero(arglist,MAX_ARGS,Argument);
        /* grab all the arguments */
        while (items > REQUIRED_ARGS  &&  nargs < MAX_ARGS) {
          items--;
          arglist[nargs] = *((Argument *)SvPV(ST(items), na));
          /*
          fprintf(stderr, "Arg type=%c id=%d\n",
                  (ARG_TYPE(arglist[nargs]) == typeInt? 'i'
                   : ARG_TYPE(arglist[nargs]) == typeDouble? 'f'
                   : ARG_TYPE(arglist[nargs]) == typeString? 's'
                   : 'x'), ARG_ID(arglist[nargs]));
           */
          nargs++;
        }
        /*  fprintf(stderr,"nargs is %d\n",nargs); */
        /* set distiller type */
        SET_DISTILLER_TYPE(dType,distType);
        /* pack input mime type, metadata and data */
        SetMimeType(&din,mimeType);
        SetData(&din, SvPV(inputData,len));
        SetDataLength(&din, len);
        SetMetadata(&din, SvPV(inputHdrs,len));
        SetMetadataLength(&din, len);
        /* do distillation... */
        result = Distill(&dType, arglist, nargs, &din, &dout);
        /* if success: push result code, metadata, data */
        if (result == distOk) {
          EXTEND(sp,3);
          PUSHs(sv_2mortal(newSViv(result)));
          if (DataLength(&dout) && DataPtr(&dout)) {
            PUSHs(sv_2mortal(newSVpv(DataPtr(&dout),DataLength(&dout))));
          } else {
            PUSHs(sv_2mortal(newSVpv("",0)));
          }
          if (MetadataLength(&dout) && MetadataPtr(&dout)) {
            PUSHs(sv_2mortal(newSVpv(MetadataPtr(&dout),MetadataLength(&dout))));
          } else {
            PUSHs(sv_2mortal(newSVpv("",0)));
          }
        } else {
          EXTEND(sp,1);
          PUSHs(sv_2mortal(newSViv(result)));
        }
        /* free output buffers?  */
        if (dout.data.freeMe == gm_True) {
          DistillerBufferFree(&dout.data);
        }
        if (dout.metadata.freeMe == gm_True) {
          DistillerBufferFree(&dout.metadata);
        }
        XSRETURN(result == distOk? 3 : 1);

int
DistillAsync(mimeType,distType,inputHdrs,inputData,...)
	char *	mimeType
	char *	distType
	SV *	inputHdrs
	SV *	inputData
	PROTOTYPE: $$$$@
        PREINIT:
        Argument arglist[MAX_ARGS];
        const int REQUIRED_ARGS = 4; /* must match #of args in XS decl above */
        int nargs = 0;
        C_DistillerType dType;
        DistillerInput din;
        DistillerStatus result;
	DistillerRequestType newrequest;
        int len;
        PPCODE:
        Zero(&din,1,DistillerInput);
        Zero(&dType,1,C_DistillerType);
        Zero(arglist,MAX_ARGS,Argument);
        /* grab all the arguments */
        while (items > REQUIRED_ARGS  &&  nargs < MAX_ARGS) {
          items--;
          arglist[nargs] = *((Argument *)SvPV(ST(items), na));
          nargs++;
        }
        /* set distiller type */
        SET_DISTILLER_TYPE(dType,distType);
        /* pack input mime type, metadata and data */
        SetMimeType(&din,mimeType);
        SetData(&din, SvPV(inputData,len));
        SetDataLength(&din, len);
        SetMetadata(&din, SvPV(inputHdrs,len));
        SetMetadataLength(&din, len);
        /* do distillation... */
	newrequest = NULL;
        result = DistillAsync(&dType, arglist, nargs, &din, &newrequest);
	if (newrequest) {
	  SV *svp = sv_newmortal();
	  EXTEND(sp,2);
	  PUSHs(sv_2mortal(newSViv(result)));
	  sv_setref_pv(svp, "DistillerRequestType", (void*)newrequest);
	  PUSHs(svp);
	} else {
          EXTEND(sp,1);
          PUSHs(sv_2mortal(newSViv(result)));
	}
	XSRETURN(newrequest ? 2 : 1);

int
DistillRendezvous(request, tvd)
	DistillerRequestType request
	double tvd
	PREINIT:
	struct timeval tv;
	DistillerOutput dout;
        DistillerStatus result;
	PPCODE:
	/* convert the tvd to a tv */
	tv.tv_sec = floor(tvd);
	tv.tv_usec = floor((tvd-(double)tv.tv_sec)*(double)1000000.0);
        Zero(&dout,1,DistillerOutput);
	result = DistillRendezvous(&request, &dout,
	    ST(1) == &sv_undef ? NULL : &tv);
	tvd = (double)tv.tv_sec + (double)tv.tv_usec / (double)1000000.0;
	/* Update the passed parameters */
	sv_setref_pv(ST(0), "DistillerRequestType", (void*)request);
	/* the timeout may not have been a variable */
	if (ST(1) != &sv_undef && !SvREADONLY(ST(1))) sv_setnv(ST(1), tvd);
        /* if success: push result code, metadata, data */
        if (result == distOk) {
          EXTEND(sp,3);
          PUSHs(sv_2mortal(newSViv(result)));
          if (DataLength(&dout) && DataPtr(&dout)) {
            PUSHs(sv_2mortal(newSVpv(DataPtr(&dout),DataLength(&dout))));
          } else {
            PUSHs(sv_2mortal(newSVpv("",0)));
          }
          if (MetadataLength(&dout) && MetadataPtr(&dout)) {
            PUSHs(sv_2mortal(newSVpv(MetadataPtr(&dout),MetadataLength(&dout))));
          } else {
            PUSHs(sv_2mortal(newSVpv("",0)));
          }
        } else {
          EXTEND(sp,1);
          PUSHs(sv_2mortal(newSViv(result)));
        }
        /* free output buffers?  */
        if (dout.data.freeMe == gm_True) {
          DistillerBufferFree(&dout.data);
        }
        if (dout.metadata.freeMe == gm_True) {
          DistillerBufferFree(&dout.metadata);
        }
        XSRETURN(result == distOk? 3 : 1);

void
DistillDestroy(request)
	DistillerRequestType &request
	PREINIT:
	if (!SvTRUE(ST(0))) XSRETURN_EMPTY;
	OUTPUT:
	request

char*
Options_Find(key)
	char*   key
        CODE:
        RETVAL = (char *)Options_Find(NULL,key);
        if (RETVAL == NULL) {
           RETVAL = "";
        }
        OUTPUT:
        RETVAL

int
MonitorClient_Send(s1,s2,s3)
	char*   s1
	char*   s2
	char*   s3
        PPCODE:
        if (mon == NULL) {
           mon = DistillerGetMonitorClientID();
        }
        if (mon) {
          MonitorClientSend(mon, s1, s2, s3);
        } else {
          fprintf(stderr, "%s: %s: %s\n", s1,s2,s3);
        }


int
Clib_Query(url)
	char*   url

int
Clib_Push(url,server_headers,data)
	char*	url
	char*	server_headers
	char*	data
        PREINIT:
        clib_data dat;
        clib_response ret;
        int mimesize,datasize;
        PPCODE:
        dat.mime_headers = (char *)SvPV(ST(1),mimesize);
        dat.mime_size = mimesize;
        dat.data = (char *)SvPV(ST(2),datasize);
        dat.data_size = datasize;
        RETVAL = Clib_Push(url,dat);
	RETVAL = 1;
        EXTEND(sp,1);
        PUSHs(sv_2mortal(newSViv(RETVAL)));
        XSRETURN(1);

int
Clib_Fetch(url,headers)
        char*  url
        char*  headers
        PREINIT:
        clib_data  dat;
        PPCODE:
        RETVAL = Clib_Fetch(url,headers,&dat);
        if (RETVAL == CLIB_OK) {
           EXTEND(sp,3);
           PUSHs(sv_2mortal(newSViv(RETVAL)));
           PUSHs(sv_2mortal(newSVpv(dat.mime_headers, dat.mime_size)));
           PUSHs(sv_2mortal(newSVpv(dat.data, dat.data_size)));
           Clib_Free(&dat);
           XSRETURN(3);
        } else {
           EXTEND(sp,1);
           PUSHs(sv_2mortal(newSViv(RETVAL)));
           XSRETURN(1);
        }

int
Clib_Redir_Fetch(url,headers,max_redirs)
        char*  url
        char*  headers
	int    max_redirs
        PREINIT:
        clib_data  dat;
        PPCODE:
        RETVAL = Clib_Redir_Fetch(url,headers,&dat,max_redirs);
        if (RETVAL == CLIB_OK) {
           EXTEND(sp,3);
           PUSHs(sv_2mortal(newSViv(RETVAL)));
           PUSHs(sv_2mortal(newSVpv(dat.mime_headers, dat.mime_size)));
           PUSHs(sv_2mortal(newSVpv(dat.data, dat.data_size)));
           Clib_Free(&dat);
           XSRETURN(3);
        } else {
           EXTEND(sp,1);
           PUSHs(sv_2mortal(newSViv(RETVAL)));
           XSRETURN(1);
        }

int
Clib_Async_Fetch(url,headers)
        char*  url
        char*  headers
        PREINIT:
        PPCODE:
        RETVAL = Clib_Async_Fetch(url,headers);
	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSViv(RETVAL)));
        XSRETURN(1);

int
Clib_Post(url,headers,data,data_len)
	char *url
	char *headers
	char *data
	unsigned data_len
	PREINIT:
	clib_data dat;
	PPCODE:
	RETVAL = Clib_Post(url,headers,data,data_len,&dat);
	if (RETVAL == CLIB_OK) {
	   EXTEND(sp,3);
	   PUSHs(sv_2mortal(newSViv(RETVAL)));
           PUSHs(sv_2mortal(newSVpv(dat.mime_headers, dat.mime_size)));
           PUSHs(sv_2mortal(newSVpv(dat.data, dat.data_size)));
           Clib_Free(&dat);
           XSRETURN(3);
        } else {
           EXTEND(sp,1);
           PUSHs(sv_2mortal(newSViv(RETVAL)));
           XSRETURN(1);
        }
