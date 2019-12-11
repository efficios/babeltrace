/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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

static bt_graph_listener_func_status port_added_listener(
	const void *component,
	swig_type_info *component_swig_type,
	bt_component_class_type component_class_type,
	const void *port,
	swig_type_info *port_swig_type,
	bt_port_type port_type,
	void *py_callable)
{
	PyObject *py_component_ptr = NULL;
	PyObject *py_port_ptr = NULL;
	PyObject *py_res = NULL;
	bt_graph_listener_func_status status;

	py_component_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(component), component_swig_type, 0);
	if (!py_component_ptr) {
		BT_LOGF_STR("Failed to create component SWIG pointer object.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	py_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(port), port_swig_type, 0);
	if (!py_port_ptr) {
		BT_LOGF_STR("Failed to create port SWIG pointer object.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	py_res = PyObject_CallFunction(py_callable, "(OiOi)",
		py_component_ptr, component_class_type, py_port_ptr, port_type);
	if (!py_res) {
		loge_exception_append_cause_clear(
			"Graph's port added listener (Python)",
			BT_LOG_OUTPUT_LEVEL);
		status = __BT_FUNC_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(py_res == Py_None);
	status = __BT_FUNC_STATUS_OK;

end:
	Py_XDECREF(py_res);
	Py_XDECREF(py_port_ptr);
	Py_XDECREF(py_component_ptr);
	return status;
}

static
bt_graph_listener_func_status
source_component_output_port_added_listener(const bt_component_source *component_source,
					    const bt_port_output *port_output, void *py_callable)
{
	return port_added_listener(
		component_source, SWIGTYPE_p_bt_component_source, BT_COMPONENT_CLASS_TYPE_SOURCE,
		port_output, SWIGTYPE_p_bt_port_output, BT_PORT_TYPE_OUTPUT, py_callable);
}

static
bt_graph_listener_func_status
filter_component_input_port_added_listener(const bt_component_filter *component_filter,
					   const bt_port_input *port_input, void *py_callable)
{
	return port_added_listener(
		component_filter, SWIGTYPE_p_bt_component_filter, BT_COMPONENT_CLASS_TYPE_FILTER,
		port_input, SWIGTYPE_p_bt_port_input, BT_PORT_TYPE_INPUT, py_callable);
}

static
bt_graph_listener_func_status
filter_component_output_port_added_listener(const bt_component_filter *component_filter,
					    const bt_port_output *port_output, void *py_callable)
{
	return port_added_listener(
		component_filter, SWIGTYPE_p_bt_component_filter, BT_COMPONENT_CLASS_TYPE_FILTER,
		port_output, SWIGTYPE_p_bt_port_output, BT_PORT_TYPE_OUTPUT, py_callable);
}

static
bt_graph_listener_func_status
sink_component_input_port_added_listener(const bt_component_sink *component_sink,
					 const bt_port_input *port_input, void *py_callable)
{
	return port_added_listener(
		component_sink, SWIGTYPE_p_bt_component_sink, BT_COMPONENT_CLASS_TYPE_SINK,
		port_input, SWIGTYPE_p_bt_port_input, BT_PORT_TYPE_INPUT, py_callable);
}

static
PyObject *bt_bt2_graph_add_port_added_listener(struct bt_graph *graph,
		PyObject *py_callable)
{
	PyObject *py_listener_ids = NULL;
	PyObject *py_listener_id = NULL;
	bt_listener_id listener_id;
	bt_graph_add_listener_status status;
	const char * const module_name =
		"graph_add_port_added_listener() (Python)";

	BT_ASSERT(graph);
	BT_ASSERT(py_callable);

	/*
	 * Behind the scene, we will be registering 4 different listeners and
	 * return all of their ids.
	 */
	py_listener_ids = PyTuple_New(4);
	if (!py_listener_ids) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
			"Failed to allocate one PyTuple.");
		goto error;
	}

	/* source output port */
	status = bt_graph_add_source_component_output_port_added_listener(
		graph, source_component_output_port_added_listener,
		py_callable, &listener_id);
	if (status != __BT_FUNC_STATUS_OK) {
		/*
		 * bt_graph_add_source_component_output_port_added_listener has
		 * already logged/appended an error cause.
		 */
		goto error;
	}

	py_listener_id = PyLong_FromUnsignedLongLong(listener_id);
	if (!py_listener_id) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
			"Failed to allocate one PyLong.");
		goto error;
	}

	PyTuple_SET_ITEM(py_listener_ids, 0, py_listener_id);
	py_listener_id = NULL;

	/* filter input port */
	status = bt_graph_add_filter_component_input_port_added_listener(
		graph, filter_component_input_port_added_listener,
		py_callable, &listener_id);
	if (status != __BT_FUNC_STATUS_OK) {
		/*
		 * bt_graph_add_filter_component_input_port_added_listener has
		 * already logged/appended an error cause.
		 */
		goto error;
	}

	py_listener_id = PyLong_FromUnsignedLongLong(listener_id);
	if (!py_listener_id) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
			"Failed to allocate one PyLong.");
		goto error;
	}

	PyTuple_SET_ITEM(py_listener_ids, 1, py_listener_id);
	py_listener_id = NULL;

	/* filter output port */
	status = bt_graph_add_filter_component_output_port_added_listener(
		graph, filter_component_output_port_added_listener,
		py_callable, &listener_id);
	if (status != __BT_FUNC_STATUS_OK) {
		/*
		 * bt_graph_add_filter_component_output_port_added_listener has
		 * already logged/appended an error cause.
		 */
		goto error;
	}

	py_listener_id = PyLong_FromUnsignedLongLong(listener_id);
	if (!py_listener_id) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
			"Failed to allocate one PyLong.");
		goto error;
	}

	PyTuple_SET_ITEM(py_listener_ids, 2, py_listener_id);
	py_listener_id = NULL;

	/* sink input port */
	status = bt_graph_add_sink_component_input_port_added_listener(
		graph, sink_component_input_port_added_listener,
		py_callable, &listener_id);
	if (status != __BT_FUNC_STATUS_OK) {
		/*
		 * bt_graph_add_sink_component_input_port_added_listener has
		 * already logged/appended an error cause.
		 */
		goto error;
	}

	py_listener_id = PyLong_FromUnsignedLongLong(listener_id);
	if (!py_listener_id) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(module_name,
			"Failed to allocate one PyLong.");
		goto error;
	}

	PyTuple_SET_ITEM(py_listener_ids, 3, py_listener_id);
	py_listener_id = NULL;
	goto end;

error:
	Py_XDECREF(py_listener_ids);
	py_listener_ids = Py_None;
	Py_INCREF(py_listener_ids);

end:
	Py_XDECREF(py_listener_id);
	return py_listener_ids;
}

static
bt_graph_add_component_status
bt_bt2_graph_add_source_component(
		bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		PyObject *obj, bt_logging_level log_level,
		const bt_component_source **component)
{
	return bt_graph_add_source_component_with_initialize_method_data(graph,
		component_class, name, params, obj == Py_None ? NULL : obj,
		log_level, component);
}

static
bt_graph_add_component_status
bt_bt2_graph_add_filter_component(
		bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		PyObject *obj, bt_logging_level log_level,
		const bt_component_filter **component)
{
	return bt_graph_add_filter_component_with_initialize_method_data(graph,
		component_class, name, params, obj == Py_None ? NULL : obj,
		log_level, component);
}

static
bt_graph_add_component_status
bt_bt2_graph_add_sink_component(
		bt_graph *graph,
		const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		PyObject *obj, bt_logging_level log_level,
		const bt_component_sink **component)
{
	return bt_graph_add_sink_component_with_initialize_method_data(graph,
		component_class, name, params, obj == Py_None ? NULL : obj,
		log_level, component);
}
