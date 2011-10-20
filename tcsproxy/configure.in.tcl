dnl autoconf rules to find tcl
dnl $Header: /Users/fox/Documents/fox/consulting/GloMop-GoodwinProcter/CVS-glomop/tcsproxy/configure.in.tcl,v 1.2 1997/06/18 01:17:40 gribble Exp $ (LBL)

AC_ARG_WITH(tcl,	--with-tcl=path	specify a pathname for tcl, d=$withval, d="")
if test "$d" != "" ; then 
	if test ! -d $d ; then 
		echo "'$d' is not a directory"
		exit 1
	fi
	TCLINCLUDE=-I$d/include
	if test ! -r $d/include/tcl.h ; then
		echo "can't find tcl.h in $d/include"
		exit 1
	fi
	places="$d/lib/libtcl7.6.so \
		$d/lib/libtcl7.5.so \
		$d/lib/libtcl7.5.a \
		$d/lib/libtcl.so \
		$d/lib/libtcl.a"
	TCLLIBS=FAIL
	for dir in $places; do
		if test -r $dir ; then
			TCLLIBS=$dir
			break
		fi
	done
	if test $TCLLIBS = FAIL ; then
		echo "can't find libtcl.a in $d/lib"
		exit 1
	fi
else
	AC_TEST_CPP([#include <tcl.h>], TCLINCLUDE="", TCLINCLUDE=FAIL)
	if test "$TCLINCLUDE" = FAIL; then
		echo "checking for tcl.h"
		places="$PWD/../tcl7.5 \
			/usr/src/local/tcl7.5 \
			/import/tcl/include/tcl7.5 \
			$prefix/include \
			$x_includes/tk \
			$x_includes \
			/usr/local/include \
			/usr/contrib/include \
			/usr/include \
			/usr/sww/tcl/include"
		for dir in $places; do
			if test -r $dir/tcl.h ; then
				TCLINCLUDE=-I$dir
			        break
			fi
		done
		if test "$TCLINCLUDE" = FAIL; then
			echo "can't find tcl.h"
			exit 1
		fi
	fi
	AC_CHECK_LIB(tcl, main, TCLLIBS="-ltcl", TCLLIBS="FAIL")
	if test "$TCLLIBS" = FAIL; then
		echo "checking for libtcl.a"
		places="\
			$prefix/lib \
			$x_libraries \
			/usr/contrib/lib \
			/usr/local/lib \
			/usr/lib \
			$PWD/../tcl7.5b3 \
			/usr/src/local/tcl7.5b3 \
			/import/tcl/lib/tcl7.5b3 \
			/usr/sww/tcl/lib"
		for dir in $places; do
			if test -r $dir/libtcl7.5.so -o -r $dir/libtcl7.5.a; then
				TCLLIBS="-L$dir -ltcl7.5"
				break
			fi
			if test -r $dir/libtcl.so -o -r $dir/libtcl.a; then
				TCLLIBS="-L$dir -ltcl"
				break
			fi
		done
		if test "$TCLLIBS" = FAIL; then
			echo "can't find libtcl.a"
			exit 1
		else
			if test $solaris ; then
				TCLLIBS="-R$dir $TCLLIBS"
			fi
		fi
	fi
fi
AC_SUBST(TCLINCLUDE)
AC_SUBST(TCLLIBS)
