AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = types metadata .

lib_LTLIBRARIES = libbabeltrace-ctf.la

libbabeltrace_ctf_la_SOURCES = \
	ctf.c \
	events.c \
	iterator.c \
	callbacks.c \
	events-private.h \
	memstream.h

# Request that the linker keeps all static libraries objects.
libbabeltrace_ctf_la_LDFLAGS = \
	-Wl,--no-as-needed \
	types/libctf-types.la \
	metadata/libctf-parser.la \
	metadata/libctf-ast.la
	
libbabeltrace_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
