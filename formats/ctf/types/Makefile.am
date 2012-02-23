AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libctf-types.la

libctf_types_la_SOURCES = \
	array.c \
	enum.c \
	float.c \
	integer.c \
	sequence.c \
	string.c \
	struct.c \
	variant.c

libctf_types_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
