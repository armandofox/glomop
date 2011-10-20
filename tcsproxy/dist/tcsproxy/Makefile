# Generated automatically from Makefile.in by configure.


USER = $(shell whoami)
ifndef OPTIONS
OPTIONS  = gm_options.$(USER)
endif
RATE = 3

all:  $(TARGETS)

run: run_monitor run_ptm run_frontend

run_monitor:
	sbin/monitor.tcl -name monitor ../$(OPTIONS) & 

run_ptm:
	sbin/ptm -o $(OPTIONS) &

run_frontend: 
	sbin/frontend $(OPTIONS) 2> frontend.errs &

run_base_system:
	sbin/fe_check.pl $(OPTIONS) 30 &

run_html_munger:
	sbin/html_munger -o $(OPTIONS) -t transend/text/html &

run_giftojpg_munger:
	sbin/giftojpg_munger -o $(OPTIONS) -t transend/image/gif &

run_jpg_munger:
	sbin/jpg_munger -o $(OPTIONS) -t transend/image/jpeg &

run_caches:
	cd ../caches; /bin/sh RunCache &

run_caches.flusth:
	cd ../caches; RunCache.Flush &
