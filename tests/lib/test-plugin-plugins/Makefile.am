noinst_LTLIBRARIES = plugin-minimal.la plugin-sfs.la

# the minimal plugin
plugin_minimal_la_SOURCES = minimal.c
plugin_minimal_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module $(LD_NOTEXT)
plugin_minimal_la_LIBADD = \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la

# source/filter/sink plugin
plugin_sfs_la_SOURCES = sfs.c
plugin_sfs_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module $(LD_NOTEXT)
plugin_sfs_la_LIBADD = \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la
