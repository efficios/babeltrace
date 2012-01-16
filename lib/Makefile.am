AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libprio_heap.la libbabeltrace.la

libprio_heap_la_SOURCES = prio_heap.c

libbabeltrace_la_SOURCES = babeltrace.c \
			   callbacks.c \
			   iterator.c

libbabeltrace_la_LIBADD = \
	$(top_builddir)/types/libbabeltrace_types.la \
	$(top_builddir)/lib/libprio_heap.la
