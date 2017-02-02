AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libctf-ir.la

libctf_ir_la_SOURCES = \
	attributes.c \
	clock.c \
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
	values.c \
	ref.c

libctf_ir_la_LDFLAGS = $(LT_NO_UNDEFINED)

libctf_ir_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_ir_la_LIBADD += -luuid
endif
if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_ir_la_LIBADD += -lc
endif
