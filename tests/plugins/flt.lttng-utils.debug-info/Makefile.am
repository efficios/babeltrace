# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils -I$(top_srcdir)/src/plugins

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

dist_check_SCRIPTS = \
	test-bin-info-i386-linux-gnu.sh \
	test-bin-info-powerpc64le-linux-gnu.sh \
	test-bin-info-powerpc-linux-gnu.sh \
	test-bin-info-x86-64-linux-gnu.sh \
	test-dwarf-i386-linux-gnu.sh \
	test-dwarf-powerpc64le-linux-gnu.sh \
	test-dwarf-powerpc-linux-gnu.sh \
	test-dwarf-x86-64-linux-gnu.sh \
	test-succeed.sh

noinst_PROGRAMS =

if !ENABLE_BUILT_IN_PLUGINS
# plugin tests here
endif # !ENABLE_BUILT_IN_PLUGINS

if ENABLE_DEBUG_INFO
noinst_PROGRAMS += test-dwarf test-bin-info

test_dwarf_LDADD = \
	$(top_builddir)/src/plugins/lttng-utils/debug-info/libdebug-info.la \
	$(top_builddir)/src/fd-cache/libfd-cache.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(ELFUTILS_LIBS) \
	$(LIBTAP)
test_dwarf_SOURCES = test-dwarf.c

test_bin_info_LDADD = \
	$(top_builddir)/src/plugins/lttng-utils/debug-info/libdebug-info.la \
	$(top_builddir)/src/fd-cache/libfd-cache.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(ELFUTILS_LIBS) \
	$(LIBTAP)
test_bin_info_SOURCES = test-bin-info.c
endif # ENABLE_DEBUG_INFO
