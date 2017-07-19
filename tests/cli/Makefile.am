SUBDIRS = intersection
check_SCRIPTS = test_trace_read test_packet_seq_num test_convert_args test_trace_copy

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh

TESTS = test_trace_read \
	test_packet_seq_num \
	test_convert_args \
	intersection/test_intersection \
	test_trace_copy \
	test_trimmer

if ENABLE_DEBUG_INFO
TESTS += test_debug_info
endif

if ENABLE_PYTHON_BINDINGS
TESTS += intersection/test_multi_trace_intersection.py
endif
