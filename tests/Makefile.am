AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = lib

EXTRA_DIST = runall.sh ctf-traces/**

check-am:
	./runall.sh
