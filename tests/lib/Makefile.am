AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LIBRARIES = libtestcommon.a

libtestcommon_a_SOURCES = tap.c common.c

test_seeks_LDADD = libtestcommon.a \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

noinst_PROGRAMS = test-seeks

test_seeks_SOURCES = test-seeks.c

check-am:
	./runall.sh
