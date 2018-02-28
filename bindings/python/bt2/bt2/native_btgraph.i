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

/* Types */
struct bt_graph;

/* Status */
enum bt_graph_status {
	BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION = 111,
	BT_GRAPH_STATUS_CANCELED = 125,
	BT_GRAPH_STATUS_AGAIN = 11,
	BT_GRAPH_STATUS_END = 1,
	BT_GRAPH_STATUS_OK = 0,
	BT_GRAPH_STATUS_INVALID = -22,
	BT_GRAPH_STATUS_NO_SINK = -6,
	BT_GRAPH_STATUS_CANNOT_CONSUME = -2,
	BT_GRAPH_STATUS_ERROR = -1,
	BT_GRAPH_STATUS_NOMEM = -12,
};

/* Functions */
struct bt_graph *bt_graph_create(void);
enum bt_graph_status bt_graph_add_component(
		struct bt_graph *graph,
		struct bt_component_class *component_class,
		const char *name, struct bt_value *params,
		struct bt_component **BTOUTCOMP);
enum bt_graph_status bt_graph_add_component_with_init_method_data(
		struct bt_graph *graph,
		struct bt_component_class *component_class,
		const char *name, struct bt_value *params,
		void *init_method_data,
		struct bt_component **BTOUTCOMP);
enum bt_graph_status bt_graph_connect_ports(struct bt_graph *graph,
		struct bt_port *upstream, struct bt_port *downstream,
		struct bt_connection **BTOUTCONN);
enum bt_graph_status bt_graph_run(struct bt_graph *graph);
enum bt_graph_status bt_graph_consume(struct bt_graph *graph);
enum bt_graph_status bt_graph_cancel(struct bt_graph *graph);
int bt_graph_is_canceled(struct bt_graph *graph);

/* Helper functions for Python */
%{
static void port_added_listener(struct bt_port *port, void *py_callable)
{
	PyObject *py_port_ptr = NULL;
	PyObject *py_res = NULL;

	py_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(port),
		SWIGTYPE_p_bt_port, 0);
	if (!py_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_port_ptr);
	BT_ASSERT(py_res == Py_None);
	Py_DECREF(py_port_ptr);
	Py_DECREF(py_res);
}

static void port_removed_listener(struct bt_component *component,
		struct bt_port *port, void *py_callable)
{
	PyObject *py_port_ptr = NULL;
	PyObject *py_res = NULL;

	py_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(port),
		SWIGTYPE_p_bt_port, 0);
	if (!py_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_port_ptr);
	BT_ASSERT(py_res == Py_None);
	Py_DECREF(py_port_ptr);
	Py_DECREF(py_res);
}

static void ports_connected_listener(struct bt_port *upstream_port,
		struct bt_port *downstream_port, void *py_callable)
{
	PyObject *py_upstream_port_ptr = NULL;
	PyObject *py_downstream_port_ptr = NULL;
	PyObject *py_res = NULL;

	py_upstream_port_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(upstream_port), SWIGTYPE_p_bt_port, 0);
	if (!py_upstream_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_downstream_port_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(downstream_port), SWIGTYPE_p_bt_port, 0);
	if (!py_downstream_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(OO)",
		py_upstream_port_ptr, py_downstream_port_ptr);
	BT_ASSERT(py_res == Py_None);
	Py_DECREF(py_upstream_port_ptr);
	Py_DECREF(py_downstream_port_ptr);
	Py_DECREF(py_res);
}

static void ports_disconnected_listener(
		struct bt_component *upstream_component,
		struct bt_component *downstream_component,
		struct bt_port *upstream_port, struct bt_port *downstream_port,
		void *py_callable)
{
	PyObject *py_upstream_comp_ptr = NULL;
	PyObject *py_downstream_comp_ptr = NULL;
	PyObject *py_upstream_port_ptr = NULL;
	PyObject *py_downstream_port_ptr = NULL;
	PyObject *py_res = NULL;

	py_upstream_comp_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(upstream_component),
		SWIGTYPE_p_bt_component, 0);
	if (!py_upstream_comp_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_downstream_comp_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(downstream_component),
		SWIGTYPE_p_bt_component, 0);
	if (!py_downstream_comp_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_upstream_port_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(upstream_port), SWIGTYPE_p_bt_port, 0);
	if (!py_upstream_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_downstream_port_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(downstream_port), SWIGTYPE_p_bt_port, 0);
	if (!py_downstream_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(OOOO)",
		py_upstream_comp_ptr, py_downstream_comp_ptr,
		py_upstream_port_ptr, py_downstream_port_ptr);
	BT_ASSERT(py_res == Py_None);
	Py_DECREF(py_upstream_comp_ptr);
	Py_DECREF(py_downstream_comp_ptr);
	Py_DECREF(py_upstream_port_ptr);
	Py_DECREF(py_downstream_port_ptr);
	Py_DECREF(py_res);
}

static void graph_listener_removed(void *py_callable)
{
	BT_ASSERT(py_callable);
	Py_DECREF(py_callable);
}

static int bt_py3_graph_add_port_added_listener(struct bt_graph *graph,
		PyObject *py_callable)
{
	int ret = 0;

	BT_ASSERT(graph);
	BT_ASSERT(py_callable);
	ret = bt_graph_add_port_added_listener(graph, port_added_listener,
		graph_listener_removed, py_callable);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	}

	return ret;
}

static int bt_py3_graph_add_port_removed_listener(struct bt_graph *graph,
		PyObject *py_callable)
{
	int ret = 0;

	BT_ASSERT(graph);
	BT_ASSERT(py_callable);
	ret = bt_graph_add_port_removed_listener(graph, port_removed_listener,
		graph_listener_removed, py_callable);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	}

	return ret;
}

static int bt_py3_graph_add_ports_connected_listener(struct bt_graph *graph,
		PyObject *py_callable)
{
	int ret = 0;

	BT_ASSERT(graph);
	BT_ASSERT(py_callable);
	ret = bt_graph_add_ports_connected_listener(graph,
		ports_connected_listener, graph_listener_removed, py_callable);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	}

	return ret;
}

static int bt_py3_graph_add_ports_disconnected_listener(struct bt_graph *graph,
		PyObject *py_callable)
{
	int ret = 0;

	BT_ASSERT(graph);
	BT_ASSERT(py_callable);
	ret = bt_graph_add_ports_disconnected_listener(graph,
		ports_disconnected_listener, graph_listener_removed,
		py_callable);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	}

	return ret;
}
%}

int bt_py3_graph_add_port_added_listener(struct bt_graph *graph,
		PyObject *py_callable);
int bt_py3_graph_add_port_removed_listener(struct bt_graph *graph,
		PyObject *py_callable);
int bt_py3_graph_add_ports_connected_listener(struct bt_graph *graph,
		PyObject *py_callable);
int bt_py3_graph_add_ports_disconnected_listener(struct bt_graph *graph,
		PyObject *py_callable);
