AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-trimmer.la

libbabeltrace_plugin_trimmer_la_SOURCES = \
	trimmer.c \
	iterator.c \
	trimmer.h \
	iterator.h

libbabeltrace_plugin_trimmer_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_trimmer_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la
