# Since the shared object used by the python bindings is not built with
# libtool, we need to add the directory containing libbabeltrace to the
# linker path.
AM_LDFLAGS=-L$(top_builddir)/lib/.libs

INSTALLED_FILES=$(builddir)/installed_files.txt

STATIC_BINDINGS_DEPS =						\
	babeltrace/common.py					\
	babeltrace/reader.py					\
	babeltrace/reader_event_declaration.py	\
	babeltrace/reader_event.py				\
	babeltrace/reader_field_declaration.py	\
	babeltrace/reader_field_definition.py	\
	babeltrace/reader_trace_collection.py	\
	babeltrace/reader_trace_handle.py		\
	babeltrace/writer.py

GENERATED_BINDINGS_DEPS =		\
	babeltrace/__init__.py			\
	setup.py


all-local: build-python-bindings.stamp

copy-static-deps.stamp: $(STATIC_BINDINGS_DEPS)
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for file in $(STATIC_BINDINGS_DEPS); do \
			cp -f $(srcdir)/$$file $(builddir)/$$file; \
		done; \
	fi
	touch $@

build-python-bindings.stamp: copy-static-deps.stamp $(GENERATED_BINDINGS_DEPS)
	$(BUILD_FLAGS) $(PYTHON) $(builddir)/setup.py build_ext
	$(BUILD_FLAGS) $(PYTHON) $(builddir)/setup.py build
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

# clean: generated Python files and stamps
CLEANFILES = copy-static-deps.stamp
