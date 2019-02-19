AM_CPPFLAGS += -I$(top_srcdir)/plugins

noinst_LTLIBRARIES = libdebug-info.la
libdebug_info_la_SOURCES = \
	bin-info.c \
	bin-info.h \
	crc32.c \
	crc32.h \
	debug-info.c \
	debug-info.h \
	dwarf.c \
	dwarf.h \
	trace-ir-data-copy.c \
	trace-ir-data-copy.h \
	trace-ir-mapping.c \
	trace-ir-mapping.h \
	trace-ir-metadata-copy.c \
	trace-ir-metadata-copy.h \
	trace-ir-metadata-field-class-copy.c \
	trace-ir-metadata-field-class-copy.h \
	logging.c \
	logging.h

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-lttng-utils.la

babeltrace_plugin_lttng_utils_la_SOURCES = \
	plugin.c \
	utils.c \
	utils.h \
	logging.h

babeltrace_plugin_lttng_utils_la_LIBADD = \
	libdebug-info.la

babeltrace_plugin_lttng_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module \
	$(ELFUTILS_LIBS)

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_lttng_utils_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/logging/libbabeltrace-logging.la
endif
