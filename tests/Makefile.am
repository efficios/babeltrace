SUBDIRS = utils cli lib bindings

EXTRA_DIST = $(srcdir)/ctf-traces/** \
	     $(srcdir)/debug-info-data/** \
	     $(srcdir)/debug-info-data/.build-id/cd/**

if !ENABLE_BUILT_IN_PLUGINS
SUBDIRS += plugins
endif

if ENABLE_PYTHON_PLUGINS
SUBDIRS += python-plugin-provider
endif
