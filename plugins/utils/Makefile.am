AM_CPPFLAGS += -I$(top_srcdir)/plugins

SUBDIRS = dummy muxer counter trimmer .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-utils.la

babeltrace_plugin_utils_la_SOURCES = plugin.c
babeltrace_plugin_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module
babeltrace_plugin_utils_la_LIBADD = \
	dummy/libbabeltrace-plugin-dummy-cc.la \
	muxer/libbabeltrace-plugin-muxer.la \
	counter/libbabeltrace-plugin-counter-cc.la \
	trimmer/libbabeltrace-plugin-trimmer.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_utils_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/logging/libbabeltrace-logging.la
endif
