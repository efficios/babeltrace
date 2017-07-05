AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = dummy trimmer muxer .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-utils.la

babeltrace_plugin_utils_la_SOURCES = plugin.c
babeltrace_plugin_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module -export-dynamic
babeltrace_plugin_utils_la_LIBADD = \
	dummy/libbabeltrace-plugin-dummy-cc.la \
	trimmer/libbabeltrace-plugin-trimmer.la \
	muxer/libbabeltrace-plugin-muxer.la

if !BUILT_IN_PLUGINS
babeltrace_plugin_utils_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace.la
endif
