# SPDX-FileCopyrightText: 2019-2024 EfficiOS, Inc.
#
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
	$(version_verbose)GREP=$(GREP) \
		SED=$(SED) \
		TOP_SRCDIR="$(top_srcdir)" \
		ENABLE_GIT_VERSION=$(ENABLE_GIT_VERSION) \
		$(SHELL) \
		$(srcdir)/gen-version-i.sh

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
	ctfser/libctfser.la \
	fd-cache/libfd-cache.la \
	logging/liblogging.la \
	param-parse/libparam-parse.la \
	plugins/common/muxing/libmuxing.la \
	plugins/common/param-validation/libparam-validation.la \
	plugins/ctf/common/metadata/libctf-ast.la \
	plugins/ctf/common/metadata/libctf-parser.la \
	so-handle/libso-handle.la \
	string-format/libstring-format.la

if ENABLE_VENDOR_FMT
noinst_LTLIBRARIES += cpp-common/vendor/fmt/libfmt.la

cpp_common_vendor_fmt_libfmt_la_SOURCES = \
	cpp-common/vendor/fmt/args.h \
	cpp-common/vendor/fmt/base.h \
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

cpp_common_vendor_fmt_libfmt_la_CXXFLAGS = \
	$(AM_CXXFLAGS) -Wno-missing-noreturn

FMT_LDADD = cpp-common/vendor/fmt/libfmt.la
else
AM_CPPFLAGS += $(FMT_CFLAGS)
FMT_LDADD = $(FMT_LIBS)
endif


argpar_libargpar_la_SOURCES = \
	argpar/argpar.c \
	argpar/argpar.h

autodisc_libautodisc_la_SOURCES = \
	autodisc/autodisc.cpp \
	autodisc/autodisc.hpp

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
	common/func-status.h \
	common/list.h \
	common/log-and-append.h \
	common/object-struct.h \
	common/object.h \
	common/log-fmt.h \
	common/macros.h \
	common/mmap-align.h \
	common/safe.h \
	common/uuid.c \
	common/uuid.h \
	common/version.h \
	common/version.i

common_libcommon_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DBABELTRACE_PLUGIN_PROVIDERS_DIR=\"$(BABELTRACE_PLUGIN_PROVIDERS_DIR)\" \
	-DBABELTRACE_PLUGINS_DIR=\"$(BABELTRACE_PLUGINS_DIR)\"

compat_libcompat_la_SOURCES = \
	compat/bitfield.h \
	compat/compiler.h \
	compat/endian.h \
	compat/fcntl.h \
	compat/limits.h \
	compat/memstream.h \
	compat/mman.c \
	compat/mman.h \
	compat/socket.hpp \
	compat/stdio.h \
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
	cpp-common/bt2/connection.hpp \
	cpp-common/bt2/graph.hpp \
	cpp-common/bt2/error.hpp \
	cpp-common/bt2/exc.hpp \
	cpp-common/bt2/field-class.hpp \
	cpp-common/bt2/field-path.hpp \
	cpp-common/bt2/field.hpp \
	cpp-common/bt2/field-location.hpp \
	cpp-common/bt2/identity-view.hpp \
	cpp-common/bt2/integer-range-set.hpp \
	cpp-common/bt2/integer-range.hpp \
	cpp-common/bt2/internal/comp-cls-bridge.hpp \
	cpp-common/bt2/internal/utils.hpp \
	cpp-common/bt2/logging.hpp \
	cpp-common/bt2/message-array.hpp \
	cpp-common/bt2/message-iterator.hpp \
	cpp-common/bt2/message.hpp \
	cpp-common/bt2/mip.hpp \
	cpp-common/bt2/optional-borrowed-object.hpp \
	cpp-common/bt2/plugin-so-dev.hpp \
	cpp-common/bt2/plugin-load.hpp \
	cpp-common/bt2/plugin-provider-set.hpp \
	cpp-common/bt2/plugin-provider.hpp \
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
	cpp-common/bt2/version.hpp \
	cpp-common/bt2/wrap.hpp \
	cpp-common/bt2c/aliases.hpp \
	cpp-common/bt2c/align.hpp \
	cpp-common/bt2c/bt2-value-from-json-val.cpp \
	cpp-common/bt2c/bt2-value-from-json-val.hpp \
	cpp-common/bt2c/c-string-view.hpp \
	cpp-common/bt2c/contains.hpp \
	cpp-common/bt2c/data-len.hpp \
	cpp-common/bt2c/endian.hpp \
	cpp-common/bt2c/exc.hpp \
	cpp-common/bt2c/file-utils.cpp \
	cpp-common/bt2c/file-utils.hpp \
	cpp-common/bt2c/fmt.hpp \
	cpp-common/bt2c/glib-up.hpp \
	cpp-common/bt2c/join.hpp \
	cpp-common/bt2c/json-val.cpp \
	cpp-common/bt2c/json-val.hpp \
	cpp-common/bt2c/json-val-req.hpp \
	cpp-common/bt2c/libc-up.hpp \
	cpp-common/bt2c/logging.hpp \
	cpp-common/bt2c/make-span.hpp \
	cpp-common/bt2c/observable.hpp \
	cpp-common/bt2c/parse-json.hpp \
	cpp-common/bt2c/parse-json-as-val.cpp \
	cpp-common/bt2c/parse-json-as-val.hpp \
	cpp-common/bt2c/prio-heap.hpp \
	cpp-common/bt2c/read-fixed-len-int.hpp \
	cpp-common/bt2c/regex.hpp \
	cpp-common/bt2c/reverse-fixed-len-int-bits.hpp \
	cpp-common/bt2c/safe-ops.hpp \
	cpp-common/bt2c/std-int.hpp \
	cpp-common/bt2c/str-scanner.cpp \
	cpp-common/bt2c/str-scanner.hpp \
	cpp-common/bt2c/text-loc.cpp \
	cpp-common/bt2c/text-loc.hpp \
	cpp-common/bt2c/text-loc-str.cpp \
	cpp-common/bt2c/text-loc-str.hpp \
	cpp-common/bt2c/type-traits.hpp \
	cpp-common/bt2c/unicode-conv.cpp \
	cpp-common/bt2c/unicode-conv.hpp \
	cpp-common/bt2c/uuid.hpp \
	cpp-common/bt2c/val-req.hpp \
	cpp-common/bt2c/vector.hpp \
	cpp-common/bt2s/span.hpp \
	cpp-common/vendor/nlohmann/json.hpp \
	cpp-common/vendor/span-lite/span.hpp \
	cpp-common/vendor/wise-enum/optional.h \
	cpp-common/vendor/wise-enum/optional_common.h \
	cpp-common/vendor/wise-enum/wise_enum.h \
	cpp-common/vendor/wise-enum/wise_enum_detail.h \
	cpp-common/vendor/wise-enum/wise_enum_generated.h

ctfser_libctfser_la_SOURCES = \
	ctfser/ctfser.c \
	ctfser/ctfser.h

fd_cache_libfd_cache_la_SOURCES = \
	fd-cache/fd-cache.cpp \
	fd-cache/fd-cache.hpp

logging_liblogging_la_SOURCES = \
	logging/comp-logging.h \
	logging/log-api.c \
	logging/log-api.h \
	logging/log.h

param_parse_libparam_parse_la_SOURCES = \
	param-parse/param-parse.cpp \
	param-parse/param-parse.hpp

so_handle_libso_handle_la_SOURCES = \
	so-handle/so-handle.c \
	so-handle/so-handle.h

string_format_libstring_format_la_SOURCES = \
	string-format/format-plugin-comp-cls-name.cpp \
	string-format/format-plugin-comp-cls-name.hpp \
	string-format/format-error.cpp \
	string-format/format-error.hpp

plugins_common_muxing_libmuxing_la_SOURCES = \
	plugins/common/muxing/muxing.cpp \
	plugins/common/muxing/muxing.hpp

plugins_common_param_validation_libparam_validation_la_SOURCES = \
	plugins/common/param-validation/param-validation.c \
	plugins/common/param-validation/param-validation.h

# Set flags for the Bison based metadata parser
#  -t : instrument the parser
#  -d : produce a header
#  -v : verbose
#  -Wno-yacc : disable POSIX Yacc incompatibilities warnings
#  -Wall : Enable all other warnings
AM_YFLAGS = \
	-t -d -v -Wno-yacc -Wall

plugins_ctf_common_metadata_libctf_parser_la_SOURCES = \
	plugins/ctf/common/src/metadata/tsdl/lexer.lpp \
	plugins/ctf/common/src/metadata/tsdl/parser.ypp \
	plugins/ctf/common/src/metadata/tsdl/objstack.cpp

# scanner-symbols.h is included to prefix generated yy_* symbols with bt_.
plugins_ctf_common_metadata_libctf_parser_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-include $(srcdir)/plugins/ctf/common/src/metadata/tsdl/scanner-symbols.hpp

# This library contains (mostly) generated code, silence some warnings that it
# produces.
plugins_ctf_common_metadata_libctf_parser_la_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	-Wno-unused-function \
	-Wno-null-dereference \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter

plugins_ctf_common_metadata_libctf_ast_la_SOURCES = \
	plugins/ctf/common/src/metadata/tsdl/visitor-generate-ir.cpp \
	plugins/ctf/common/src/metadata/tsdl/visitor-semantic-validator.cpp \
	plugins/ctf/common/src/metadata/tsdl/visitor-parent-links.cpp \
	plugins/ctf/common/src/metadata/tsdl/ast.hpp \
	plugins/ctf/common/src/metadata/tsdl/objstack.hpp \
	plugins/ctf/common/src/metadata/tsdl/parser.hpp \
	plugins/ctf/common/src/metadata/tsdl/parser-wrap.hpp \
	plugins/ctf/common/src/metadata/tsdl/scanner.hpp \
	plugins/ctf/common/src/metadata/tsdl/scanner-symbols.hpp \
	plugins/ctf/common/src/metadata/tsdl/logging.hpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta.hpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-visitors.hpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-validate.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-update-meanings.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-update-default-clock-classes.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-update-text-array-sequence.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-update-alignments.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-warn-meaningless-header-fields.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-resolve.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-configure-ir-trace.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-meta-configure-ir-trace.hpp

if BABELTRACE_BUILD_WITH_MINGW
plugins_ctf_common_metadata_libctf_ast_la_LIBADD = -lintl -liconv -lole32
endif

BUILT_SOURCES += \
	plugins/ctf/common/src/metadata/tsdl/parser.hpp

ALL_LOCAL =

if HAVE_BISON
# We have bison: we can clean the generated parser files
CLEANFILES += \
	plugins/ctf/common/src/metadata/tsdl/parser.cpp \
	plugins/ctf/common/src/metadata/tsdl/parser.hpp \
	plugins/ctf/common/src/metadata/tsdl/parser.output
else # HAVE_BISON
# Create target used to stop the build if we want to build the parser,
# but we don't have the necessary tool to do so
plugins/ctf/common/src/metadata/tsdl/parser.cpp plugins/ctf/common/src/metadata/tsdl/parser.hpp: plugins/ctf/common/src/metadata/tsdl/parser.ypp
	@echo "Error: Cannot build target because bison is missing."
	@echo "Make sure bison is installed and run the configure script again."
	@false

ALL_LOCAL += \
	plugins/ctf/common/src/metadata/tsdl/parser.cpp \
	plugins/ctf/common/src/metadata/tsdl/parser.hpp
endif # HAVE_BISON

if HAVE_FLEX
# We have flex: we can clean the generated lexer files
CLEANFILES += plugins/ctf/src/common/metadata/tsdl/lexer.cpp
else # HAVE_FLEX
# Create target used to stop the build if we want to build the lexer,
# but we don't have the necessary tool to do so
plugins/ctf/common/src/metadata/tsdl/lexer.cpp: plugins/ctf/common/src/metadata/tsdl/lexer.lpp
	@echo "Error: Cannot build target because flex is missing."
	@echo "Make sure flex is installed and run the configure script again."
	@false

ALL_LOCAL += plugins/ctf/common/src/metadata/tsdl/lexer.cpp
endif # HAVE_FLEX

all-local: $(ALL_LOCAL)

if ENABLE_PYTHON_COMMON_DEPS
noinst_LTLIBRARIES += py-common/libpy-common.la

py_common_libpy_common_la_SOURCES = \
	py-common/py-common.cpp \
	py-common/py-common.h \
	py-common/py-common.hpp

py_common_libpy_common_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(PYTHON_INCLUDE)

endif # ENABLE_PYTHON_COMMON_DEPS

if ENABLE_DEBUG_INFO
noinst_LTLIBRARIES += plugins/lttng-utils/debug-info/libdebug-info.la

plugins_lttng_utils_debug_info_libdebug_info_la_SOURCES = \
	plugins/lttng-utils/debug-info/bin-info.cpp \
	plugins/lttng-utils/debug-info/bin-info.hpp \
	plugins/lttng-utils/debug-info/crc32.c \
	plugins/lttng-utils/debug-info/crc32.h \
	plugins/lttng-utils/debug-info/debug-info.cpp \
	plugins/lttng-utils/debug-info/debug-info.hpp \
	plugins/lttng-utils/debug-info/dwarf.cpp \
	plugins/lttng-utils/debug-info/dwarf.hpp \
	plugins/lttng-utils/debug-info/trace-ir-data-copy.cpp \
	plugins/lttng-utils/debug-info/trace-ir-data-copy.hpp \
	plugins/lttng-utils/debug-info/trace-ir-mapping.cpp \
	plugins/lttng-utils/debug-info/trace-ir-mapping.hpp \
	plugins/lttng-utils/debug-info/trace-ir-metadata-copy.cpp \
	plugins/lttng-utils/debug-info/trace-ir-metadata-copy.hpp \
	plugins/lttng-utils/debug-info/trace-ir-metadata-field-class-copy.cpp \
	plugins/lttng-utils/debug-info/trace-ir-metadata-field-class-copy.hpp \
	plugins/lttng-utils/debug-info/utils.cpp \
	plugins/lttng-utils/debug-info/utils.hpp

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
	lib/plugin/plugin-provider.c \
	lib/plugin/plugin-provider.h \
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
	lib/trace-ir/field-location.c \
	lib/trace-ir/field-location.h \
	lib/trace-ir/field-path.c \
	lib/trace-ir/field-path.h \
	lib/trace-ir/packet.c \
	lib/trace-ir/packet.h \
	lib/trace-ir/resolve-field-path.c \
	lib/trace-ir/resolve-field-path.h \
	lib/trace-ir/resolve-field-xref.h \
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
	lib/integer-range-set.c \
	lib/integer-range-set.h \
	lib/lib-logging.c \
	lib/logging.c \
	lib/logging.h \
	lib/object-pool.c \
	lib/object-pool.h \
	lib/property.h \
	lib/util.c \
	lib/value.c \
	lib/value.h

lib_libbabeltrace2_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION)

lib_libbabeltrace2_la_LIBADD = \
	logging/liblogging.la \
	common/libcommon.la \
	compat/libcompat.la \
	clock-correlation-validator/libclock-correlation-validator.la \
	so-handle/libso-handle.la

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
# Shared plugin provider
#

pluginproviderdir = "$(BABELTRACE_PLUGIN_PROVIDERS_DIR)"
pluginprovider_LTLIBRARIES = so-plugin-provider/babeltrace2-so-plugin-provider.la

so_plugin_provider_babeltrace2_so_plugin_provider_la_SOURCES = \
       so-plugin-provider/so-plugin-provider.c

so_plugin_provider_babeltrace2_so_plugin_provider_la_LDFLAGS = \
       $(AM_LDFLAGS) \
       $(LT_NO_UNDEFINED) \
       -avoid-version -module

so_plugin_provider_babeltrace2_so_plugin_provider_la_LIBADD = \
	logging/liblogging.la \
	common/libcommon.la \
	so-handle/libso-handle.la \
	lib/libbabeltrace2.la

#
# Python plugin provider
#

python_plugin_provider_sources = \
	python-plugin-provider/python-plugin-provider.c

if ENABLE_PYTHON_PLUGINS
pluginprovider_LTLIBRARIES += python-plugin-provider/babeltrace2-python-plugin-provider.la

python_plugin_provider_babeltrace2_python_plugin_provider_la_SOURCES = \
	$(python_plugin_provider_sources)

nodist_EXTRA_python_plugin_provider_babeltrace2_python_plugin_provider_la_SOURCES = dummy.cpp

python_plugin_provider_babeltrace2_python_plugin_provider_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module \
	$(PYTHON_LDFLAGS)

python_plugin_provider_babeltrace2_python_plugin_provider_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(PYTHON_INCLUDE)

python_plugin_provider_babeltrace2_python_plugin_provider_la_LIBADD = \
	logging/liblogging.la \
	common/libcommon.la \
	py-common/libpy-common.la \
	lib/libbabeltrace2.la
endif # ENABLE_PYTHON_PLUGINS

#
# Built-in Python plugin provider convenience library
#
# When --enable-built-in-python-plugin-support is used, this convenience
# library is linked into libbabeltrace2 so that Python plugin support is
# available without needing to dlopen() the plugin provider
# shared object.
#

if ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT

noinst_LTLIBRARIES += python-plugin-provider/libpython-plugin-provider-builtin.la

python_plugin_provider_libpython_plugin_provider_builtin_la_SOURCES = \
	$(python_plugin_provider_sources)

python_plugin_provider_libpython_plugin_provider_builtin_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(PYTHON_INCLUDE)

python_plugin_provider_libpython_plugin_provider_builtin_la_LIBADD = \
	py-common/libpy-common.la \
	$(PYTHON_LDFLAGS)

# Add the built-in Python plugin provider to libbabeltrace2.
lib_libbabeltrace2_la_LIBADD += \
	python-plugin-provider/libpython-plugin-provider-builtin.la

endif # ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT

#
# Plugins
#

plugindir = "$(BABELTRACE_PLUGINS_DIR)"
plugin_LTLIBRARIES = \
	plugins/ctf/babeltrace-plugin-ctf.la \
	plugins/text/babeltrace-plugin-text.la \
	plugins/utils/babeltrace-plugin-utils.la


# utils plugin
plugin_utils_sources = \
	plugins/utils/counter/counter.cpp \
	plugins/utils/counter/counter.hpp \
	plugins/utils/dummy/dummy.cpp \
	plugins/utils/dummy/dummy.hpp \
	plugins/utils/muxer/comp.cpp \
	plugins/utils/muxer/comp.hpp \
	plugins/utils/muxer/msg-iter.cpp \
	plugins/utils/muxer/msg-iter.hpp \
	plugins/utils/muxer/upstream-msg-iter.cpp \
	plugins/utils/muxer/upstream-msg-iter.hpp \
	plugins/utils/trimmer/trimmer.c \
	plugins/utils/trimmer/trimmer.h \
	plugins/utils/plugin.cpp

plugins_utils_babeltrace_plugin_utils_la_SOURCES = $(plugin_utils_sources)

plugins_utils_babeltrace_plugin_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

plugins_utils_babeltrace_plugin_utils_la_LIBADD = \
	plugins/common/muxing/libmuxing.la \
	lib/libbabeltrace2.la \
	common/libcommon.la \
	$(FMT_LDADD) \
	logging/liblogging.la \
	plugins/common/param-validation/libparam-validation.la \
	clock-correlation-validator/libclock-correlation-validator.la

# ctf plugin
plugin_ctf_sources = \
	plugins/ctf/common/metadata/ctf-ir.hpp \
	plugins/ctf/common/metadata/int-range.hpp \
	plugins/ctf/common/metadata/int-range-set.hpp \
	plugins/ctf/common/metadata/json-strings.hpp \
	plugins/ctf/common/src/clk-cls-cfg.hpp \
	plugins/ctf/common/lttng-index.hpp \
	plugins/ctf/common/src/item-seq/item.cpp \
	plugins/ctf/common/src/item-seq/item.hpp \
	plugins/ctf/common/src/item-seq/item-seq-iter.cpp \
	plugins/ctf/common/src/item-seq/item-seq-iter.hpp \
	plugins/ctf/common/src/item-seq/item-visitor.cpp \
	plugins/ctf/common/src/item-seq/item-visitor.hpp \
	plugins/ctf/common/src/item-seq/logging-item-visitor.cpp \
	plugins/ctf/common/src/item-seq/logging-item-visitor.hpp \
	plugins/ctf/common/src/item-seq/medium.cpp \
	plugins/ctf/common/src/item-seq/medium.hpp \
	plugins/ctf/common/src/metadata/ctf-ir.cpp \
	plugins/ctf/common/src/metadata/ctf-ir.hpp \
	plugins/ctf/common/src/metadata/json/ctf-2-fc-builder.cpp \
	plugins/ctf/common/src/metadata/json/ctf-2-fc-builder.hpp \
	plugins/ctf/common/src/metadata/json/ctf-2-metadata-stream-parser.cpp \
	plugins/ctf/common/src/metadata/json/ctf-2-metadata-stream-parser.hpp \
	plugins/ctf/common/src/metadata/json/fcs-with-role.cpp \
	plugins/ctf/common/src/metadata/json/fcs-with-role.hpp \
	plugins/ctf/common/src/metadata/json/key-fc-types.cpp \
	plugins/ctf/common/src/metadata/json/key-fc-types.hpp \
	plugins/ctf/common/src/metadata/json/normalize-field-locs.cpp \
	plugins/ctf/common/src/metadata/json/normalize-field-locs.hpp \
	plugins/ctf/common/src/metadata/json/resolve-fcs-with-int-sel.cpp \
	plugins/ctf/common/src/metadata/json/resolve-fcs-with-int-sel.hpp \
	plugins/ctf/common/src/metadata/json/utils.cpp \
	plugins/ctf/common/src/metadata/json/utils.hpp \
	plugins/ctf/common/src/metadata/json/val-req.cpp \
	plugins/ctf/common/src/metadata/json/val-req.hpp \
	plugins/ctf/common/src/metadata/json/validate-scope-fc-roles.cpp \
	plugins/ctf/common/src/metadata/json/validate-scope-fc-roles.hpp \
	plugins/ctf/common/src/metadata/metadata-stream-parser.cpp \
	plugins/ctf/common/src/metadata/metadata-stream-parser.hpp \
	plugins/ctf/common/src/metadata/metadata-stream-parser-utils.cpp \
	plugins/ctf/common/src/metadata/metadata-stream-parser-utils.hpp \
	plugins/ctf/common/src/metadata/normalize-clk-offset.cpp \
	plugins/ctf/common/src/metadata/normalize-clk-offset.hpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-1-metadata-stream-parser.cpp \
	plugins/ctf/common/src/metadata/tsdl/ctf-1-metadata-stream-parser.hpp \
	plugins/ctf/common/src/metadata/tsdl/metadata-stream-decoder.cpp \
	plugins/ctf/common/src/metadata/tsdl/metadata-stream-decoder.hpp \
	plugins/ctf/common/src/msg-iter.cpp \
	plugins/ctf/common/src/msg-iter.hpp \
	plugins/ctf/common/src/null-cp-finder.hpp \
	plugins/ctf/common/src/pkt-props.cpp \
	plugins/ctf/common/src/pkt-props.hpp \
	plugins/ctf/fs-sink/fs-sink-ctf-meta.hpp \
	plugins/ctf/fs-sink/fs-sink-stream.cpp \
	plugins/ctf/fs-sink/fs-sink-stream.hpp \
	plugins/ctf/fs-sink/fs-sink-trace.cpp \
	plugins/ctf/fs-sink/fs-sink-trace.hpp \
	plugins/ctf/fs-sink/fs-sink.cpp \
	plugins/ctf/fs-sink/fs-sink.hpp \
	plugins/ctf/fs-sink/translate-ctf-ir-to-json.cpp \
	plugins/ctf/fs-sink/translate-ctf-ir-to-json.hpp \
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

plugins_ctf_babeltrace_plugin_ctf_la_SOURCES = $(plugin_ctf_sources)

plugins_ctf_babeltrace_plugin_ctf_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

plugins_ctf_babeltrace_plugin_ctf_la_LIBADD = \
	plugins/ctf/common/metadata/libctf-parser.la \
	plugins/ctf/common/metadata/libctf-ast.la \
	plugins/common/param-validation/libparam-validation.la \
	cpp-common/libcpp-common.la \
	lib/libbabeltrace2.la \
	logging/liblogging.la \
	plugins/common/muxing/libmuxing.la \
	common/libcommon.la \
	ctfser/libctfser.la \
	$(FMT_LDADD)

if BABELTRACE_BUILD_WITH_MINGW
plugins_ctf_babeltrace_plugin_ctf_la_LIBADD += -lws2_32
endif

# text plugin
plugin_text_sources = \
	plugins/text/details/colors.h \
	plugins/text/details/details.c \
	plugins/text/details/details.h \
	plugins/text/details/obj-lifetime-mgmt.c \
	plugins/text/details/obj-lifetime-mgmt.h \
	plugins/text/details/write.c \
	plugins/text/details/write.h \
	plugins/text/dmesg/dmesg.c \
	plugins/text/dmesg/dmesg.h \
	plugins/text/pretty/pretty.cpp \
	plugins/text/pretty/pretty.hpp \
	plugins/text/pretty/writer.hpp \
	plugins/text/plugin.cpp

plugins_text_babeltrace_plugin_text_la_SOURCES = $(plugin_text_sources)

plugins_text_babeltrace_plugin_text_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT)

plugins_text_babeltrace_plugin_text_la_LIBADD = \
	lib/libbabeltrace2.la \
	common/libcommon.la \
	$(FMT_LDADD) \
	logging/liblogging.la \
	compat/libcompat.la \
	plugins/common/param-validation/libparam-validation.la

# lttng-utils plugin
plugin_lttng_utils_sources = \
	plugins/lttng-utils/plugin.cpp

if ENABLE_DEBUG_INFO
plugin_LTLIBRARIES += \
	plugins/lttng-utils/babeltrace-plugin-lttng-utils.la

plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_SOURCES = $(plugin_lttng_utils_sources)

plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module $(LD_NOTEXT) \
	$(ELFUTILS_LIBS)

plugins_lttng_utils_babeltrace_plugin_lttng_utils_la_LIBADD = \
	plugins/lttng-utils/debug-info/libdebug-info.la \
	lib/libbabeltrace2.la \
	common/libcommon.la \
	logging/liblogging.la \
	plugins/common/param-validation/libparam-validation.la
endif # ENABLE_DEBUG_INFO

#
# Built-in plugin convenience libraries
#
# These are separate convenience libraries that are compiled with
# -DBT_BUILT_IN_PLUGINS so that they skip BT_PLUGIN_MODULE(). They are
# linked into the CLI with --whole-archive when
# --enable-built-in-plugins is used. The regular .so plugins above are
# always built the same way and remain loadable regardless of
# --enable-built-in-plugins.
#

if ENABLE_BUILT_IN_PLUGINS

noinst_LTLIBRARIES += \
	plugins/ctf/libbabeltrace-plugin-ctf-builtin.la \
	plugins/text/libbabeltrace-plugin-text-builtin.la \
	plugins/utils/libbabeltrace-plugin-utils-builtin.la

# ctf builtin
plugins_ctf_libbabeltrace_plugin_ctf_builtin_la_SOURCES = $(plugin_ctf_sources)
plugins_ctf_libbabeltrace_plugin_ctf_builtin_la_CPPFLAGS = \
	$(AM_CPPFLAGS) -DBT_BUILT_IN_PLUGINS
plugins_ctf_libbabeltrace_plugin_ctf_builtin_la_LIBADD = \
	plugins/ctf/common/metadata/libctf-parser.la \
	plugins/ctf/common/metadata/libctf-ast.la

# text builtin
plugins_text_libbabeltrace_plugin_text_builtin_la_SOURCES = $(plugin_text_sources)
plugins_text_libbabeltrace_plugin_text_builtin_la_CPPFLAGS = \
	$(AM_CPPFLAGS) -DBT_BUILT_IN_PLUGINS

# utils builtin
plugins_utils_libbabeltrace_plugin_utils_builtin_la_SOURCES = $(plugin_utils_sources)
plugins_utils_libbabeltrace_plugin_utils_builtin_la_CPPFLAGS = \
	$(AM_CPPFLAGS) -DBT_BUILT_IN_PLUGINS

if ENABLE_DEBUG_INFO
noinst_LTLIBRARIES += \
	plugins/lttng-utils/libbabeltrace-plugin-lttng-utils-builtin.la

# lttng-utils builtin
plugins_lttng_utils_libbabeltrace_plugin_lttng_utils_builtin_la_SOURCES = $(plugin_lttng_utils_sources)
plugins_lttng_utils_libbabeltrace_plugin_lttng_utils_builtin_la_CPPFLAGS = \
	$(AM_CPPFLAGS) -DBT_BUILT_IN_PLUGINS
plugins_lttng_utils_libbabeltrace_plugin_lttng_utils_builtin_la_LIBADD = \
	plugins/lttng-utils/debug-info/libdebug-info.la
endif # ENABLE_DEBUG_INFO

endif # ENABLE_BUILT_IN_PLUGINS

#
# Built-in libbabeltrace2 convenience library
#
# This is a convenience library version of libbabeltrace2, with the same
# sources as the shared library. When --enable-built-in-plugins is used,
# the CLI links against this instead of the shared library so that the
# BT_PLUGIN_MODULE() accessor functions defined in the library can see
# the plugin descriptor sections from the statically linked builtin
# plugin libraries.
#

if ENABLE_BUILT_IN_PLUGINS
noinst_LTLIBRARIES += lib/libbabeltrace2-builtin.la

lib_libbabeltrace2_builtin_la_SOURCES = $(lib_libbabeltrace2_la_SOURCES)
lib_libbabeltrace2_builtin_la_LIBADD = $(lib_libbabeltrace2_la_LIBADD)
nodist_EXTRA_lib_libbabeltrace2_builtin_la_SOURCES = dummy.cpp
endif # ENABLE_BUILT_IN_PLUGINS

EXTRA_DIST = \
	cpp-common/vendor/span-lite/span.hpp.license \
	cpp-common/vendor/REUSE.toml
