AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = common fs-src fs-sink lttng-live

noinst_HEADERS = print.h

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-ctf.la

# ctf plugin
libbabeltrace_plugin_ctf_la_SOURCES = plugin.c

libbabeltrace_plugin_ctf_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	fs-src/libbabeltrace-plugin-ctf-fs.la \
	lttng-live/libbabeltrace-plugin-ctf-lttng-live.la \
	fs-sink/libbabeltrace-plugin-ctf-writer.la
