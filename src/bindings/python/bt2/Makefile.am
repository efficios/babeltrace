# SPDX-FileCopyrightText: 2019-2024 EfficiOS, Inc.
#
# SPDX-License-Identifier: MIT

# Libtool relinks the native extension module against the installed
# libbabeltrace2 at install time: also search the build directory of
# libbabeltrace2 so that installing only this directory works even when
# libbabeltrace2 isn't installed yet.
AM_LDFLAGS += -L$(top_builddir)/src/lib/.libs

# Sources of the native extension module `bt2.native_bt`
NATIVE_BT_SRCS =					\
	native/api.hpp					\
	native/api/autodisc.cpp				\
	native/api/clock-class.cpp			\
	native/api/clock-snapshot.cpp			\
	native/api/component-class-dev.cpp		\
	native/api/component-class.cpp			\
	native/api/component.cpp			\
	native/api/connection.cpp			\
	native/api/error.cpp				\
	native/api/event-class.cpp			\
	native/api/event.cpp				\
	native/api/field-class.cpp			\
	native/api/field-location.cpp			\
	native/api/field-path.cpp			\
	native/api/field.cpp				\
	native/api/format-error.cpp			\
	native/api/graph.cpp				\
	native/api/integer-range-set.cpp		\
	native/api/interrupter.cpp			\
	native/api/logging.cpp				\
	native/api/message-iterator.cpp			\
	native/api/message.cpp				\
	native/api/mip.cpp				\
	native/api/packet.cpp				\
	native/api/plugin.cpp				\
	native/api/port.cpp				\
	native/api/private-query-executor.cpp		\
	native/api/query-executor.cpp			\
	native/api/self-component-port.cpp		\
	native/api/self-component.cpp			\
	native/api/self-message-iterator.cpp		\
	native/api/stream-class.cpp			\
	native/api/stream.cpp				\
	native/api/trace-class.cpp			\
	native/api/trace.cpp				\
	native/api/value.cpp				\
	native/api/version.cpp				\
	native/native-bt.cpp				\
	native/runtime/exc.cpp				\
	native/runtime/exc.hpp				\
	native/runtime/logging.cpp			\
	native/runtime/logging.hpp			\
	native/runtime/ptr-obj.cpp			\
	native/runtime/ptr-obj.hpp			\
	native/runtime/py-conv.hpp			\
	native/runtime/py-module-builder.cpp		\
	native/runtime/py-module-builder.hpp		\
	native/runtime/utils.hpp

STATIC_BINDINGS_DEPS =					\
	bt2/__init__.py					\
	bt2/clock_class.py				\
	bt2/clock_snapshot.py				\
	bt2/component.py				\
	bt2/component_descriptor.py			\
	bt2/connection.py				\
	bt2/error.py					\
	bt2/event.py					\
	bt2/event_class.py				\
	bt2/field.py					\
	bt2/field_class.py				\
	bt2/field_location.py				\
	bt2/field_path.py				\
	bt2/graph.py					\
	bt2/integer_range_set.py			\
	bt2/interrupter.py				\
	bt2/logging.py					\
	bt2/message.py					\
	bt2/message_iterator.py				\
	bt2/mip.py					\
	bt2/object.py					\
	bt2/packet.py					\
	bt2/plugin.py					\
	bt2/port.py					\
	bt2/py_plugin.py				\
	bt2/query_executor.py				\
	bt2/stream.py					\
	bt2/stream_class.py				\
	bt2/trace.py					\
	bt2/trace_class.py				\
	bt2/trace_collection_message_iterator.py	\
	bt2/user_attributes.py				\
	bt2/utils.py					\
	bt2/value.py

# Convenience static libraries on which the native extension
# module depends.
STATIC_LIBRARIES_DEPS = \
	$(top_builddir)/src/autodisc/libautodisc.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/py-common/libpy-common.la \
	$(top_builddir)/src/string-format/libstring-format.la

if ENABLE_VENDOR_FMT
# {fmt}'s CPPFLAGS are already in AM_CPPFLAGS, added by configure.
STATIC_LIBRARIES_DEPS += $(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la
else
AM_CPPFLAGS += $(FMT_CFLAGS)
AM_LDFLAGS += $(FMT_LIBS)
endif

# Generated Python modules of the `bt2` package.
GENERATED_BINDINGS_DEPS = bt2/version.py

# Directory into which to install the `bt2` package.
#
# As the `bt2` package contains a native extension module, the whole
# package goes into the Python platform library directory.
bt2pyexecdir = $(pyexecdir)/bt2

# Native extension module `bt2.native_bt`.
#
# Its file name ends with the extension module file name suffix of the
# selected Python interpreter, for example
# `.cpython-314-x86_64-linux-gnu.so`, so that modules built for
# different Python versions may coexist in the same directory.
bt2pyexec_LTLIBRARIES = native_bt.la

native_bt_la_SOURCES = $(NATIVE_BT_SRCS)

# Without this, Automake mistakes the `native_` prefix of the target
# name for the `native` source directory and names the objects
# `bt_la-*.lo`.
native_bt_la_SHORTNAME = native_bt

native_bt_la_CPPFLAGS = \
	$(AM_CPPFLAGS) $(PYTHON_INCLUDE)

native_bt_la_CXXFLAGS = \
	$(GLIB_CFLAGS) $(AM_CXXFLAGS)

native_bt_la_LDFLAGS = \
	$(AM_LDFLAGS) $(LT_NO_UNDEFINED) \
	-avoid-version -module -shared \
	-shrext $(PYTHON_EXT_SUFFIX) $(PYTHON_LDFLAGS)

native_bt_la_LIBADD = \
	$(STATIC_LIBRARIES_DEPS) \
	$(top_builddir)/src/lib/libbabeltrace2.la $(GLIB_LIBS)

# Python modules of the `bt2` package.
dist_bt2pyexec_DATA = $(STATIC_BINDINGS_DEPS)
nodist_bt2pyexec_DATA = $(GENERATED_BINDINGS_DEPS)

# Distribution metadata of the `bt2` package, making it visible to
# Python packaging tools.
#
# There's intentionally no `RECORD` file: without it, Python packaging
# tools (pip, for example) refuse to uninstall or overwrite the `bt2`
# package, leaving this job to `make uninstall` or to the package
# manager of the system.
bt2distinfopyexecdir = $(pyexecdir)/bt2-$(PACKAGE_VERSION).dist-info

dist_bt2distinfopyexec_DATA = \
	dist-info/INSTALLER \
	dist-info/top_level.txt

# Generated by `configure`.
nodist_bt2distinfopyexec_DATA = dist-info/METADATA

# Only keep the native extension module itself: remove the libtool
# library file.
install-exec-hook:
	rm -f "$(DESTDIR)$(bt2pyexecdir)/native_bt.la"

uninstall-hook:
	rm -f "$(DESTDIR)$(bt2pyexecdir)/native_bt$(PYTHON_EXT_SUFFIX)"
	-rmdir "$(DESTDIR)$(bt2pyexecdir)" "$(DESTDIR)$(bt2distinfopyexecdir)"

# Directory containing a usable, uninstalled `bt2` package, used by the
# tests and to build the documentation.
BUILD_LIB_DIR = $(builddir)/build/build_lib

all-local: build-python-bindings.stamp

build-python-bindings.stamp: native_bt.la $(STATIC_BINDINGS_DEPS) \
		$(GENERATED_BINDINGS_DEPS) Makefile
	$(AM_V_GEN)rm -rf "$(BUILD_LIB_DIR)/bt2" && \
	$(MKDIR_P) "$(BUILD_LIB_DIR)/bt2" && \
	cp -f $(addprefix $(srcdir)/,$(STATIC_BINDINGS_DEPS)) \
		$(GENERATED_BINDINGS_DEPS) \
		".libs/native_bt$(PYTHON_EXT_SUFFIX)" "$(BUILD_LIB_DIR)/bt2" && \
	touch $@

clean-local:
	rm -rf $(builddir)/build

# distribute: internal documentation of the native extension module
EXTRA_DIST = native/README.adoc

CLEANFILES = build-python-bindings.stamp
