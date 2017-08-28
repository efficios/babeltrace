SUBDIRS = utils cli lib bindings plugins

EXTRA_DIST = $(srcdir)/ctf-traces/** \
	     $(srcdir)/debug-info-data/** \
	     $(srcdir)/debug-info-data/.build-id/cd/**

if ENABLE_PYTHON_PLUGINS
SUBDIRS += python-plugin-provider
endif
