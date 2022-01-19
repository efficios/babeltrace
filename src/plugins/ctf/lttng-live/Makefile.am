# SPDX-License-Identifier: MIT

libbabeltrace2_plugin_ctf_lttng_live_la_SOURCES = \
		lttng-live.cpp \
		lttng-live.hpp \
		data-stream.cpp \
		data-stream.hpp \
		metadata.cpp \
		metadata.hpp \
		viewer-connection.cpp \
		viewer-connection.hpp \
		lttng-viewer-abi.hpp

libbabeltrace2_plugin_ctf_lttng_live_la_LIBADD =

if !ENABLE_BUILT_IN_PLUGINS
libbabeltrace2_plugin_ctf_lttng_live_la_LIBADD += \
	$(top_builddir)/src/plugins/common/muxing/libbabeltrace2-plugins-common-muxing.la
endif

if BABELTRACE_BUILD_WITH_MINGW
libbabeltrace2_plugin_ctf_lttng_live_la_LIBADD += -lws2_32
endif

noinst_LTLIBRARIES = libbabeltrace2-plugin-ctf-lttng-live.la
