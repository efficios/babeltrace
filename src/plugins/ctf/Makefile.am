SUBDIRS = common \
	fs-src \
	fs-sink \
	lttng-live

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-ctf.la

# ctf plugin
babeltrace_plugin_ctf_la_SOURCES = plugin.c

babeltrace_plugin_ctf_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

babeltrace_plugin_ctf_la_LIBADD = \
	common/libbabeltrace2-plugin-ctf-common.la \
	fs-sink/libbabeltrace2-plugin-ctf-fs-sink.la \
	fs-src/libbabeltrace2-plugin-ctf-fs-src.la \
	lttng-live/libbabeltrace2-plugin-ctf-lttng-live.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_ctf_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/ctfser/libbabeltrace2-ctfser.la
endif
