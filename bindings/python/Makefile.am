if USE_PYTHON
SUBDIRS = babeltrace .

INSTALLED_FILES=$(builddir)/installed_files.txt

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(srcdir)/babeltrace/

# Since the shared object used by the python bindings is not built with libtool
# we need to manually set the `rpath` during linkage
AM_LDFLAGS=-L$(top_builddir)/formats/ctf/.libs -L$(top_builddir)/lib/.libs
all-local: build-python-bindings.stamp

$(builddir)/babeltrace/__init__.py: $(srcdir)/babeltrace/__init__.py.in
	cd babeltrace && $(MAKE) __init__.py

$(builddir)/babeltrace/babeltrace.i: $(srcdir)/babeltrace/babeltrace.i.in
	cd babeltrace && $(MAKE) babeltrace.i

BINDINGS_DEPS=setup.py \
		babeltrace/__init__.py \
		babeltrace/babeltrace.i \
		babeltrace/python-complements.c \
		babeltrace/python-complements.h

BUILD_FLAGS=CC="$(CC)" \
		CFLAGS="$(GLIB_CFLAGS) $(AM_CFLAGS) $(CFLAGS)" \
		CPPFLAGS="$(DEFS) $(DEFAULT_INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS)" \
		LDFLAGS="$(AM_LDFLAGS) $(LDFLAGS) $(GLIB_LIBS) -shared $(LIBS)"

build-python-bindings.stamp: $(BINDINGS_DEPS)
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

CLEANFILES = babeltrace/babeltrace_wrap.c babeltrace/babeltrace.py build-python-bindings.stamp
DISTCLEANFILES = setup.py
endif
