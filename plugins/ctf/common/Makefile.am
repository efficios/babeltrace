SUBDIRS = btr notif-iter metadata

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-common.la
libbabeltrace_plugin_ctf_common_la_SOURCES = print.h
libbabeltrace_plugin_ctf_common_la_LIBADD =		\
	$(builddir)/btr/libctf-btr.la		\
	$(builddir)/metadata/libctf-parser.la	\
	$(builddir)/metadata/libctf-ast.la	\
	$(builddir)/notif-iter/libctf-notif-iter.la
