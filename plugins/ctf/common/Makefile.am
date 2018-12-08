SUBDIRS = metadata bfcr msg-iter utils

noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-common.la
libbabeltrace_plugin_ctf_common_la_SOURCES = print.h
libbabeltrace_plugin_ctf_common_la_LIBADD =		\
	$(builddir)/metadata/libctf-parser.la		\
	$(builddir)/metadata/libctf-ast.la		\
	$(builddir)/bfcr/libctf-bfcr.la			\
	$(builddir)/msg-iter/libctf-msg-iter.la	\
	$(builddir)/utils/libctf-utils.la
