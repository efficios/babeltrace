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

/* From component-class-const.h */

typedef enum bt_component_class_status {
	BT_COMPONENT_CLASS_STATUS_OK = 0,
	BT_COMPONENT_CLASS_STATUS_NOMEM = -12,
} bt_component_class_status;

typedef enum bt_component_class_type {
	BT_COMPONENT_CLASS_TYPE_SOURCE = 0,
	BT_COMPONENT_CLASS_TYPE_FILTER = 1,
	BT_COMPONENT_CLASS_TYPE_SINK = 2,
} bt_component_class_type;

extern const char *bt_component_class_get_name(
		const bt_component_class *component_class);

extern const char *bt_component_class_get_description(
		const bt_component_class *component_class);

extern const char *bt_component_class_get_help(
		const bt_component_class *component_class);

extern bt_component_class_type bt_component_class_get_type(
		const bt_component_class *component_class);

bt_bool bt_component_class_is_source(
		const bt_component_class *component_class);

bt_bool bt_component_class_is_filter(
		const bt_component_class *component_class);

bt_bool bt_component_class_is_sink(
		const bt_component_class *component_class);

extern void bt_component_class_get_ref(
		const bt_component_class *component_class);

extern void bt_component_class_put_ref(
		const bt_component_class *component_class);

/* From component-class-source-const.h */

const bt_component_class *
bt_component_class_source_as_component_class_const(
		const bt_component_class_source *comp_cls_source);

extern void bt_component_class_source_get_ref(
		const bt_component_class_source *component_class_source);

extern void bt_component_class_source_put_ref(
		const bt_component_class_source *component_class_source);

/* From component-class-source.h */

typedef bt_self_component_status
(*bt_component_class_source_init_method)(
		bt_self_component_source *self_component,
		const bt_value *params, void *init_method_data);

typedef void (*bt_component_class_source_finalize_method)(
		bt_self_component_source *self_component);

typedef bt_self_message_iterator_status
(*bt_component_class_source_message_iterator_init_method)(
		bt_self_message_iterator *message_iterator,
		bt_self_component_source *self_component,
		bt_self_component_port_output *port);

typedef void
(*bt_component_class_source_message_iterator_finalize_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_self_message_iterator_status
(*bt_component_class_source_message_iterator_next_method)(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

typedef bt_self_message_iterator_status
(*bt_component_class_source_message_iterator_seek_ns_from_origin_method)(
		bt_self_message_iterator *message_iterator,
		int64_t ns_from_origin);

typedef bt_self_message_iterator_status
(*bt_component_class_source_message_iterator_seek_beginning_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_bool
(*bt_component_class_source_message_iterator_can_seek_ns_from_origin_method)(
		bt_self_message_iterator *message_iterator,
		int64_t ns_from_origin);

typedef bt_bool
(*bt_component_class_source_message_iterator_can_seek_beginning_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_query_status (*bt_component_class_source_query_method)(
		bt_self_component_class_source *comp_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result);

typedef bt_self_component_status
(*bt_component_class_source_accept_output_port_connection_method)(
		bt_self_component_source *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

typedef bt_self_component_status
(*bt_component_class_source_output_port_connected_method)(
		bt_self_component_source *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

bt_component_class *bt_component_class_source_as_component_class(
		bt_component_class_source *comp_cls_source);

extern
bt_component_class_source *bt_component_class_source_create(
		const char *name,
		bt_component_class_source_message_iterator_next_method method);

extern bt_component_class_status
bt_component_class_source_set_init_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_init_method method);

extern bt_component_class_status
bt_component_class_source_set_finalize_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_finalize_method method);

extern bt_component_class_status
bt_component_class_source_set_accept_output_port_connection_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_accept_output_port_connection_method method);

extern bt_component_class_status
bt_component_class_source_set_output_port_connected_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_output_port_connected_method method);

extern bt_component_class_status
bt_component_class_source_set_query_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_query_method method);

extern bt_component_class_status
bt_component_class_source_set_message_iterator_init_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_message_iterator_init_method method);

extern bt_component_class_status
bt_component_class_source_set_message_iterator_finalize_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_message_iterator_finalize_method method);

extern bt_component_class_status
bt_component_class_source_set_message_iterator_seek_ns_from_origin_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_message_iterator_seek_ns_from_origin_method method);

extern bt_component_class_status
bt_component_class_source_set_message_iterator_seek_beginning_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_message_iterator_seek_beginning_method method);

extern bt_bool
bt_component_class_source_set_message_iterator_can_seek_ns_from_origin_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_message_iterator_can_seek_ns_from_origin_method method);

extern bt_bool
bt_component_class_source_set_message_iterator_can_seek_beginning_method(
		bt_component_class_source *comp_class,
		bt_component_class_source_message_iterator_can_seek_beginning_method method);

/* From component-class-filter-const.h */

const bt_component_class *
bt_component_class_filter_as_component_class_const(
		const bt_component_class_filter *comp_cls_filter);

extern void bt_component_class_filter_get_ref(
		const bt_component_class_filter *component_class_filter);

extern void bt_component_class_filter_put_ref(
		const bt_component_class_filter *component_class_filter);

/* From component-class-filter.h */

typedef bt_self_component_status
(*bt_component_class_filter_init_method)(
		bt_self_component_filter *self_component,
		const bt_value *params, void *init_method_data);

typedef void (*bt_component_class_filter_finalize_method)(
		bt_self_component_filter *self_component);

typedef bt_self_message_iterator_status
(*bt_component_class_filter_message_iterator_init_method)(
		bt_self_message_iterator *message_iterator,
		bt_self_component_filter *self_component,
		bt_self_component_port_output *port);

typedef void
(*bt_component_class_filter_message_iterator_finalize_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_self_message_iterator_status
(*bt_component_class_filter_message_iterator_next_method)(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

typedef bt_self_message_iterator_status
(*bt_component_class_filter_message_iterator_seek_ns_from_origin_method)(
		bt_self_message_iterator *message_iterator,
		int64_t ns_from_origin);

typedef bt_self_message_iterator_status
(*bt_component_class_filter_message_iterator_seek_beginning_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_bool
(*bt_component_class_filter_message_iterator_can_seek_ns_from_origin_method)(
		bt_self_message_iterator *message_iterator,
		int64_t ns_from_origin);

typedef bt_bool
(*bt_component_class_filter_message_iterator_can_seek_beginning_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_query_status
(*bt_component_class_filter_query_method)(
		bt_self_component_class_filter *comp_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result);

typedef bt_self_component_status
(*bt_component_class_filter_accept_input_port_connection_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_filter_accept_output_port_connection_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

typedef bt_self_component_status
(*bt_component_class_filter_input_port_connected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_filter_output_port_connected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

bt_component_class *bt_component_class_filter_as_component_class(
		bt_component_class_filter *comp_cls_filter);

extern
bt_component_class_filter *bt_component_class_filter_create(
		const char *name,
		bt_component_class_filter_message_iterator_next_method method);

extern bt_component_class_status
bt_component_class_filter_set_init_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_init_method method);

extern bt_component_class_status
bt_component_class_filter_set_finalize_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_finalize_method method);

extern bt_component_class_status
bt_component_class_filter_set_accept_input_port_connection_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_accept_input_port_connection_method method);

extern bt_component_class_status
bt_component_class_filter_set_accept_output_port_connection_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_accept_output_port_connection_method method);

extern bt_component_class_status
bt_component_class_filter_set_input_port_connected_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_input_port_connected_method method);

extern bt_component_class_status
bt_component_class_filter_set_output_port_connected_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_output_port_connected_method method);

extern bt_component_class_status
bt_component_class_filter_set_query_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_query_method method);

extern bt_component_class_status
bt_component_class_filter_set_message_iterator_init_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_init_method method);

extern bt_component_class_status
bt_component_class_filter_set_message_iterator_finalize_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_finalize_method method);

extern bt_component_class_status
bt_component_class_filter_set_message_iterator_seek_ns_from_origin_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_seek_ns_from_origin_method method);

extern bt_component_class_status
bt_component_class_filter_set_message_iterator_seek_beginning_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_seek_beginning_method method);

extern bt_bool
bt_component_class_filter_set_message_iterator_can_seek_ns_from_origin_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_can_seek_ns_from_origin_method method);

extern bt_bool
bt_component_class_filter_set_message_iterator_can_seek_beginning_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_can_seek_beginning_method method);

/* From component-class-sink-const.h */

const bt_component_class *
bt_component_class_sink_as_component_class_const(
		const bt_component_class_sink *comp_cls_sink);

extern void bt_component_class_sink_get_ref(
		const bt_component_class_sink *component_class_sink);

extern void bt_component_class_sink_put_ref(
		const bt_component_class_sink *component_class_sink);

/* From component-class-sink.h */

typedef bt_self_component_status (*bt_component_class_sink_init_method)(
		bt_self_component_sink *self_component,
		const bt_value *params, void *init_method_data);

typedef void (*bt_component_class_sink_finalize_method)(
		bt_self_component_sink *self_component);

typedef bt_query_status
(*bt_component_class_sink_query_method)(
		bt_self_component_class_sink *comp_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result);

typedef bt_self_component_status
(*bt_component_class_sink_accept_input_port_connection_method)(
		bt_self_component_sink *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_sink_input_port_connected_method)(
		bt_self_component_sink *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_sink_graph_is_configured_method)(
		bt_self_component_sink *self_component);

typedef bt_self_component_status (*bt_component_class_sink_consume_method)(
	bt_self_component_sink *self_component);

bt_component_class *bt_component_class_sink_as_component_class(
		bt_component_class_sink *comp_cls_sink);

extern
bt_component_class_sink *bt_component_class_sink_create(
		const char *name,
		bt_component_class_sink_consume_method method);

extern bt_component_class_status bt_component_class_sink_set_init_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_init_method method);

extern bt_component_class_status bt_component_class_sink_set_finalize_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_finalize_method method);

extern bt_component_class_status
bt_component_class_sink_set_accept_input_port_connection_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_accept_input_port_connection_method method);

extern bt_component_class_status
bt_component_class_sink_set_input_port_connected_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_input_port_connected_method method);

extern bt_component_class_status
bt_component_class_sink_set_graph_is_configured_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_graph_is_configured_method method);

extern bt_component_class_status bt_component_class_sink_set_query_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_query_method method);

/* From self-component-class-source.h */

const bt_component_class_source *
bt_self_component_class_source_as_component_class_source(
		bt_self_component_class_source *self_comp_cls_source);

/* From self-component-class-filter.h */

const bt_component_class_filter *
bt_self_component_class_filter_as_component_class_filter(
		bt_self_component_class_filter *self_comp_cls_filter);

/* From self-component-class-sink.h */

const bt_component_class_sink *
bt_self_component_class_sink_as_component_class_sink(
		bt_self_component_class_sink *self_comp_cls_sink);

%{
/*
 * This hash table associates a BT component class object address to a
 * user-defined Python class (PyObject *). The keys and values are NOT
 * owned by this hash table. The Python class objects are owned by the
 * Python module, which should not be unloaded until it is not possible
 * to create a user Python component anyway.
 *
 * This hash table is written to when a user-defined Python component
 * class is created by one of the bt_py3_component_class_*_create()
 * functions.
 *
 * This function is read from when a user calls bt_component_create()
 * with a component class pointer created by one of the functions above.
 * In this case, the original Python class needs to be found to
 * instantiate it and associate the created Python component object with
 * a BT component object instance.
 */

static GHashTable *bt_cc_ptr_to_py_cls;

static void register_cc_ptr_to_py_cls(struct bt_component_class *bt_cc,
		PyObject *py_cls)
{
	if (!bt_cc_ptr_to_py_cls) {
		/*
		 * Lazy-initializing this GHashTable because GLib
		 * might not be initialized yet and it needs to be
		 * before we call g_hash_table_new()
		 */
		BT_LOGD_STR("Creating native component class to Python component class hash table.");
		bt_cc_ptr_to_py_cls = g_hash_table_new(g_direct_hash, g_direct_equal);
		BT_ASSERT(bt_cc_ptr_to_py_cls);
	}

	g_hash_table_insert(bt_cc_ptr_to_py_cls, (gpointer) bt_cc,
		(gpointer) py_cls);
}

static PyObject *lookup_cc_ptr_to_py_cls(const bt_component_class *bt_cc)
{
	if (!bt_cc_ptr_to_py_cls) {
		BT_LOGW("Cannot look up Python component class because hash table is NULL: "
			"comp-cls-addr=%p", bt_cc);
		return NULL;
	}

	return (PyObject *) g_hash_table_lookup(bt_cc_ptr_to_py_cls,
		(gconstpointer) bt_cc);
}


/*
 * Useful Python objects.
 */

static PyObject *py_mod_bt2 = NULL;
static PyObject *py_mod_bt2_exc_error_type = NULL;
static PyObject *py_mod_bt2_exc_try_again_type = NULL;
static PyObject *py_mod_bt2_exc_stop_type = NULL;
static PyObject *py_mod_bt2_exc_port_connection_refused_type = NULL;
static PyObject *py_mod_bt2_exc_msg_iter_canceled_type = NULL;
static PyObject *py_mod_bt2_exc_invalid_query_object_type = NULL;
static PyObject *py_mod_bt2_exc_invalid_query_params_type = NULL;

static void bt_py3_cc_init_from_bt2(void)
{
	/*
	 * This is called once the bt2 package is loaded.
	 *
	 * Those modules and functions are needed while the package is
	 * used. Loading them here is safe because we know the bt2
	 * package is imported, and we know that the user cannot use the
	 * code here without importing bt2 first.
	 */
	py_mod_bt2 = PyImport_ImportModule("bt2");
	BT_ASSERT(py_mod_bt2);
	py_mod_bt2_exc_error_type =
		PyObject_GetAttrString(py_mod_bt2, "Error");
	BT_ASSERT(py_mod_bt2_exc_error_type);
	py_mod_bt2_exc_try_again_type =
		PyObject_GetAttrString(py_mod_bt2, "TryAgain");
	BT_ASSERT(py_mod_bt2_exc_try_again_type);
	py_mod_bt2_exc_stop_type =
		PyObject_GetAttrString(py_mod_bt2, "Stop");
	BT_ASSERT(py_mod_bt2_exc_stop_type);
	py_mod_bt2_exc_port_connection_refused_type =
		PyObject_GetAttrString(py_mod_bt2, "PortConnectionRefused");
	BT_ASSERT(py_mod_bt2_exc_port_connection_refused_type);
	py_mod_bt2_exc_invalid_query_object_type =
		PyObject_GetAttrString(py_mod_bt2, "InvalidQueryObject");
	BT_ASSERT(py_mod_bt2_exc_invalid_query_object_type);
	py_mod_bt2_exc_invalid_query_params_type =
		PyObject_GetAttrString(py_mod_bt2, "InvalidQueryParams");
	BT_ASSERT(py_mod_bt2_exc_invalid_query_params_type);
}

static void bt_py3_cc_exit_handler(void)
{
	/*
	 * This is an exit handler (set by the bt2 package).
	 *
	 * We only give back the references that we took in
	 * bt_py3_cc_init_from_bt2() here. The global variables continue
	 * to exist for the code of this file, but they are now borrowed
	 * references. If this code is executed, it means that somehow
	 * the modules are still loaded, so it should be safe to use
	 * them even without a strong reference.
	 *
	 * We cannot do this in the library's destructor because it
	 * gets executed once Python is already finalized.
	 */
	Py_XDECREF(py_mod_bt2);
	Py_XDECREF(py_mod_bt2_exc_error_type);
	Py_XDECREF(py_mod_bt2_exc_try_again_type);
	Py_XDECREF(py_mod_bt2_exc_stop_type);
	Py_XDECREF(py_mod_bt2_exc_port_connection_refused_type);
	Py_XDECREF(py_mod_bt2_exc_msg_iter_canceled_type);
	Py_XDECREF(py_mod_bt2_exc_invalid_query_object_type);
	Py_XDECREF(py_mod_bt2_exc_invalid_query_params_type);
}


/* Library destructor */

__attribute__((destructor))
static void bt_py3_native_comp_class_dtor(void) {
	/* Destroy component class association hash table */
	if (bt_cc_ptr_to_py_cls) {
		BT_LOGD_STR("Destroying native component class to Python component class hash table.");
		g_hash_table_destroy(bt_cc_ptr_to_py_cls);
	}
}


// TODO: maybe we can wrap code in the Python methods (e.g. _query_from_native)
// in a try catch and print the error there instead, it would be simpler.
static
void bt2_py_loge_exception(void)
{
	PyObject *type = NULL;
	PyObject *value = NULL;
	PyObject *traceback = NULL;
	PyObject *traceback_module = NULL;
	PyObject *format_exception_func = NULL;
	PyObject *exc_str_list = NULL;
	GString *msg_buf = NULL;
	Py_ssize_t i;

	BT_ASSERT(PyErr_Occurred() != NULL);

	PyErr_Fetch(&type, &value, &traceback);

	BT_ASSERT(type != NULL);

	/*
	* traceback can be NULL, when we fail to call a Python function from the
	* native code (there is not Python stack at that point).  E.g.:
	*
	*   TypeError: _accept_port_connection_from_native() takes 3 positional arguments but 4 were given
	*/


	/* Make sure `value` is what we expected - an instance of `type`. */
	PyErr_NormalizeException(&type, &value, &traceback);

	traceback_module = PyImport_ImportModule("traceback");
	if (!traceback_module) {
		BT_LOGE_STR("Failed to log Python exception (could not import traceback module).");
		goto end;
	}

	format_exception_func = PyObject_GetAttrString(traceback_module,
		traceback ? "format_exception" : "format_exception_only");
	if (!format_exception_func) {
		BT_LOGE_STR("Failed to log Python exception (could not find format_exception).");
		goto end;
	}

	if (!PyCallable_Check(format_exception_func)) {
		BT_LOGE_STR("Failed to log Python exception (format_exception is not callable).");
		goto end;
	}

	exc_str_list = PyObject_CallFunctionObjArgs(format_exception_func, type, value, traceback, NULL);
	if (!exc_str_list) {
		PyErr_Print();
		BT_LOGE_STR("Failed to log Python exception (call to format_exception failed).");
		goto end;
	}

	msg_buf = g_string_new(NULL);

	for (i = 0; i < PyList_Size(exc_str_list); i++) {
		PyObject *exc_str = PyList_GetItem(exc_str_list, i);
		const char *str = PyUnicode_AsUTF8(exc_str);
		if (!str) {
			BT_LOGE_STR("Failed to log Python exception (failed to convert exception to string).");
			goto end;
		}

		g_string_append(msg_buf, str);
	}

	BT_LOGE_STR(msg_buf->str);

end:
	if (msg_buf) {
		g_string_free(msg_buf, TRUE);
	}
	Py_XDECREF(exc_str_list);
	Py_XDECREF(format_exception_func);
	Py_XDECREF(traceback_module);

	/* PyErr_Restore takes our references. */
	PyErr_Restore(type, value, traceback);
}

static bt_self_component_status bt_py3_exc_to_self_component_status(void)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
		goto end;
	}

	if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_try_again_type)) {
		status = BT_SELF_COMPONENT_STATUS_AGAIN;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_stop_type)) {
		status = BT_SELF_COMPONENT_STATUS_END;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_port_connection_refused_type)) {
		status = BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION;
	} else {
		bt2_py_loge_exception();
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

/* Component class proxy methods (delegate to the attached Python object) */

static bt_self_message_iterator_status
bt_py3_exc_to_self_message_iterator_status(void)
{
	enum bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
		goto end;
	}

	if (PyErr_GivenExceptionMatches(exc, py_mod_bt2_exc_stop_type)) {
		status = BT_MESSAGE_ITERATOR_STATUS_END;
	} else if (PyErr_GivenExceptionMatches(exc, py_mod_bt2_exc_try_again_type)) {
		status = BT_MESSAGE_ITERATOR_STATUS_AGAIN;
	} else {
		bt2_py_loge_exception();
		status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

static enum bt_query_status bt_py3_exc_to_query_status(void)
{
	enum bt_query_status status = BT_QUERY_STATUS_OK;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
		goto end;
	}

	if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_invalid_query_object_type)) {
		status = BT_QUERY_STATUS_INVALID_OBJECT;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_invalid_query_params_type)) {
		status = BT_QUERY_STATUS_INVALID_PARAMS;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_try_again_type)) {
		status = BT_QUERY_STATUS_AGAIN;
	} else {
		bt2_py_loge_exception();
		status = BT_QUERY_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

static bt_self_component_status
bt_py3_component_class_init(
		bt_self_component *self_component,
		void *self_component_v,
		swig_type_info *self_comp_cls_type_swig_type,
		const bt_value *params,
		void *init_method_data)
{
	const bt_component *component = bt_self_component_as_component(self_component);
	const bt_component_class *component_class = bt_component_borrow_class_const(component);
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	PyObject *py_cls = NULL;
	PyObject *py_comp = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_comp_ptr = NULL;

	(void) init_method_data;

	BT_ASSERT(self_component);
	BT_ASSERT(self_component_v);
	BT_ASSERT(self_comp_cls_type_swig_type);

	/*
	 * Get the user-defined Python class which created this
	 * component's class in the first place (borrowed
	 * reference).
	 */
	py_cls = lookup_cc_ptr_to_py_cls(component_class);
	if (!py_cls) {
		BT_LOGE("Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", component_class);
		goto error;
	}

	/* Parameters pointer -> SWIG pointer Python object */
	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_comp_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_component_v),
		self_comp_cls_type_swig_type, 0);
	if (!py_comp_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	/*
	 * Do the equivalent of this:
	 *
	 *     py_comp = py_cls._init_from_native(py_comp_ptr, py_params_ptr)
	 *
	 * _UserComponentType._init_from_native() calls the Python
	 * component object's __init__() function.
	 */
	py_comp = PyObject_CallMethod(py_cls,
		"_init_from_native", "(OO)", py_comp_ptr, py_params_ptr);
	if (!py_comp) {
		bt2_py_loge_exception();
		BT_LOGE("Failed to call Python class's _init_from_native() method: "
			"py-cls-addr=%p", py_cls);

		goto error;
	}

	/*
	 * Our user Python component object is now fully created and
	 * initialized by the user. Since we just created it, this
	 * native component is its only (persistent) owner.
	 */
	bt_self_component_set_data(self_component, py_comp);
	py_comp = NULL;
	goto end;

error:
	status = BT_SELF_COMPONENT_STATUS_ERROR;

	/*
	 * Clear any exception: we're returning a bad status anyway. If
	 * this call originated from Python (creation from a plugin's
	 * component class, for example), then the user gets an
	 * appropriate creation error.
	 */
	PyErr_Clear();

end:
	Py_XDECREF(py_comp);
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_comp_ptr);
	return status;
}

/*
 * Method of bt_component_class_source to initialize a bt_self_component_source
 * of that class.
 */

static bt_self_component_status
bt_py3_component_class_source_init(bt_self_component_source *self_component_source,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);
	return bt_py3_component_class_init(
		self_component,
		self_component_source,
		SWIGTYPE_p_bt_self_component_source,
		params, init_method_data);
}

static bt_self_component_status
bt_py3_component_class_filter_init(bt_self_component_filter *self_component_filter,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	return bt_py3_component_class_init(
		self_component,
		self_component_filter,
		SWIGTYPE_p_bt_self_component_filter,
		params, init_method_data);
}

static bt_self_component_status
bt_py3_component_class_sink_init(bt_self_component_sink *self_component_sink,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	return bt_py3_component_class_init(
		self_component,
		self_component_sink,
		SWIGTYPE_p_bt_self_component_sink,
		params, init_method_data);
}

static void bt_py3_component_class_finalize(bt_self_component *self_component)
{
	PyObject *py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT(py_comp);

	/* Call user's _finalize() method */
	PyObject *py_method_result = PyObject_CallMethod(py_comp,
		"_finalize", NULL);

	if (PyErr_Occurred()) {
		BT_LOGW("User's _finalize() method raised an exception: ignoring.");
	}

	/*
	 * Ignore any exception raised by the _finalize() method because
	 * it won't change anything at this point: the component is
	 * being destroyed anyway.
	 */
	PyErr_Clear();
	Py_XDECREF(py_method_result);
	Py_DECREF(py_comp);
}

static void
bt_py3_component_class_source_finalize(bt_self_component_source *self_component_source)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);
	bt_py3_component_class_finalize(self_component);
}

static void
bt_py3_component_class_filter_finalize(bt_self_component_filter *self_component_filter)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	bt_py3_component_class_finalize(self_component);
}

static void
bt_py3_component_class_sink_finalize(bt_self_component_sink *self_component_sink)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	bt_py3_component_class_finalize(self_component);
}

static bt_self_component_status
bt_py3_component_class_accept_port_connection(
		bt_self_component *self_component,
		bt_self_component_port *self_component_port,
		bt_port_type self_component_port_type,
		const bt_port *other_port)
{
	enum bt_self_component_status status;
	PyObject *py_comp = NULL;
	PyObject *py_self_port_ptr = NULL;
	PyObject *py_other_port_ptr = NULL;
	PyObject *py_method_result = NULL;

	py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT(py_comp);

	swig_type_info *self_component_port_swig_type = NULL;
	swig_type_info *other_port_swig_type = NULL;
	switch (self_component_port_type) {
	case BT_PORT_TYPE_INPUT:
		self_component_port_swig_type = SWIGTYPE_p_bt_self_component_port_input;
		other_port_swig_type = SWIGTYPE_p_bt_port_output;
		break;
	case BT_PORT_TYPE_OUTPUT:
		self_component_port_swig_type = SWIGTYPE_p_bt_self_component_port_output;
		other_port_swig_type = SWIGTYPE_p_bt_port_input;
		break;
	}
	BT_ASSERT(self_component_port_swig_type != NULL);
	BT_ASSERT(other_port_swig_type != NULL);

	py_self_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_component_port),
		self_component_port_swig_type, 0);
	if (!py_self_port_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_other_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(other_port),
		other_port_swig_type, 0);
	if (!py_other_port_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_accept_port_connection_from_native", "(OiO)", py_self_port_ptr,
		self_component_port_type, py_other_port_ptr);

	status = bt_py3_exc_to_self_component_status();
	if (!py_method_result && status == BT_SELF_COMPONENT_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		BT_LOGE("User's _accept_port_connection() method failed without raising an exception: "
			"status=%d", status);
		goto error;
	}

	if (status == BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION) {
		/*
		 * Looks like the user method raised
		 * PortConnectionRefused: accept this like if it
		 * returned False.
		 */
		goto end;
	} else if (status != BT_SELF_COMPONENT_STATUS_OK) {
		BT_LOGE("User's _accept_port_connection() raised an unexpected exception: "
			"status=%d", status);
		goto error;
	}

	BT_ASSERT(PyBool_Check(py_method_result));

	if (py_method_result == Py_True) {
		status = BT_SELF_COMPONENT_STATUS_OK;
	} else {
		status = BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION;
	}

	goto end;

error:
	status = BT_SELF_COMPONENT_STATUS_ERROR;

	/*
	 * Clear any exception: we're returning a bad status anyway. If
	 * this call originated from Python, then the user gets an
	 * appropriate error.
	 */
	PyErr_Clear();

end:
	Py_XDECREF(py_self_port_ptr);
	Py_XDECREF(py_other_port_ptr);
	Py_XDECREF(py_method_result);
	return status;
}

static bt_self_component_status
bt_py3_component_class_source_accept_output_port_connection(bt_self_component_source *self_component_source,
	 bt_self_component_port_output *self_component_port_output,
	 const bt_port_input *other_port_input)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);
	bt_self_component_port *self_component_port = bt_self_component_port_output_as_self_component_port(self_component_port_output);
	const bt_port *other_port = bt_port_input_as_port_const(other_port_input);
	return bt_py3_component_class_accept_port_connection(self_component, self_component_port, BT_PORT_TYPE_OUTPUT, other_port);
}

static bt_self_component_status
bt_py3_component_class_filter_accept_input_port_connection(bt_self_component_filter *self_component_filter,
		bt_self_component_port_input *self_component_port_input,
		const bt_port_output *other_port_output)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	bt_self_component_port *self_component_port = bt_self_component_port_input_as_self_component_port(self_component_port_input);
	const bt_port *other_port = bt_port_output_as_port_const(other_port_output);
	return bt_py3_component_class_accept_port_connection(self_component, self_component_port, BT_PORT_TYPE_INPUT, other_port);
}

static bt_self_component_status
bt_py3_component_class_filter_accept_output_port_connection(bt_self_component_filter *self_component_filter,
		bt_self_component_port_output *self_component_port_output,
		const bt_port_input *other_port_input)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	bt_self_component_port *self_component_port = bt_self_component_port_output_as_self_component_port(self_component_port_output);
	const bt_port *other_port = bt_port_input_as_port_const(other_port_input);
	return bt_py3_component_class_accept_port_connection(self_component, self_component_port, BT_PORT_TYPE_OUTPUT, other_port);
}

static bt_self_component_status
bt_py3_component_class_sink_accept_input_port_connection(bt_self_component_sink *self_component_sink,
		bt_self_component_port_input *self_component_port_input,
		const bt_port_output *other_port_output)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	bt_self_component_port *self_component_port = bt_self_component_port_input_as_self_component_port(self_component_port_input);
	const bt_port *other_port = bt_port_output_as_port_const(other_port_output);
	return bt_py3_component_class_accept_port_connection(self_component, self_component_port, BT_PORT_TYPE_INPUT, other_port);
}

static bt_self_component_status
bt_py3_component_class_port_connected(
		bt_self_component *self_component,
		void *self_component_port,
		swig_type_info *self_component_port_swig_type,
		bt_port_type self_component_port_type,
		const void *other_port,
		swig_type_info *other_port_swig_type)
{
	bt_self_component_status status;
	PyObject *py_comp = NULL;
	PyObject *py_self_port_ptr = NULL;
	PyObject *py_other_port_ptr = NULL;
	PyObject *py_method_result = NULL;

	py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT(py_comp);

	py_self_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_component_port),
		self_component_port_swig_type, 0);
	if (!py_self_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	py_other_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(other_port),
		other_port_swig_type, 0);
	if (!py_other_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_port_connected_from_native", "(OiO)", py_self_port_ptr,
		self_component_port_type, py_other_port_ptr);

	BT_ASSERT(!py_method_result || py_method_result == Py_None);

	status = bt_py3_exc_to_self_component_status();

end:
	Py_XDECREF(py_self_port_ptr);
	Py_XDECREF(py_other_port_ptr);
	Py_XDECREF(py_method_result);

	return status;
}

static bt_self_component_status
bt_py3_component_class_source_output_port_connected(
		bt_self_component_source *self_component_source,
		bt_self_component_port_output *self_component_port_output,
		const bt_port_input *other_port_input)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);

	return bt_py3_component_class_port_connected(
		self_component,
		self_component_port_output,
		SWIGTYPE_p_bt_self_component_port_output,
		BT_PORT_TYPE_OUTPUT,
		other_port_input,
		SWIGTYPE_p_bt_port_input);
}

static bt_self_component_status
bt_py3_component_class_filter_input_port_connected(
		bt_self_component_filter *self_component_filter,
		bt_self_component_port_input *self_component_port_input,
		const bt_port_output *other_port_output)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);

	return bt_py3_component_class_port_connected(
		self_component,
		self_component_port_input,
		SWIGTYPE_p_bt_self_component_port_input,
		BT_PORT_TYPE_INPUT,
		other_port_output,
		SWIGTYPE_p_bt_port_output);
}

static bt_self_component_status
bt_py3_component_class_filter_output_port_connected(
		bt_self_component_filter *self_component_filter,
		bt_self_component_port_output *self_component_port_output,
		const bt_port_input *other_port_input)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);

	return bt_py3_component_class_port_connected(
		self_component,
		self_component_port_output,
		SWIGTYPE_p_bt_self_component_port_output,
		BT_PORT_TYPE_OUTPUT,
		other_port_input,
		SWIGTYPE_p_bt_port_input);
}

static bt_self_component_status
bt_py3_component_class_sink_input_port_connected(
		bt_self_component_sink *self_component_sink,
		bt_self_component_port_input *self_component_port_input,
		const bt_port_output *other_port_output)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);

	return bt_py3_component_class_port_connected(
		self_component,
		self_component_port_input,
		SWIGTYPE_p_bt_self_component_port_input,
		BT_PORT_TYPE_INPUT,
		other_port_output,
		SWIGTYPE_p_bt_port_output);
}

static bt_self_component_status
bt_py3_component_class_sink_graph_is_configured(bt_self_component_sink *self_component_sink)
{
	PyObject *py_comp = NULL;
	PyObject *py_method_result = NULL;
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);

	py_comp = bt_self_component_get_data(self_component);
	py_method_result = PyObject_CallMethod(py_comp,
		"_graph_is_configured_from_native", NULL);

	BT_ASSERT(!py_method_result || py_method_result == Py_None);

	status = bt_py3_exc_to_self_component_status();

	Py_XDECREF(py_method_result);

	return status;
}

static bt_query_status
bt_py3_component_class_query(
		const bt_component_class *component_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	PyObject *py_cls = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_query_exec_ptr = NULL;
	PyObject *py_query_func = NULL;
	PyObject *py_object = NULL;
	PyObject *py_results_addr = NULL;
	bt_query_status status = BT_QUERY_STATUS_OK;

	py_cls = lookup_cc_ptr_to_py_cls(component_class);
	if (!py_cls) {
		BT_LOGE("Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", component_class);
		goto error;
	}

	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_query_exec_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(query_executor),
		SWIGTYPE_p_bt_query_executor, 0);
	if (!py_query_exec_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_object = SWIG_FromCharPtr(object);
	if (!py_object) {
		BT_LOGE_STR("Failed to create a Python string.");
		goto error;
	}

	py_results_addr = PyObject_CallMethod(py_cls,
		"_query_from_native", "(OOO)", py_query_exec_ptr,
		py_object, py_params_ptr);

	if (!py_results_addr) {
		BT_LOGE("Failed to call Python class's _query_from_native() method: "
			"py-cls-addr=%p", py_cls);
		status = bt_py3_exc_to_query_status();
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a BT value object (new
	 * reference).
	 */
	*result = (void *) PyLong_AsUnsignedLongLong(py_results_addr);
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(*result);
	goto end;

error:
	PyErr_Clear();
	status = BT_QUERY_STATUS_ERROR;

end:
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_query_exec_ptr);
	Py_XDECREF(py_query_func);
	Py_XDECREF(py_object);
	Py_XDECREF(py_results_addr);
	return status;
}

static bt_query_status
bt_py3_component_class_source_query(
		bt_self_component_class_source *self_component_class_source,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	const bt_component_class_source *component_class_source = bt_self_component_class_source_as_component_class_source(self_component_class_source);
	const bt_component_class *component_class = bt_component_class_source_as_component_class_const(component_class_source);
	return bt_py3_component_class_query(component_class, query_executor, object, params, result);
}

static bt_query_status
bt_py3_component_class_filter_query(
		bt_self_component_class_filter *self_component_class_filter,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	const bt_component_class_filter *component_class_filter = bt_self_component_class_filter_as_component_class_filter(self_component_class_filter);
	const bt_component_class *component_class = bt_component_class_filter_as_component_class_const(component_class_filter);
	return bt_py3_component_class_query(component_class, query_executor, object, params, result);
}

static bt_query_status
bt_py3_component_class_sink_query(
		bt_self_component_class_sink *self_component_class_sink,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	const bt_component_class_sink *component_class_sink = bt_self_component_class_sink_as_component_class_sink(self_component_class_sink);
	const bt_component_class *component_class = bt_component_class_sink_as_component_class_const(component_class_sink);
	return bt_py3_component_class_query(component_class, query_executor, object, params, result);
}

static bt_self_message_iterator_status
bt_py3_component_class_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component *self_component,
		bt_self_component_port_output *self_component_port_output)
{
	bt_self_message_iterator_status status = BT_MESSAGE_ITERATOR_STATUS_OK;
	PyObject *py_comp_cls = NULL;
	PyObject *py_iter_cls = NULL;
	PyObject *py_iter_ptr = NULL;
	PyObject *py_init_method_result = NULL;
	PyObject *py_iter = NULL;
	PyObject *py_comp;

	py_comp = bt_self_component_get_data(self_component);

	/* Find user's Python message iterator class */
	py_comp_cls = PyObject_GetAttrString(py_comp, "__class__");
	if (!py_comp_cls) {
		BT_LOGE_STR("Cannot get Python object's `__class__` attribute.");
		goto error;
	}

	py_iter_cls = PyObject_GetAttrString(py_comp_cls, "_iter_cls");
	if (!py_iter_cls) {
		BT_LOGE_STR("Cannot get Python class's `_iter_cls` attribute.");
		goto error;
	}

	py_iter_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_message_iterator),
		SWIGTYPE_p_bt_self_message_iterator, 0);
	if (!py_iter_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	/*
	 * Create object with borrowed native message iterator
	 * reference:
	 *
	 *     py_iter = py_iter_cls.__new__(py_iter_cls, py_iter_ptr)
	 */
	py_iter = PyObject_CallMethod(py_iter_cls, "__new__",
		"(OO)", py_iter_cls, py_iter_ptr);
	if (!py_iter) {
		BT_LOGE("Failed to call Python class's __new__() method: "
			"py-cls-addr=%p", py_iter_cls);
		bt2_py_loge_exception();
		goto error;
	}

	/*
	 * Initialize object:
	 *
	 *     py_iter.__init__()
	 *
	 * At this point, py_iter._ptr is set, so this initialization
	 * function has access to self._component (which gives it the
	 * user Python component object from which the iterator was
	 * created).
	 */
	py_init_method_result = PyObject_CallMethod(py_iter, "__init__", NULL);
	if (!py_init_method_result) {
		BT_LOGE_STR("User's __init__() method failed.");
		bt2_py_loge_exception();
		goto error;
	}

	/*
	 * Since the Python code can never instantiate a user-defined
	 * message iterator class, the native message iterator
	 * object does NOT belong to a user Python message iterator
	 * object (borrowed reference). However this Python object is
	 * owned by this native message iterator object.
	 *
	 * In the Python world, the lifetime of the native message
	 * iterator is managed by a _GenericMessageIterator
	 * instance:
	 *
	 *     _GenericMessageIterator instance:
	 *         owns a native bt_message_iterator object (iter)
	 *             owns a _UserMessageIterator instance (py_iter)
	 *                 self._ptr is a borrowed reference to the
	 *                 native bt_private_connection_private_message_iterator
	 *                 object (iter)
	 */
	bt_self_message_iterator_set_data(self_message_iterator, py_iter);
	py_iter = NULL;
	goto end;

error:
	status = bt_py3_exc_to_self_message_iterator_status();
	if (status == BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
		/*
		 * Looks like there wasn't any exception from the Python
		 * side, but we're still in an error state here.
		 */
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
	}

	/*
	 * Clear any exception: we're returning a bad status anyway. If
	 * this call originated from Python, then the user gets an
	 * appropriate creation error.
	 */
	PyErr_Clear();

end:
	Py_XDECREF(py_comp_cls);
	Py_XDECREF(py_iter_cls);
	Py_XDECREF(py_iter_ptr);
	Py_XDECREF(py_init_method_result);
	Py_XDECREF(py_iter);
	return status;
}

static bt_self_message_iterator_status
bt_py3_component_class_source_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component_source *self_component_source,
		bt_self_component_port_output *self_component_port_output)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);
	return bt_py3_component_class_message_iterator_init(self_message_iterator, self_component, self_component_port_output);
}

static bt_self_message_iterator_status
bt_py3_component_class_filter_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component_filter *self_component_filter,
		bt_self_component_port_output *self_component_port_output)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	return bt_py3_component_class_message_iterator_init(self_message_iterator, self_component, self_component_port_output);
}

static void
bt_py3_component_class_message_iterator_finalize(
		bt_self_message_iterator *message_iterator)
{
	PyObject *py_message_iter = bt_self_message_iterator_get_data(message_iterator);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_message_iter);

	/* Call user's _finalize() method */
	py_method_result = PyObject_CallMethod(py_message_iter,
		"_finalize", NULL);

	if (PyErr_Occurred()) {
		BT_LOGW("User's _finalize() method raised an exception: ignoring.");
	}

	/*
	 * Ignore any exception raised by the _finalize() method because
	 * it won't change anything at this point: the component is
	 * being destroyed anyway.
	 */
	PyErr_Clear();
	Py_XDECREF(py_method_result);
	Py_DECREF(py_message_iter);
}

/* Valid for both sources and filters. */

static bt_self_message_iterator_status
bt_py3_component_class_message_iterator_next(
			bt_self_message_iterator *message_iterator,
			bt_message_array_const msgs, uint64_t capacity,
			uint64_t *count)
{
	bt_self_message_iterator_status status = BT_MESSAGE_ITERATOR_STATUS_OK;
	PyObject *py_message_iter = bt_self_message_iterator_get_data(message_iterator);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_message_iter);
	py_method_result = PyObject_CallMethod(py_message_iter,
		"_next_from_native", NULL);
	if (!py_method_result) {
		status = bt_py3_exc_to_self_message_iterator_status();
		BT_ASSERT(status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK);
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a native message
	 * object (which is now ours).
	 */
	msgs[0] =
		(const bt_message *) PyLong_AsUnsignedLongLong(
			py_method_result);
	*count = 1;

	/* Clear potential overflow error; should never happen */
	BT_ASSERT(!PyErr_Occurred());
	goto end;

end:
	Py_XDECREF(py_method_result);
	return status;
}

static bt_self_component_status
bt_py3_component_class_sink_consume(bt_self_component_sink *self_component_sink)
{
   	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	PyObject *py_comp = bt_self_component_get_data(self_component);
	PyObject *py_method_result = NULL;
	bt_self_component_status status;

	BT_ASSERT(py_comp);
	py_method_result = PyObject_CallMethod(py_comp,
		"_consume", NULL);

	status = bt_py3_exc_to_self_component_status();
	if (!py_method_result && status == BT_SELF_COMPONENT_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		BT_LOGE("User's _consume() method failed without raising an exception: "
			"status=%d", status);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

	Py_XDECREF(py_method_result);
	return status;
}

static
int bt_py3_component_class_set_help_and_desc(
		bt_component_class *component_class,
		const char *description, const char *help)
{
	int ret;

	if (description) {
		ret = bt_component_class_set_description(component_class, description);
		if (ret) {
			BT_LOGE("Cannot set component class's description: "
				"comp-cls-addr=%p", component_class);
			goto end;
		}
	}

	if (help) {
		ret = bt_component_class_set_help(component_class, help);
		if (ret) {
			BT_LOGE("Cannot set component class's help text: "
				"comp-cls-addr=%p", component_class);
			goto end;
		}
	}

	ret = 0;

end:
	return ret;
}

static
bt_component_class_source *bt_py3_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class_source *component_class_source;
	bt_component_class *component_class;
	int ret;

	BT_ASSERT(py_cls);

	component_class_source = bt_component_class_source_create(name,
		bt_py3_component_class_message_iterator_next);
	if (!component_class_source) {
		BT_LOGE_STR("Cannot create source component class.");
		goto end;
	}

	component_class = bt_component_class_source_as_component_class(component_class_source);

	if (bt_py3_component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_source_set_init_method(component_class_source, bt_py3_component_class_source_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_finalize_method (component_class_source, bt_py3_component_class_source_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_accept_output_port_connection_method(component_class_source,
		bt_py3_component_class_source_accept_output_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_output_port_connected_method(component_class_source,
		bt_py3_component_class_source_output_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_query_method(component_class_source, bt_py3_component_class_source_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_message_iterator_init_method(
		component_class_source, bt_py3_component_class_source_message_iterator_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_message_iterator_finalize_method(
		component_class_source, bt_py3_component_class_message_iterator_finalize);
	BT_ASSERT(ret == 0);

	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	return component_class_source;
}

static
bt_component_class_filter *bt_py3_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class *component_class;
	bt_component_class_filter *component_class_filter;
	int ret;

	BT_ASSERT(py_cls);

	component_class_filter = bt_component_class_filter_create(name,
		bt_py3_component_class_message_iterator_next);
	if (!component_class_filter) {
		BT_LOGE_STR("Cannot create filter component class.");
		goto end;
	}

	component_class = bt_component_class_filter_as_component_class(component_class_filter);

	if (bt_py3_component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_filter_set_init_method(component_class_filter, bt_py3_component_class_filter_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_finalize_method (component_class_filter, bt_py3_component_class_filter_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_accept_input_port_connection_method(component_class_filter,
		bt_py3_component_class_filter_accept_input_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_accept_output_port_connection_method(component_class_filter,
		bt_py3_component_class_filter_accept_output_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_input_port_connected_method(component_class_filter,
		bt_py3_component_class_filter_input_port_connected);
   	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_output_port_connected_method(component_class_filter,
		bt_py3_component_class_filter_output_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_query_method(component_class_filter, bt_py3_component_class_filter_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_message_iterator_init_method(
		component_class_filter, bt_py3_component_class_filter_message_iterator_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_message_iterator_finalize_method(
		component_class_filter, bt_py3_component_class_message_iterator_finalize);
	BT_ASSERT(ret == 0);

	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	return component_class_filter;
}

static
bt_component_class_sink *bt_py3_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class_sink *component_class_sink;
	bt_component_class *component_class;
	int ret;

	BT_ASSERT(py_cls);

	component_class_sink = bt_component_class_sink_create(name, bt_py3_component_class_sink_consume);

	if (!component_class_sink) {
		BT_LOGE_STR("Cannot create sink component class.");
		goto end;
	}

	component_class = bt_component_class_sink_as_component_class(component_class_sink);

	if (bt_py3_component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_sink_set_init_method(component_class_sink, bt_py3_component_class_sink_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_finalize_method(component_class_sink, bt_py3_component_class_sink_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_accept_input_port_connection_method(component_class_sink,
		bt_py3_component_class_sink_accept_input_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_input_port_connected_method(component_class_sink,
		bt_py3_component_class_sink_input_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_graph_is_configured_method(component_class_sink,
		bt_py3_component_class_sink_graph_is_configured);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_query_method(component_class_sink, bt_py3_component_class_sink_query);
	BT_ASSERT(ret == 0);

	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	return component_class_sink;
}
%}

struct bt_component_class_source *bt_py3_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_filter *bt_py3_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_sink *bt_py3_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
void bt_py3_cc_init_from_bt2(void);
void bt_py3_cc_exit_handler(void);
