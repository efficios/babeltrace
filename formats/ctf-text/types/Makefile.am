AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libctf-text-types.la

libctf_text_types_la_SOURCES = \
	array.c \
	enum.c \
	float.c \
	integer.c \
	sequence.c \
	string.c \
	struct.c \
	variant.c

libctf_text_types_la_LDFLAGS = $(LT_NO_UNDEFINED)

libctf_text_types_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
