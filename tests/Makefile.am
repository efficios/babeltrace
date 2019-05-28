SUBDIRS = utils cli lib bindings plugins

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = $(srcdir)/ctf-traces \
	     $(srcdir)/debug-info-data

TESTS_BINDINGS =

if ENABLE_PYTHON_PLUGINS
SUBDIRS += python-plugin-provider
endif

if ENABLE_PYTHON_BINDINGS
TESTS_BINDINGS += bindings/python/bt2/test_python_bt2 \
	bindings/python/babeltrace/test_python_babeltrace
endif

TESTS_CLI = \
	cli/test_trace_read \
	cli/test_packet_seq_num \
	cli/test_convert_args \
	cli/intersection/test_intersection \
	cli/test_trace_copy \
	cli/test_trimmer

TESTS_LIB = \
	lib/test_bitfield \
	lib/test_bt_values \
	lib/test_ctf_writer_complete \
	lib/test_graph_topo \
	lib/test_trace_ir_ref

if !ENABLE_BUILT_IN_PLUGINS
TESTS_LIB += lib/test_plugin_complete
endif

if ENABLE_PYTHON_BINDINGS
TESTS_LIB += lib/trace-ir/test_trace_ir
TESTS_LIB += lib/ctf-writer/test_ctf_writer
endif

TESTS_PLUGINS =

if !ENABLE_BUILT_IN_PLUGINS
if ENABLE_PYTHON_BINDINGS
TESTS_PLUGINS += plugins/ctf/test_ctf_plugin

if ENABLE_DEBUG_INFO
TESTS_PLUGINS += plugins/test_lttng_utils_debug_info
endif
endif
endif

if ENABLE_DEBUG_INFO
TESTS_PLUGINS += \
	plugins/test_dwarf_i386-linux-gnu \
	plugins/test_dwarf_powerpc-linux-gnu \
	plugins/test_dwarf_x86_64-linux-gnu \
	plugins/test_bin_info_i386-linux-gnu \
	plugins/test_bin_info_powerpc-linux-gnu \
	plugins/test_bin_info_x86_64-linux-gnu
endif

TESTS_PYTHON_PLUGIN_PROVIDER =

if ENABLE_PYTHON_PLUGINS
TESTS_PYTHON_PLUGIN_PROVIDER += python-plugin-provider/test_python_plugin_provider
endif

LOG_DRIVER_FLAGS = '--merge'
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/config/tap-driver.sh

TESTS = $(TESTS_BINDINGS) $(TESTS_CLI) $(TESTS_LIB) \
	$(TESTS_PLUGINS) $(TESTS_PYTHON_PLUGIN_PROVIDER)

define check_target
check-$(1):
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$2" check
endef

$(eval $(call check_target,bindings,$(TESTS_BINDINGS)))
$(eval $(call check_target,cli,$(TESTS_CLI)))
$(eval $(call check_target,lib,$(TESTS_LIB)))
$(eval $(call check_target,plugins,$(TESTS_PLUGINS)))
$(eval $(call check_target,python-plugin-provider,$(TESTS_PYTHON_PLUGIN_PROVIDER)))
