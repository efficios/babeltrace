AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -lpopt

bin_PROGRAMS = babeltrace

#lib_LTLIBRARIES = libbabeltrace.la
#
#libbabeltrace_la_SOURCES = \
#	babeltrace-lib.c
#
#libbabeltrace_la_LIBADD = \
#	$(top_builddir)/types/libbabeltrace_types.la \
#	$(top_builddir)/formats/libbabeltrace_registry.la \
#	$(top_builddir)/formats/ctf/libctf.la \
#	$(top_builddir)/formats/ctf-text/libctf-text.la

babeltrace_SOURCES = \
	babeltrace.c

#babeltrace_LDADD = \
#	libbabeltrace.la

babeltrace_LDADD = \
	$(top_builddir)/types/libbabeltrace_types.la \
	$(top_builddir)/formats/libbabeltrace_registry.la \
	$(top_builddir)/formats/ctf/libctf.la \
	$(top_builddir)/formats/ctf-text/libctf-text.la
