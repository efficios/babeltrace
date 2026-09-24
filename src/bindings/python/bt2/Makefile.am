# SPDX-FileCopyrightText: 2019-2024 EfficiOS, Inc.
#
# SPDX-License-Identifier: MIT

# Libtool relinks the native extension module against the installed
# libbabeltrace2 at install time: also search the build directory of
# libbabeltrace2 so that installing only this directory works even when
# libbabeltrace2 isn't installed yet.
AM_LDFLAGS += -L$(top_builddir)/src/lib/.libs

SWIG_INTERFACE_FILES =					\
	bt2/native_bt.i					\
	bt2/native_bt_autodisc.i			\
	bt2/native_bt_autodisc.i.hpp			\
	bt2/native_bt_bt2_objects.hpp			\
	bt2/native_bt_clock_class.i			\
	bt2/native_bt_clock_snapshot.i			\
	bt2/native_bt_component.i			\
	bt2/native_bt_component_class.i			\
	bt2/native_bt_component_class.i.hpp		\
	bt2/native_bt_connection.i			\
	bt2/native_bt_error.i				\
	bt2/native_bt_error.i.hpp			\
	bt2/native_bt_event.i				\
	bt2/native_bt_event_class.i			\
	bt2/native_bt_field.i				\
	bt2/native_bt_field_class.i			\
	bt2/native_bt_field_location.i			\
	bt2/native_bt_field_path.i			\
	bt2/native_bt_graph.i				\
	bt2/native_bt_graph.i.hpp			\
	bt2/native_bt_integer_range_set.i		\
	bt2/native_bt_interrupter.i			\
	bt2/native_bt_log_and_append_error.hpp		\
	bt2/native_bt_logging.i				\
	bt2/native_bt_message.i				\
	bt2/native_bt_message_iterator.i		\
	bt2/native_bt_message_iterator.i.hpp		\
	bt2/native_bt_mip.i				\
	bt2/native_bt_mip.i.hpp				\
	bt2/native_bt_packet.i				\
	bt2/native_bt_plugin.i				\
	bt2/native_bt_plugin.i.hpp			\
	bt2/native_bt_port.i				\
	bt2/native_bt_query_exec.i			\
	bt2/native_bt_query_exec.i.hpp			\
	bt2/native_bt_stream.i				\
	bt2/native_bt_stream_class.i			\
	bt2/native_bt_trace.i				\
	bt2/native_bt_trace.i.hpp			\
	bt2/native_bt_trace_class.i			\
	bt2/native_bt_trace_class.i.hpp			\
	bt2/native_bt_value.i				\
	bt2/native_bt_value.i.hpp			\
	bt2/native_bt_version.i

# Non-generated files built into the native library.
STATIC_NATIVE_LIB_SRCS =				\
	bt2/logging.cpp					\
	bt2/logging.hpp

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

# Convenience static libraries on which the Python bindings library depends.
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
GENERATED_BINDINGS_DEPS =	\
	bt2/native_bt.py	\
	bt2/version.py

# Some versions of SWIG (like 3.0.12) generate code where a parameter called
# `swig_this` shadows a global of the same name.  Later versions (like 4.0.1)
# have renamed this global to `Swig_This_global`.
#
# Disable -Wshadow for that file.
WARN_CXXFLAGS = -Wno-shadow

WARN_CXXFLAGS += -Wno-null-dereference

# Python 3.8 with SWIG 4.0.2 generates a deprecated warning for 'tp_print'
# and in general there is not much we can do about deprecations in generated
# code.
WARN_CXXFLAGS += -Wno-deprecated-declarations

# Python 3.11 + gcc 12.2.0 gave warnings of this kind in Python.h.
WARN_CXXFLAGS += -Wno-redundant-decls

# For SWIG generated code
WARN_CXXFLAGS += -Wno-missing-field-initializers
WARN_CXXFLAGS += -Wno-unused-parameter

# Directory into which to install the `bt2` package.
#
# As the `bt2` package contains a native extension module, the whole
# package goes into the Python platform library directory.
bt2pyexecdir = $(pyexecdir)/bt2

# Native extension module `bt2._native_bt`.
#
# Its file name ends with the extension module file name suffix of the
# selected Python interpreter, for example
# `.cpython-314-x86_64-linux-gnu.so`, so that modules built for
# different Python versions may coexist in the same directory.
bt2pyexec_LTLIBRARIES = _native_bt.la

_native_bt_la_SOURCES = $(STATIC_NATIVE_LIB_SRCS)
nodist__native_bt_la_SOURCES = bt2/native_bt.cpp

_native_bt_la_CPPFLAGS = \
	$(AM_CPPFLAGS) -I$(srcdir)/bt2 $(PYTHON_INCLUDE)

_native_bt_la_CXXFLAGS = \
	$(GLIB_CFLAGS) $(AM_CXXFLAGS) $(WARN_CXXFLAGS)

_native_bt_la_LDFLAGS = \
	$(AM_LDFLAGS) $(LT_NO_UNDEFINED) \
	-avoid-version -module -shared \
	-shrext $(PYTHON_EXT_SUFFIX) $(PYTHON_LDFLAGS)

_native_bt_la_LIBADD = \
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
	rm -f "$(DESTDIR)$(bt2pyexecdir)/_native_bt.la"

uninstall-hook:
	rm -f "$(DESTDIR)$(bt2pyexecdir)/_native_bt$(PYTHON_EXT_SUFFIX)"
	-rmdir "$(DESTDIR)$(bt2pyexecdir)" "$(DESTDIR)$(bt2distinfopyexecdir)"

# Directory containing a usable, uninstalled `bt2` package, used by the
# tests and to build the documentation.
BUILD_LIB_DIR = $(builddir)/build/build_lib

all-local: build-python-bindings.stamp

build-python-bindings.stamp: _native_bt.la $(STATIC_BINDINGS_DEPS) \
		$(GENERATED_BINDINGS_DEPS) Makefile
	$(AM_V_GEN)rm -rf "$(BUILD_LIB_DIR)/bt2" && \
	$(MKDIR_P) "$(BUILD_LIB_DIR)/bt2" && \
	cp -f $(addprefix $(srcdir)/,$(STATIC_BINDINGS_DEPS)) \
		$(GENERATED_BINDINGS_DEPS) \
		".libs/_native_bt$(PYTHON_EXT_SUFFIX)" "$(BUILD_LIB_DIR)/bt2" && \
	touch $@

swig_verbose = $(swig_verbose_@AM_V@)
swig_verbose_ = $(swig_verbose_@AM_DEFAULT_V@)
swig_verbose_0 = @echo "  SWIG     " $@;

bt2/native_bt.cpp: $(SWIG_INTERFACE_FILES)
	$(swig_verbose)$(SWIG) -c++ -MD -python -I"$(top_srcdir)/include" -o "$@" "$<"

# SWIG generates `bt2/native_bt.py` along with `bt2/native_bt.cpp`.
bt2/native_bt.py: bt2/native_bt.cpp

-include bt2/native_bt.d

clean-local:
	rm -rf $(builddir)/build

# distribute: SWIG interface files
EXTRA_DIST = $(SWIG_INTERFACE_FILES)

# clean: generated C++ and Python files (by SWIG)
CLEANFILES = \
	build-python-bindings.stamp \
	bt2/native_bt.cpp \
	bt2/native_bt.d \
	bt2/native_bt.py
