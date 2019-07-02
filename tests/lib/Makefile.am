SUBDIRS =

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

COMMON_TEST_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/tests/utils/libtestcommon.la

test_bitfield_LDADD = $(COMMON_TEST_LDADD)

test_trace_ir_ref_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la

noinst_PROGRAMS = test_bitfield test_trace_ir_ref

test_bitfield_SOURCES = test_bitfield.c
test_trace_ir_ref_SOURCES = test_trace_ir_ref.c

if !ENABLE_BUILT_IN_PLUGINS
noinst_PROGRAMS += plugin
plugin_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la
plugin_SOURCES = plugin.c
SUBDIRS += test-plugin-plugins
endif

dist_check_SCRIPTS = test_plugin
