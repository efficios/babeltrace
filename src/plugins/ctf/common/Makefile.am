SUBDIRS = metadata bfcr msg-iter

noinst_LTLIBRARIES = libbabeltrace2-plugin-ctf-common.la
libbabeltrace2_plugin_ctf_common_la_SOURCES = print.h
libbabeltrace2_plugin_ctf_common_la_LIBADD =		\
	$(builddir)/metadata/libctf-parser.la		\
	$(builddir)/metadata/libctf-ast.la		\
	$(builddir)/bfcr/libctf-bfcr.la			\
	$(builddir)/msg-iter/libctf-msg-iter.la
