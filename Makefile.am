AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =			\
	include			\
	common			\
	compat

if WITH_PYTHON_PLUGINS
SUBDIRS += python-plugin-provider
endif

SUBDIRS +=			\
	lib			\
	plugins			\
	converter		\
	bindings		\
	tests			\
	doc			\
	extras

dist_doc_DATA = ChangeLog LICENSE mit-license.txt gpl-2.0.txt \
		std-ext-lib.txt README

dist_noinst_DATA = CodingStyle

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = babeltrace.pc babeltrace-ctf.pc

# Create a symlink from libbabeltrace-ctf to libbabeltrace.
# CTF writer used to be in libbabeltrace-ctf in Babeltrace 1, so this
# file must still exist. As of Babeltrace 2, CTF writer is implemented
# in libbabeltrace, hence the symlink.
install-exec-hook:
	$(LN_S) -f libbabeltrace.so $(DESTDIR)$(libdir)/libbabeltrace-ctf.so
