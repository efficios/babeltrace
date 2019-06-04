SUBDIRS = trace-ir ctf-writer prio_heap plugin graph .

lib_LTLIBRARIES = libbabeltrace2.la libbabeltrace2-ctf.la

libbabeltrace2_la_SOURCES = \
	babeltrace2.c \
	value.c \
	util.c \
	lib-logging.c \
	logging.c \
	object-pool.c
libbabeltrace2_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace2_la_LIBADD = \
	prio_heap/libprio_heap.la \
	graph/libgraph.la \
	plugin/libplugin.la \
	trace-ir/libtrace-ir.la \
	ctf-writer/libctf-writer.la \
	$(top_builddir)/logging/libbabeltrace2-logging.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/ctfser/libbabeltrace2-ctfser.la \
	$(top_builddir)/compat/libcompat.la

if ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT
libbabeltrace2_la_LIBADD += $(top_builddir)/python-plugin-provider/libbabeltrace2-python-plugin-provider.la
endif

# FIXME: Should we drop this? We changed the basename of the library,
#        libbabeltrace2 is not a drop-in replacement for libbabeltrace anymore.
#
# Build a libbabeltrace2-ctf library for backwards compatibility.
# CTF writer used to be in libbabeltrace2-ctf in Babeltrace 1, so this
# file must still exist. As of Babeltrace 2, CTF writer is implemented
# in libbabeltrace2.
libbabeltrace2_ctf_la_SOURCES = $(libbabeltrace2_la_SOURCES)
libbabeltrace2_ctf_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace2_ctf_la_LIBADD = \
	graph/libgraph.la \
	trace-ir/libtrace-ir.la \
	ctf-writer/libctf-writer.la \
	$(top_builddir)/logging/libbabeltrace2-logging.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/ctfser/libbabeltrace2-ctfser.la \
	$(top_builddir)/compat/libcompat.la
