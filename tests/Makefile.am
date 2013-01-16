AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = lib

noinst_PROGRAMS = test-bitfield

test_bitfield_SOURCES = test-bitfield.c

EXTRA_DIST = runall.sh ctf-traces/**

check-am:
	./runall.sh
