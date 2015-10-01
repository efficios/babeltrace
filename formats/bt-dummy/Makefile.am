AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libbabeltrace-dummy.la

libbabeltrace_dummy_la_SOURCES = \
	bt-dummy.c

# Request that the linker keeps all static libraries objects.
libbabeltrace_dummy_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED) -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_dummy_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
