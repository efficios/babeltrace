AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/tests/utils

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la
COMMON_TEST_LDADD = $(LIBTAP) $(top_builddir)/lib/libbabeltrace.la

noinst_PROGRAMS = test-utils-muxer

test_utils_muxer_SOURCES = test-utils-muxer.c
test_utils_muxer_LDADD = $(COMMON_TEST_LDADD)

check_SCRIPTS = test-utils-muxer-complete

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh
LOG_DRIVER_FLAGS='--merge'

TESTS = test-utils-muxer
