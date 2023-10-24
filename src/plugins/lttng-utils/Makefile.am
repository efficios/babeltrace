# SPDX-License-Identifier: MIT

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-lttng-utils.la

babeltrace_plugin_lttng_utils_la_SOURCES = \
	plugin.c

babeltrace_plugin_lttng_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT) \
	$(ELFUTILS_LIBS)

babeltrace_plugin_lttng_utils_la_LIBADD =

if ENABLE_DEBUG_INFO
noinst_LTLIBRARIES = debug-info/libdebug-info.la

debug_info_libdebug_info_la_SOURCES = \
	debug-info/bin-info.c \
	debug-info/bin-info.h \
	debug-info/crc32.c \
	debug-info/crc32.h \
	debug-info/debug-info.c \
	debug-info/debug-info.h \
	debug-info/dwarf.c \
	debug-info/dwarf.h \
	debug-info/trace-ir-data-copy.c \
	debug-info/trace-ir-data-copy.h \
	debug-info/trace-ir-mapping.c \
	debug-info/trace-ir-mapping.h \
	debug-info/trace-ir-metadata-copy.c \
	debug-info/trace-ir-metadata-copy.h \
	debug-info/trace-ir-metadata-field-class-copy.c \
	debug-info/trace-ir-metadata-field-class-copy.h \
	debug-info/utils.c \
	debug-info/utils.h

debug_info_libdebug_info_la_LIBADD = \
	$(top_builddir)/src/fd-cache/libfd-cache.la

babeltrace_plugin_lttng_utils_la_LIBADD += \
	debug-info/libdebug-info.la
endif

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_lttng_utils_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la
endif
