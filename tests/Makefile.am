SUBDIRS = utils bin lib bindings

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

TESTS = bin/test_trace_read \
	bin/test_trace_read \
	bin/test_packet_seq_num \
	bin/test_formats \
	bin/test_convert_args \
	bin/intersection/test_intersection \
	lib/test_bitfield \
	lib/test_seek_empty_packet \
	lib/test_seek_big_trace \
	lib/test_ctf_writer_complete \
	lib/test_bt_values \
	lib/test_ctf_ir_ref \
	lib/test_bt_ctf_field_type_validation \
	lib/test_ir_visit \
	lib/test_trace_listener \
	lib/test_bt_notification_heap \
	lib/test_plugin_complete \
	lib/test_graph_topo \
	lib/test_cc_prio_map

EXTRA_DIST = $(srcdir)/ctf-traces/** \
	     $(srcdir)/debug-info-data/** \
	     $(srcdir)/debug-info-data/.build-id/cd/**

if ENABLE_DEBUG_INFO
TESTS += lib/test_dwarf_complete \
	lib/test_bin_info_complete
endif

if USE_PYTHON
TESTS += bin/intersection/test_multi_trace_intersection.py \
	lib/writer/test_ctf_writer_no_packet_context.py \
	lib/writer/test_ctf_writer_empty_packet.py \
	bindings/python/bt2/testall.sh
endif
