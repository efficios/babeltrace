# Since the shared object used by the python bindings is not built with
# libtool, we need to add the directory containing libbabeltrace to the
# linker path.
AM_LDFLAGS=-L$(top_builddir)/lib/.libs

INSTALLED_FILES=$(builddir)/installed_files.txt

STATIC_BINDINGS_DEPS =					\
	bt2/logging.c					\
	bt2/logging.h					\
	bt2/native_bt_clock_class.i			\
	bt2/native_bt_clock_snapshot.i			\
	bt2/native_bt_component_class.i			\
	bt2/native_bt_component.i			\
	bt2/native_bt_connection.i			\
	bt2/native_bt_event_class.i			\
	bt2/native_bt_event.i				\
	bt2/native_bt_field_class.i			\
	bt2/native_bt_field_path.i			\
	bt2/native_bt_field.i				\
	bt2/native_bt_graph.i				\
	bt2/native_bt.i					\
	bt2/native_bt_logging.i				\
	bt2/native_bt_message.i				\
	bt2/native_bt_notifier.i			\
	bt2/native_bt_packet.i				\
	bt2/native_bt_plugin.i				\
	bt2/native_bt_port.i				\
	bt2/native_bt_query_exec.i			\
	bt2/native_bt_stream_class.i			\
	bt2/native_bt_stream.i				\
	bt2/native_bt_trace_class.i			\
	bt2/native_bt_trace.i				\
	bt2/native_bt_value.i				\
	bt2/native_bt_version.i				\
	bt2/clock_class_priority_map.py			\
	bt2/clock_class.py				\
	bt2/clock_snapshot.py				\
	bt2/component.py				\
	bt2/connection.py				\
	bt2/ctf_writer.py				\
	bt2/event_class.py				\
	bt2/event.py					\
	bt2/fields.py					\
	bt2/field_class.py				\
	bt2/graph.py					\
	bt2/logging.py					\
	bt2/message_iterator.py				\
	bt2/message.py					\
	bt2/object.py					\
	bt2/packet.py					\
	bt2/plugin.py					\
	bt2/port.py					\
	bt2/py_plugin.py				\
	bt2/query_executor.py				\
	bt2/stream_class.py				\
	bt2/stream.py					\
	bt2/trace.py					\
	bt2/trace_collection_message_iterator.py	\
	bt2/utils.py					\
	bt2/values.py

GENERATED_BINDINGS_DEPS =	\
	bt2/__init__.py		\
	setup.py

BUILD_FLAGS=CC="$(CC)" \
		CFLAGS="$(GLIB_CFLAGS) $(AM_CFLAGS) $(CFLAGS)" \
		CPPFLAGS="$(AM_CPPFLAGS) $(CPPFLAGS)" \
		LDFLAGS="$(AM_LDFLAGS) $(LDFLAGS) $(GLIB_LIBS) $(LIBS)"

all-local: build-python-bindings.stamp

copy-static-deps.stamp: $(STATIC_BINDINGS_DEPS)
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for file in $(STATIC_BINDINGS_DEPS); do \
			cp -f $(srcdir)/$$file $(builddir)/$$file; \
		done; \
	fi
	touch $@

build-python-bindings.stamp: copy-static-deps.stamp $(GENERATED_BINDINGS_DEPS)
	$(BUILD_FLAGS) $(PYTHON) $(builddir)/setup.py build_ext --force --swig "$(SWIG)"
	$(BUILD_FLAGS) $(PYTHON) $(builddir)/setup.py build --force
	touch $@

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
EXTRA_DIST = $(STATIC_BINDINGS_DEPS)

# clean: generated C and Python files (by SWIG)
CLEANFILES = bt2/native_bt.py bt2/native_bt_wrap.c build-python-bindings.stamp copy-static-deps.stamp
