# Generated automatically from Makefile.in by configure.
#!make

ROOT      = .
SRC       = $(ROOT)/src
OUTPUT    = $(ROOT)/output

DATABASE  = $(SRC)/database
SHARE     = $(SRC)/share
NETWORK   = $(SRC)/network
PTM       = $(SRC)/ptm
CACHE     = $(SRC)/cache
DIST      = $(SRC)/dist


CC  = gcc
CXX = g++
INSTALL = /usr/bin/ginstall -c

ifneq "$(PURIFY)" ""
	PURIFYPATH = purify -cache-dir=/tmp
endif
LD = $(PURIFYPATH) $(CXX)

RM  = rm -f
AR  = ar cq
RANLIB = ranlib


INCLUDEPATH = .. ../frontend/include ../client_lib/include ../ptm/include ../monitor/include ../playback/include ../gui
INCLUDES= $(patsubst %,-I%,$(INCLUDEPATH))
#DEFINES = -DHAVE_CONFIG_H -D__INSTRUMENT__
DEFINES = -DHAVE_CONFIG_H
CFLAGS =  -I/usr/sww/include -I/opt/include -I/usr/sww/include -I/usr/sww/share/include -D_MIT_POSIX_THREADS -Wall -pedantic -g  -D_REENTRANT $(DEFINES) $(INCLUDES) 
LDFLAGS=  -L/usr/sww/lib -L/opt/lib -L/usr/sww/lib
LIBS =  -ldl -lpthreads 

DATABASE_OBJ= database.o distdb.o
SHARE_OBJ   = error.o threads.o eventhan.o reqrep.o monitor.o options.o \
		rexec.o ptmqueue.o buffer.o
SHARE_OBJ_C = utils.o
NETWORK_OBJ = sockets.o multicast.o packet.o
PTM_OBJ     = ptm.o ptmnet.o ptmdb.o distlaunch.o optionst.o
PTMSTUB_OBJ = cacheman.o cachenet.o cache.o
DIST_OBJ    = distiller.o distnet.o 

	## Default compilation rules
DEFAULT_O = $(RM) $@; $(CXX) -c $(CFLAGS) $< -o $@

%.o : $(PTM)/%.cc
	$(DEFAULT_O)
%.o : $(CACHE)/%.cc
	$(DEFAULT_O)
%.o : $(CACHE)/%.c
	$(DEFAULT_O)
%.o : $(DIST)/%.cc
	$(DEFAULT_O)
%.o : $(DIST)/%.c
	$(RM) $@; $(CC) -c $(CFLAGS) $< -o $@
%.o : $(DATABASE)/%.cc
	$(DEFAULT_O)
%.o : $(SHARE)/%.cc
	$(DEFAULT_O)
%.o : $(NETWORK)/%.cc
	$(DEFAULT_O)

TARGETS = ptm libptmstub.a libdist.a TAGS

all: $(TARGETS)

install: ../sbin/ptm ../lib/libptmstub.a ../lib/libdist.a

../sbin/%: %
	$(INSTALL)  $< $@

../lib/%: %
	$(INSTALL)  $< $@

TAGS: $(DATABASE)/*.cc $(SHARE)/*.cc $(NETWORK)/*.cc $(PTM)/*.cc $(CACHE)/*.cc $(DIST)/*.cc include/*.h
	-etags $^

ptm: $(PTM_OBJ) $(DATABASE_OBJ) $(SHARE_OBJ) $(SHARE_OBJ_C) $(NETWORK_OBJ)
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

libptmstub.a: $(PTMSTUB_OBJ) $(DATABASE_OBJ) $(SHARE_OBJ) $(SHARE_OBJ_C) \
		$(NETWORK_OBJ)
	$(RM) $@
	$(AR) $@ $^
	$(RANLIB) $@

proxy: proxy.o libptmstub.a
	$(RM) $@
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

stub: stub.o libptmstub.a
	$(RM) $@
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

libdist.a: $(DIST_OBJ) $(PTMSTUB_OBJ) $(DATABASE_OBJ) $(SHARE_OBJ) \
		$(SHARE_OBJ_C) $(NETWORK_OBJ)
	$(RM) $@
	$(AR) $@ $^
	$(RANLIB) $@

DISTLIB = -L../lib -ldist -lclib -lmon
distengine: distengine.o libdist.a
	$(LD) -o $@ $(LDFLAGS) $^ $(DISTLIB) $(LIBS)

test: test.o libptmstub.a
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

clean:
	$(RM) *.o include/*~ src/*/*~ core ptm proxy dist stub

utils.o: ../client_lib/src/utils.c
	$(CC) -c $(CFLAGS) $<

ALL_OBJ= $(PTM_OBJ) $(DIST_OBJ) $(PTMSTUB_OBJ) $(DATABASE_OBJ) $(SHARE_OBJ) \
		$(NETWORK_OBJ)
ALL_SRC= $(patsubst %.o,src/*/%.cc,$(ALL_OBJ)) \
		$(patsubst %.o,../*/src/%.c,$(SHARE_OBJ_C)) 

depend:
	../mkdep -c $(CC) -f Makefile.depend $(CFLAGS) $(ALL_SRC)

include Makefile.depend
