SUBDIRS = utils cli lib bindings

EXTRA_DIST = $(srcdir)/ctf-traces/** \
	     $(srcdir)/debug-info-data/** \
	     $(srcdir)/debug-info-data/.build-id/cd/**

if !BUILT_IN_PLUGINS
SUBDIRS += plugins
endif
