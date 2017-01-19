AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include
COMMONLIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

# It seems like Automake won't make Libtool create a shared object
# (which we need here since we're building plugins) with a noinst_
# destination (no install). Indeed we don't want to install those test
# plugins. Libtool creates a shared object when it gets an -rpath
# argument. In this case, Libtool does not seem to care much about
# the actual argument, so we use / (because Libtool needs an absolute
# path), which in the end is not even part of the linked object's
# rpath.
#
# -module makes it possible to not use the `lib` prefix and it makes
# sure that all symbols are dynamically exported.
COMMONLDFLAGS = -rpath / -avoid-version -module

noinst_LTLIBRARIES = plugin-minimal.la plugin-sfs.la plugin-invalid.la

# the minimal plugin
plugin_minimal_la_SOURCES = minimal.c
plugin_minimal_la_LDFLAGS = $(COMMONLDFLAGS)
plugin_minimal_la_LIBADD = $(COMMONLIBADD)

# source/filter/sink plugin
plugin_sfs_la_SOURCES = sfs.c
plugin_sfs_la_LDFLAGS = $(COMMONLDFLAGS)
plugin_sfs_la_LIBADD = $(COMMONLIBADD)

# invalid plugin
plugin_invalid_la_SOURCES = invalid.c
plugin_invalid_la_LDFLAGS = $(COMMONLDFLAGS)
plugin_invalid_la_LIBADD = $(COMMONLIBADD)
