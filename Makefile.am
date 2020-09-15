## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2001/08/12 03:29:57 thosch Rel $
## Copyright (C) 2001 Thomas Schulz
##
## automakefile for ic35link

pkgdata_DATA =	\
	README	\
	COPYING

EXTRA_DIST = $(pkgdata_DATA)

SUBDIRS = doc src

if MAINTAINER_MODE
MAINTAINERCLEANFILES = \
	aclocal.m4 install-sh missing mkinstalldirs INSTALL \
	Makefile.in stamp-h.in config.h.in configure
else
MAINTAINERCLEANFILES =
endif
