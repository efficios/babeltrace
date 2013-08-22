AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/tests/utils

noinst_LIBRARIES = libtestcommon.a

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

libtestcommon_a_SOURCES = common.c common.h

test_seeks_LDADD = $(LIBTAP) libtestcommon.a \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

test_bitfield_LDADD = $(LIBTAP) libtestcommon.a

noinst_PROGRAMS = test-seeks test-bitfield

test_seeks_SOURCES = test-seeks.c
test_bitfield_SOURCES = test-bitfield.c

EXTRA_DIST = runall.sh

check-am:
	./runall.sh
