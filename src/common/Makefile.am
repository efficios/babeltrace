# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -DBABELTRACE_PLUGINS_DIR=\"$(BABELTRACE_PLUGINS_DIR)\"

noinst_LTLIBRARIES = libbabeltrace2-common.la

libbabeltrace2_common_la_SOURCES = \
	assert.c \
	assert.h \
	common.c \
	common.h \
	uuid.c \
	uuid.h

noinst_HEADERS = \
	align.h \
	list.h \
	macros.h \
	mmap-align.h \
	safe.h

## This target generates an include file that contains the git version
## string of the current branch, it must be continuously updated when
## we build in the git repo and shipped in dist tarballs to reflect the
## status of the tree when it was generated. If the tree is clean and
## the current commit is tag a starting with "v", consider this a
## release version and set an empty git version.

version_verbose = $(version_verbose_@AM_V@)
version_verbose_ = $(version_verbose_@AM_DEFAULT_V@)
version_verbose_0 = @echo "  GEN     " $@;

version.i:
	$(version_verbose)GREP=$(GREP) SED=$(SED) TOP_SRCDIR="$(top_srcdir)" $(SHELL) $(srcdir)/gen-version-i.sh

dist_noinst_SCRIPTS = gen-version-i.sh

##
## version.i is defined as a .PHONY target even if it's a real file,
## we want the target to be re-run on every make.
##
.PHONY: version.i

CLEANFILES = version.i.tmp

##
## Only clean "version.i" on dist-clean, we need to keep it on regular
## clean when it's part of a dist tarball.
##
DISTCLEANFILES = version.i

noinst_HEADERS += version.h version.i
