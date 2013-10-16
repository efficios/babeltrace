AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libctf-writer.la

libctf_writer_la_SOURCES = \
	writer.c \
	clock.c \
	stream.c \
	event-types.c \
	event-fields.c \
	event.c \
	functor.c

libctf_writer_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_writer_la_LIBADD += -luuid
endif
if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_writer_la_LIBADD += -lc
endif
