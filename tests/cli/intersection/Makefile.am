check_SCRIPTS = test_intersection \
		bt_python_helper.py \
		test_multi_trace_intersection.py

dist_noinst_SCRIPTS = test_multi_trace_intersection.py
EXTRA_DIST=test_multi_trace_intersection.py

all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(EXTRA_DIST); do \
			cp -f $(srcdir)/$$script $(builddir); \
		done; \
	fi

clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(EXTRA_DIST); do \
			rm -f $(builddir)/$$script; \
		done; \
	fi
