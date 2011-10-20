/*
 *  TACC glue to implement the Java Servlet class interface
 *  Mike Chen & Armando Fox  06/98
 */


#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include "config_tr.h"
#include "optdb.h"
#include "distinterface.h"

#if 0
JNI_GetDefaultJavaVMInitArgs(void *x)  { return 0; }
JNI_CreateJavaVM(JavaVM **x,JNIEnv **y,void *z) { return -1; }
#endif

MonitorClientID mon;

/* Denotes a Java VM */
static JavaVM *jvm;

/* Pointer to native method interface */
static JNIEnv *env;

/* Java type info */
static jclass byteArrayInputStreamClass;
static jmethodID byteArrayInputStreamConstructor;
static jclass pushbackInputStreamClass;
static jmethodID pushbackInputStreamConstructor;
static jclass byteArrayOutputStreamClass;
static jmethodID byteArrayOutputStreamConstructor;
static jmethodID byteArrayOutputStreamToByteArray;

/* the actual worker class */
static jclass ServletSubclass;
static jmethodID ServletSubclassInit;
static jmethodID ServletSubclassMain;

/*
 *  Append ${proxy.home}/classes to CLASSPATH in environment
 */
static void
SetClassPath(void)
{
  char *clspath;
  char *newclspath;
  const char *proxyhome;
  /*
   *  Add ${proxy.home}/classes to the default CLASSPATH.
   */
  proxyhome = Options_Find((Options) NULL, "proxy.home");
  if (proxyhome == NULL) {
    fprintf(stderr, "Warning: no proxy.home configuration option found--"
            "not setting CLASSPATH--this will almost certainly "
            "cause an error\n");
  } else {
    clspath = getenv("CLASSPATH");
    if (clspath) {
      newclspath = ALLOCA(strlen(clspath)+1 /* for colon */
                          + strlen(proxyhome)
                          + strlen("/classes")
                          + 1);   /* for terminating null */
      strcpy(newclspath, clspath);
      strcat(newclspath, ":");
    } else {
      newclspath = ALLOCA(strlen(proxyhome) + strlen("/classes") + 1);
      *newclspath = 0;
    }
    strcat(newclspath, proxyhome);
    strcat(newclspath, "/classes");
    setenv("CLASSPATH", (const char *)newclspath, 1); /* 1=overwrite old val */
  }
}

/*
 *  DistillerInit for Java servlets:
 *  - Initialize java VM
 *  - Setup classpath to include classes in proxy source tree
 *  - Call the Initialize method of the Servlet subclass instantiated
 */

#define GM_JNI_VERSION_WE_WANT  0x00010001
#define FindClassOrFail(var,clsname) \
  if ((var = (*env)->FindClass(env,clsname)) == NULL) { \
    fprintf(stderr, "Couldn't load class %s, bye bye\n", clsname); \
    return distFatalError; \
  }
#define GetMethodIDOrFail(var,cls,mthname,type) \
  if ((var = (*env)->GetMethodID(env,cls,mthname,type)) == NULL) { \
    fprintf(stderr, "Couldn't find method %s, bye bye\n", mthname); \
    return distFatalError; \
  }

DistillerStatus
DistillerInit(C_DistillerType distType, int argc, const char * const *argv) 
{
  JDK1_1InitArgs vm_args;

  /*
   * Java version that we expect VM to support
   */
  vm_args.version = GM_JNI_VERSION_WE_WANT;
  if (JNI_GetDefaultJavaVMInitArgs(&vm_args) != 0) {
    fprintf(stderr, "JNI version is 0x%x (expected 0x%x), bye bye\n",
            vm_args.version, GM_JNI_VERSION_WE_WANT);
    return distFatalError;
  }
  /*
   *  Set up the classpath in the environment var CLASSPATH.
   */
  SetClassPath();
  /*
   *  Instantiate the Java VM and try to load classfile specified in argv[0].
   */
  if (JNI_CreateJavaVM(&jvm, &env, &vm_args) != 0) {
    fprintf(stderr, "Failed to create Java VM\n");
    return distFatalError;
  }
    
  /* Retrieve type info for the Java classes we are going to use */

  FindClassOrFail(byteArrayInputStreamClass, "java/io/ByteArrayInputStream");
  FindClassOrFail(byteArrayOutputStreamClass, "java/io/ByteArrayOutputStream");
  FindClassOrFail(pushbackInputStreamClass, "java/io/PushbackInputStream");
  FindClassOrFail(ServletSubclass, argv[0]);

  GetMethodIDOrFail(byteArrayInputStreamConstructor,
                    byteArrayInputStreamClass, "<init>", "([B)V");
  GetMethodIDOrFail(byteArrayOutputStreamConstructor,
                    byteArrayOutputStreamClass, "<init>", "()V");
  GetMethodIDOrFail(byteArrayOutputStreamToByteArray,
                    byteArrayOutputStreamClass, "toByteArray", "()[B");
  GetMethodIDOrFail(pushbackInputStreamConstructor,
                    pushbackInputStreamClass, "<init>",
                    "(Ljava/io/InputStream;)V");
  GetMethodIDOrFail(ServletSubclassInit,
                    ServletSubclass, "<init>",
                    "(Ljava/io/PushbackInputStream;Ljava/io/OutputStream;)V");
  GetMethodIDOrFail(ServletSubclassMain,
                    ServletSubclass, "main", "()V");

  /*
   *  At this point we're ready to call the Init method, and later we'll be
   *  able to call the Main method in DistillerMain()
   */

  return distOk;
}

void
  DistillerCleanup()
{
  /* This call should fail because JDK 1.1 does not support unloading the VM */
  
  (*jvm)->DestroyJavaVM(jvm);
}


DistillerStatus
DistillerMain(Argument *args, int numberOfArgs,
              DistillerInput *input, DistillerOutput *output)
{
#if 0
  jbyteArray inputArray, outputArray;
  jbyte *nativeInputArray, *nativeOutputArray;
  jobject byteArrayInputStream;
  jobject pushbackInputStream;
  jobject byteArrayOutputStream;
  jobject HTMLParser;
  jsize outputArrayLen;

  if (strcmp(input->mimeType, "text/html")) {
    return distFatalError;
  }

  /* Create a ByteArray for input */
  inputArray = (*env)->NewByteArray(env, input->length);
  if (inputArray == NULL) {
    return distFatalError;
  }

  /* Get a C buffer of the ByteArray */
  nativeInputArray = (*env)->GetByteArrayElements(env, inputArray, NULL);
  if (nativeInputArray == NULL) {
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;
  }

  /* Copy the input data into the ByteArray */
  memcpy(nativeInputArray, input->data, input->length);

  /* Release the C buffer */
  (*env)->ReleaseByteArrayElements(env, inputArray, nativeInputArray, 0);

  /* Create a ByteArrayInputStream from the ByteArray */
  byteArrayInputStream = (*env)->NewObject(env, byteArrayInputStreamClass,
					   byteArrayInputStreamConstructor,
					   inputArray);
  if (byteArrayInputStream == NULL) {
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;
  }
    
  /* Create a PushbackInputStream from the ByteArrayInputStream */
  pushbackInputStream = (*env)->NewObject(env, pushbackInputStreamClass,
					  pushbackInputStreamConstructor,
					  byteArrayInputStream);
  if (pushbackInputStream == NULL) {
    (*env)->DeleteLocalRef(env, byteArrayInputStream);
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;
  }

  /* Create a ByteArrayOutputStream */
  byteArrayOutputStream = (*env)->NewObject(env, byteArrayOutputStreamClass,
					    byteArrayOutputStreamConstructor);
  if (byteArrayOutputStream == NULL) {
    (*env)->DeleteLocalRef(env, pushbackInputStream);
    (*env)->DeleteLocalRef(env, byteArrayInputStream);
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;
  }

  /* Create a HTMLParser */
  HTMLParser = (*env)->NewObject(env, HTMLParserClass, HTMLParserConstructor,
				 pushbackInputStream, byteArrayOutputStream);
  if (HTMLParser == NULL) {
    (*env)->DeleteLocalRef(env, byteArrayOutputStream);
    (*env)->DeleteLocalRef(env, pushbackInputStream);
    (*env)->DeleteLocalRef(env, byteArrayInputStream);
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;
  }

  /* Call the parse method */
  (*env)->CallVoidMethod(env, HTMLParser, HTMLParserParse);
  if ((*env)->ExceptionOccurred(env) != NULL) {
    (*env)->ExceptionDescribe(env);
    (*env)->DeleteLocalRef(env, HTMLParser);
    (*env)->DeleteLocalRef(env, byteArrayOutputStream);
    (*env)->DeleteLocalRef(env, pushbackInputStream);
    (*env)->DeleteLocalRef(env, byteArrayInputStream);
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;    
  }

  /* Get a C buffer of the ByteArrayOutputStream */
  outputArray = (*env)->CallObjectMethod(env, byteArrayOutputStream,
					 byteArrayOutputStreamToByteArray);
  outputArrayLen = (*env)->GetArrayLength(env, outputArray);
  nativeOutputArray = (*env)->GetByteArrayElements(env, outputArray, NULL);
  if (nativeOutputArray == NULL) {
    (*env)->DeleteLocalRef(env, outputArray);
    (*env)->DeleteLocalRef(env, HTMLParser);
    (*env)->DeleteLocalRef(env, byteArrayOutputStream);
    (*env)->DeleteLocalRef(env, pushbackInputStream);
    (*env)->DeleteLocalRef(env, byteArrayInputStream);
    (*env)->DeleteLocalRef(env, inputArray);
    return distFatalError;
  }

  /* Allocate a buffer to copy the array and return to caller */
  output->data = DistillerMalloc(outputArrayLen);
  memcpy(output->data, nativeOutputArray, outputArrayLen);
  output->length = outputArrayLen;
  strcpy(output->mimeType, "application/x-distilled-html");
  *freeOutputBuffer = gm_True;

  /* Release the C buffer of the ByteArrayOutputStream */  
  (*env)->ReleaseByteArrayElements(env, outputArray, nativeOutputArray, 0);

  /* Remove all references */
  (*env)->DeleteLocalRef(env, outputArray);
  (*env)->DeleteLocalRef(env, HTMLParser);
  (*env)->DeleteLocalRef(env, byteArrayOutputStream);
  (*env)->DeleteLocalRef(env, pushbackInputStream);
  (*env)->DeleteLocalRef(env, byteArrayInputStream);
  (*env)->DeleteLocalRef(env, inputArray);
#endif
  return distOk;
}
