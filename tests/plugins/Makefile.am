SUBDIRS = ctf

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils -I$(top_srcdir)/plugins

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

check_SCRIPTS =
noinst_PROGRAMS =

if !ENABLE_BUILT_IN_PLUGINS
# plugin tests here
endif # !ENABLE_BUILT_IN_PLUGINS

if ENABLE_DEBUG_INFO
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

noinst_PROGRAMS += test_dwarf test_bin_info

if !ENABLE_BUILT_IN_PLUGINS
if ENABLE_PYTHON_BINDINGS
check_SCRIPTS += test_lttng_utils_debug_info
endif # !ENABLE_BUILT_IN_PLUGINS
endif # ENABLE_PYTHON_BINDINGS
endif # ENABLE_DEBUG_INFO

EXTRA_DIST = test_lttng_utils_debug_info.py
