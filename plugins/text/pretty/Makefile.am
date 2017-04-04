AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

SUBDIRS = .

noinst_LTLIBRARIES = libbabeltrace-plugin-text-pretty-cc.la

# ctf-text plugin
libbabeltrace_plugin_text_pretty_cc_la_SOURCES = \
	pretty.c \
	print.c \
	pretty.h

libbabeltrace_plugin_text_pretty_cc_la_LIBADD = \
	$(top_builddir)/common/libbabeltrace-common.la
