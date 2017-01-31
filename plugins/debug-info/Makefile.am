AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins \
	    -I$(top_srcdir)/plugins/libctfcopytrace

SUBDIRS = .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-debug-info.la

noinst_HEADERS = \
	crc32.h \
	debug-info.h \
	dwarf.h \
	bin-info.h \
	utils.h \
	copy.h

libbabeltrace_plugin_debug_info_la_SOURCES = \
	plugin.c debug-info.h debug-info.c bin-info.c dwarf.c crc32.c utils.c \
	copy.c

libbabeltrace_plugin_debug_info_la_LDFLAGS = \
	-version-info $(BABELTRACE_LIBRARY_VERSION) -lelf -ldw

libbabeltrace_plugin_debug_info_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la
