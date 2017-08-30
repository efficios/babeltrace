INIT_PY = __init__.py
INIT_PY_IN = $(srcdir)/$(INIT_PY).in

all-local: $(INIT_PY)

$(INIT_PY): $(INIT_PY_IN)
	sed "s/BABELTRACE_VERSION_STR/$(PACKAGE_VERSION)/g" < $< > $@

EXTRA_DIST = common.py reader.py writer.py $(INIT_PY_IN)
nodist_btpackage_PYTHON = $(INIT_PY) common.py reader.py writer.py
SCRIPT_LIST = common.py reader.py writer.py

btpackagedir = $(pythondir)/babeltrace

CLEANFILES = $(INIT_PY)

all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(SCRIPT_LIST); do \
			cp -f $(srcdir)/$$script $(builddir); \
		done; \
	fi

clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(SCRIPT_LIST); do \
			rm -f $(builddir)/$$script; \
		done; \
	fi
