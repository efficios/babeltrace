AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-ctf-text.la

# ctf-text plugin
libbabeltrace_plugin_ctf_text_la_SOURCES = \
	text.c \
	print.c \
	text.h

libbabeltrace_plugin_ctf_text_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_text_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la
