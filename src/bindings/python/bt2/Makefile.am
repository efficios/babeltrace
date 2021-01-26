# Since the shared object used by the python bindings is not built with
# libtool, we need to add the directory containing libbabeltrace2 to the
# linker path.
AM_LDFLAGS=-L$(top_builddir)/src/lib/.libs

INSTALLED_FILES=$(builddir)/installed_files.txt

SWIG_INTERFACE_FILES =					\
	bt2/native_bt.i					\
	bt2/native_bt_autodisc.i			\
	bt2/native_bt_autodisc.i.h			\
	bt2/native_bt_bt2_objects.h			\
	bt2/native_bt_clock_class.i			\
	bt2/native_bt_clock_snapshot.i			\
	bt2/native_bt_component.i			\
	bt2/native_bt_component_class.i			\
	bt2/native_bt_component_class.i.h		\
	bt2/native_bt_connection.i			\
	bt2/native_bt_error.i				\
	bt2/native_bt_error.i.h				\
	bt2/native_bt_event.i				\
	bt2/native_bt_event_class.i			\
	bt2/native_bt_field.i				\
	bt2/native_bt_field_class.i			\
	bt2/native_bt_field_path.i			\
	bt2/native_bt_graph.i				\
	bt2/native_bt_graph.i.h				\
	bt2/native_bt_integer_range_set.i		\
	bt2/native_bt_interrupter.i			\
	bt2/native_bt_log_and_append_error.h		\
	bt2/native_bt_logging.i				\
	bt2/native_bt_message.i				\
	bt2/native_bt_message_iterator.i		\
	bt2/native_bt_message_iterator.i.h		\
	bt2/native_bt_mip.i				\
	bt2/native_bt_mip.i.h				\
	bt2/native_bt_packet.i				\
	bt2/native_bt_plugin.i				\
	bt2/native_bt_plugin.i.h			\
	bt2/native_bt_port.i				\
	bt2/native_bt_query_exec.i			\
	bt2/native_bt_query_exec.i.h			\
	bt2/native_bt_stream.i				\
	bt2/native_bt_stream_class.i			\
	bt2/native_bt_trace.i				\
	bt2/native_bt_trace.i.h				\
	bt2/native_bt_trace_class.i			\
	bt2/native_bt_trace_class.i.h			\
	bt2/native_bt_value.i				\
	bt2/native_bt_value.i.h				\
	bt2/native_bt_version.i

# Non-generated files built into the native library.
STATIC_NATIVE_LIB_SRCS =				\
	bt2/logging.c					\
	bt2/logging.h

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
	bt2/utils.py					\
	bt2/value.py

# Convenience static libraries on which the Python bindings library depends.
# These are listed in the setup.py(.in) file.
STATIC_LIBRARIES_DEPS = \
	$(top_builddir)/src/autodisc/libbabeltrace2-autodisc.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/py-common/libbabeltrace2-py-common.la \
	$(top_builddir)/src/string-format/libbabeltrace2-string-format.la

GENERATED_BINDINGS_DEPS =	\
	bt2/native_bt.c		\
	bt2/version.py		\
	setup.py

# Some versions of SWIG (like 3.0.12) generate code where a parameter called
# `swig_this` shadows a global of the same name.  Later versions (like 4.0.1)
# have renamed this global to `Swig_This_global`.
#
# Disable -Wshadow for that file.
WARN_CFLAGS = -Wno-shadow

WARN_CFLAGS += -Wno-null-dereference

# Python 3.8 with SWIG 4.0.2 generates a deprecated warning for 'tp_print'
# and in general there is not much we can do about deprecations in generated
# code.
WARN_CFLAGS += -Wno-deprecated-declarations

BUILD_FLAGS=CC="$(CC)" \
		CFLAGS="$(GLIB_CFLAGS) $(AM_CFLAGS) $(CFLAGS) $(WARN_CFLAGS)" \
		CPPFLAGS="$(AM_CPPFLAGS) $(CPPFLAGS) -I$(srcdir)/bt2" \
		LDFLAGS="$(AM_LDFLAGS) $(LDFLAGS) $(GLIB_LIBS) $(LIBS)"

all-local: build-python-bindings.stamp

copy-static-deps.stamp: $(addprefix $(srcdir)/, $(STATIC_BINDINGS_DEPS))
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for file in $(STATIC_BINDINGS_DEPS); do \
			cp -f $(srcdir)/$$file $(builddir)/$$file; \
		done; \
	fi
	touch $@

build-python-bindings.stamp: copy-static-deps.stamp $(GENERATED_BINDINGS_DEPS) \
		$(STATIC_NATIVE_LIB_SRCS) $(STATIC_LIBRARIES_DEPS)
	$(BUILD_FLAGS) $(PYTHON) $(builddir)/setup.py build --force
	touch $@

swig_verbose = $(swig_verbose_@AM_V@)
swig_verbose_ = $(swig_verbose_@AM_DEFAULT_V@)
swig_verbose_0 = @echo "  SWIG     " $@;

$(builddir)/bt2/native_bt.c: $(SWIG_INTERFACE_FILES)
	$(swig_verbose)$(SWIG) -MD -python -I"$(top_srcdir)/include" -o "$@" "$<"

-include bt2/native_bt.d

install-exec-local: build-python-bindings.stamp
	@opts="--prefix=$(prefix) --record $(INSTALLED_FILES) --verbose --no-compile $(DISTSETUPOPTS)"; \
	if [ "$(DESTDIR)" != "" ]; then \
		opts="$$opts --root=$(DESTDIR)"; \
	fi; \
	$(PYTHON) $(builddir)/setup.py install $$opts;

clean-local:
	rm -rf $(builddir)/build
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for file in $(STATIC_BINDINGS_DEPS); do \
			rm -f $(builddir)/$$file; \
		done; \
	fi

# Distutils' setup.py does not include an uninstall target, we thus need to do
# it manually. We save the path of the files that were installed during the install target
# and delete them during the uninstallation.
uninstall-local:
	if [ "$(DESTDIR)" != "" ]; then \
		$(SED) -i "s|^|$(DESTDIR)/|g" $(INSTALLED_FILES); \
	fi
	cat $(INSTALLED_FILES) | xargs rm -rf || true
	$(GREP) "__init__.py" $(INSTALLED_FILES) | xargs dirname | xargs rm -rf || true
	rm -f $(INSTALLED_FILES)

# distribute: extra Python modules and SWIG interface files
EXTRA_DIST = $(STATIC_BINDINGS_DEPS) $(SWIG_INTERFACE_FILES) $(STATIC_NATIVE_LIB_SRCS)

# clean: generated C and Python files (by SWIG)
CLEANFILES = \
	build-python-bindings.stamp \
	bt2/native_bt.c \
	bt2/native_bt.d \
	bt2/native_bt.py \
	copy-static-deps.stamp
