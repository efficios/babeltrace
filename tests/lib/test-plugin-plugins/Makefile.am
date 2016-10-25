AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = plugin-minimal.la plugin-sfs.la

# the minimal plugin
plugin_minimal_la_SOURCES = minimal.c
plugin_minimal_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module
plugin_minimal_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la

# source/filter/sink plugin
plugin_sfs_la_SOURCES = sfs.c
plugin_sfs_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module
plugin_sfs_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
