AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

noinst_LTLIBRARIES = libbabeltrace-plugin-text-dmesg-cc.la

libbabeltrace_plugin_text_dmesg_cc_la_SOURCES = dmesg.c dmesg.h
libbabeltrace_plugin_text_dmesg_cc_la_LIBADD = \
	$(top_builddir)/common/libbabeltrace-common.la
