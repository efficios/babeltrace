AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = . types
#metadata

lib_LTLIBRARIES = libctf.la

libctf_la_SOURCES = \
	ctf.c \
	types/ctf_types.o
#	metadata/ctf_parse.o
