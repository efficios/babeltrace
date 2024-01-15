# SPDX-License-Identifier: MIT

SUBDIRS = utils

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

COMMON_TEST_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/tests/utils/libtestcommon.la \
	$(top_builddir)/tests/lib/utils/liblib-utils.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/logging/liblogging.la

test_bt_values_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_fields_bin_LDADD = $(COMMON_TEST_LDADD) \
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
	test-bt-uuid \
	test-bt-values \
	test-graph-topo \
	test-fields-bin \
	test-remove-destruction-listener-in-destruction-listener \
	test-simple-sink \
	test-trace-ir-ref

test_bt_values_SOURCES = test-bt-values.c
test_fields_bin_SOURCES = test-fields-bin.cpp
test_simple_sink_SOURCES = test-simple-sink.c
test_bt_uuid_SOURCES = test-bt-uuid.c
test_trace_ir_ref_SOURCES = test-trace-ir-ref.c
test_graph_topo_SOURCES = test-graph-topo.c
test_remove_destruction_listener_in_destruction_listener_SOURCES = \
	test-remove-destruction-listener-in-destruction-listener.c

if !ENABLE_BUILT_IN_PLUGINS
noinst_PROGRAMS += test-plugins
test_plugins_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la
test_plugins_SOURCES = test-plugins.c
SUBDIRS += test-plugins-plugins
endif

dist_check_SCRIPTS = test-plugins.sh test-fields.sh

if HAVE_PYTHON
if DEV_MODE
SUBDIRS += conds
endif
endif
