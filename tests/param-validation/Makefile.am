# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

noinst_PROGRAMS = test-param-validation
test_param_validation_SOURCES = test-param-validation.c
test_param_validation_LDADD = \
	$(top_builddir)/src/param-parse/libparam-parse.la \
	$(top_builddir)/src/plugins/common/param-validation/libparam-validation.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/tests/utils/tap/libtap.la
