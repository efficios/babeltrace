AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = types .

lib_LTLIBRARIES = libctf-text.la

libctf_text_la_SOURCES = \
	ctf-text.c

libctf_text_la_LIBADD = \
	types/libctf-text-types.la \
	$(top_builddir)/lib/libbabeltrace.la
