SUBDIRS = ctf-ir ctf-writer

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

libtestcommon_la_SOURCES = common.c common.h
noinst_LTLIBRARIES = libtestcommon.la

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
COMMON_TEST_LDADD = $(LIBTAP) $(builddir)/libtestcommon.la \
	$(top_builddir)/lib/libbabeltrace.la

test_bitfield_LDADD = $(LIBTAP) $(builddir)/libtestcommon.la

test_ctf_writer_LDADD = $(COMMON_TEST_LDADD)

test_bt_values_LDADD = $(COMMON_TEST_LDADD)

test_ctf_ir_ref_LDADD = $(COMMON_TEST_LDADD)

test_bt_ctf_field_type_validation_LDADD = $(COMMON_TEST_LDADD)

test_ir_visit_LDADD = $(COMMON_TEST_LDADD)

test_graph_topo_LDADD = $(COMMON_TEST_LDADD)

test_cc_prio_map_LDADD = $(COMMON_TEST_LDADD)

test_bt_notification_iterator_LDADD = $(COMMON_TEST_LDADD)

noinst_PROGRAMS = test_bitfield test_ctf_writer test_bt_values \
	test_ctf_ir_ref test_bt_ctf_field_type_validation test_ir_visit \
	test_graph_topo test_cc_prio_map test_bt_notification_iterator

test_bitfield_SOURCES = test_bitfield.c
test_ctf_writer_SOURCES = test_ctf_writer.c
test_bt_values_SOURCES = test_bt_values.c
test_ctf_ir_ref_SOURCES = test_ctf_ir_ref.c
test_bt_ctf_field_type_validation_SOURCES = test_bt_ctf_field_type_validation.c
test_ir_visit_SOURCES = test_ir_visit.c
test_graph_topo_SOURCES = test_graph_topo.c
test_cc_prio_map_SOURCES = test_cc_prio_map.c
test_bt_notification_iterator_SOURCES = test_bt_notification_iterator.c

check_SCRIPTS = test_ctf_writer_complete

if !ENABLE_BUILT_IN_PLUGINS
noinst_PROGRAMS += test_plugin
test_plugin_LDADD = $(COMMON_TEST_LDADD)
test_plugin_SOURCES = test_plugin.c
check_SCRIPTS += test_plugin_complete
SUBDIRS += test-plugin-plugins
endif
