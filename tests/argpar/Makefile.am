AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

noinst_PROGRAMS = test_argpar
test_argpar_SOURCES = test_argpar.c
test_argpar_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/argpar/libbabeltrace2-argpar.la
