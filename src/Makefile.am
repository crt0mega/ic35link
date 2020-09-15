## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2001/02/19 01:09:43 tsch Rel $
## Copyright (C) 2001 Thomas Schulz
##
## automakefile for ic35link/src

versinfo.c:	Makefile
	-@echo "Creating versinfo.c"
	-@rm -f versinfo.c
	-@echo '/* versinfo.c */'				 > versinfo.c
	-@echo '/* automatically made by Makefile */'		>> versinfo.c
	-@echo '/* DO NOT EDIT! */'				>> versinfo.c
	-@echo '#include "../config.h"'				>> versinfo.c
	-@echo 'char * pkgvers = VERSION;'			>> versinfo.c
	-@echo 'char * pkgdate = ISODATE;'			>> versinfo.c
	-@echo 'char * bldinfo = "compiled '`date '+%Y-%m-%d %T'` \
				 'by '`whoami`'@'`hostname`'";'	>> versinfo.c

bin_PROGRAMS = ic35sync ic35mgr vcaconv
ic35sync_SOURCES = \
	ic35sync.c		\
	syntrans.c syntrans.h	\
	synproto.c synproto.h	\
	genproto.c genproto.h	\
	dataio.c dataio.h datatxt.c databin.c datavca.c	\
	ic35frec.c ic35frec.h	\
	vcutil.c vcutil.h	\
	vcc.y vcc.h vobject.c vobject.h port.h	\
	comio.c comio.h 	\
	util.c  util.h		\
	versinfo.c
ic35mgr_SOURCES = \
	ic35mgr.c		\
	mgrtrans.c mgrtrans.h	\
	mgrproto.c mgrproto.h	\
	genproto.c genproto.h	\
	comio.c comio.h 	\
	util.c  util.h		\
	versinfo.c
vcaconv_SOURCES = \
	vcaconv.c		\
	dataio.c dataio.h datatxt.c databin.c datavca.c	\
	ic35frec.c ic35frec.h	\
	vcutil.c vcutil.h	\
	vcc.y vcc.h vobject.c vobject.h port.h	\
	util.c util.h		\
	versinfo.c

bin_SCRIPTS = ic35log
ic35log:
	cat $@.sh >$@
	chmod a+x $@

EXTRA_DIST = ic35log.sh

# tell automake to not strip scripts:
INSTALL_SCRIPT = $(INSTALL)

CLEANFILES = ic35log .deps/* versinfo.c
if MAINTAINER_MODE
MAINTAINERCLEANFILES = vcc.c Makefile.in
else
MAINTAINERCLEANFILES =
endif
