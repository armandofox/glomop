/* $Id: sgmllexmodule.c,v 1.1.1.1 1996/10/25 02:37:16 fox Exp $ */

#include "Python.h"
#include "abstract.h" /*@# python 1.3 install bug? */
#include "sgml_lex.h"

#include <assert.h>

static PyObject *ErrorObject;
static PyObject *tokTypes[SGML_MAXTOK];

/* ----------------------------------------------------- */

/* Declarations for objects of type SGML_Lexer */

typedef struct {
  PyObject_HEAD
    SGML_Lexer *l;
} SGML_Lexerobject;

staticforward PyTypeObject SGML_Lexertype;



/* ---------------------------------------------------------------- */

static char SGML_Lexer_scan__doc__[] = 
"def dataTagFunc(types, strings): ... \
\
def auxMarkupFunc(types, strings): ... \
\
def errFunc(types, strings): ... \
\
lexer.scan(chars, tagDataFunc, auxMarkupFunc, errFunc)";

static char SGML_Lexer_line__doc__[] = 
"lexer.line() -- current line number, i.e. \
the number of newline chars the lexer has seen. 1-based.";

static char SGML_Lexer_normalize__doc__[] = 
"lexer.normalize(bool) -- true => convert names to lower case,\
eliminate trailing whitespace.";


static void
py_lexfunc(void *rock,
	   SGML_Lexer *l,
	   int qty,
	   SGML_TokType types[],
	   SGML_String strings[],
	   size_t lengths[])
{
  PyObject *proc = rock;
  PyObject *tytuple;

  if( (tytuple = PyTuple_New(qty)) ){
    PyObject *strtuple;

    if( (strtuple = PyTuple_New(qty)) ){
      int i;
      PyObject *args;

      for(i = 0; i < qty; i++){
	PyObject *strO;

	if( (strO = Py_BuildValue("z#", strings[i], lengths[i])) ){
	  /* @# use more efficient SET_ITEM macro? */
	  PyTuple_SetItem(strtuple, i, strO);

	  assert(types[i] < SGML_MAXTOK);

	  PyTuple_SetItem(tytuple, i, tokTypes[types[i]]);
	  Py_INCREF(tokTypes[types[i]]);

	}else{
	  assert(0); /* how to report errors? @@ */
	  break;
	}
      }


      if( (args = Py_BuildValue("OO", tytuple, strtuple)) ){
	PyObject *res;

	res = PyEval_CallObject(proc, args);

	if(res){
	  Py_DECREF(res);
	}else{
	  assert(0);  /* @@ how to report errors? */
	}

	Py_DECREF(args);
      }

      Py_DECREF(strtuple);
    }

    Py_DECREF(tytuple);
  }
}

static PyObject *
SGML_Lexer_scan(self, args)
	SGML_Lexerobject *self;
	PyObject *args;
{
  char *chars;
  int len;
  PyObject *structure;
  PyObject *aux;
  PyObject *errs;

  if (!PyArg_ParseTuple(args, "s#OOO", &chars, &len,
			&structure, &aux, &errs))
    return NULL;

  if(!(PyCallable_Check(structure)
       && PyCallable_Check(aux)
       && PyCallable_Check(errs))){
    PyErr_SetString(PyExc_TypeError, "last 3 args must be callable");
    return NULL;
  }


  SGML_lex(self->l, chars, len,
	   py_lexfunc, structure,
	   py_lexfunc, aux,
	   py_lexfunc, errs);

  if(PyErr_Occurred()) return NULL;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *
SGML_Lexer_line(self, args)
	SGML_Lexerobject *self;
	PyObject *args;
{
  int line;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  line = SGML_lexLine(self->l);

  return Py_BuildValue("i", line);
}


static PyObject *
SGML_Lexer_normalize(self, args)
	SGML_Lexerobject *self;
	PyObject *args;
{
  int oc;
  int c;

  if (!PyArg_ParseTuple(args, "i", &c))
    return NULL;

  oc = SGML_lexNorm(self->l, c);

  return Py_BuildValue("i", oc);
}


static struct PyMethodDef SGML_Lexer_methods[] = {
	{"scan",	SGML_Lexer_scan,	1,	SGML_Lexer_scan__doc__},
	{"line",	SGML_Lexer_line,	1,	SGML_Lexer_line__doc__},
	{"normalize",	SGML_Lexer_normalize,	1,	SGML_Lexer_normalize__doc__},
 
	{NULL,		NULL}		/* sentinel */
};

/* ---------- */


static SGML_Lexerobject *
newSGML_Lexerobject()
{
	SGML_Lexerobject *self;
	
	self = PyObject_NEW(SGML_Lexerobject, &SGML_Lexertype);
	if (self == NULL)
		return NULL;

	self->l = SGML_newLexer();
	assert(self->l); /* @@ set PyErr values? */

	return self;
}


static void
SGML_Lexer_dealloc(self)
	SGML_Lexerobject *self;
{
  SGML_delLexer(self->l);
  PyMem_DEL(self);
}

static PyObject *
SGML_Lexer_getattr(self, name)
	SGML_Lexerobject *self;
	char *name;
{
	return Py_FindMethod(SGML_Lexer_methods, (PyObject *)self, name);
}

static char SGML_Lexertype__doc__[] = 
"See W3C tech report: 'A Lexical Analyzer for HTML and Basic SGML' \
http://www.w3.org/pub/WWW/MarkUp/SGML/#sgml-lex";

static PyTypeObject SGML_Lexertype = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/*ob_size*/
	"SGML_Lexer",			/*tp_name*/
	sizeof(SGML_Lexerobject),		/*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	(destructor)SGML_Lexer_dealloc,	/*tp_dealloc*/
	(printfunc)0,		/*tp_print*/
	(getattrfunc)SGML_Lexer_getattr,	/*tp_getattr*/
	(setattrfunc)0,	/*tp_setattr*/
	(cmpfunc)0,		/*tp_compare*/
	(reprfunc)0,		/*tp_repr*/
	0,			/*tp_as_number*/
	0,		/*tp_as_sequence*/
	0,		/*tp_as_mapping*/
	(hashfunc)0,		/*tp_hash*/
	0,			/*tp_call*/
	(reprfunc)0,		/*tp_str*/

	/* Space for future expansion */
	0L,0L,0L,0L,
	SGML_Lexertype__doc__ /* Documentation string */
};

/* End of code for SGML_Lexer objects */
/* -------------------------------------------------------- */


static char sgml_scanner__doc__[] =
"Create an SGML lexical analyzer object"
;

static PyObject *
sgml_scanner(self, args)
	PyObject *self;	/* Not used */
	PyObject *args;
{

	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	return (PyObject*)newSGML_Lexerobject();
}

/* List of methods defined in the module */

static struct PyMethodDef sgml_methods[] = {
	{"scanner",	sgml_scanner,	1,	sgml_scanner__doc__},
 
	{NULL,		NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called initsgmllex) */

static char sgmllex_module_documentation[] = 
"$Id: sgmllexmodule.c,v 1.1.1.1 1996/10/25 02:37:16 fox Exp $ \
See W3C tech report: 'A Lexical Analyzer for HTML and Basic SGML' \
http://www.w3.org/pub/WWW/MarkUp/SGML/#sgml-lex";

void
initsgmllex()
{
  PyObject *m, *d;
  int i;
  char buf[80]; /* big enough for sgml.XXX forall XXX below: */

  const char *token_names[] = {
    NULL,
    "data", "startTag", "endTag", "tagClose",
    "attrName", "name", "number", "nameToken", "literal",
    "generalEntity", "numCharRef", "refc",
    "processingInstruction",
    "markupDecl", "comment",
    "limitation", "error", "warning"
  };


#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

  assert(ARRAY_SIZE(token_names) == SGML_MAXTOK);

  /* Create the module and add the functions */
  m = Py_InitModule4("sgmllex", sgml_methods,
		     sgmllex_module_documentation,
		     (PyObject*)NULL,PYTHON_API_VERSION);
  
  /* Add some symbolic constants to the module */
  d = PyModule_GetDict(m);
  ErrorObject = PyString_FromString("sgmllex.error");
  PyDict_SetItemString(d, "error", ErrorObject);

  for(i = 1; i < SGML_MAXTOK; i++){
    PyObject *key, *val;

    strcpy(buf, "sgmllex.");
    strcat(buf, token_names[i]);

    if( (val = PyString_FromString(buf)) ){
      if( (key = PyString_FromString((char*)token_names[i])) ){
	tokTypes[i] = val;
	Py_INCREF(tokTypes[i]);

	PyDict_SetItem(d, key, val);
      }
    }
  }
    
  /* Check for errors */
  if (PyErr_Occurred())
    Py_FatalError("can't initialize module sgmllex");
}
