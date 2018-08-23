SUBDIRS = metadata btr notif-iter utils

noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-common.la
libbabeltrace_plugin_ctf_common_la_SOURCES = print.h
libbabeltrace_plugin_ctf_common_la_LIBADD =		\
	$(builddir)/metadata/libctf-parser.la		\
	$(builddir)/metadata/libctf-ast.la		\
	$(builddir)/btr/libctf-btr.la			\
	$(builddir)/notif-iter/libctf-notif-iter.la	\
	$(builddir)/utils/libctf-utils.la
