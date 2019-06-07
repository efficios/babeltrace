SUBDIRS =

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

libtestcommon_la_SOURCES = common.c common.h
noinst_LTLIBRARIES = libtestcommon.la

COMMON_TEST_LDADD = $(LIBTAP) $(builddir)/libtestcommon.la

test_bitfield_LDADD = $(LIBTAP) $(builddir)/libtestcommon.la

test_ctf_writer_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/ctf-writer/libbabeltrace2-ctf-writer.la

test_bt_values_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/lib/libbabeltrace2.la

test_trace_ir_ref_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/lib/libbabeltrace2.la \
	$(top_builddir)/ctf-writer/libbabeltrace2-ctf-writer.la

test_graph_topo_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/lib/libbabeltrace2.la

noinst_PROGRAMS = test_bitfield test_ctf_writer test_bt_values \
	test_trace_ir_ref test_graph_topo

test_bitfield_SOURCES = test_bitfield.c
test_ctf_writer_SOURCES = test_ctf_writer.c
test_bt_values_SOURCES = test_bt_values.c
test_trace_ir_ref_SOURCES = test_trace_ir_ref.c
test_graph_topo_SOURCES = test_graph_topo.c

if !ENABLE_BUILT_IN_PLUGINS
noinst_PROGRAMS += test_plugin
test_plugin_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/lib/libbabeltrace2.la
test_plugin_SOURCES = test_plugin.c
SUBDIRS += test-plugin-plugins
endif
