AM_CPPFLAGS += -I$(top_srcdir)/tests/utils -I$(top_srcdir)/src/plugins

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

dist_check_SCRIPTS = \
	test_bin_info_i386-linux-gnu \
	test_bin_info_powerpc64le-linux-gnu \
	test_bin_info_powerpc-linux-gnu \
	test_bin_info_x86_64-linux-gnu \
	test_dwarf_i386-linux-gnu \
	test_dwarf_powerpc64le-linux-gnu \
	test_dwarf_powerpc-linux-gnu \
	test_dwarf_x86_64-linux-gnu \
	test_succeed

noinst_PROGRAMS =

if !ENABLE_BUILT_IN_PLUGINS
# plugin tests here
endif # !ENABLE_BUILT_IN_PLUGINS

if ENABLE_DEBUG_INFO
noinst_PROGRAMS += test_dwarf test_bin_info

test_dwarf_LDADD = \
	$(top_builddir)/src/plugins/lttng-utils/debug-info/libdebug-info.la \
	$(top_builddir)/src/fd-cache/libbabeltrace2-fd-cache.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(ELFUTILS_LIBS) \
	$(LIBTAP)
test_dwarf_SOURCES = test_dwarf.c

test_bin_info_LDADD = \
	$(top_builddir)/src/plugins/lttng-utils/debug-info/libdebug-info.la \
	$(top_builddir)/src/fd-cache/libbabeltrace2-fd-cache.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(ELFUTILS_LIBS) \
	$(LIBTAP)
test_bin_info_SOURCES = test_bin_info.c
endif # ENABLE_DEBUG_INFO
