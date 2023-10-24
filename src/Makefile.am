# SPDX-License-Identifier: MIT

# Build 'common' first to ensure 'version.h' is generated before any
# compilation.
SUBDIRS = \
	common \
	. \
	plugins \
	cli \
	bindings

noinst_HEADERS = \
	cpp-common/bt2/clock-class.hpp \
	cpp-common/bt2/clock-snapshot.hpp \
	cpp-common/bt2/common-iter.hpp \
	cpp-common/bt2/exc.hpp \
	cpp-common/bt2/field-class.hpp \
	cpp-common/bt2/field.hpp \
	cpp-common/bt2/field-path.hpp \
	cpp-common/bt2/integer-range.hpp \
	cpp-common/bt2/integer-range-set.hpp \
	cpp-common/bt2/internal/borrowed-obj.hpp \
	cpp-common/bt2/internal/shared-obj.hpp \
	cpp-common/bt2/internal/utils.hpp \
	cpp-common/bt2/logging.hpp \
	cpp-common/bt2/message.hpp \
	cpp-common/bt2/trace-ir.hpp \
	cpp-common/bt2/type-traits.hpp \
	cpp-common/bt2/value.hpp \
	cpp-common/align.hpp \
	cpp-common/cfg-error-reporting.hpp \
	cpp-common/cfg-error-reporting-throw.hpp \
	cpp-common/cfg-logging-error-reporting.hpp \
	cpp-common/cfg-logging-error-reporting-throw.hpp \
	cpp-common/cfg-logging.hpp \
	cpp-common/endian.hpp \
	cpp-common/exc.hpp \
	cpp-common/glib-up.hpp \
	cpp-common/libc-up.hpp \
	cpp-common/lib-str.hpp \
	cpp-common/log-cfg.hpp \
	cpp-common/make-unique.hpp \
	cpp-common/nlohmann/json.hpp \
	cpp-common/optional.hpp \
	cpp-common/read-fixed-len-int.hpp \
	cpp-common/safe-ops.hpp \
	cpp-common/std-int.hpp \
	cpp-common/string_view.hpp \
	cpp-common/uuid.hpp \
	cpp-common/uuid-view.hpp \
	cpp-common/vector.hpp

#
# Convenience libraries
#

noinst_LTLIBRARIES = \
	argpar/libargpar.la \
	autodisc/libautodisc.la \
	compat/libcompat.la \
	ctfser/libctfser.la \
	fd-cache/libfd-cache.la \
	logging/liblogging.la \
	param-parse/libparam-parse.la \
	string-format/libstring-format.la

argpar_libargpar_la_SOURCES = \
	argpar/argpar.c \
	argpar/argpar.h

autodisc_libautodisc_la_SOURCES = \
	autodisc/autodisc.c \
	autodisc/autodisc.h

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

ctfser_libctfser_la_SOURCES = \
	ctfser/ctfser.c \
	ctfser/ctfser.h

fd_cache_libfd_cache_la_SOURCES = \
	fd-cache/fd-cache.c \
	fd-cache/fd-cache.h

logging_liblogging_la_SOURCES = \
	logging/comp-logging.h \
	logging/log.c \
	logging/log.h

param_parse_libparam_parse_la_SOURCES = \
	param-parse/param-parse.c \
	param-parse/param-parse.h

string_format_libstring_format_la_SOURCES = \
	string-format/format-plugin-comp-cls-name.c \
	string-format/format-plugin-comp-cls-name.h \
	string-format/format-error.c \
	string-format/format-error.h

if ENABLE_PYTHON_COMMON_DEPS
noinst_LTLIBRARIES += py-common/libpy-common.la

py_common_libpy_common_la_SOURCES = \
	py-common/py-common.c \
	py-common/py-common.h

py_common_libpy_common_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(PYTHON_INCLUDE)

endif # ENABLE_PYTHON_COMMON_DEPS

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
	lib/graph/message/iterator.h \
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
	lib/graph/mip.c \
	lib/graph/port.c \
	lib/graph/port.h \
	lib/graph/query-executor.c \
	lib/graph/query-executor.h \
	lib/plugin/plugin.c \
	lib/plugin/plugin.h \
	lib/plugin/plugin-so.c \
	lib/plugin/plugin-so.h \
	lib/prio-heap/prio-heap.c \
	lib/prio-heap/prio-heap.h \
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
	common/libbabeltrace2-common.la \
	compat/libcompat.la

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
	common/libbabeltrace2-common.la \
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
	common/libbabeltrace2-common.la \
	py-common/libpy-common.la \
	lib/libbabeltrace2.la
endif
endif # ENABLE_PYTHON_PLUGINS
