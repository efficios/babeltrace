AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = reader text

# Shared code between all CTF plug-ins
#libbabeltrace_plugin_ctf_la_SOURCES = \
	plugin.c \
	source.c \
	sink.c \
	iterator.c

# Request that the linker keeps all static librarie objects.
#libbabeltrace_plugin_la_LDFLAGS = \
	-Wl,--no-as-needed -version-info $(BABELTRACE_LIBRARY_VERSION)

#libbabeltrace_plugin_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/ctf/ir/libctf-ir.la
