SUBDIRS = test-plugin-plugins writer .

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/tests/utils

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

test_bt_values_LDADD = $(LIBTAP) \
	$(top_builddir)/lib/libbabeltrace.la

test_ctf_ir_ref_LDADD = $(COMMON_TEST_LDADD)

test_bt_ctf_field_type_validation_LDADD = $(COMMON_TEST_LDADD)

test_ir_visit_LDADD = $(COMMON_TEST_LDADD)

test_trace_listener_LDADD = $(COMMON_TEST_LDADD)

test_bt_notification_heap_LDADD = $(COMMON_TEST_LDADD)

test_plugin_LDADD = $(COMMON_TEST_LDADD)

test_graph_topo_LDADD = $(COMMON_TEST_LDADD)

test_cc_prio_map_LDADD = $(COMMON_TEST_LDADD)

noinst_PROGRAMS = test_bitfield test_ctf_writer test_bt_values \
	test_ctf_ir_ref test_bt_ctf_field_type_validation test_ir_visit \
	test_trace_listener test_bt_notification_heap test_plugin test_graph_topo \
	test_cc_prio_map

test_bitfield_SOURCES = test_bitfield.c
test_ctf_writer_SOURCES = test_ctf_writer.c
test_bt_values_SOURCES = test_bt_values.c
test_ctf_ir_ref_SOURCES = test_ctf_ir_ref.c
test_bt_ctf_field_type_validation_SOURCES = test_bt_ctf_field_type_validation.c
test_ir_visit_SOURCES = test_ir_visit.c
test_trace_listener_SOURCES = test_trace_listener.c
test_bt_notification_heap_SOURCES = test_bt_notification_heap.c
test_plugin_SOURCES = test_plugin.c
test_graph_topo_SOURCES = test_graph_topo.c
test_cc_prio_map_SOURCES = test_cc_prio_map.c

check_SCRIPTS = test_ctf_writer_complete \
		test_plugin_complete

#FIXME
#if ENABLE_DEBUG_INFO
#test_dwarf_LDFLAGS = -static
#test_dwarf_LDADD = $(LIBTAP) \
#	$(top_builddir)/lib/libbabeltrace.la \
#	$(top_builddir)/lib/libdebug-info.la
#test_dwarf_SOURCES = test_dwarf.c

#test_bin_info_LDFLAGS = -static
#test_bin_info_LDADD = $(LIBTAP) \
#	$(top_builddir)/lib/libbabeltrace.la \
#	$(top_builddir)/lib/libdebug-info.la
#test_bin_info_SOURCES = test_bin_info.c

#noinst_PROGRAMS += test_dwarf test_bin_info
#check_SCRIPTS += test_dwarf_complete test_bin_info_complete
#endif
