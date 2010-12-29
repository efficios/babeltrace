AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

bin_PROGRAMS = babeltrace

babeltrace_SOURCES = \
	babeltrace.c

babeltrace_LDADD = \
	$(top_srcdir)/types/libtypes.a \
	$(top_srcdir)/formats/libregistry.a


