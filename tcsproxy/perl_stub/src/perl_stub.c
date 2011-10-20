#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "EXTERN.h"
#include "perl.h"

#include "config_tr.h"
#include "optdb.h"
#include "distinterface.h"
#ifdef INST
FILE *Log;
#endif
MonitorClientID mon;

#ifdef __cplusplus
#  define EXTERN_C extern "C"
#else
#  define EXTERN_C extern
#endif

static PerlInterpreter *dist_perl;
static void xs_init(void);

/*
 * given a list of user args and a stack pointer SV** it pushes the args
 * onto the stack in the same ofrmat that perl pushes a %array on the stack
 * keys = argument ids, values = argument values.
 */
static SV**
push_args_onto_stack(Argument *args, int nargs, register SV **sp) {
   int i;
   char tmparg[1 + 10 + 1]; /* [sfi]\d+\0, e.g. "s11" */
   char argtype;
   char *tmpstr;
   SV *argsv;

   for (i = 0; i < nargs; i++) {
    switch ARG_TYPE(args[i]) {      
    case typeString:           
      argtype = 's';
      tmpstr = ARG_STRING(args[i]);
      argsv = newSVpv(tmpstr,strlen(tmpstr));
      break;
    case typeDouble:
      argtype = 'f';
      argsv = newSVnv(ARG_DOUBLE(args[i]));
      break;
    case typeInt:
      argtype = 'i';
      argsv = newSViv(ARG_INT(args[i]));
      break;
    default:
      fprintf(stderr, "Argument id %ld has bad type\n", ARG_ID(args[i]));
      break;
    }

    snprintf(tmparg, sizeof(tmparg)-1, "%c%d", argtype, ARG_ID(args[i]));
/*    sprintf(tmparg, "%c%d", argtype, ARG_ID(args[i])); */
    XPUSHs(sv_2mortal(newSVpv(tmparg, strlen(tmparg))));
    XPUSHs(sv_2mortal(argsv));
  }
  return sp;
}

/*
 * should become a perl function.
 */
DistillerStatus
ComputeDistillationCost(Argument *args, int numArgs,
                        DistillerInput *in,
                        DistillationCost *cost)
{
  cost->estimatedTime_ms = DataLength(in);
  return distOk;
}

/*
 * opens the perl file in PERL_DIR with a name = to the distiller type (with 
 * /'s turned into .'s.  Then calls the perl function DisitillerInit in 
 * that file.
 */
DistillerStatus
DistillerInit(C_DistillerType distType, int argc, const char * const *argv) {
  int count;
  int ret_val;
  char **perl_args;
  const char *tmp;
  register SV **sp;
  int i,j;
  int exitstatus;

  dist_perl = perl_alloc();
  perl_construct(dist_perl);

  perl_args = DistillerMalloc((4+argc) * sizeof(char *));

  i = 0;
  perl_args[i++] = "";          /* perl "binary" name */
  
  tmp = Options_Find((Options) NULL, "proxy.perlmod");
  if (tmp != NULL  &&  *tmp != '\0') {
    /* include path for perl modules on local site */
    perl_args[i] = ALLOCA(1 + 2 + strlen(tmp));
    if (strncmp(tmp, "-I", 2) == 0) {
      strcpy(perl_args[i], tmp);
    } else {
      strcpy(perl_args[i], "-I");
      strcat(perl_args[i], tmp);
    }
    i++;
  }
  if ((tmp = Options_Find((Options) NULL, "proxy.home")) != NULL) {
    /* include path for 'xs' stuff in proxy */
    const char *xspath = "/xs/lib";
    perl_args[i] = ALLOCA(1 + strlen("-I") +  strlen(tmp) + strlen(xspath));
    strcpy(perl_args[i], "-I");
    strcat(perl_args[i], tmp);
    strcat(perl_args[i], xspath);
    i++;
  } else {
    /* no proxy.home -- probably an error */
    fprintf(stderr, "Warning: no proxy.home configuration option found--"
            "this will almost certainly cause an error\n");
  }

  /* copy remaining arguments */
  for (j=0; j<argc; j++)
    perl_args[i++] = (char *) argv[j];
  /* make last arg NULL */
  perl_args[i] = NULL;
  
  exitstatus = perl_parse(dist_perl, xs_init, i, (char **)perl_args, NULL);

  DistillerFree(perl_args);

  if (exitstatus)
    return distFatalError;

  /*  dSP; */
  sp = stack_sp;
  ENTER;
  SAVETMPS;

  PUSHMARK(sp);

  count = perl_call_pv("DistillerInit", G_NOARGS | G_SCALAR | G_EVAL);

  /* Check the eval first */
  if (SvTRUE(GvSV(errgv))) {
    fprintf(stderr,"perl_stub: calling DistillerInit: %s\n", SvPV(GvSV(errgv), na));
    (void)POPs ;
    return distFatalError;
  }
  if (count != 1)
    return distFatalError;      /* init routine didn't return 1 arg */

  ret_val = POPi;
  
  PUTBACK;
  FREETMPS;
  LEAVE;
  
  return ret_val;
}


/* 
 * perl distiller takes 4 vals
 * 1) input data
 * 2) input metadata (ie headers)
 * 3) input mimetype as deduced by frontend (obsolete?)
 * 4) assoc array of args
 * you should say something like:
 * my ($data,$metadata,$mimetype,%args) = @_;
 *
 * perl distiller main returns 4 vals
 * 1) output data
 * 2) output metadata
 * 3) output mimetype
 * 4) return status
 */
DistillerStatus
DistillerMain(Argument *args, int nargs,
              DistillerInput *din,
              DistillerOutput *dout)
{
  int count;
  int ret_stat = distFatalError;
  SV * ret_sv;
  char * ret_sv_val;
  unsigned int i;

  dSP;

  DataNeedsFree(dout, gm_True);
  MetadataNeedsFree(dout, gm_True);

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);

  /* if we have a page of size zero we can't just create a zero length SVpv */
  if (DataLength(din)) {
    XPUSHs (sv_2mortal(newSVpv(DataPtr(din), DataLength(din))));
  } else {
    XPUSHs (sv_2mortal(newSVpv("", 0)));
  }
  if (MetadataLength(din)) {
    XPUSHs (sv_2mortal(newSVpv(MetadataPtr(din), MetadataLength(din))));
  } else {
    XPUSHs (sv_2mortal(newSVpv("", 0)));
  }
  XPUSHs (sv_2mortal(newSVpv(din->mimeType, strlen(din->mimeType))));
  sp = push_args_onto_stack(args, nargs, sp);

  PUTBACK;

  /* call perl */
  count = perl_call_pv("DistillerMain", G_ARRAY | G_EVAL);

  SPAGAIN;

  /* count is the number of arguments returned */
  /* only first 4 args are looked at */
  while (count > 4) {
    (void)POPs;
    count--;
  }
  
  /* Check the eval first */
  if (!SvTRUE(GvSV(errgv)) && (count == 4)) {
    /* return status */
    ret_stat = POPi;

    /* mimetype */
    ret_sv = POPs;
    strncpy(dout->mimeType, SvPV(ret_sv, na), MAX_MIMETYPE-1);
    dout->mimeType[MAX_MIMETYPE-1] = 0;
    
    /* metadata */
    ret_sv = POPs;
    ret_sv_val = SvPV(ret_sv, i);
    SetMetadataLength(dout, i);
    SetMetadata(dout, (char *) DistillerMalloc(i));
    memcpy(MetadataPtr(dout), ret_sv_val, i);

    /* and data */
    ret_sv = POPs;
    ret_sv_val = SvPV(ret_sv, i);
    SetDataLength(dout, i);
    SetData(dout, (char *) DistillerMalloc(i));
    memcpy(DataPtr(dout), ret_sv_val, i);
   
  } else {
    ret_stat = distFatalError;
    DataNeedsFree(dout, gm_False);
    MetadataNeedsFree(dout, gm_False);
    if (count != 4) {                      /* wrong number of args */
      fprintf(stderr,"perl_stub: wrong number of arguments returned from DistillerMain\n");
    }
    fprintf(stderr,"perl_stub: calling DistillerMain $@ = %s\n", SvPV(GvSV(errgv), na));
  } 

  PUTBACK;
  FREETMPS;
  LEAVE;

  return((DistillerStatus)ret_stat);
}

/*
 *  The following magic is necessary to allow the embedded Perl interpreter
 *  to dynamically load extensions.  Don't touch it unless you know what
 *  you're doing.  (This code stolen from perl_main.c)
 */

/* Do not delete this line--writemain depends on it */
EXTERN_C void boot_DynaLoader _((CV* cv));

static void
xs_init()
{
  /*  dXSUB_SYS; */
    char *file = __FILE__;
    {
        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
    }
}
