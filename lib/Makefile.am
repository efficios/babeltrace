SUBDIRS = ctf-ir ctf-writer prio_heap plugin graph .

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libbabeltrace.la

libbabeltrace_la_SOURCES = babeltrace.c values.c ref.c logging.c
libbabeltrace_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_la_LIBADD = \
	prio_heap/libprio_heap.la \
	graph/libgraph.la \
	plugin/libplugin.la \
	ctf-ir/libctf-ir.la \
	ctf-writer/libctf-writer.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/compat/libcompat.la

if BUILT_IN_PYTHON_PLUGIN_SUPPORT
libbabeltrace_la_LIBADD += $(top_builddir)/python-plugin-provider/libbabeltrace-python-plugin-provider.la
endif

