AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-ctf-writer.la

# ctf-writer plugin
libbabeltrace_plugin_ctf_writer_la_SOURCES = \
	writer.c \
	write.c \
	writer.h

libbabeltrace_plugin_ctf_writer_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_writer_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
