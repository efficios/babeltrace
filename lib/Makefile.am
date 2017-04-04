SUBDIRS = ctf-ir ctf-writer prio_heap plugin graph .

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libbabeltrace.la

libbabeltrace_la_SOURCES = babeltrace.c values.c ref.c
libbabeltrace_la_LDFLAGS = -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_la_LIBADD = \
	prio_heap/libprio_heap.la \
	$(top_builddir)/compat/libcompat.la \
	graph/libgraph.la \
	plugin/libplugin.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	ctf-ir/libctf-ir.la \
	ctf-writer/libctf-writer.la

if BUILT_IN_PYTHON_PLUGIN_SUPPORT
libbabeltrace_la_LIBADD += $(top_builddir)/python-plugin-provider/libbabeltrace-python-plugin-provider.la
endif
