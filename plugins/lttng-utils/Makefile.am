AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins \
	    -I$(top_srcdir)/plugins/libctfcopytrace

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-lttng-utils.la

noinst_HEADERS = \
	crc32.h \
	debug-info.h \
	dwarf.h \
	bin-info.h \
	utils.h \
	copy.h

libbabeltrace_plugin_lttng_utils_la_SOURCES = \
	plugin.c debug-info.h debug-info.c bin-info.c dwarf.c crc32.c utils.c \
	copy.c

libbabeltrace_plugin_lttng_utils_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION) -lelf -ldw

if !BUILT_IN_PLUGINS
libbabeltrace_plugin_lttng_utils_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/plugins/libctfcopytrace/libctfcopytrace.la
endif
