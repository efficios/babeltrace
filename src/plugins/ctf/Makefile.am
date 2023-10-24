# SPDX-License-Identifier: MIT

SUBDIRS = common/metadata

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = babeltrace-plugin-ctf.la

# ctf plugin
babeltrace_plugin_ctf_la_SOURCES = \
	plugin.cpp \
	common/bfcr/bfcr.cpp \
	common/bfcr/bfcr.hpp \
	common/msg-iter/msg-iter.cpp \
	common/msg-iter/msg-iter.hpp \
	common/print.hpp \
	fs-sink/fs-sink.cpp \
	fs-sink/fs-sink.hpp \
	fs-sink/fs-sink-ctf-meta.hpp \
	fs-sink/fs-sink-stream.cpp \
	fs-sink/fs-sink-stream.hpp \
	fs-sink/fs-sink-trace.cpp \
	fs-sink/fs-sink-trace.hpp \
	fs-sink/translate-ctf-ir-to-tsdl.cpp \
	fs-sink/translate-ctf-ir-to-tsdl.hpp \
	fs-sink/translate-trace-ir-to-ctf-ir.cpp \
	fs-sink/translate-trace-ir-to-ctf-ir.hpp \
	fs-src/data-stream-file.cpp \
	fs-src/data-stream-file.hpp \
	fs-src/file.cpp \
	fs-src/file.hpp \
	fs-src/fs.cpp \
	fs-src/fs.hpp \
	fs-src/lttng-index.hpp \
	fs-src/metadata.cpp \
	fs-src/metadata.hpp \
	fs-src/query.hpp \
	fs-src/query.cpp \
	lttng-live/data-stream.cpp \
	lttng-live/data-stream.hpp \
	lttng-live/lttng-live.cpp \
	lttng-live/lttng-live.hpp \
	lttng-live/metadata.cpp \
	lttng-live/metadata.hpp \
	lttng-live/viewer-connection.cpp \
	lttng-live/viewer-connection.hpp \
	lttng-live/lttng-viewer-abi.hpp

babeltrace_plugin_ctf_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

babeltrace_plugin_ctf_la_LIBADD = \
	common/metadata/libctf-parser.la \
	common/metadata/libctf-ast.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la

if BABELTRACE_BUILD_WITH_MINGW
babeltrace_plugin_ctf_la_LIBADD += -lws2_32
endif

if !ENABLE_BUILT_IN_PLUGINS
babeltrace_plugin_ctf_la_LIBADD += \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/plugins/common/muxing/libbabeltrace2-plugins-common-muxing.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/ctfser/libbabeltrace2-ctfser.la
endif
