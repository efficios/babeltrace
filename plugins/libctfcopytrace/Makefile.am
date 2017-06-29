SUBDIRS = .

noinst_LTLIBRARIES = libctfcopytrace.la

libctfcopytrace_la_SOURCES = ctfcopytrace.c clock-fields.c \
	ctfcopytrace.h clock-fields.h

libctfcopytrace_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
