AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = types .

lib_LTLIBRARIES = libbabeltrace-ctf-text.la

libbabeltrace_ctf_text_la_SOURCES = \
	ctf-text.c

libbabeltrace_ctf_text_la_LDFLAGS = \
	-Wl,--no-as-needed \
	types/libctf-text-types.la

libbabeltrace_ctf_text_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la
