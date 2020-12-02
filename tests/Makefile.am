SUBDIRS = \
	utils \
	lib \
	bitfield \
	ctf-writer \
	plugins \
	param-validation

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = $(srcdir)/data \
	     bindings/python/bt2/.coveragerc

dist_check_SCRIPTS = \
	bindings/python/bt2/test_clock_class.py \
	bindings/python/bt2/test_component_class.py \
	bindings/python/bt2/test_component.py \
	bindings/python/bt2/test_connection.py \
	bindings/python/bt2/test_event_class.py \
	bindings/python/bt2/test_event.py \
	bindings/python/bt2/test_field_class.py \
	bindings/python/bt2/test_field.py \
	bindings/python/bt2/test_graph.py \
	bindings/python/bt2/test_integer_range_set.py \
	bindings/python/bt2/test_interrupter.py \
	bindings/python/bt2/test_message_iterator.py \
	bindings/python/bt2/test_message.py \
	bindings/python/bt2/test_package.py \
	bindings/python/bt2/test_packet.py \
	bindings/python/bt2/test_plugin.py \
	bindings/python/bt2/test_port.py \
	bindings/python/bt2/test_python_bt2 \
	bindings/python/bt2/test_query_executor.py \
	bindings/python/bt2/test_stream_class.py \
	bindings/python/bt2/test_stream.py \
	bindings/python/bt2/test_trace_collection_message_iterator.py \
	bindings/python/bt2/test_trace.py \
	bindings/python/bt2/test_value.py \
	bindings/python/bt2/utils.py \
	cli/convert/test_auto_source_discovery_grouping \
	cli/convert/test_auto_source_discovery_params \
	cli/convert/test_auto_source_discovery_log_level \
	cli/convert/test_convert_args \
	cli/list-plugins/test_list_plugins \
	cli/params/test_params \
	cli/query/test_query \
	cli/test_exit_status \
	cli/test_help \
	cli/test_intersection \
	cli/test_output_ctf_metadata \
	cli/test_output_path_ctf_non_lttng_trace \
	cli/test_packet_seq_num \
	cli/test_trace_copy \
	cli/test_trace_read \
	cli/test_trimmer \
	plugins/sink.text.details/succeed/test_succeed \
	plugins/sink.text.pretty/test_pretty \
	plugins/sink.text.pretty/test_pretty.py \
	plugins/src.ctf.lttng-live/test_live \
	python-plugin-provider/bt_plugin_test_python_plugin_provider.py \
	python-plugin-provider/test_python_plugin_provider \
	python-plugin-provider/test_python_plugin_provider.py

TESTS_BINDINGS =

if ENABLE_PYTHON_BINDINGS
TESTS_BINDINGS += bindings/python/bt2/test_python_bt2
endif

TESTS_CLI = \
	cli/convert/test_convert_args \
	cli/test_help \
	cli/test_intersection \
	cli/test_output_path_ctf_non_lttng_trace \
	cli/test_packet_seq_num \
	cli/test_trace_copy \
	cli/test_trace_read \
	cli/test_trimmer

TESTS_LIB = \
	lib/test_bt_uuid \
	lib/test_bt_values \
	lib/test_graph_topo \
	lib/test_remove_destruction_listener_in_destruction_listener \
	lib/test_simple_sink \
	lib/test_trace_ir_ref

TESTS_BITFIELD = \
	bitfield/test_bitfield

TESTS_CTF_WRITER = \
	ctf-writer/test_ctf_writer

if !ENABLE_BUILT_IN_PLUGINS
TESTS_LIB += lib/test_plugin
endif

TESTS_PLUGINS = \
	plugins/src.ctf.fs/fail/test_fail \
	plugins/src.ctf.fs/succeed/test_succeed \
	plugins/src.ctf.fs/test_deterministic_ordering \
	plugins/sink.ctf.fs/succeed/test_succeed \
	plugins/sink.text.details/succeed/test_succeed

if !ENABLE_BUILT_IN_PLUGINS
if ENABLE_PYTHON_BINDINGS
TESTS_PLUGINS += plugins/src.ctf.fs/query/test_query_support_info
TESTS_PLUGINS += plugins/src.ctf.fs/query/test_query_trace_info
TESTS_PLUGINS += plugins/src.ctf.fs/query/test_query_metadata_info
TESTS_PLUGINS += plugins/sink.ctf.fs/test_assume_single_trace
TESTS_PLUGINS += plugins/sink.ctf.fs/test_stream_names
endif
endif

if ENABLE_DEBUG_INFO
TESTS_PLUGINS += \
	plugins/flt.lttng-utils.debug-info/test_dwarf_i386-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_dwarf_powerpc-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_dwarf_powerpc64le-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_dwarf_x86_64-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_bin_info_i386-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_bin_info_powerpc-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_bin_info_powerpc64le-linux-gnu \
	plugins/flt.lttng-utils.debug-info/test_bin_info_x86_64-linux-gnu
endif

if ENABLE_PYTHON_PLUGINS
if ENABLE_PYTHON_BINDINGS
TESTS_CLI += \
	cli/convert/test_auto_source_discovery_grouping \
	cli/convert/test_auto_source_discovery_log_level \
	cli/convert/test_auto_source_discovery_params \
	cli/list-plugins/test_list_plugins \
	cli/params/test_params \
	cli/query/test_query \
	cli/test_exit_status

TESTS_PLUGINS += plugins/flt.utils.trimmer/test_trimming \
	plugins/flt.utils.muxer/succeed/test_succeed
endif
endif

if HAVE_PYTHON
TESTS_PLUGINS += plugins/src.ctf.lttng-live/test_live
endif

TESTS_PYTHON_PLUGIN_PROVIDER =

if ENABLE_PYTHON_PLUGINS
TESTS_PYTHON_PLUGIN_PROVIDER += python-plugin-provider/test_python_plugin_provider
TESTS_PLUGINS += plugins/sink.text.pretty/test_pretty
if ENABLE_DEBUG_INFO
TESTS_PLUGINS += \
	plugins/flt.lttng-utils.debug-info/test_succeed
endif
endif

TESTS_PARAM_VALIDATION = \
	param-validation/test_param_validation

LOG_DRIVER_FLAGS = --merge --comments
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' \
	     BT_TESTS_SRCDIR='$(abs_top_srcdir)/tests' \
	     BT_TESTS_BUILDDIR='$(abs_top_builddir)/tests' \
	     BT_TESTS_AWK_BIN="$(AWK)" \
	     BT_TESTS_GREP_BIN="$(GREP)" \
	     BT_TESTS_PYTHON_BIN="$(PYTHON)" \
	     BT_TESTS_PYTHON_CONFIG_BIN="$(PYTHON_CONFIG)" \
	     BT_TESTS_SED_BIN="$(SED)" \
	     $(SHELL) $(srcdir)/utils/tap-driver.sh

TESTS_NO_BITFIELD = \
	$(TESTS_BINDINGS) \
	$(TESTS_CLI) \
	$(TESTS_CTF_WRITER) \
	$(TESTS_LIB) \
	$(TESTS_PARAM_VALIDATION) \
	$(TESTS_PLUGINS) \
	$(TESTS_PYTHON_PLUGIN_PROVIDER)

TESTS = $(TESTS_NO_BITFIELD) $(TESTS_BITFIELD)

define check_target
check-$(1):
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$2" check
endef

$(eval $(call check_target,bindings,$(TESTS_BINDINGS)))
$(eval $(call check_target,bitfield,$(TESTS_BITFIELD)))
$(eval $(call check_target,cli,$(TESTS_CLI)))
$(eval $(call check_target,ctf-writer,$(TESTS_CTF_WRITER)))
$(eval $(call check_target,lib,$(TESTS_LIB)))
$(eval $(call check_target,plugins,$(TESTS_PLUGINS)))
$(eval $(call check_target,python-plugin-provider,$(TESTS_PYTHON_PLUGIN_PROVIDER)))

check-no-bitfield:
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$(TESTS_NO_BITFIELD)" check
