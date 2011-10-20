#include <tcl.h>
#include "distinterface.h"
#include "threads.h"
extern "C" {
#include "clib.h"
}


class TclStub {
public:
	TclStub();
	DistillerStatus DistillerInit_tacc(C_DistillerType distType, 
					   int argc, const char* const * argv);
  
	DistillerStatus DistillerMain_tacc(Argument *args, int numberOfArgs,
					   DistillerInput *input, 
					   DistillerOutput *output);
  
	static TclStub instance;

private:
	void Signal(int fd, const char *string);
	void Wait(int fd, char *string=NULL, int len=0);

	static DistillerStatus DistillerInit_tcl(Tcl_Interp *interp,
				     C_DistillerType distType);
	static DistillerStatus DistillerMain_tcl(Tcl_Interp *interp,
						 Argument *args,
						 int numberOfArgs,
						 DistillerInput *input,
						 DistillerOutput *output);

	static void  Abort();
	static void* TclMain_static(void *dummy);
	static int   Tcl_AppInit_static(Tcl_Interp *interp);
	static void  Tcl_ReadHandler_static(ClientData cd, int mask);

	static int Tcl_ClibInit(Tcl_Interp *interp);
	static int Tcl_ClibFetch(ClientData clientData, Tcl_Interp *interp,
				 int objc, Tcl_Obj *CONST objv[]);
	static int Tcl_ClibPush(ClientData clientData, Tcl_Interp *interp,
				int objc, Tcl_Obj *CONST objv[]);

	static void Args2Tcl_(Tcl_Interp *interp, Argument *args,
			      int numberOfArgs, char *varName);
	static void MakeCopy_(char *&var, const char *str) {
		var = new char [strlen(str)+1];
		strcpy(var, str);
	}

	Thread thread_;
	int taccReadFD_, taccWriteFD_, tclReadFD_, tclWriteFD_;
	C_DistillerType distType_;
	Tcl_Interp *interp_;

	int argc_;
	char **argv_;

	Argument *args_;
	int numberOfArgs_;
	DistillerInput *input_;
	DistillerOutput *output_;

	char *distMainResult_;
	int distMainResultLen_;
};


TclStub TclStub::instance;


TclStub::TclStub()
	: taccReadFD_(-1), taccWriteFD_(-1), tclReadFD_(-1), tclWriteFD_(-1), 
	  interp_(NULL), /*otclClass_(NULL),*/ argc_(0), argv_(NULL), 
	  input_(NULL), output_(NULL),
	  distMainResult_(NULL), distMainResultLen_(0)
{
}


TclStub::~TclStub()
{
	if (distMainResult_!=NULL) Tcl_Free(distMainResult_);
}


void
TclStub::Abort()
{
	fprintf(stderr, "%s: ", Tcl_GetStringResult(instance.interp_));
	fprintf(stderr, "%s\n", Tcl_GetVar(instance.interp_, "errorInfo", 
					   TCL_GLOBAL_ONLY));
	Tcl_Exit(-1);
}


void
TclStub::Signal(int fd, const char *string)
{
	write(fd, string, strlen(string)+1);
}


void
TclStub::Wait(int fd, char *string, int len)
{
	char *ptr = string, dummy;
	int bytesRead=0;

	if (string==NULL || len<=0) ptr = &dummy;

	while( read(fd, ptr, 1)!=-1 || errno==EBADF || errno==EINTR ) {
		if (*ptr=='\0') break;
		if (ptr != &dummy) {
			ptr++;
			bytesRead++;
			if (bytesRead >= len) ptr = &dummy;
		}
	}
}


DistillerStatus
TclStub::DistillerInit_tacc(C_DistillerType distType, 
			    int argc, const char * const * argv)
{
	int fd[2];

	// create the pipes for communication with the tcl thread
	if (pipe(fd)==-1) return distFatalError;
	taccReadFD_  = fd[0];
	tclWriteFD_  = fd[1];

	if (pipe(fd)==-1) {
		close(taccReadFD_);
		close(tclWriteFD_);
		return distFatalError;
	}
	tclReadFD_   = fd[0];
	taccWriteFD_ = fd[1];

	distType_ = distType;

	argv_ = new char * [argc+5];
	argc_ = 0;
	memset(argv_, 0, (argc+5)*sizeof(char*));
	MakeCopy_(argv_[argc_++], "tclsh");
	for (int i=0; i < argc; i++) {
		MakeCopy_(argv_[argc_++], argv[i]);
	}

	// fork off the tcl thread
	thread_.Fork(TclMain_static, NULL);

	char retval[256], *end;
	int result;

	Signal(taccWriteFD_, "DistillerInit");
	Wait(taccReadFD_, retval, sizeof(retval));

	result = strtoul(retval, &end, 10);
	if (*end!='\0') {
		return distFatalError;
	} else {
		return (DistillerStatus) result;
	}
}


void*
TclStub::TclMain_static(void */*dummy*/)
{
	Tcl_Main(instance.argc_, instance.argv_, Tcl_AppInit_static);
	return NULL;
}


int 
TclStub::Tcl_AppInit_static(Tcl_Interp *interp)
{
	// create Tcl channels for the communication pipes with tacc
#if TCL_MAJOR_VERSION < 8
	Tcl_File tclFD  = Tcl_GetFile((void*)instance.tclReadFD_, TCL_UNIX_FD);
	Tcl_CreateFileHandler(tclFD, TCL_READABLE, Tcl_ReadHandler_static, 
			      NULL);
#else
	Tcl_CreateFileHandler(instance.tclReadFD_, TCL_READABLE,
			      Tcl_ReadHandler_static, NULL);	
#endif
	instance.interp_ = interp;

	if (Tcl_ClibInit(interp)!=TCL_OK) return TCL_ERROR;
	
	return TCL_OK;
}


int
TclStub::Tcl_ClibInit(Tcl_Interp *interp)
{
	clib_response response = Clib_Initialize(Options_DefaultDb(), NULL);
	if (response!=TCL_OK) {
		Tcl_SetResult(interp, (char*) clib_strerror(response), NULL);
		Tcl_AddErrorInfo(interp,
				 "\nwhile initializing cache sub-system");
		Tcl_AddErrorInfo(interp,
				 "\ninvoked from within Tcl_ClibInit");
		return TCL_ERROR;
	}
	
	Tcl_CreateObjCommand(interp, "Clib_Fetch", Tcl_ClibFetch, NULL, NULL);
	Tcl_CreateObjCommand(interp, "Clib_Push", Tcl_ClibPush, NULL, NULL);
	return TCL_OK;
}


int
TclStub::Tcl_ClibFetch(ClientData /*clientData*/, Tcl_Interp *interp,
		       int objc, Tcl_Obj *CONST objv[])
{
	char *url, *clientHeaders;
	int fetchFromInternet;
	clib_response response;
	clib_data data;
	Tcl_Obj *result, *elem;
	
	if (objc < 2 && objc > 4) {
		Tcl_WrongNumArgs(interp, 1, objv,
				 "url client_headers [fetch_from_internet]");
		return TCL_ERROR;
	}
	
	url = Tcl_GetStringFromObj(objv[1], NULL);
	if (url==NULL) {
		Tcl_SetResult(interp, "invalid url", TCL_STATIC);
		Tcl_AddErrorInfo(interp, "\ninvoked from within Clib_Fetch\n");
		return TCL_ERROR;
	}
	if (objc > 2) {
		clientHeaders = Tcl_GetStringFromObj(objv[2], NULL);
		if (clientHeaders==NULL) {
			Tcl_SetResult(interp, "invalid client headers",
				      TCL_STATIC);
			Tcl_AddErrorInfo(interp,
					 "\ninvoked from within Clib_Fetch\n");
			return TCL_ERROR;
		}
		if (*clientHeaders=='\0') {
			clientHeaders = NULL;
		}
	} else {
		clientHeaders = NULL;
	}

	if (objc > 3) {
		if (Tcl_GetBooleanFromObj(interp, objv[3],
					  &fetchFromInternet)!=TCL_OK) {
			return TCL_ERROR;
		}
	}
	else {
		fetchFromInternet = 1;
	}

	memset(&data, 0, sizeof(data));
	if (!fetchFromInternet) {
		response = Clib_Query(url);
		if (response != CLIB_CACHE_HIT) goto end;
	}

	response = Clib_Fetch(url, clientHeaders, &data);
	if (response != CLIB_OK) {
		goto end;
	}

end:
	if (response==CLIB_CUTTHROUGH) {
		close(data.fd);
	}
	
	result = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	elem = Tcl_NewIntObj(response);
	Tcl_ListObjAppendElement(interp, result, elem);
	elem = Tcl_NewStringObj(data.mime_headers, data.mime_size);
	Tcl_ListObjAppendElement(interp, result, elem);
	elem = Tcl_NewStringObj(data.data, data.data_size);
	Tcl_ListObjAppendElement(interp, result, elem);
	Tcl_SetObjResult(interp, result);
	
	if (data.mime_headers!=NULL) free(data.mime_headers);
	if (data.data!=NULL) free(data.data);
	return TCL_OK;
}


int
TclStub::Tcl_ClibPush(ClientData /*clientData*/, Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
	char *url;
	clib_response response;
	clib_data data;
	Tcl_Obj *result;
	int len;
	
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv,
				 "url server_headers data");
		return TCL_ERROR;
	}
	
	url = Tcl_GetStringFromObj(objv[1], NULL);
	if (url==NULL) {
		Tcl_SetResult(interp, "invalid url", TCL_STATIC);
		Tcl_AddErrorInfo(interp, "\ninvoked from within Clib_Push\n");
		return TCL_ERROR;
	}
	
	data.mime_headers = Tcl_GetStringFromObj(objv[2], &len);
	data.mime_size = len;
	if (data.mime_headers==NULL) {
		Tcl_SetResult(interp, "invalid server headers",
			      TCL_STATIC);
		Tcl_AddErrorInfo(interp,
				 "\ninvoked from within Clib_Push\n");
		return TCL_ERROR;
	}

	data.data = Tcl_GetStringFromObj(objv[2], &len);
	data.data_size = len;
	if (data.data==NULL) {
		Tcl_SetResult(interp, "invalid data",
			      TCL_STATIC);
		Tcl_AddErrorInfo(interp,
				 "\ninvoked from within Clib_Push\n");
		return TCL_ERROR;
	}

	response = Clib_Push(url, data);
	result = Tcl_NewIntObj(response);
	Tcl_SetObjResult(interp, result);

	return TCL_OK;
}


void 
TclStub::Tcl_ReadHandler_static(ClientData /*cd*/, int /*mask*/)
{
	static char string[256];
	DistillerStatus retval;

	instance.Wait(instance.tclReadFD_, string, sizeof(string));

	if (strcmp(string, "DistillerInit")==0) {
		retval = DistillerInit_tcl(instance.interp_,
					   instance.distType_);
	}
	else if (strcmp(string, "DistillerMain")==0) {
		retval = DistillerMain_tcl(instance.interp_,
					   instance.args_,
					   instance.numberOfArgs_,
					   instance.input_,
					   instance.output_);
	}
	else {
		static char errorStr[256];
		sprintf(errorStr, "Cannot understand message from TACC worker:"
			" %s", string);
		Tcl_SetResult(instance.interp_, errorStr, TCL_STATIC);
		Abort();
	}

	sprintf(string, "%d", retval);
	instance.Signal(instance.tclWriteFD_, string);
}


DistillerStatus 
TclStub::DistillerMain_tacc(Argument *args, int numberOfArgs,
			    DistillerInput *input, 
			    DistillerOutput *output)
{
	args_ = args;
	numberOfArgs_ = numberOfArgs;
	input_  = input;
	output_ = output;

	char retval[256], *end;
	int result;

	Signal(taccWriteFD_, "DistillerMain");
	Wait(taccReadFD_, retval, sizeof(retval));

	result = strtoul(retval, &end, 10);
	if (*end!='\0') {
		return distFatalError;
	} else {
		return (DistillerStatus) result;
	}
}


DistillerStatus 
TclStub::DistillerInit_tcl(Tcl_Interp *interp, C_DistillerType distType)
{
	Tcl_Obj *objv[2], *cmd, *result;
	int objc=2, i, retval;

	objv[0] = Tcl_NewStringObj("DistillerInit", -1);
	objv[1] = Tcl_NewStringObj(GET_DISTILLER_TYPE(distType), -1);
	for (i=0; i<objc; i++) Tcl_IncrRefCount(objv[i]);
	cmd = Tcl_NewListObj(objc, objv);
	Tcl_IncrRefCount(cmd);
	if (Tcl_GlobalEvalObj(interp, cmd) != TCL_OK) {
		TclStub::Abort();
	}
	Tcl_DecrRefCount(cmd);
	for (i=0; i<objc; i++) Tcl_DecrRefCount(objv[i]);

	result = Tcl_GetObjResult(interp);
	if (Tcl_GetIntFromObj(interp, result, &retval)!=TCL_OK)
		TclStub::Abort();
	return (DistillerStatus) retval;
}


DistillerStatus
TclStub::DistillerMain_tcl(Tcl_Interp *interp, Argument *args,int numberOfArgs,
		  DistillerInput *input, DistillerOutput *output)
{
	char *argsVar="DistillerMain_Args__";
	Tcl_Obj *objv[5], *cmd, *result, *obj;
	char *string, *copy;
	int objc=5, i, len, retval;

	Args2Tcl_(interp, args, numberOfArgs, argsVar);
	objv[0] = Tcl_NewStringObj("DistillerMain", -1);
	objv[1] = Tcl_NewStringObj((char*)DataPtr(input), DataLength(input));
	objv[2] = Tcl_NewStringObj((char*)MetadataPtr(input),
				   MetadataLength(input));
	objv[3] = Tcl_NewStringObj(MimeType(input), -1);
	objv[4] = Tcl_NewStringObj(argsVar, -1);
	for (i=0; i<objc; i++) Tcl_IncrRefCount(objv[i]);
	cmd = Tcl_NewListObj(objc, objv);
	Tcl_IncrRefCount(cmd);
	if (Tcl_GlobalEvalObj(interp, cmd) != TCL_OK) {
		TclStub::Abort();
	}
	Tcl_DecrRefCount(cmd);
	for (i=0; i<objc; i++) Tcl_DecrRefCount(objv[i]);

	result = Tcl_GetObjResult(interp);
	if (Tcl_ConvertToType(interp,result,Tcl_GetObjType("list")) !=TCL_OK) {
		TclStub::Abort();
	}

	if (Tcl_ListObjIndex(interp, result, 0, &obj)!=TCL_OK) 
		TclStub::Abort();
	string = Tcl_GetStringFromObj(obj, &len);
	copy = (char*) DistillerMalloc(len+1);
	memcpy(copy, string, len);
	copy[len] = '\0';
	SetData(output, copy);
	SetDataLength(output, len);
	DataNeedsFree(output, gm_True);

	if (Tcl_ListObjIndex(interp, result, 1, &obj)!=TCL_OK)TclStub::Abort();
	string = Tcl_GetStringFromObj(obj, &len);
	copy = (char*) DistillerMalloc(len+1);
	memcpy(copy, string, len);
	copy[len] = '\0';
	SetMetadata(output, copy);
	SetMetadataLength(output, len);
	MetadataNeedsFree(output, gm_True);

	if (Tcl_ListObjIndex(interp, result, 2, &obj)!=TCL_OK)TclStub::Abort();
	string = Tcl_GetStringFromObj(obj, &len);
	SetMimeType(output, string);
  
	if (Tcl_ListObjIndex(interp, result, 3, &obj)!=TCL_OK)TclStub::Abort();
	if (Tcl_GetIntFromObj(interp, obj, &retval)!=TCL_OK) TclStub::Abort();

	return (DistillerStatus) retval;
}






#if 0
void
TclStub::DistillerMain_create_args()
{
	int idx;
	Argument *argPtr;
	char id[256], value[256], argsName[]="DistillerMain_Args__";

	for (idx=0, argPtr = args_; idx < numberOfArgs_; idx++, argPtr++) {
		switch(ARG_TYPE(*argPtr)) {
		case typeInt:
			sprintf(id, "i%ld", ARG_ID(*argPtr));
			sprintf(value, "%ld", ARG_INT(*argPtr));
			Tcl_SetVar2(interp_, argsName, id, value, TCL_GLOBAL_ONLY);
			break;
		case typeString:
			sprintf(id, "s%ld", ARG_ID(*argPtr));
			Tcl_SetVar2(interp_, argsName, id, ARG_STRING(*argPtr), TCL_GLOBAL_ONLY);
			break;
		case typeDouble:
			sprintf(id, "f%ld", ARG_ID(*argPtr));
			sprintf(value, "%g", ARG_DOUBLE(*argPtr));
			Tcl_SetVar2(interp_, argsName, id, value, TCL_GLOBAL_ONLY);
			break;
		}
	}
}


void
TclStub::CreateListElement(Tcl_DString *bufPtr, char *string, int len)
{
	char *ptr = string, *start = string, special[3] = "\\ ";

	Tcl_DStringStartSublist(bufPtr);

	while ((ptr-string) < len && *ptr!='\0') {
		if (*ptr=='{') {
			if (start!=ptr) {
				Tcl_DStringAppend(bufPtr, start, (ptr-start));
				start = ptr+1;
			}

			special[1] = *ptr;
			Tcl_DStringAppend(bufPtr, special, 1);
		}
		ptr++;
	}

	if (start!=ptr) Tcl_DStringAppend(bufPtr, start, (ptr-start));
	Tcl_DStringEndSublist(bufPtr);
}



void 
TclStub::DistillerMain_tcl(char *retval)
{
	Tcl_DString buf;

	DistillerMain_create_args();

	Tcl_DStringInit(&buf);
	Tcl_DStringAppendElement(&buf, "DistillerMain");

	CreateListElement(&buf, (char*) DataPtr(input_), DataLength(input_));
	CreateListElement(&buf, (char*) MetadataPtr(input_), MetadataLength(input_));

	Tcl_DStringAppendElement(&buf, (char*) MimeType(input_));
	Tcl_DStringAppendElement(&buf, "DistillerMain_Args__");

	if (Tcl_GlobalEval(interp_, Tcl_DStringValue(&buf)) != TCL_OK) {
		Abort();
		//Tcl_DStringFree(&buf);
		//return;
	}

	Tcl_DStringFree(&buf);

	/*if (strcmp(interp_->result, "")==0) {
	  sprintf(retval, "%d", distOk);

	  SetData(output_, DataPtr(input_));
	  SetDataLength(output_, DataLength(input_));
	  DataNeedsFree(output_, gm_False);

	  SetMetadata(output_, MetadataPtr(input_));
	  SetMetadataLength(output_, MetadataLength(input_));
	  MetadataNeedsFree(output_, gm_False);

	  SetMimeType(output_, MimeType(input_));
	  }
	  else {*/
	int argc, len;
	char **argv, *str;

	len = strlen(interp_->result) + 1;
	if (len > distMainResultLen_) {
		distMainResult_ = Tcl_Realloc(distMainResult_, len);
		distMainResultLen_ = len;
	}

	strcpy(distMainResult_, interp_->result);
	Tcl_SplitList(interp_, distMainResult_, &argc, &argv);
	if (argc!=4) {
		static char errorStr[256];
		sprintf(errorStr, "MashStub::DistillerMain should return a list "
			"of 4 elements. got %d elements", argc);
		Tcl_SetResult(instance.interp_, errorStr, TCL_STATIC);
		Abort();
		return;
	}

	strcpy(retval, argv[3]);

	len = strlen(argv[0]) + 1;
	str = (char*) DistillerMalloc(len);
	strcpy(str, argv[0]);
	SetData(output_, str);
	SetDataLength(output_, len);
	DataNeedsFree(output_, gm_True);

	len = strlen(argv[1]) + 1;
	str = (char*) DistillerMalloc(len);
	strcpy(str, argv[1]);
	SetMetadata(output_, str);
	SetMetadataLength(output_, len);
	MetadataNeedsFree(output_, gm_True);

	SetMimeType(output_, argv[2]);

	Tcl_Free((char *) argv);
	//}
}
#endif


void
TclStub::Args2Tcl_(Tcl_Interp *interp, Argument *args, int numberOfArgs,
		   char *varName)
{
	int idx;
	Argument *argPtr;
	char id[256], value[256];

	for (idx=0, argPtr = args; idx < numberOfArgs; idx++, argPtr++) {
		switch(ARG_TYPE(*argPtr)) {
		case typeInt:
			sprintf(id, "i%ld", ARG_ID(*argPtr));
			sprintf(value, "%ld", ARG_INT(*argPtr));
			Tcl_SetVar2(interp, varName, id, value, TCL_GLOBAL_ONLY);
			break;
		case typeString:
			sprintf(id, "s%ld", ARG_ID(*argPtr));
			Tcl_SetVar2(interp, varName, id, ARG_STRING(*argPtr), TCL_GLOBAL_ONLY);
			break;
		case typeDouble:
			sprintf(id, "f%ld", ARG_ID(*argPtr));
			sprintf(value, "%g", ARG_DOUBLE(*argPtr));
			Tcl_SetVar2(interp, varName, id, value, TCL_GLOBAL_ONLY);
			break;
		}
	}
}


/*void
CreateListElement(Tcl_DString *bufPtr, char *string, int len)
{
  char *ptr = string, *start = string, special[3] = "\\ ";

  Tcl_DStringStartSublist(bufPtr);

  while ((ptr-string) < len && *ptr!='\0') {
    if (*ptr=='{') {
      if (start!=ptr) {
	Tcl_DStringAppend(bufPtr, start, (ptr-start));
	start = ptr+1;
      }

      special[1] = *ptr;
      Tcl_DStringAppend(bufPtr, special, 1);
    }
    ptr++;
  }

  if (start!=ptr) Tcl_DStringAppend(bufPtr, start, (ptr-start));
  Tcl_DStringEndSublist(bufPtr);
}*/







DistillerStatus 
DistillerInit(C_DistillerType distType, 
	      int argc, const char * const * argv)
{
	return TclStub::instance.DistillerInit_tacc(distType, argc, argv);
}


DistillerStatus
DistillerMain(Argument *args, int numberOfArgs,
	      DistillerInput *input, DistillerOutput *output)
{
	return TclStub::instance.DistillerMain_tacc(args, numberOfArgs, input, 
						    output);
}
