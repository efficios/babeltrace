SUBDIRS = prio_heap .

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libbabeltrace.la

libbabeltrace_la_SOURCES = babeltrace.c \
			   iterator.c \
			   context.c \
			   trace-handle.c \
			   trace-collection.c \
			   registry.c

libbabeltrace_la_LDFLAGS = -version-info $(BABELTRACE_LIBRARY_VERSION)

if ENABLE_DEBUG_INFO
noinst_LTLIBRARIES = libdebug-info.la

libdebug_info_la_SOURCES = debug-info.c \
			  bin-info.c \
			  dwarf.c \
			  crc32.c \
			  utils.c
libdebug_info_la_LDFLAGS = -lelf -ldw
libdebug_info_la_LIBADD = libbabeltrace.la
endif

libbabeltrace_la_LIBADD = \
	prio_heap/libprio_heap.la \
	$(top_builddir)/types/libbabeltrace_types.la \
	$(top_builddir)/compat/libcompat.la
