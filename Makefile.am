AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include types compat lib formats converter bindings tests doc extras

dist_doc_DATA = ChangeLog LICENSE mit-license.txt gpl-2.0.txt \
		std-ext-lib.txt

dist_noinst_DATA = CodingStyle

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = babeltrace.pc babeltrace-ctf.pc
