# SPDX-License-Identifier: MIT

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-utils.la

babeltrace_plugin_utils_la_SOURCES = \
	counter/counter.c \
	counter/counter.h \
	dummy/dummy.c \
	dummy/dummy.h \
	muxer/muxer.c \
	muxer/muxer.h \
	trimmer/trimmer.c \
	trimmer/trimmer.h \
	plugin.c

babeltrace_plugin_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

babeltrace_plugin_utils_la_LIBADD = \
	$(top_builddir)/src/plugins/common/muxing/libbabeltrace2-plugins-common-muxing.la

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_utils_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la
endif
