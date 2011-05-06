AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_PROGRAMS = test-bitfield test-dummytrace

test_bitfield_SOURCES = test-bitfield.c

test_dummytrace_SOURCES = test-dummytrace.c


test_dummytrace_LDADD = \
	$(top_builddir)/formats/libbabeltrace_registry.la \
	$(top_builddir)/formats/ctf/libctf.la
