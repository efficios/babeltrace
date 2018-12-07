SUBDIRS = trace-ir ctf-writer prio_heap plugin graph .

lib_LTLIBRARIES = libbabeltrace.la libbabeltrace-ctf.la

libbabeltrace_la_SOURCES = \
	babeltrace.c \
	value.c \
	lib-logging.c \
	logging.c \
	object-pool.c
libbabeltrace_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_la_LIBADD = \
	prio_heap/libprio_heap.la \
	graph/libgraph.la \
	plugin/libplugin.la \
	trace-ir/libtrace-ir.la \
	ctf-writer/libctf-writer.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/compat/libcompat.la

if ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT
libbabeltrace_la_LIBADD += $(top_builddir)/python-plugin-provider/libbabeltrace-python-plugin-provider.la
endif

# Build a libbabeltrace-ctf library for backwards compatibility.
# CTF writer used to be in libbabeltrace-ctf in Babeltrace 1, so this
# file must still exist. As of Babeltrace 2, CTF writer is implemented
# in libbabeltrace.
libbabeltrace_ctf_la_SOURCES = $(libbabeltrace_la_SOURCES)
libbabeltrace_ctf_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_ctf_la_LIBADD = \
	graph/libgraph.la \
	trace-ir/libtrace-ir.la \
	ctf-writer/libctf-writer.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/compat/libcompat.la
