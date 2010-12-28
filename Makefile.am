AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = types . tests

bin_PROGRAMS = babeltrace

babeltrace_LDADD = \
	types/types.a \
	formats/registry.a

# converter/
