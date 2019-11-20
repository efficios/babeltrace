SUBDIRS =

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

COMMON_TEST_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/tests/utils/libtestcommon.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la

test_bt_values_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_bt_uuid_LDADD = $(COMMON_TEST_LDADD)

test_trace_ir_ref_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la

test_graph_topo_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_simple_sink_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_remove_destruction_listener_in_destruction_listener_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

noinst_PROGRAMS = \
	test_bt_uuid \
	test_bt_values \
	test_graph_topo \
	test_remove_destruction_listener_in_destruction_listener \
	test_simple_sink \
	test_trace_ir_ref

test_bt_values_SOURCES = test_bt_values.c
test_simple_sink_SOURCES = test_simple_sink.c
test_bt_uuid_SOURCES = test_bt_uuid.c
test_trace_ir_ref_SOURCES = test_trace_ir_ref.c
test_graph_topo_SOURCES = test_graph_topo.c
test_remove_destruction_listener_in_destruction_listener_SOURCES = \
	test_remove_destruction_listener_in_destruction_listener.c

if !ENABLE_BUILT_IN_PLUGINS
noinst_PROGRAMS += plugin
plugin_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la
plugin_SOURCES = plugin.c
SUBDIRS += test-plugin-plugins
endif

dist_check_SCRIPTS = test_plugin
