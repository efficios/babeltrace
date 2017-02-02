noinst_LTLIBRARIES = libtap.la
libtap_la_SOURCES = tap.c tap.h
libtap_la_LDFLAGS = $(LT_NO_UNDEFINED)

SCRIPT_LIST = tap.sh

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
