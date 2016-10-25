AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = pretty .

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = libbabeltrace-plugin-text.la

libbabeltrace_plugin_text_la_SOURCES = plugin.c
libbabeltrace_plugin_text_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION)
libbabeltrace_plugin_text_la_LIBADD = \
	pretty/libbabeltrace-plugin-text-pretty-cc.la

if !BUILT_IN_PLUGINS
libbabeltrace_plugin_text_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la
endif
