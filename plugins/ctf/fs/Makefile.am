AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_HEADERS = \
	fs.h \
	data-stream.h \
	file.h \
	metadata.h \
	print.h
noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-fs.la

libbabeltrace_plugin_ctf_fs_la_LIBADD = \
	$(builddir)/../common/libbabeltrace-plugin-ctf-common.la
libbabeltrace_plugin_ctf_fs_la_SOURCES = \
	fs.c \
	data-stream.c \
	metadata.c \
	file.c
