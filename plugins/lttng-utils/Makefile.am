SUBDIRS =
if ENABLE_DEBUG_INFO
SUBDIRS += debug-info
endif

AM_CPPFLAGS += -I$(top_srcdir)/plugins

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-lttng-utils.la

babeltrace_plugin_lttng_utils_la_SOURCES = \
	plugin.c

babeltrace_plugin_lttng_utils_la_LIBADD = \
	debug-info/libdebug-info.la

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
