# SPDX-FileCopyrightText: 2019-2023 EfficiOS, Inc.
# SPDX-License-Identifier: MIT

# Build the current dir containing the library and plugins before the cli and
# bindings.
SUBDIRS = \
	. \
	cli

if ENABLE_PYTHON_BINDINGS
SUBDIRS += bindings/python/bt2
endif

## This target generates an include file that contains the git version
## string of the current branch, it must be continuously updated when
## we build in the git repo and shipped in dist tarballs to reflect the
## status of the tree when it was generated. If the tree is clean and
## the current commit is a tag starting with "v", consider this a
## release version and set an empty git version.

version_verbose = $(version_verbose_@AM_V@)
version_verbose_ = $(version_verbose_@AM_DEFAULT_V@)
version_verbose_0 = @echo "  GEN     " $@;

common/version.i:
	$(version_verbose)GREP=$(GREP) SED=$(SED) TOP_SRCDIR="$(top_srcdir)" $(SHELL) $(srcdir)/gen-version-i.sh

dist_noinst_SCRIPTS = gen-version-i.sh

# Ensure version.i is generated before any code is built.
BUILT_SOURCES = common/version.i

##
## version.i is defined as a .PHONY target even if it's a real file,
## we want the target to be re-run on every make.
##
.PHONY: common/version.i

CLEANFILES = common/version.i.tmp

##
## Only clean "version.i" on dist-clean, we need to keep it on regular
## clean when it's part of a dist tarball.
##
DISTCLEANFILES = common/version.i

#
# Convenience libraries
#

noinst_LTLIBRARIES = \
	argpar/libargpar.la \
	autodisc/libautodisc.la \
	clock-correlation-validator/libclock-correlation-validator.la \
	common/libcommon.la \
	compat/libcompat.la \
	cpp-common/libcpp-common.la \
	cpp-common/vendor/fmt/libfmt.la \
	ctfser/libctfser.la \
	fd-cache/libfd-cache.la \
	logging/liblogging.la \
	param-parse/libparam-parse.la \
	plugins/common/muxing/libmuxing.la \
	plugins/common/param-validation/libparam-validation.la \
	plugins/ctf/common/metadata/libctf-ast.la \
	plugins/ctf/common/metadata/libctf-parser.la \
	string-format/libstring-format.la


argpar_libargpar_la_SOURCES = \
	argpar/argpar.c \
	argpar/argpar.h

autodisc_libautodisc_la_SOURCES = \
	autodisc/autodisc.c \
	autodisc/autodisc.h

clock_correlation_validator_libclock_correlation_validator_la_SOURCES = \
	clock-correlation-validator/clock-correlation-validator.cpp \
	clock-correlation-validator/clock-correlation-validator.h \
	clock-correlation-validator/clock-correlation-validator.hpp

common_libcommon_la_SOURCES = \
	common/align.h \
	common/assert.c \
	common/assert.h \
	common/common.c \
	common/common.h \
	common/list.h \
	common/macros.h \
	common/mmap-align.h \
	common/safe.h \
	common/uuid.c \
	common/uuid.h \
	common/version.h \
	common/version.i

common_libcommon_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DBABELTRACE_PLUGINS_DIR=\"$(BABELTRACE_PLUGINS_DIR)\"

compat_libcompat_la_SOURCES = \
	compat/bitfield.h \
	compat/compiler.h \
	compat/endian.h \
	compat/fcntl.h \
	compat/glib.h \
	compat/limits.h \
	compat/memstream.h \
	compat/mman.c \
	compat/mman.h \
	compat/socket.h \
	compat/stdio.h \
	compat/stdlib.h \
	compat/string.h \
	compat/time.h \
	compat/unistd.h \
	compat/utc.h

cpp_common_libcpp_common_la_SOURCES = \
	cpp-common/bt2/borrowed-object-iterator.hpp \
	cpp-common/bt2/borrowed-object-proxy.hpp \
	cpp-common/bt2/borrowed-object.hpp \
	cpp-common/bt2/clock-class.hpp \
	cpp-common/bt2/clock-snapshot.hpp \
	cpp-common/bt2/component-class-dev.hpp \
	cpp-common/bt2/component-class.hpp \
	cpp-common/bt2/component-port.hpp \
	cpp-common/bt2/graph.hpp \
	cpp-common/bt2/error.hpp \
	cpp-common/bt2/exc.hpp \
	cpp-common/bt2/field-class.hpp \
	cpp-common/bt2/field-path.hpp \
	cpp-common/bt2/field.hpp \
	cpp-common/bt2/integer-range-set.hpp \
	cpp-common/bt2/integer-range.hpp \
	cpp-common/bt2/internal/comp-cls-bridge.hpp \
	cpp-common/bt2/internal/utils.hpp \
	cpp-common/bt2/logging.hpp \
	cpp-common/bt2/message-array.hpp \
	cpp-common/bt2/message-iterator.hpp \
	cpp-common/bt2/message.hpp \
	cpp-common/bt2/optional-borrowed-object.hpp \
	cpp-common/bt2/plugin-dev.hpp \
	cpp-common/bt2/plugin-load.hpp \
	cpp-common/bt2/plugin-set.hpp \
	cpp-common/bt2/plugin.hpp \
	cpp-common/bt2/private-query-executor.hpp \
	cpp-common/bt2/query-executor.hpp \
	cpp-common/bt2/raw-value-proxy.hpp \
	cpp-common/bt2/self-component-class.hpp \
	cpp-common/bt2/self-component-port.hpp \
	cpp-common/bt2/self-message-iterator-configuration.hpp \
	cpp-common/bt2/self-message-iterator.hpp \
	cpp-common/bt2/shared-object.hpp \
	cpp-common/bt2/trace-ir.hpp \
	cpp-common/bt2/type-traits.hpp \
	cpp-common/bt2/value.hpp \
	cpp-common/bt2/wrap.hpp \
	cpp-common/bt2c/align.hpp \
	cpp-common/bt2c/c-string-view.hpp \
	cpp-common/bt2c/call.hpp \
	cpp-common/bt2c/contains.hpp \
	cpp-common/bt2c/dummy.cpp \
	cpp-common/bt2c/endian.hpp \
	cpp-common/bt2c/exc.hpp \
	cpp-common/bt2c/fmt.hpp \
	cpp-common/bt2c/glib-up.hpp \
	cpp-common/bt2c/libc-up.hpp \
	cpp-common/bt2c/logging.hpp \
	cpp-common/bt2c/prio-heap.hpp \
	cpp-common/bt2c/read-fixed-len-int.hpp \
	cpp-common/bt2c/safe-ops.hpp \
	cpp-common/bt2c/span.hpp \
	cpp-common/bt2c/std-int.hpp \
	cpp-common/bt2c/type-traits.hpp \
	cpp-common/bt2c/uuid.hpp \
	cpp-common/bt2c/vector.hpp \
	cpp-common/bt2s/make-unique.hpp \
	cpp-common/bt2s/optional.hpp \
	cpp-common/bt2s/span.hpp \
	cpp-common/bt2s/string-view.hpp \
	cpp-common/vendor/nlohmann/json.hpp \
	cpp-common/vendor/optional-lite/optional.hpp \
	cpp-common/vendor/span-lite/span.hpp \
	cpp-common/vendor/string-view-lite/string_view.hpp

cpp_common_vendor_fmt_libfmt_la_SOURCES = \
	cpp-common/vendor/fmt/args.h \
	cpp-common/vendor/fmt/chrono.h \
	cpp-common/vendor/fmt/color.h \
	cpp-common/vendor/fmt/compile.h \
	cpp-common/vendor/fmt/core.h \
	cpp-common/vendor/fmt/format-inl.h \
	cpp-common/vendor/fmt/format.cc \
	cpp-common/vendor/fmt/format.h \
	cpp-common/vendor/fmt/os.cc \
	cpp-common/vendor/fmt/os.h \
	cpp-common/vendor/fmt/ostream.h \
	cpp-common/vendor/fmt/printf.h \
	cpp-common/vendor/fmt/ranges.h \
	cpp-common/vendor/fmt/std.h \
	cpp-common/vendor/fmt/xchar.h

ctfser_libctfser_la_SOURCES = \
	ctfser/ctfser.c \
	ctfser/ctfser.h

fd_cache_libfd_cache_la_SOURCES = \
	fd-cache/fd-cache.c \
	fd-cache/fd-cache.h

logging_liblogging_la_SOURCES = \
	logging/comp-logging.h \
	logging/log-api.c \
	logging/log-api.h \
	logging/log.h

param_parse_libparam_parse_la_SOURCES = \
	param-parse/param-parse.c \
	param-parse/param-parse.h

string_format_libstring_format_la_SOURCES = \
	string-format/format-plugin-comp-cls-name.c \
	string-format/format-plugin-comp-cls-name.h \
	string-format/format-error.c \
	string-format/format-error.h

plugins_common_muxing_libmuxing_la_SOURCES = \
	plugins/common/muxing/muxing.c \
	plugins/common/muxing/muxing.h

plugins_common_param_validation_libparam_validation_la_SOURCES = \
	plugins/common/param-validation/param-validation.c \
	plugins/common/param-validation/param-validation.h

# Set flags for the Bison based metadata parser
#  -t : instrument the parser
#  -d : produce a header
#  -v : verbose
#  -Wno-yacc : disable POSIX Yacc incompatibilities warnings
AM_YFLAGS = \
	-t -d -v -Wno-yacc

plugins_ctf_common_metadata_libctf_parser_la_SOURCES = \
	plugins/ctf/common/metadata/lexer.lpp \
	plugins/ctf/common/metadata/parser.ypp \
	plugins/ctf/common/metadata/objstack.cpp

# scanner-symbols.h is included to prefix generated yy_* symbols with bt_.
plugins_ctf_common_metadata_libctf_parser_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-include $(srcdir)/plugins/ctf/common/metadata/scanner-symbols.hpp

# This library contains (mostly) generated code, silence some warnings that it
# produces.
plugins_ctf_common_metadata_libctf_parser_la_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	-Wno-unused-function \
	-Wno-null-dereference \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter

plugins_ctf_common_metadata_libctf_ast_la_SOURCES = \
	plugins/ctf/common/metadata/visitor-generate-ir.cpp \
	plugins/ctf/common/metadata/visitor-semantic-validator.cpp \
	plugins/ctf/common/metadata/visitor-parent-links.cpp \
	plugins/ctf/common/metadata/ast.hpp \
	plugins/ctf/common/metadata/objstack.hpp \
	plugins/ctf/common/metadata/parser.hpp \
	plugins/ctf/common/metadata/parser-wrap.hpp \
	plugins/ctf/common/metadata/scanner.hpp \
	plugins/ctf/common/metadata/scanner-symbols.hpp \
	plugins/ctf/common/metadata/decoder.cpp \
	plugins/ctf/common/metadata/decoder.hpp \
	plugins/ctf/common/metadata/decoder-packetized-file-stream-to-buf.cpp \
	plugins/ctf/common/metadata/decoder-packetized-file-stream-to-buf.hpp \
	plugins/ctf/common/metadata/logging.cpp \
	plugins/ctf/common/metadata/logging.hpp \
	plugins/ctf/common/metadata/ctf-meta.hpp \
	plugins/ctf/common/metadata/ctf-meta-visitors.hpp \
	plugins/ctf/common/metadata/ctf-meta-validate.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-meanings.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-in-ir.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-default-clock-classes.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-text-array-sequence.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-alignments.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-value-storing-indexes.cpp \
	plugins/ctf/common/metadata/ctf-meta-update-stream-class-config.cpp \
	plugins/ctf/common/metadata/ctf-meta-warn-meaningless-header-fields.cpp \
	plugins/ctf/common/metadata/ctf-meta-translate.cpp \
	plugins/ctf/common/metadata/ctf-meta-resolve.cpp \
	plugins/ctf/common/metadata/ctf-meta-configure-ir-trace.cpp \
	plugins/ctf/common/metadata/ctf-meta-configure-ir-trace.hpp

if BABELTRACE_BUILD_WITH_MINGW
plugins_ctf_common_metadata_libctf_ast_la_LIBADD = -lintl -liconv -lole32
endif

BUILT_SOURCES += \
	plugins/ctf/common/metadata/parser.hpp

ALL_LOCAL =

if HAVE_BISON
# We have bison: we can clean the generated parser files
CLEANFILES += \
	plugins/ctf/common/metadata/parser.cpp \
	plugins/ctf/common/metadata/parser.hpp \
	plugins/ctf/common/metadata/parser.output
else # HAVE_BISON
# Create target used to stop the build if we want to build the parser,
# but we don't have the necessary tool to do so
plugins/ctf/common/metadata/parser.cpp plugins/ctf/common/metadata/parser.hpp: plugins/ctf/common/metadata/parser.ypp
	@echo "Error: Cannot build target because bison is missing."
	@echo "Make sure bison is installed and run the configure script again."
	@false

ALL_LOCAL += \
	plugins/ctf/common/metadata/parser.cpp \
	plugins/ctf/common/metadata/parser.hpp
endif # HAVE_BISON

if HAVE_FLEX
# We have flex: we can clean the generated lexer files
CLEANFILES += plugins/ctf/common/metadata/lexer.cpp
else # HAVE_FLEX
# Create target used to stop the build if we want to build the lexer,
# but we don't have the necessary tool to do so
plugins/ctf/common/metadata/lexer.cpp: plugins/ctf/common/metadata/lexer.lpp
	@echo "Error: Cannot build target because flex is missing."
	@echo "Make sure flex is installed and run the configure script again."
	@false

ALL_LOCAL += plugins/ctf/common/metadata/lexer.cpp
endif # HAVE_FLEX

all-local: $(ALL_LOCAL)

if ENABLE_PYTHON_COMMON_DEPS
noinst_LTLIBRARIES += py-common/libpy-common.la

py_common_libpy_common_la_SOURCES = \
	py-common/py-common.c \
	py-common/py-common.h

py_common_libpy_common_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(PYTHON_INCLUDE)

endif # ENABLE_PYTHON_COMMON_DEPS

if ENABLE_DEBUG_INFO
noinst_LTLIBRARIES += plugins/lttng-utils/debug-info/libdebug-info.la

plugins_lttng_utils_debug_info_libdebug_info_la_SOURCES = \
	plugins/lttng-utils/debug-info/bin-info.c \
	plugins/lttng-utils/debug-info/bin-info.h \
	plugins/lttng-utils/debug-info/crc32.c \
	plugins/lttng-utils/debug-info/crc32.h \
	plugins/lttng-utils/debug-info/debug-info.c \
	plugins/lttng-utils/debug-info/debug-info.h \
	plugins/lttng-utils/debug-info/dwarf.c \
	plugins/lttng-utils/debug-info/dwarf.h \
	plugins/lttng-utils/debug-info/trace-ir-data-copy.c \
	plugins/lttng-utils/debug-info/trace-ir-data-copy.h \
	plugins/lttng-utils/debug-info/trace-ir-mapping.c \
	plugins/lttng-utils/debug-info/trace-ir-mapping.h \
	plugins/lttng-utils/debug-info/trace-ir-metadata-copy.c \
	plugins/lttng-utils/debug-info/trace-ir-metadata-copy.h \
	plugins/lttng-utils/debug-info/trace-ir-metadata-field-class-copy.c \
	plugins/lttng-utils/debug-info/trace-ir-metadata-field-class-copy.h \
	plugins/lttng-utils/debug-info/utils.c \
	plugins/lttng-utils/debug-info/utils.h

plugins_lttng_utils_debug_info_libdebug_info_la_LIBADD = \
	fd-cache/libfd-cache.la

endif # ENABLE_DEBUG_INFO


#
# Public libraries
#

lib_LTLIBRARIES = \
	ctf-writer/libbabeltrace2-ctf-writer.la \
	lib/libbabeltrace2.la

lib_libbabeltrace2_la_SOURCES = \
	lib/graph/message/discarded-items.c \
	lib/graph/message/discarded-items.h \
	lib/graph/message/event.c \
	lib/graph/message/event.h \
	lib/graph/message-iterator-class.c \
	lib/graph/message-iterator-class.h \
	lib/graph/message/message.c \
	lib/graph/message/message.h \
	lib/graph/message/message-iterator-inactivity.c \
	lib/graph/message/message-iterator-inactivity.h \
	lib/graph/message/packet.c \
	lib/graph/message/packet.h \
	lib/graph/message/stream.c \
	lib/graph/message/stream.h \
	lib/graph/component.c \
	lib/graph/component-class.c \
	lib/graph/component-class.h \
	lib/graph/component-class-sink-simple.c \
	lib/graph/component-class-sink-simple.h \
	lib/graph/component-descriptor-set.c \
	lib/graph/component-descriptor-set.h \
	lib/graph/component-filter.c \
	lib/graph/component-filter.h \
	lib/graph/component.h \
	lib/graph/component-sink.c \
	lib/graph/component-sink.h \
	lib/graph/component-source.c \
	lib/graph/component-source.h \
	lib/graph/connection.c \
	lib/graph/connection.h \
	lib/graph/graph.c \
	lib/graph/graph.h \
	lib/graph/interrupter.c \
	lib/graph/interrupter.h \
	lib/graph/iterator.c \
	lib/graph/iterator.h \
	lib/graph/mip.c \
	lib/graph/port.c \
	lib/graph/port.h \
	lib/graph/query-executor.c \
	lib/graph/query-executor.h \
	lib/plugin/plugin.c \
	lib/plugin/plugin.h \
	lib/plugin/plugin-so.c \
	lib/plugin/plugin-so.h \
	lib/trace-ir/attributes.c \
	lib/trace-ir/attributes.h \
	lib/trace-ir/clock-class.c \
	lib/trace-ir/clock-class.h \
	lib/trace-ir/clock-snapshot.c \
	lib/trace-ir/clock-snapshot.h \
	lib/trace-ir/event.c \
	lib/trace-ir/event-class.c \
	lib/trace-ir/event-class.h \
	lib/trace-ir/event.h \
	lib/trace-ir/field.c \
	lib/trace-ir/field-class.c \
	lib/trace-ir/field-class.h \
	lib/trace-ir/field.h \
	lib/trace-ir/field-path.c \
	lib/trace-ir/field-path.h \
	lib/trace-ir/field-wrapper.c \
	lib/trace-ir/field-wrapper.h \
	lib/trace-ir/packet.c \
	lib/trace-ir/packet.h \
	lib/trace-ir/resolve-field-path.c \
	lib/trace-ir/resolve-field-path.h \
	lib/trace-ir/stream.c \
	lib/trace-ir/stream-class.c \
	lib/trace-ir/stream-class.h \
	lib/trace-ir/stream.h \
	lib/trace-ir/trace.c \
	lib/trace-ir/trace-class.c \
	lib/trace-ir/trace-class.h \
	lib/trace-ir/trace.h \
	lib/trace-ir/utils.c \
	lib/trace-ir/utils.h \
	lib/assert-cond-base.h \
	lib/assert-cond.h \
	lib/assert-cond.c \
	lib/babeltrace2.c \
	lib/current-thread.c \
	lib/error.c \
	lib/error.h \
	lib/func-status.h \
	lib/integer-range-set.c \
	lib/integer-range-set.h \
	lib/lib-logging.c \
	lib/logging.c \
	lib/logging.h \
	lib/object-pool.c \
	lib/object-pool.h \
	lib/object.h \
	lib/property.h \
	lib/util.c \
	lib/value.c \
	lib/value.h

lib_libbabeltrace2_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

lib_libbabeltrace2_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	'-DBABELTRACE_PLUGIN_PROVIDERS_DIR="$(BABELTRACE_PLUGIN_PROVIDERS_DIR)"'

lib_libbabeltrace2_la_LIBADD = \
	logging/liblogging.la \
	common/libcommon.la \
	compat/libcompat.la \
	clock-correlation-validator/libclock-correlation-validator.la

nodist_EXTRA_lib_libbabeltrace2_la_SOURCES = dummy.cpp

ctf_writer_libbabeltrace2_ctf_writer_la_SOURCES = \
	ctf-writer/assert-pre.h \
	ctf-writer/attributes.c \
	ctf-writer/attributes.h \
	ctf-writer/clock.c \
	ctf-writer/clock-class.c \
	ctf-writer/clock-class.h \
	ctf-writer/clock.h \
	ctf-writer/event.c \
	ctf-writer/event-class.c \
	ctf-writer/event-class.h \
	ctf-writer/event.h \
	ctf-writer/field-path.c \
	ctf-writer/field-path.h \
	ctf-writer/fields.c \
	ctf-writer/fields.h \
	ctf-writer/field-types.c \
	ctf-writer/field-types.h \
	ctf-writer/field-wrapper.c \
	ctf-writer/field-wrapper.h \
	ctf-writer/functor.c \
	ctf-writer/functor.h \
	ctf-writer/logging.c \
	ctf-writer/logging.h \
	ctf-writer/object.c \
	ctf-writer/object.h \
	ctf-writer/object-pool.c \
	ctf-writer/object-pool.h \
	ctf-writer/resolve.c \
	ctf-writer/resolve.h \
	ctf-writer/stream.c \
	ctf-writer/stream-class.c \
	ctf-writer/stream-class.h \
	ctf-writer/stream.h \
	ctf-writer/trace.c \
	ctf-writer/trace.h \
	ctf-writer/utils.c \
	ctf-writer/utils.h \
	ctf-writer/validation.c \
	ctf-writer/validation.h \
	ctf-writer/values.c \
	ctf-writer/values.h \
	ctf-writer/visitor.c \
	ctf-writer/visitor.h \
	ctf-writer/writer.c \
	ctf-writer/writer.h

ctf_writer_libbabeltrace2_ctf_writer_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

ctf_writer_libbabeltrace2_ctf_writer_la_LIBADD = \
	logging/liblogging.la \
	common/libcommon.la \
	ctfser/libctfser.la \
	compat/libcompat.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	babeltrace2-ctf-writer.pc \
	babeltrace2.pc

#
# Python plugin provider
#

if ENABLE_PYTHON_PLUGINS
pluginproviderdir = "$(BABELTRACE_PLUGIN_PROVIDERS_DIR)"
pluginprovider_LTLIBRARIES = python-plugin-provider/babeltrace2-python-plugin-provider.la

python_plugin_provider_babeltrace2_python_plugin_provider_la_SOURCES = \
	python-plugin-provider/python-plugin-provider.c \
	python-plugin-provider/python-plugin-provider.h

python_plugin_provider_babeltrace2_python_plugin_provider_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module \
	$(PYTHON_LDFLAGS)

python_plugin_provider_babeltrace2_python_plugin_provider_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(PYTHON_INCLUDE)

python_plugin_provider_babeltrace2_python_plugin_provider_la_LIBADD =

# Link the Python plugin provider library with libbabeltrace2
# when it's not built-in the babeltrace2 executable.
if !ENABLE_BUILT_IN_PLUGINS
python_plugin_provider_babeltrace2_python_plugin_provider_la_LIBADD += \
	logging/liblogging.la \
	common/libcommon.la \
	py-common/libpy-common.la \
	lib/libbabeltrace2.la
endif
endif # ENABLE_PYTHON_PLUGINS

#
# Plugins
#

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = \
	plugins/ctf/babeltrace-plugin-ctf.la \
	plugins/text/babeltrace-plugin-text.la \
	plugins/utils/babeltrace-plugin-utils.la


# utils plugin
plugins_utils_babeltrace_plugin_utils_la_SOURCES = \
	plugins/utils/counter/counter.c \
	plugins/utils/counter/counter.h \
	plugins/utils/dummy/dummy.c \
	plugins/utils/dummy/dummy.h \
	plugins/utils/muxer/comp.cpp \
	plugins/utils/muxer/comp.hpp \
	plugins/utils/muxer/msg-iter.cpp \
	plugins/utils/muxer/msg-iter.hpp \
	plugins/utils/muxer/upstream-msg-iter.cpp \
	plugins/utils/muxer/upstream-msg-iter.hpp \
	plugins/utils/trimmer/trimmer.c \
	plugins/utils/trimmer/trimmer.h \
	plugins/utils/plugin.cpp

plugins_utils_babeltrace_plugin_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

plugins_utils_babeltrace_plugin_utils_la_LIBADD = \
	plugins/common/muxing/libmuxing.la

if !ENABLE_BUILT_IN_PLUGINS
plugins_utils_babeltrace_plugin_utils_la_LIBADD += \
	lib/libbabeltrace2.la \
	common/libcommon.la \
	cpp-common/vendor/fmt/libfmt.la \
	logging/liblogging.la \
	plugins/common/param-validation/libparam-validation.la
endif

# ctf plugin
plugins_ctf_babeltrace_plugin_ctf_la_SOURCES = \
	plugins/ctf/common/bfcr/bfcr.cpp \
	plugins/ctf/common/bfcr/bfcr.hpp \
	plugins/ctf/common/msg-iter/msg-iter.cpp \
	plugins/ctf/common/msg-iter/msg-iter.hpp \
	plugins/ctf/common/print.hpp \
	plugins/ctf/fs-sink/fs-sink.cpp \
	plugins/ctf/fs-sink/fs-sink-ctf-meta.hpp \
	plugins/ctf/fs-sink/fs-sink.hpp \
	plugins/ctf/fs-sink/fs-sink-stream.cpp \
	plugins/ctf/fs-sink/fs-sink-stream.hpp \
	plugins/ctf/fs-sink/fs-sink-trace.cpp \
	plugins/ctf/fs-sink/fs-sink-trace.hpp \
	plugins/ctf/fs-sink/translate-ctf-ir-to-tsdl.cpp \
	plugins/ctf/fs-sink/translate-ctf-ir-to-tsdl.hpp \
	plugins/ctf/fs-sink/translate-trace-ir-to-ctf-ir.cpp \
	plugins/ctf/fs-sink/translate-trace-ir-to-ctf-ir.hpp \
	plugins/ctf/fs-src/data-stream-file.cpp \
	plugins/ctf/fs-src/data-stream-file.hpp \
	plugins/ctf/fs-src/file.cpp \
	plugins/ctf/fs-src/file.hpp \
	plugins/ctf/fs-src/fs.cpp \
	plugins/ctf/fs-src/fs.hpp \
	plugins/ctf/fs-src/lttng-index.hpp \
	plugins/ctf/fs-src/metadata.cpp \
	plugins/ctf/fs-src/metadata.hpp \
	plugins/ctf/fs-src/query.cpp \
	plugins/ctf/fs-src/query.hpp \
	plugins/ctf/lttng-live/data-stream.cpp \
	plugins/ctf/lttng-live/data-stream.hpp \
	plugins/ctf/lttng-live/lttng-live.cpp \
	plugins/ctf/lttng-live/lttng-live.hpp \
	plugins/ctf/lttng-live/lttng-viewer-abi.hpp \
	plugins/ctf/lttng-live/metadata.cpp \
	plugins/ctf/lttng-live/metadata.hpp \
	plugins/ctf/lttng-live/viewer-connection.cpp \
	plugins/ctf/lttng-live/viewer-connection.hpp \
	plugins/ctf/plugin.cpp

plugins_ctf_babeltrace_plugin_ctf_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

plugins_ctf_babeltrace_plugin_ctf_la_LIBADD = \
	plugins/ctf/common/metadata/libctf-parser.la \
	plugins/ctf/common/metadata/libctf-ast.la \
	plugins/common/param-validation/libparam-validation.la

if BABELTRACE_BUILD_WITH_MINGW
plugins_ctf_babeltrace_plugin_ctf_la_LIBADD += -lws2_32
endif

if !ENABLE_BUILT_IN_PLUGINS
plugins_ctf_babeltrace_plugin_ctf_la_LIBADD += \
	lib/libbabeltrace2.la \
	logging/liblogging.la \
	plugins/common/muxing/libmuxing.la \
	common/libcommon.la \
	ctfser/libctfser.la
endif

# text plugin
plugins_text_babeltrace_plugin_text_la_SOURCES = \
	plugins/text/details/colors.h \
	plugins/text/details/details.c \
	plugins/text/details/details.h \
	plugins/text/details/obj-lifetime-mgmt.c \
	plugins/text/details/obj-lifetime-mgmt.h \
	plugins/text/details/write.c \
	plugins/text/details/write.h \
	plugins/text/dmesg/dmesg.c \
	plugins/text/dmesg/dmesg.h \
	plugins/text/pretty/pretty.c \
	plugins/text/pretty/pretty.h \
	plugins/text/pretty/print.c \
	plugins/text/plugin.c

plugins_text_babeltrace_plugin_text_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

plugins_text_babeltrace_plugin_text_la_LIBADD =

if !ENABLE_BUILT_IN_PLUGINS
plugins_text_babeltrace_plugin_text_la_LIBADD += \
	lib/libbabeltrace2.la \
	common/libcommon.la \
	logging/liblogging.la \
	compat/libcompat.la \
	plugins/common/param-validation/libparam-validation.la
endif

# lttng-utils plugin
if ENABLE_DEBUG_INFO
plugin_LTLIBRARIES += \
	plugins/lttng-utils/babeltrace-plugin-lttng-utils.la

plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_SOURCES = \
	plugins/lttng-utils/plugin.c

plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT) \
	$(ELFUTILS_LIBS)

plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_LIBADD = \
	plugins/lttng-utils/debug-info/libdebug-info.la

if !ENABLE_BUILT_IN_PLUGINS
plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_LIBADD += \
	lib/libbabeltrace2.la \
	common/libcommon.la \
	logging/liblogging.la \
	plugins/common/param-validation/libparam-validation.la
endif # !ENABLE_BUILT_IN_PLUGINS
endif # ENABLE_DEBUG_INFO

EXTRA_DIST = \
	cpp-common/vendor/optional-lite/optional.hpp.license \
	cpp-common/vendor/span-lite/span.hpp.license \
	cpp-common/vendor/string-view-lite/string_view.hpp.license
