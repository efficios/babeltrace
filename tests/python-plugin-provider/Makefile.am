check_SCRIPTS = test_python_plugin_provider
EXTRA_DIST = 						\
	test_python_plugin_provider.py			\
	bt_plugin_test_python_plugin_provider.py

LOG_DRIVER_FLAGS='--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh

if ENABLE_PYTHON_BINDINGS
TESTS = test_python_plugin_provider
endif
