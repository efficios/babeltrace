SUBDIRS = utils bin lib

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

TESTS = bin/test_trace_read \
	bin/test_trace_read \
	bin/test_packet_seq_num \
	bin/intersection/test_intersection \
	lib/test_bitfield \
	lib/test_seek_empty_packet \
	lib/test_seek_big_trace \
	lib/test_ctf_writer_complete

EXTRA_DIST = $(srcdir)/ctf-traces/** \
	     $(srcdir)/debug-info-data/** \
	     $(srcdir)/debug-info-data/.build-id/cd/**

if ENABLE_DEBUG_INFO
TESTS += lib/test_dwarf_complete \
	lib/test_bin_info_complete
endif

if USE_PYTHON
TESTS += bin/intersection/test_multi_trace_intersection.py
endif
