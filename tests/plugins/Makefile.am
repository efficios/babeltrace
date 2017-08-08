AM_CPPFLAGS += -I$(top_srcdir)/tests/utils -I$(top_srcdir)/plugins

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la
COMMON_TEST_LDADD = $(LIBTAP) \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/compat/libcompat.la

noinst_PROGRAMS = test-utils-muxer

test_utils_muxer_SOURCES = test-utils-muxer.c
test_utils_muxer_LDADD = $(COMMON_TEST_LDADD)

check_SCRIPTS = test-utils-muxer-complete

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh
LOG_DRIVER_FLAGS='--merge'

TESTS = test-utils-muxer-complete

if ENABLE_DEBUG_INFO
test_dwarf_LDFLAGS = -static
test_dwarf_LDADD = $(LIBTAP) \
	$(top_builddir)/plugins/lttng-utils/babeltrace-plugin-lttng-utils.la
test_dwarf_SOURCES = test_dwarf.c

test_bin_info_LDFLAGS = -static
test_bin_info_LDADD = $(LIBTAP) \
	$(top_builddir)/plugins/lttng-utils/babeltrace-plugin-lttng-utils.la
test_bin_info_SOURCES = test_bin_info.c

noinst_PROGRAMS += test_dwarf test_bin_info
check_SCRIPTS += test_dwarf_complete test_bin_info_complete
TESTS += test_dwarf_complete \
	test_bin_info_complete
endif
