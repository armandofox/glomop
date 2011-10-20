#include "userpref.h"
#include "ALLARGS.h"


void help() {
    printf ("file is a set of lines of the form...\n");
    printf ("uUINT32 : sets the userkeys to the uint32\n");
    printf ("r : reads that userkeys prefs to pref buffer\n");
    printf ("R : read with intent to write\n");
    printf ("w : writes  prefs to database\n");
    printf ("p : prints the pref buffer\n");
    printf ("sID,TYPE,VAL : set pref id ID to type TYPE amd value VAL");
    exit (1);
}

void setpref(UINT32 id, ArgumentType type, void * val, ArgumentList * al) {
  int i;

  for (i = 0; i < al->nargs; i++) { 
    if (ARG_ID(al->arg[i]) == id) {
      switch (type) {
      case 0:
	SET_ARG_INT(al->arg[i], *((int *) val));
	break;
      case 1:
	SET_ARG_STRING(al->arg[i], (char *) val);
	break;
      case 2:
	SET_ARG_DOUBLE(al->arg[i], *((double *) val));
	break;
      default:
	printf ("DANGER!  DANGER!  TYPE WRONG!\n");
	assert(0);
      }
     
      break;
    }
  }
  if (i == al->nargs) {
    al->nargs++;
    SET_ARG_ID(al->arg[i], id);
    switch (type) {
    case 0:
      SET_ARG_INT(al->arg[i], *((int *) val));
      break;
    case 1:
      SET_ARG_STRING(al->arg[i], (char *) val);
      break;
    case 2:
      SET_ARG_DOUBLE(al->arg[i], *((double *) val));
      break;
    default:
      printf ("DANGER!  DANGER!  TYPE WRONG!\n");
      assert(0);
    }
  }
}

void main (int argc, char **argv) {
  userkey key;
  char buf[80];
  char *bufp;
  ArgumentList al;
  UINT32 id;
  ArgumentType type;
  FILE * f;
  INT32 i;
  char   s[MAX_ARG_STRING];
  double d;  

  al.nargs = 0; 

  key = 0;

  if (argc != 3) {
    printf ("usage: %s database file\n", argv[0]);
    help();
  }
 
  if (!(f = fopen(argv[2], "r"))) {
    printf ("can't open %s, exiting\n", argv[2]);
    exit(1);
  }
  
  if (!init_prefs(argv[1])) {
    printf ("can't open database %s, exiting\n", argv[1]);
    exit (1);
  }

  while (fgets(buf, 79, f)) {
    if (buf[strlen(buf) - 1] == '\n') {
      buf[strlen(buf) - 1] = 0 ;
    }
    buf[79] = 0;
    switch (buf[0]) {
    case 'u':
      bufp = &buf[1]; 
      if (*bufp) {
	key = strtoul(bufp, NULL, 10);
      }
      break;
    case 'r':
      get_userprefs(key, &al);
      break;
    case 'R':
      get_userprefs_with_write_intent(key, &al);
      break;
    case 'w':
      set_userprefs(key, &al);
      break;
    case 'p':
      if (al.nargs == 0) break;
      printf ("userkey %lu has the following prefs...\n", key);
      for (i = 0; i < al.nargs; i++) {
	switch (al.arg[i].type) {
	case 0:
	  printf ("\tID %lu, TYPE %i, VALUE %li\n", ARG_ID(al.arg[i]), 
		  al.arg[i].type, ARG_INT(al.arg[i]));
	  break;
	case 1:
	  printf ("\tID %lu, TYPE %i, VALUE %s\n", ARG_ID(al.arg[i]), 
		  al.arg[i].type, ARG_STRING(al.arg[i]));
	  break;
	case 2:
	  printf ("\tID %lu, TYPE %i, VALUE %f\n", ARG_ID(al.arg[i]), 
		  al.arg[i].type, ARG_DOUBLE(al.arg[i]));
	  break;
	default:
	  printf ("WHOOP WHOOP WHOOP!  BAD TYPE %i\n", al.arg[i].type);
	}
      }
      break;
    case 's':
      bufp = &buf[1];
      if (*bufp) {
	id = strtoul(bufp, &bufp, 10);
	bufp++;
	if (*bufp) {
	  type = strtol(bufp, &bufp, 10);
	  bufp++;
	  if (*bufp) {
	    switch (type) {
	    case 0:
	      i = atoi(bufp);	      
	      printf("setting int id = %lu val = %li\n", id, i);	      
	      setpref(id, type, (void *) &i, &al);
	      break;
	    case 1:
	      strcpy(s, bufp);	     
	      printf("setting string id = %lu val = %s\n", id, s);
	      setpref(id, type, (void *) &s, &al);
	      break;
	    case 2:
	      d = atof(bufp);
	      printf("setting double id = %lu val = %f\n", id, d);
	      setpref(id, type, (void *) &d, &al);
	      break;
	    default:
	      printf ("I don't know how to handle type %i\n", type);
	    }	    	      
	  } else {
	    printf ("%s: no value for id %lu type %i\n", buf, id, type);
	  }
	} else {
	  printf("%s: no type for id %lu\n", buf, id);	
	}
      } else {
	printf("%s: no id\n", buf);
      }
    }
  }
}






