AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = reader lttng-live

lib_LTLIBRARIES = libbabeltrace-plugin-ctf.la

# ctf plugin
libbabeltrace_plugin_ctf_la_SOURCES = symbols.c

libbabeltrace_plugin_ctf_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/plugins/ctf/reader/libbabeltrace-plugin-ctf-reader.la \
	$(top_builddir)/plugins/ctf/lttng-live/libbabeltrace-plugin-ctf-lttng-live.la
