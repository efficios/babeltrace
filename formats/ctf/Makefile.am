AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = types metadata .

lib_LTLIBRARIES = libctf.la

libctf_la_SOURCES = \
	ctf.c

libctf_la_LIBADD = \
	types/libctf-types.la \
	metadata/libctf-parser.la \
	metadata/libctf-ast.la
