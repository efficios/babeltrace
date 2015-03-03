AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = .

lib_LTLIBRARIES = libbabeltrace-plugin.la

libbabeltrace_plugin_la_SOURCES = \
	plugin.c \
	source.c \
	sink.c \
	iterator.c

# Request that the linker keeps all static libraries objects.
libbabeltrace_plugin_la_LDFLAGS = \
	-Wl,--no-as-needed -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
	$(top_builddir)/ctf/ir/libctf-ir.la
