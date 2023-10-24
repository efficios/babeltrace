# SPDX-License-Identifier: MIT

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-text.la

babeltrace_plugin_text_la_SOURCES = \
	details/colors.h \
	details/details.c \
	details/details.h \
	details/obj-lifetime-mgmt.c \
	details/obj-lifetime-mgmt.h \
	details/write.c \
	details/write.h \
	dmesg/dmesg.c \
	dmesg/dmesg.h \
	pretty/pretty.c \
	pretty/pretty.h \
	pretty/print.c \
	plugin.c

babeltrace_plugin_text_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

babeltrace_plugin_text_la_LIBADD =

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_text_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/compat/libcompat.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la
endif
