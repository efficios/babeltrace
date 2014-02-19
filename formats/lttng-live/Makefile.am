AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_builddir)/include

lib_LTLIBRARIES = libbabeltrace-lttng-live.la

noinst_HEADERS = \
		 lttng-viewer-abi.h \
		 lttng-live.h

libbabeltrace_lttng_live_la_SOURCES = \
	lttng-live-plugin.c lttng-live-comm.c

# Request that the linker keeps all static libraries objects.
libbabeltrace_lttng_live_la_LDFLAGS = \
	-Wl,--no-as-needed -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_lttng_live_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
