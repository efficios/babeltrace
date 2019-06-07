SUBDIRS = trace-ir prio_heap plugin graph

lib_LTLIBRARIES = libbabeltrace2.la

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
	$(top_builddir)/logging/libbabeltrace2-logging.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/compat/libcompat.la

if ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT
libbabeltrace2_la_LIBADD += $(top_builddir)/python-plugin-provider/libbabeltrace2-python-plugin-provider.la
endif
