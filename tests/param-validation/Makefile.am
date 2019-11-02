AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

noinst_PROGRAMS = test_param_validation
test_param_validation_SOURCES = test_param_validation.c
test_param_validation_LDADD = \
	$(top_builddir)/src/param-parse/libbabeltrace2-param-parse.la \
	$(top_builddir)/src/plugins/common/param-validation/libbabeltrace2-param-validation.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/tests/utils/tap/libtap.la
