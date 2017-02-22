AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins \
	    -I$(top_srcdir)/plugins/libctfcopytrace

noinst_LTLIBRARIES = libbabeltrace-plugin-trimmer.la
libbabeltrace_plugin_trimmer_la_SOURCES = \
	trimmer.c \
	iterator.c \
	copy.c \
	trimmer.h \
	iterator.h \
	copy.h
