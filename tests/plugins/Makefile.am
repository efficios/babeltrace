AM_CPPFLAGS += -I$(top_srcdir)/tests/utils -I$(top_srcdir)/plugins

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

dist_check_SCRIPTS = \
	test_lttng_utils_debug_info.py\
	ctf/test_query_trace_info.py

noinst_PROGRAMS =

if !ENABLE_BUILT_IN_PLUGINS
# plugin tests here
endif # !ENABLE_BUILT_IN_PLUGINS

if ENABLE_DEBUG_INFO
noinst_PROGRAMS += test_dwarf test_bin_info

test_dwarf_LDADD = \
	$(top_builddir)/plugins/lttng-utils/debug-info/libdebug-info.la \
	$(top_builddir)/fd-cache/libbabeltrace-fd-cache.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(ELFUTILS_LIBS) \
	$(LIBTAP)
test_dwarf_SOURCES = test_dwarf.c

test_bin_info_LDADD = \
	$(top_builddir)/plugins/lttng-utils/debug-info/libdebug-info.la \
	$(top_builddir)/fd-cache/libbabeltrace-fd-cache.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(ELFUTILS_LIBS) \
	$(LIBTAP)
test_bin_info_SOURCES = test_bin_info.c
endif # ENABLE_DEBUG_INFO
