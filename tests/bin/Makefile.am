SUBDIRS = intersection
check_SCRIPTS = test_trace_read test_packet_seq_num test_convert_args

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh

TESTS = test_trace_read \
	test_trace_read \
	test_packet_seq_num \
	test_convert_args \
	intersection/test_intersection

if USE_PYTHON
TESTS += intersection/test_multi_trace_intersection.py
endif
