AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins \
	    -I$(top_srcdir)/plugins/libctfcopytrace

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-lttng-utils.la

babeltrace_plugin_lttng_utils_la_SOURCES = \
	plugin.c \
	debug-info.h \
	debug-info.c \
	bin-info.c \
	dwarf.c \
	crc32.c \
	utils.c \
	copy.c \
	crc32.h \
	debug-info.h \
	dwarf.h \
	bin-info.h \
	utils.h \
	copy.h \
	logging.c \
	logging.h

babeltrace_plugin_lttng_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module -export-dynamic \
	-lelf -ldw

if !BUILT_IN_PLUGINS
babeltrace_plugin_lttng_utils_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/plugins/libctfcopytrace/libctfcopytrace.la
endif
