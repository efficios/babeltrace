# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

conds_triggers_SOURCES = \
	conds-triggers.cpp \
	utils.cpp utils.hpp

conds_triggers_LDADD = \
	$(top_builddir)/tests/lib/utils/liblib-utils.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/lib/libbabeltrace2.la

noinst_PROGRAMS = conds-triggers

dist_check_SCRIPTS = test-conds.sh test.py
