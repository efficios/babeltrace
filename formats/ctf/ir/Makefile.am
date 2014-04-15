AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libctf-ir.la

libctf_ir_la_SOURCES = \
	clock.c \
	event.c \
	event-fields.c \
	event-types.c \
	stream-class.c

libctf_ir_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_ir_la_LIBADD += -luuid
endif
if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_ir_la_LIBADD += -lc
endif
