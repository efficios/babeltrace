EXTRA_DIST = 							\
	test_reader.py						\
	test_reader_event.py				\
	test_reader_event_declaration.py	\
	test_reader_field_declaration.py

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh

if ENABLE_PYTHON_BINDINGS
TESTS = test_python_babeltrace
endif
