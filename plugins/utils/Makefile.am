AM_CPPFLAGS += -I$(top_srcdir)/plugins

SUBDIRS = dummy muxer counter trimmer .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-utils.la

babeltrace_plugin_utils_la_SOURCES = plugin.c
babeltrace_plugin_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module
babeltrace_plugin_utils_la_LIBADD = \
	dummy/libbabeltrace2-plugin-dummy-cc.la \
	muxer/libbabeltrace2-plugin-muxer.la \
	counter/libbabeltrace2-plugin-counter-cc.la \
	trimmer/libbabeltrace2-plugin-trimmer.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_utils_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace2.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/logging/libbabeltrace2-logging.la
endif
