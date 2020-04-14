# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

conds_triggers_SOURCES = conds-triggers.c utils.c utils.h
conds_triggers_LDADD = \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/lib/libbabeltrace2.la

noinst_PROGRAMS = conds-triggers

dist_check_SCRIPTS = test_conds test.py
