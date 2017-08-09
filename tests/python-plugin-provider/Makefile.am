check_SCRIPTS = test.sh
EXTRA_DIST = 						\
	$(check_SCRIPTS)				\
	test_python_plugin_provider.py			\
	bt_plugin_test_python_plugin_provider.py

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh

if ENABLE_PYTHON_BINDINGS
TESTS = test.sh
endif
