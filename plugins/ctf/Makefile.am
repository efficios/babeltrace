SUBDIRS = common \
	fs-src \
	fs-sink \
	lttng-live

noinst_HEADERS = print.h

plugindir = "$(PLUGINSDIR)"
plugin_LTLIBRARIES = babeltrace-plugin-ctf.la

# ctf plugin
babeltrace_plugin_ctf_la_SOURCES = plugin.c

babeltrace_plugin_ctf_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module

babeltrace_plugin_ctf_la_LIBADD = \
	common/libbabeltrace2-plugin-ctf-common.la \
	fs-sink/libbabeltrace2-plugin-ctf-fs-sink.la \
	fs-src/libbabeltrace2-plugin-ctf-fs-src.la \
	lttng-live/libbabeltrace2-plugin-ctf-lttng-live.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_ctf_la_LIBADD += \
	$(top_builddir)/lib/libbabeltrace2.la \
	$(top_builddir)/logging/libbabeltrace2-logging.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/ctfser/libbabeltrace2-ctfser.la
endif
