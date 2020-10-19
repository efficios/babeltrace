SUBDIRS = dummy muxer counter trimmer

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-utils.la

babeltrace_plugin_utils_la_SOURCES = plugin.c
babeltrace_plugin_utils_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)
babeltrace_plugin_utils_la_LIBADD = \
	dummy/libbabeltrace2-plugin-dummy-cc.la \
	muxer/libbabeltrace2-plugin-muxer.la \
	counter/libbabeltrace2-plugin-counter-cc.la \
	trimmer/libbabeltrace2-plugin-trimmer.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_utils_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la
endif
