AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_PROGRAMS = test-bitfield

test_bitfield_SOURCES = test-bitfield.c

EXTRA_DIST = runall.sh

check-am:
	./runall.sh
