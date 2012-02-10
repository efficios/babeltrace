SUBDIRS = prio_heap .

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libbabeltrace.la

libbabeltrace_la_SOURCES = babeltrace.c \
			   callbacks.c \
			   iterator.c \
			   context.c \
			   trace-handle.c \
			   trace-collection.c

libbabeltrace_la_LIBADD = \
	$(top_builddir)/types/libbabeltrace_types.la \
	prio_heap/libprio_heap.la
