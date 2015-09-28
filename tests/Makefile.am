SUBDIRS = utils bin lib

EXTRA_DIST = $(srcdir)/ctf-traces/** tests
if ENABLE_DEBUGINFO
EXTRA_DIST += $(srcdir)/debuginfo-data/** tests_debuginfo
endif

SCRIPT_LIST = run.sh

dist_noinst_SCRIPTS = $(SCRIPT_LIST)

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

check-am:
	./run.sh $(srcdir)/tests
if ENABLE_DEBUGINFO
	./run.sh $(srcdir)/tests_debuginfo
endif
