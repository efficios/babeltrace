# SPDX-License-Identifier: MIT

noinst_LTLIBRARIES = plugin-init-fail.la

plugin_init_fail_la_SOURCES = plugin-init-fail.cpp
plugin_init_fail_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module $(LD_NOTEXT)
plugin_init_fail_la_LIBADD = \
	$(top_builddir)/src/lib/libbabeltrace2.la
