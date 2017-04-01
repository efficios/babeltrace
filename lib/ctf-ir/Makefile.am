AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I.

noinst_LTLIBRARIES = libctf-ir.la

libctf_ir_la_SOURCES = \
	attributes.c \
	clock-class.c \
	event.c \
	event-class.c \
	fields.c \
	field-types.c \
	field-path.c \
	packet.c \
	stream.c \
	stream-class.c \
	trace.c \
	utils.c \
	resolve.c \
	validation.c \
	visitor.c

libctf_ir_la_LIBADD =

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_ir_la_LIBADD += -luuid
endif

if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_ir_la_LIBADD += -lc
endif
