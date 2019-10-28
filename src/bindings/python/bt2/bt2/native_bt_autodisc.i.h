/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <autodisc/autodisc.h>
#include <common/common.h>

/*
 * Interface between the Python bindings and the auto source discovery system:
 * input strings go in, specs for components to instantiate go out.
 *
 * `inputs` must be an array of strings, the list of inputs in which to look
 * for traces.  `plugin_set` is the set of plugins to query.
 *
 * Returns a map with the following entries:
 *
 *   - status: signed integer, return status of this function
 *   - results: array, each element is an array describing one auto
 *              source discovery result (see `struct
 *              auto_source_discovery_result` for more details about these):
 *
 *     - 0: plugin name, string
 *     - 1: class name, string
 *     - 2: inputs, array of strings
 *     - 3: original input indices, array of unsigned integers
 *
 * This function can also return None, if it failed to allocate memory
 * for the return value and status code.
 */
static
bt_value *bt_bt2_auto_discover_source_components(const bt_value *inputs,
		const bt_plugin_set *plugin_set)
{
	uint64_t i;
	int status = 0;
	static const char * const module_name = "Automatic source discovery";
	const bt_plugin **plugins = NULL;
	const uint64_t plugin_count = bt_plugin_set_get_plugin_count(plugin_set);
	struct auto_source_discovery auto_disc = { 0 };
	bt_value *result = NULL;
	bt_value *components_list = NULL;
	bt_value *component_info = NULL;
	bt_value_map_insert_entry_status insert_entry_status;

	BT_ASSERT(bt_value_get_type(inputs) == BT_VALUE_TYPE_ARRAY);
	for (i = 0; i < bt_value_array_get_length(inputs); i++) {
		const bt_value *elem = bt_value_array_borrow_element_by_index_const(inputs, i);
		BT_ASSERT(bt_value_get_type(elem) == BT_VALUE_TYPE_STRING);
	}

	result = bt_value_map_create();
	if (!result) {
		static const char * const err = "Failed to create a map value.";
		BT_LOGE_STR(err);
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name, err);
		PyErr_NoMemory();
		goto end;
	}

	status = auto_source_discovery_init(&auto_disc);
	if (status != 0) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                	"Failed to initialize auto source discovery structure.");
                goto error;
	}

	/* Create array of bt_plugin pointers from plugin set. */
	plugins = g_new(const bt_plugin *, plugin_count);
	if (!plugins) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                	"Failed to allocate plugins array.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
                goto error;
	}

	for (i = 0; i < plugin_count; i++) {
		plugins[i] = bt_plugin_set_borrow_plugin_by_index_const(plugin_set, i);
	}

	status = auto_discover_source_components(
		inputs,
		plugins,
		plugin_count,
		NULL,
		bt_python_bindings_bt2_log_level,
		&auto_disc,
		NULL);
	if (status != 0) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                	"Failed to auto discover sources.");
		goto error;
	}

	components_list = bt_value_array_create();
	if (!components_list) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                	"Failed to allocate one array value.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	insert_entry_status = bt_value_map_insert_entry(result, "results", components_list);
	if (insert_entry_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                	"Failed to insert a map entry.");
		status = (int) insert_entry_status;
		goto error;
	}

	for (i = 0; i < auto_disc.results->len; i++) {
		struct auto_source_discovery_result *autodisc_result =
			g_ptr_array_index(auto_disc.results, i);
		bt_value_array_append_element_status append_element_status;

		component_info = bt_value_array_create();
		if (!component_info) {
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                		"Failed to allocate one array value.");
			status = __BT_FUNC_STATUS_MEMORY_ERROR;
			goto error;
		}

		append_element_status = bt_value_array_append_string_element(
			component_info, autodisc_result->plugin_name);
		if (append_element_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                		"Failed to append one array element.");
			status = (int) append_element_status;
			goto error;
		}

		append_element_status = bt_value_array_append_string_element(
			component_info, autodisc_result->source_cc_name);
		if (append_element_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                		"Failed to append one array element.");
			status = (int) append_element_status;
			goto error;
		}

		append_element_status = bt_value_array_append_element(
			component_info, autodisc_result->inputs);
		if (append_element_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                		"Failed to append one array element.");
			status = (int) append_element_status;
			goto error;
		}

		append_element_status = bt_value_array_append_element(
			component_info, autodisc_result->original_input_indices);
		if (append_element_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                		"Failed to append one array element.");
			status = (int) append_element_status;
			goto error;
		}

		append_element_status = bt_value_array_append_element(components_list,
			component_info);
		if (append_element_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
                		"Failed to append one array element.");
			status = (int) append_element_status;
			goto error;
		}

		BT_VALUE_PUT_REF_AND_RESET(component_info);
	}

	status = 0;
	goto end;
error:
	BT_ASSERT(status != 0);

end:
	if (result) {
		insert_entry_status = bt_value_map_insert_signed_integer_entry(result, "status", status);
		if (insert_entry_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
			BT_VALUE_PUT_REF_AND_RESET(result);
			PyErr_NoMemory();
		}
	}

	auto_source_discovery_fini(&auto_disc);
	g_free(plugins);
	bt_value_put_ref(components_list);
	bt_value_put_ref(component_info);

	return result;
}
