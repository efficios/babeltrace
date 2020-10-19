SUBDIRS = pretty dmesg details

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-text.la

babeltrace_plugin_text_la_SOURCES = plugin.c
babeltrace_plugin_text_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

babeltrace_plugin_text_la_LIBADD = \
	pretty/libbabeltrace2-plugin-text-pretty-cc.la \
	dmesg/libbabeltrace2-plugin-text-dmesg-cc.la \
	details/libbabeltrace2-plugin-text-details-cc.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_text_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/compat/libcompat.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la
endif
