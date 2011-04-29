AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -lpopt

bin_PROGRAMS = babeltrace

babeltrace_SOURCES = \
	babeltrace.c

babeltrace_LDADD = \
	$(top_builddir)/types/libbabeltrace_types.la \
	$(top_builddir)/formats/libbabeltrace_registry.la \
	$(top_builddir)/formats/ctf/libctf.la
