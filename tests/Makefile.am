SUBDIRS = utils bin lib

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

TESTS = bin/test_trace_read \
	bin/test_trace_read \
	bin/test_packet_seq_num \
	bin/test_intersection \
	lib/test_bitfield \
	lib/test_seek_empty_packet \
	lib/test_seek_big_trace \
	lib/test_ctf_writer_complete \
	lib/test_bt_values \
	lib/test_ctf_ir_ref \
	lib/test_bt_ctf_field_type_validation

EXTRA_DIST = $(srcdir)/ctf-traces/** \
	     $(srcdir)/debuginfo-data/**

if ENABLE_DEBUGINFO
TESTS += lib/test_dwarf_complete \
	lib/test_bin_info_complete
endif
