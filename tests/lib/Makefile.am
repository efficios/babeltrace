# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

noinst_LTLIBRARIES =

COMMON_TEST_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/tests/utils/libtestcommon.la \
	utils/liblib-utils.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/logging/liblogging.la

test_bt_values_SOURCES = test-bt-values.c
test_bt_values_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_fields_bin_SOURCES = test-fields-bin.cpp
test_fields_bin_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la

test_bt_uuid_SOURCES = test-bt-uuid.c
test_bt_uuid_LDADD = $(COMMON_TEST_LDADD)

test_trace_ir_ref_SOURCES = test-trace-ir-ref.c
test_trace_ir_ref_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la

test_graph_topo_SOURCES = test-graph-topo.c
test_graph_topo_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_simple_sink_SOURCES = test-simple-sink.c
test_simple_sink_LDADD = $(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

test_remove_destruction_listener_in_destruction_listener_SOURCES = \
	test-remove-destruction-listener-in-destruction-listener.c
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

if !ENABLE_BUILT_IN_PLUGINS

# test-plugins

noinst_PROGRAMS += test-plugins
test_plugins_SOURCES = test-plugins.c
test_plugins_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la

noinst_LTLIBRARIES += test-plugins-plugins/plugin-minimal.la
test_plugins_plugins_plugin_minimal_la_SOURCES = \
	test-plugins-plugins/minimal.c
test_plugins_plugins_plugin_minimal_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module $(LD_NOTEXT)
test_plugins_plugins_plugin_minimal_la_LIBADD = \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/logging/liblogging.la

noinst_LTLIBRARIES += test-plugins-plugins/plugin-sfs.la
test_plugins_plugins_plugin_sfs_la_SOURCES = test-plugins-plugins/sfs.c
test_plugins_plugins_plugin_sfs_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module $(LD_NOTEXT)
test_plugins_plugins_plugin_sfs_la_LIBADD = \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/logging/liblogging.la

# test-plugin-init-fail

noinst_PROGRAMS +=  test-plugin-init-fail
test_plugin_init_fail_SOURCES = test-plugin-init-fail.cpp
test_plugin_init_fail_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la \
	$(top_builddir)/src/lib/libbabeltrace2.la

noinst_LTLIBRARIES += test-plugin-init-fail-plugin/plugin-init-fail.la

test_plugin_init_fail_plugin_plugin_init_fail_la_SOURCES = \
	test-plugin-init-fail-plugin/plugin-init-fail.cpp
test_plugin_init_fail_plugin_plugin_init_fail_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LT_NO_UNDEFINED) \
	-rpath / -avoid-version -module $(LD_NOTEXT)
test_plugin_init_fail_plugin_plugin_init_fail_la_LIBADD = \
	$(top_builddir)/src/lib/libbabeltrace2.la

endif

dist_check_SCRIPTS = test-plugins.sh test-fields.sh

# utils

noinst_LTLIBRARIES += utils/liblib-utils.la
utils_liblib_utils_la_SOURCES = \
	utils/run-in.cpp \
	utils/run-in.hpp

if HAVE_PYTHON
if DEV_MODE

# conds

noinst_PROGRAMS += conds/conds-triggers

conds_conds_triggers_SOURCES = \
	conds/conds-triggers.cpp \
	conds/utils.cpp \
	conds/utils.hpp

conds_conds_triggers_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la

dist_check_SCRIPTS += conds/test-conds.sh conds/test.py

endif
endif
