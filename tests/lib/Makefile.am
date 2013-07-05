AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LIBRARIES = libtestcommon.a

libtestcommon_a_SOURCES = tap.c tap.h common.c common.h

test_seeks_LDADD = libtestcommon.a \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

test_bitfield_LDADD = libtestcommon.a

noinst_PROGRAMS = test-seeks test-bitfield

test_seeks_SOURCES = test-seeks.c
test_bitfield_SOURCES = test-bitfield.c

EXTRA_DIST = README.tap runall.sh

check-am:
	./runall.sh
