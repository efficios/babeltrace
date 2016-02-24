AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = .

lib_LTLIBRARIES = libbabeltrace-plugin-ctf-text.la

# Plug-in system library
libbabeltrace_plugin_ctf_text_la_SOURCES = \
	text.c

# Request that the linker keeps all static library objects.
libbabeltrace_plugin_ctf_text_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED) -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_ctf_text_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/plugins/libbabeltrace-plugin.la
