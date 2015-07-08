AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = . ctf

lib_LTLIBRARIES = libbabeltrace-plugin.la

# Plug-in system library
libbabeltrace_plugin_la_SOURCES = \
	component.c \
	source.c \
	sink.c \
	iterator.c

# Request that the linker keeps all static librarie objects.
libbabeltrace_plugin_la_LDFLAGS = \
	-Wl,--no-as-needed -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_plugin_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
