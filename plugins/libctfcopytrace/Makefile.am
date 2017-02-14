AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = .

lib_LTLIBRARIES = libctfcopytrace.la

libctfcopytrace_la_SOURCES = ctfcopytrace.c clock-fields.c

libctfcopytrace_la_LDFLAGS = -version-info $(BABELTRACE_LIBRARY_VERSION)

libctfcopytrace_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

noinst_HEADERS = ctfcopytrace.h clock-fields.h
