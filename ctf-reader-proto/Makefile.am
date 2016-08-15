SUBDIRS = metadata-parsing ctf-notif-iter ctf-btr

AM_CFLAGS = $(PACKAGE_CFLAGS)
AM_CPPFLAGS =  -I$(top_srcdir)/include

noinst_PROGRAMS = protorectoral

protorectoral_SOURCES = \
		protorectoral.c \
		ctf-fs.c \
		ctf-fs-file.c \
		ctf-fs-metadata.c \
		ctf-fs-data-stream.c
protorectoral_LDADD = \
		metadata-parsing/libctf-parser.la \
		metadata-parsing/libctf-ast.la \
		ctf-notif-iter/libctf-notif-iter.la \
		ctf-btr/libctf-btr.la \
		$(top_builddir)/lib/libbabeltrace.la \
		$(top_builddir)/formats/ctf/libbabeltrace-ctf.la
