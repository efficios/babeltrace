AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_HEADERS = reader-internal.h
noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-reader.la

# Plug-in system library
libbabeltrace_plugin_ctf_reader_la_SOURCES = \
	reader.c

# Request that the linker keeps all static librarie objects.
libbabeltrace_plugin_ctf_reader_la_LDFLAGS = \
	-Wl,--no-as-needed

libbabeltrace_plugin_ctf_reader_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/plugins/libbabeltrace-plugin.la
