AM_CPPFLAGS += -I$(top_srcdir)/plugins

noinst_LTLIBRARIES = libdebug-info.la

libdebug_info_la_LIBADD = \
	$(top_builddir)/fd-cache/libbabeltrace-fd-cache.la

libdebug_info_la_SOURCES = \
	bin-info.c \
	bin-info.h \
	crc32.c \
	crc32.h \
	debug-info.c \
	debug-info.h \
	dwarf.c \
	dwarf.h \
	logging.c \
	logging.h \
	trace-ir-data-copy.c \
	trace-ir-data-copy.h \
	trace-ir-mapping.c \
	trace-ir-mapping.h \
	trace-ir-metadata-copy.c \
	trace-ir-metadata-copy.h \
	trace-ir-metadata-field-class-copy.c \
	trace-ir-metadata-field-class-copy.h \
	utils.c \
	utils.h
