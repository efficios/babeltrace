AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = fs lttng-live

lib_LTLIBRARIES = libbabeltrace-plugin-ctf.la

# ctf plugin
libbabeltrace_plugin_ctf_la_SOURCES = plugin.c

libbabeltrace_plugin_ctf_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/plugins/ctf/fs/libbabeltrace-plugin-ctf-fs.la \
	$(top_builddir)/plugins/ctf/lttng-live/libbabeltrace-plugin-ctf-lttng-live.la
