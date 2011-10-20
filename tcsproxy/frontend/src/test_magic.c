#include "url_magic.h"
#include "distinterface.h"

main () {
  char URLbuf[1024];
  char mURLbuf[1024];
  char buf[1024];
  char *mURL, *URL;
  ArgumentList al;
  
  al.nargs = 1;
  SET_ARG_INT(al.arg[0], 100);
  SET_ARG_ID(al.arg[0], 1);

  while (gets(buf)) {
    if (buf[0] == '0') al.nargs = 0; 
    else 
      if (buf[0] == '1') {
	al.nargs=1;	
	SET_ARG_INT(al.arg[0], 100);
	SET_ARG_ID(al.arg[0], 1);
      } else {
	if (is_magic(buf) && buf[0] != 'm') {
	  printf("that there url is MAGIC\n");
	  mURL = buf;
	  URL = URLbuf;
	  from_magic(mURL, URL, &al);
	  printf("%s->%s\n", mURL, URL);
	} else {
	  printf("that there url isn't MAGIC\n");
	  URL = buf;
	  mURL = mURLbuf;
	  to_magic(mURL, URL, &al);
	  printf("%s->%s\n", URL, mURL);
	}
      } 
  }
}
