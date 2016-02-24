AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-ctf-text.la

# ctf-text plugin
libbabeltrace_plugin_ctf_text_la_SOURCES = \
	text.c

libbabeltrace_plugin_ctf_text_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_text_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la
