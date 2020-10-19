SUBDIRS =

babeltrace_plugin_lttng_utils_la_LIBADD =

if ENABLE_DEBUG_INFO
SUBDIRS += debug-info
babeltrace_plugin_lttng_utils_la_LIBADD += \
	debug-info/libdebug-info.la
endif

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-lttng-utils.la

babeltrace_plugin_lttng_utils_la_SOURCES = \
	plugin.c

babeltrace_plugin_lttng_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT) \
	$(ELFUTILS_LIBS)

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_lttng_utils_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la
endif
