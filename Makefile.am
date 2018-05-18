ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include		\
	common			\
	compat			\
	logging			\
	lib

if ENABLE_PYTHON_PLUGINS
# Only build the Python plugin provider shared object if the
# configuration doesn't ask to make it built-in.
SUBDIRS += python-plugin-provider
endif

SUBDIRS +=			\
	plugins			\
	cli			\
	bindings		\
	doc			\
	extras			\
	tests

dist_doc_DATA = ChangeLog LICENSE mit-license.txt gpl-2.0.txt \
		std-ext-lib.txt README CONTRIBUTING.adoc

dist_noinst_DATA = CodingStyle

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = babeltrace.pc babeltrace-ctf.pc

CONTRIBUTING.html: $(srcdir)/CONTRIBUTING.adoc
	asciidoc --verbose -a source-highlighter=pygments $<

CLEANFILES = CONTRIBUTING.html
