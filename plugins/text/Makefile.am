AM_CPPFLAGS += -I$(top_srcdir)/plugins

SUBDIRS = pretty dmesg

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-text.la

babeltrace_plugin_text_la_SOURCES = plugin.c
babeltrace_plugin_text_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module

babeltrace_plugin_text_la_LIBADD = \
	pretty/libbabeltrace-plugin-text-pretty-cc.la \
	dmesg/libbabeltrace-plugin-text-dmesg-cc.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_text_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/compat/libcompat.la
endif
