AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-muxer.la

libbabeltrace_plugin_muxer_la_SOURCES = \
	muxer.c \
	muxer.h

libbabeltrace_plugin_muxer_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_muxer_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
