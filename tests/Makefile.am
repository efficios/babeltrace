SUBDIRS = utils lib bitfield ctf-writer plugins argpar

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = $(srcdir)/data \
	     bindings/python/bt2/.coveragerc

dist_check_SCRIPTS = \
	bindings/python/bt2/test_clock_class.py \
	bindings/python/bt2/test_component_class.py \
	bindings/python/bt2/test_component.py \
	bindings/python/bt2/test_connection.py \
	bindings/python/bt2/test_ctf_writer_clock.py \
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
	cli/auto-source-discovery/test_auto_source_discovery \
	cli/test_convert_args \
	cli/test_intersection \
	cli/test_output_ctf_metadata \
	cli/test_packet_seq_num \
	cli/test_trace_copy \
	cli/test_trace_read \
	cli/test_trimmer \
	python-plugin-provider/bt_plugin_test_python_plugin_provider.py \
	python-plugin-provider/test_python_plugin_provider \
	python-plugin-provider/test_python_plugin_provider.py

TESTS_BINDINGS =

if ENABLE_PYTHON_BINDINGS
TESTS_BINDINGS += bindings/python/bt2/test_python_bt2
endif

TESTS_ARGPAR = \
	argpar/test_argpar

TESTS_CLI = \
	cli/test_trace_read \
	cli/test_packet_seq_num \
	cli/test_convert_args \
	cli/test_intersection \
	cli/test_trace_copy \
	cli/test_trimmer

TESTS_LIB = \
	lib/test_bt_values \
	lib/test_bt_uuid \
	lib/test_graph_topo \
	lib/test_trace_ir_ref

TESTS_BITFIELD = \
	bitfield/test_bitfield

TESTS_CTF_WRITER = \
	ctf-writer/test_ctf_writer

if !ENABLE_BUILT_IN_PLUGINS
TESTS_LIB += lib/test_plugin
endif

TESTS_PLUGINS = \
	plugins/src.ctf.fs/succeed/test_succeed \
	plugins/sink.ctf.fs/succeed/test_succeed

if !ENABLE_BUILT_IN_PLUGINS
if ENABLE_PYTHON_BINDINGS
TESTS_PLUGINS += plugins/src.ctf.fs/query/test_query
endif

if ENABLE_DEBUG_INFO
TESTS_PLUGINS += \
	plugins/flt.lttng-utils.debug-info/test_succeed
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
TESTS_CLI += cli/auto-source-discovery/test_auto_source_discovery
TESTS_PLUGINS += plugins/flt.utils.trimmer/test_trimming
endif
endif

TESTS_PYTHON_PLUGIN_PROVIDER =

if ENABLE_PYTHON_PLUGINS
TESTS_PYTHON_PLUGIN_PROVIDER += python-plugin-provider/test_python_plugin_provider
endif

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
	$(TESTS_ARGPAR) \
	$(TESTS_BINDINGS) \
	$(TESTS_CLI) \
	$(TESTS_CTF_WRITER) \
	$(TESTS_LIB) \
	$(TESTS_PLUGINS) \
	$(TESTS_PYTHON_PLUGIN_PROVIDER)

TESTS = $(TESTS_NO_BITFIELD) $(TESTS_BITFIELD)

define check_target
check-$(1):
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$2" check
endef

$(eval $(call check_target,argpar,$(TESTS_ARGPAR)))
$(eval $(call check_target,bindings,$(TESTS_BINDINGS)))
$(eval $(call check_target,bitfield,$(TESTS_BITFIELD)))
$(eval $(call check_target,cli,$(TESTS_CLI)))
$(eval $(call check_target,ctf-writer,$(TESTS_CTF_WRITER)))
$(eval $(call check_target,lib,$(TESTS_LIB)))
$(eval $(call check_target,plugins,$(TESTS_PLUGINS)))
$(eval $(call check_target,python-plugin-provider,$(TESTS_PYTHON_PLUGIN_PROVIDER)))

check-no-bitfield:
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$(TESTS_NO_BITFIELD)" check
