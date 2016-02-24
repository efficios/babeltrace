AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = reader lttng-live

lib_LTLIBRARIES = libbabeltrace-plugin-ctf.la

# Plug-in system library
libbabeltrace_plugin_ctf_la_SOURCES = symbols.c

# Request that the linker keeps all static library objects.
libbabeltrace_plugin_ctf_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED) -version-info $(BABELTRACE_LIBRARY_VERSION)
#	-Wl,--whole-archive,reader/.libs/libbabeltrace-plugin-ctf-reader.a,--no-whole-archive

libbabeltrace_plugin_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/plugins/libbabeltrace-plugin.la \
	$(top_builddir)/plugins/ctf/reader/libbabeltrace-plugin-ctf-reader.la \
	$(top_builddir)/plugins/ctf/lttng-live/libbabeltrace-plugin-ctf-lttng-live.la
