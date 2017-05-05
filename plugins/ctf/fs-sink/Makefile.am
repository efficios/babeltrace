AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins \
	-I$(top_srcdir)/plugins/libctfcopytrace

noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-writer.la

libbabeltrace_plugin_ctf_writer_la_LIBADD = \
	$(top_builddir)/plugins/libctfcopytrace/libctfcopytrace.la
libbabeltrace_plugin_ctf_writer_la_SOURCES = writer.c writer.h write.c
