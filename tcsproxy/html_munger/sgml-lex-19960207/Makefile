# $Id: Makefile,v 1.1.1.1 1996/10/25 02:37:15 fox Exp $
#
# Makefile for Basic SGML scanner
#
# Targets:
#	libsgml_lex.a    -- lexer library (see sgml_lex.h)
#	sgml_lex         -- test harness
#       test             -- build and run test harness
#       sgmllexmodule.o -- python module
#       dist             -- build distribution

PACKAGE = sgml-lex
RELEASE = 19960207

# Tools not included in this directory:
#
# Perl (tested with perl5.002 beta)
PERL = perl
# flex (tested with flex 2.5.2)
FLEX = flex
# rm
RM = rm
# Tar (gnu tar)
GNUTAR = tar
ZIP = zip

# CC
CC = gcc
#
# Config options
#
CDEBUGFLAGS = -g
CFLAGS = $(CDEBUGFLAGS) $(CWARNFLAGS)
CWARNFLAGS = -ansi -Wall -Werror -pedantic-errors
# not too stable yet... @#

LFLAGS=-p -s -t -8 -b
# -p -> report performance statistics
# -s -> no "default rule"
# -t -> write to stdout rather than lex.yy.c
# -8 -> 8-bit clean scanner
# -b -> backup info

OBJS = sgml_lex.o sgml_dumpevent.o
LIB = libsgml_lex.a
BUILD = *.o *.a sgml_lex filter\
	filter.output test.output

DERIVED = sgml.c sgml_act.c sgml_tab.c test.expected lex-test.sgm

PYTHON_SUPPORT=	sgmllexmodule.c \
	sgmllib.py

ORIGINALS = README Makefile \
	sgml-lex.html \
	sgml.l \
	sgml_lex.h sgml_lex.c \
	main.c sgml_dumpevent.c \
	grab-pre.pl excerpt.pl \
	$(PYTHON_SUPPORT)

all: html_munger

html_munger: $(LIB) munge.o
	$(CC) -o $@ munge.o $(LIB)

# main target/product is sgml_lex library
$(LIB): sgml_lex.o sgml_dumpevent.o
	$(AR) cq $@ $(OBJS)
	ranlib $(LIB)

python-module: sgmllexmodule.so

PYTHON_INCLUDES=-I/usr/local/include/Py

sgmllexmodule.o: sgmllexmodule.c sgml_lex.h
	$(CC) $(CFLAGS) $(PYTHON_INCLUDES) -c sgmllexmodule.c

sgmllexmodule.so: sgmllexmodule.o
	ld -shared -o sgmllexmodule.so sgmllexmodule.o

test: sgml_lex filter lex-test.sgm test.expected
	./sgml_lex -n <lex-test.sgm >test.output
	diff test.expected test.output
	./filter <sgml-lex.html >filter.output
	diff sgml-lex.html filter.output

#@@
problem: filter
	./filter <lex-test.sgm >filter.output
	diff lex-test.sgm filter.output

# Null filter
filter: filter.o $(LIB)
	$(CC) -o $@ filter.o $(LIB)

# Test driver
sgml_lex: main.o $(LIB)
	$(CC) -o $@ main.o $(LIB)

main.o: main.c sgml_lex.h

sgml_dumpevent.o: sgml_lex.h sgml_dumpevent.c

sgml_lex.o: sgml.c sgml_act.c sgml_tab.c sgml_lex.h


#
# You shouldn't generally have to build these...
#
sgml.c: sgml.l
	$(FLEX) $(LFLAGS) sgml.l >$@ || rm $@

sgml_tab.c: sgml.c excerpt.pl
	$(PERL) excerpt.pl tables sgml.c >$@ || rm $@

sgml_act.c: sgml.c excerpt.pl
	$(PERL) excerpt.pl actions sgml.c >$@ || rm $@

lex-test.sgm: sgml-lex.html grab-pre.pl
	$(PERL) grab-pre.pl sgml-lex.html >$@

test.expected: sgml_lex lex-test.sgm
	./sgml_lex -n <lex-test.sgm >test.expected

$(PACKAGE)-$(RELEASE).tar.gz: $(ORIGINALS) $(DERIVED)
	$(GNUTAR) czvf $@ $(ORIGINALS) $(DERIVED)

$(PACKAGE)-$(RELEASE).zip: $(ORIGINALS) $(DERIVED)
	$(ZIP) -k $@ $(ORIGINALS) $(DERIVED)

dist: $(PACKAGE)-$(RELEASE).tar.gz $(PACKAGE)-$(RELEASE).zip

clean:
	$(RM) -f $(BUILD) *~

clobber: clean
	$(RM) -f $(DERIVED)
