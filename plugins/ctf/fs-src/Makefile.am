AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-fs.la

libbabeltrace_plugin_ctf_fs_la_LIBADD = \
	$(builddir)/../common/libbabeltrace-plugin-ctf-common.la
libbabeltrace_plugin_ctf_fs_la_SOURCES = \
	data-stream-file.c \
	data-stream-file.h \
	file.c \
	file.h \
	fs.c \
	fs.h \
	lttng-index.h \
	metadata.c \
	metadata.h \
	print.h
