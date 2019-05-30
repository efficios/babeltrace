ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include		\
	common			\
	ctfser			\
	fd-cache		\
	compat			\
	logging			\
	lib			\
	python-plugin-provider	\
	plugins			\
	cli			\
	bindings		\
	doc			\
	tests

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = \
	extras

dist_doc_DATA = ChangeLog LICENSE mit-license.txt gpl-2.0.txt \
		std-ext-lib.txt README CONTRIBUTING.adoc

dist_noinst_DATA = CodingStyle

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = babeltrace2.pc babeltrace2-ctf.pc

# This is a convenience target, it's not part of the build process.
CONTRIBUTING.html: CONTRIBUTING.adoc
	$(ASCIIDOC) --verbose -a source-highlighter=pygments $<

CLEANFILES = CONTRIBUTING.html
