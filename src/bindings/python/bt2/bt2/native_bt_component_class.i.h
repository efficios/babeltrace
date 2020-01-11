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

#include "logging/comp-logging.h"
#include "compat/glib.h"

/*
 * This hash table associates a BT component class object address to a
 * user-defined Python class (PyObject *). The keys and values are NOT
 * owned by this hash table. The Python class objects are owned by the
 * Python module, which should not be unloaded until it is not possible
 * to create a user Python component anyway.
 *
 * This hash table is written to when a user-defined Python component
 * class is created by one of the bt_bt2_component_class_*_create()
 * functions.
 *
 * This function is read from when a user calls bt_component_create()
 * with a component class pointer created by one of the functions above.
 * In this case, the original Python class needs to be found to
 * instantiate it and associate the created Python component object with
 * a BT component object instance.
 */

static GHashTable *bt_cc_ptr_to_py_cls;

static
void bt_bt2_unregister_cc_ptr_to_py_cls(const bt_component_class *comp_cls)
{
	gboolean existed;

	if (!bt_cc_ptr_to_py_cls) {
		return;
	}

	existed = g_hash_table_remove(bt_cc_ptr_to_py_cls, comp_cls);
	BT_ASSERT(existed);
}

static
void register_cc_ptr_to_py_cls(struct bt_component_class *bt_cc,
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

static
PyObject *lookup_cc_ptr_to_py_cls(const bt_component_class *bt_cc)
{
	if (!bt_cc_ptr_to_py_cls) {
		BT_LOGW("Cannot look up Python component class because hash table is NULL: "
			"comp-cls-addr=%p", bt_cc);
		return NULL;
	}

	return (PyObject *) g_hash_table_lookup(bt_cc_ptr_to_py_cls,
		(gconstpointer) bt_cc);
}

/* Library destructor */

__attribute__((destructor))
static
void native_comp_class_dtor(void) {
	/* Destroy component class association hash table */
	if (bt_cc_ptr_to_py_cls) {
		BT_LOGD_STR("Destroying native component class to Python component class hash table.");
		g_hash_table_destroy(bt_cc_ptr_to_py_cls);
		bt_cc_ptr_to_py_cls = NULL;
	}
}

static inline
int py_exc_to_status_clear(
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name, int active_log_level)
{
	int status;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
		status = __BT_FUNC_STATUS_OK;
		goto end;
	}

	if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_try_again_type)) {
		status = __BT_FUNC_STATUS_AGAIN;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_stop_type)) {
		status = __BT_FUNC_STATUS_END;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_unknown_object_type)) {
		status = __BT_FUNC_STATUS_UNKNOWN_OBJECT;
	} else {
		/*
		 * Unknown exception: convert to general error.
		 *
		 * Because we only want to fetch the log level when
		 * we actually get an exception, and not systematically
		 * when we call py_exc_to_status() (as py_exc_to_status()
		 * can return `__BT_FUNC_STATUS_OK`), we get it here
		 * depending on the actor's type.
		 */
		if (self_component) {
			active_log_level = get_self_component_log_level(
				self_component);
		} else if (self_message_iterator) {
			active_log_level = get_self_message_iterator_log_level(
				self_message_iterator);
		}

		BT_ASSERT(active_log_level != -1);
		log_exception_and_maybe_append_cause(BT_LOG_WARNING,
			active_log_level, true,
			self_component_class, self_component,
			self_message_iterator, module_name);

		if (PyErr_GivenExceptionMatches(exc,
				py_mod_bt2_exc_memory_error)) {
			status = __BT_FUNC_STATUS_MEMORY_ERROR;
		} else {
			status = __BT_FUNC_STATUS_ERROR;
		}
	}

end:
	PyErr_Clear();
	return status;
}

static
int py_exc_to_status_component_class_clear(
		bt_self_component_class *self_component_class,
		int active_log_level)
{
	return py_exc_to_status_clear(self_component_class, NULL, NULL, NULL,
		active_log_level);
}

static
int py_exc_to_status_component_clear(bt_self_component *self_component)
{
	return py_exc_to_status_clear(NULL, self_component, NULL, NULL, -1);
}

static
int py_exc_to_status_message_iterator_clear(
		bt_self_message_iterator *self_message_iterator)
{
	return py_exc_to_status_clear(NULL, NULL, self_message_iterator, NULL, -1);
}

static
bool bt_bt2_is_python_component_class(const bt_component_class *comp_cls)
{
	return bt_g_hash_table_contains(bt_cc_ptr_to_py_cls, comp_cls);
}

/* Component class proxy methods (delegate to the attached Python object) */

static
bt_component_class_initialize_method_status component_class_init(
		bt_self_component *self_component,
		void *self_component_v,
		swig_type_info *self_comp_cls_type_swig_type,
		const bt_value *params,
		void *init_method_data)
{
	const bt_component *component = bt_self_component_as_component(self_component);
	const bt_component_class *component_class = bt_component_borrow_class_const(component);
	bt_component_class_initialize_method_status status;
	PyObject *py_cls = NULL;
	PyObject *py_comp = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_comp_ptr = NULL;
	bt_logging_level log_level = get_self_component_log_level(
		self_component);

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
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", component_class);
		goto error;
	}

	/* Parameters pointer -> SWIG pointer Python object */
	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Failed to create a SWIG pointer object.");
		goto error;
	}

	py_comp_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_component_v),
		self_comp_cls_type_swig_type, 0);
	if (!py_comp_ptr) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Failed to create a SWIG pointer object.");
		goto error;
	}

	/*
	 * Do the equivalent of this:
	 *
	 *     py_comp = py_cls._bt_init_from_native(py_comp_ptr,
	 *         py_params_ptr, init_method_data ? init_method_data : Py_None)
	 *
	 * _UserComponentType._bt_init_from_native() calls the Python
	 * component object's __init__() function.
	 *
	 * We don't take any reference on `init_method_data` which, if
	 * not `NULL`, is assumed to be a `PyObject *`: the user's
	 * __init__() function will eventually take a reference if
	 * needed. If `init_method_data` is `NULL`, then we pass
	 * `Py_None` as the initialization's Python object.
	 */
	py_comp = PyObject_CallMethod(py_cls,
		"_bt_init_from_native", "(OOO)", py_comp_ptr, py_params_ptr,
		init_method_data ? init_method_data : Py_None);
	if (!py_comp) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_WARNING, log_level, self_component,
			"Failed to call Python class's _bt_init_from_native() method: "
			"py-cls-addr=%p", py_cls);
		status = py_exc_to_status_component_clear(self_component);
		goto end;
	}

	/*
	 * Our user Python component object is now fully created and
	 * initialized by the user. Since we just created it, this
	 * native component is its only (persistent) owner.
	 */
	bt_self_component_set_data(self_component, py_comp);
	py_comp = NULL;

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;

	goto end;

error:
	/* This error path is for non-Python errors only. */
	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;

end:
	BT_ASSERT(!PyErr_Occurred());
	Py_XDECREF(py_comp);
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_comp_ptr);
	return status;
}

static
bt_component_class_get_supported_mip_versions_method_status
component_class_get_supported_mip_versions(
		const bt_component_class *component_class,
		bt_self_component_class *self_component_class,
		const bt_value *params, void *init_method_data,
		bt_logging_level log_level,
		bt_integer_range_set_unsigned *supported_versions)
{
	uint64_t i;
	PyObject *py_cls = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_range_set_addr = NULL;
	bt_integer_range_set_unsigned *ret_range_set = NULL;
	bt_component_class_get_supported_mip_versions_method_status status;

	py_cls = lookup_cc_ptr_to_py_cls(component_class);
	if (!py_cls) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", component_class);
		goto error;
	}

	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Failed to create a SWIG pointer object.");
		goto error;
	}

	/*
	 * We don't take any reference on `init_method_data` which, if
	 * not `NULL`, is assumed to be a `PyObject *`: the user's
	 * _user_get_supported_mip_versions() function will eventually
	 * take a reference if needed. If `init_method_data` is `NULL`,
	 * then we pass `Py_None` as the initialization's Python object.
	 */
	py_range_set_addr = PyObject_CallMethod(py_cls,
		"_bt_get_supported_mip_versions_from_native", "(OOi)",
		py_params_ptr, init_method_data ? init_method_data : Py_None,
		(int) log_level);
	if (!py_range_set_addr) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING, log_level, BT_LOG_TAG,
			"Failed to call Python class's _bt_get_supported_mip_versions_from_native() method: "
			"py-cls-addr=%p", py_cls);
		status = py_exc_to_status_component_class_clear(self_component_class,
			log_level);
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a BT unsigned integer
	 * range set object (new reference).
	 */
	ret_range_set = PyLong_AsVoidPtr(py_range_set_addr);
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(ret_range_set);

	/* Copy returned ranges to input range set */
	for (i = 0; i < bt_integer_range_set_get_range_count(
			bt_integer_range_set_unsigned_as_range_set_const(ret_range_set));
			i++) {
		const bt_integer_range_unsigned *range =
			bt_integer_range_set_unsigned_borrow_range_by_index_const(
				ret_range_set, i);
		bt_integer_range_set_add_range_status add_range_status;

		add_range_status = bt_integer_range_set_unsigned_add_range(
			supported_versions,
			bt_integer_range_unsigned_get_lower(range),
			bt_integer_range_unsigned_get_upper(range));
		if (add_range_status) {
			BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
				"Failed to add range to supported MIP versions range set.");
			goto error;
		}
	}

	status = BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK;

	goto end;

error:
	/* This error path is for non-Python errors only. */
	status = BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR;

end:
	BT_ASSERT(!PyErr_Occurred());
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_range_set_addr);
	bt_integer_range_set_unsigned_put_ref(ret_range_set);
	return status;
}

static
bt_component_class_get_supported_mip_versions_method_status
component_class_source_get_supported_mip_versions(
		bt_self_component_class_source *self_component_class_source,
		const bt_value *params, void *init_method_data,
		bt_logging_level log_level,
		bt_integer_range_set_unsigned *supported_versions)
{
	const bt_component_class_source *component_class_source = bt_self_component_class_source_as_component_class_source(self_component_class_source);
	const bt_component_class *component_class = bt_component_class_source_as_component_class_const(component_class_source);
	bt_self_component_class *self_component_class = bt_self_component_class_source_as_self_component_class(self_component_class_source);

	return component_class_get_supported_mip_versions(
		component_class, self_component_class,
		params, init_method_data, log_level, supported_versions);
}

static
bt_component_class_get_supported_mip_versions_method_status
component_class_filter_get_supported_mip_versions(
		bt_self_component_class_filter *self_component_class_filter,
		const bt_value *params, void *init_method_data,
		bt_logging_level log_level,
		bt_integer_range_set_unsigned *supported_versions)
{
	const bt_component_class_filter *component_class_filter = bt_self_component_class_filter_as_component_class_filter(self_component_class_filter);
	const bt_component_class *component_class = bt_component_class_filter_as_component_class_const(component_class_filter);
	bt_self_component_class *self_component_class = bt_self_component_class_filter_as_self_component_class(self_component_class_filter);

	return component_class_get_supported_mip_versions(
		component_class, self_component_class,
		params, init_method_data, log_level, supported_versions);
}

static
bt_component_class_get_supported_mip_versions_method_status
component_class_sink_get_supported_mip_versions(
		bt_self_component_class_sink *self_component_class_sink,
		const bt_value *params, void *init_method_data,
		bt_logging_level log_level,
		bt_integer_range_set_unsigned *supported_versions)
{
	const bt_component_class_sink *component_class_sink = bt_self_component_class_sink_as_component_class_sink(self_component_class_sink);
	const bt_component_class *component_class = bt_component_class_sink_as_component_class_const(component_class_sink);
	bt_self_component_class *self_component_class = bt_self_component_class_sink_as_self_component_class(self_component_class_sink);

	return component_class_get_supported_mip_versions(
		component_class, self_component_class,
		params, init_method_data, log_level, supported_versions);
}

/*
 * Method of bt_component_class_source to initialize a bt_self_component_source
 * of that class.
 */

static
bt_component_class_initialize_method_status component_class_source_init(
		bt_self_component_source *self_component_source,
		bt_self_component_source_configuration *config,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);
	return component_class_init(
		self_component,
		self_component_source,
		SWIGTYPE_p_bt_self_component_source,
		params, init_method_data);
}

static
bt_component_class_initialize_method_status component_class_filter_init(
		bt_self_component_filter *self_component_filter,
		bt_self_component_filter_configuration *config,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	return component_class_init(
		self_component,
		self_component_filter,
		SWIGTYPE_p_bt_self_component_filter,
		params, init_method_data);
}

static
bt_component_class_initialize_method_status component_class_sink_init(
		bt_self_component_sink *self_component_sink,
		bt_self_component_sink_configuration *config,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	return component_class_init(
		self_component,
		self_component_sink,
		SWIGTYPE_p_bt_self_component_sink,
		params, init_method_data);
}

static
void component_class_finalize(bt_self_component *self_component)
{
	PyObject *py_comp = bt_self_component_get_data(self_component);
	PyObject *py_method_result;

	BT_ASSERT(py_comp);

	/* Call user's _user_finalize() method */
	py_method_result = PyObject_CallMethod(py_comp, "_user_finalize", NULL);
	if (!py_method_result) {
		bt_logging_level log_level = get_self_component_log_level(
			self_component);

		/*
		 * Ignore any exception raised by the _user_finalize() method
		 * because it won't change anything at this point: the component
		 * is being destroyed anyway.
		 */
		BT_COMP_LOG_CUR_LVL(BT_LOG_WARNING, log_level, self_component,
			"User component's _user_finalize() method raised an exception: ignoring:");
		logw_exception_clear(log_level);

		goto end;
	}

	BT_ASSERT(py_method_result == Py_None);

end:
	Py_XDECREF(py_method_result);
	Py_DECREF(py_comp);
}

/* Decref the Python object in the user data associated to `port`. */

static
void delete_port_user_data(bt_self_component_port *port)
{
	Py_DECREF(bt_self_component_port_get_data(port));
}

static
void delete_port_input_user_data(bt_self_component_port_input *port_input)
{
	bt_self_component_port *port =
		bt_self_component_port_input_as_self_component_port(port_input);

	delete_port_user_data(port);
}

static
void delete_port_output_user_data(bt_self_component_port_output *port_output)
{
	bt_self_component_port *port =
		bt_self_component_port_output_as_self_component_port(port_output);

	delete_port_user_data(port);
}

static
void component_class_source_finalize(bt_self_component_source *self_component_source)
{
	uint64_t i;
	bt_self_component *self_component;
	const bt_component_source *component_source;

	self_component = bt_self_component_source_as_self_component(
		self_component_source);
	component_source = bt_self_component_source_as_component_source(
		self_component_source);

	component_class_finalize(self_component);

	/*
	 * Free the user data Python object attached to the port.  The
	 * corresponding incref was done by the `void *` typemap in
	 * native_bt_port.i.
	 */
	for (i = 0; i < bt_component_source_get_output_port_count(component_source); i++) {
		bt_self_component_port_output *port_output;

		port_output = bt_self_component_source_borrow_output_port_by_index(
			self_component_source, i);

		delete_port_output_user_data(port_output);
	}
}

static
void component_class_filter_finalize(bt_self_component_filter *self_component_filter)
{
	uint64_t i;
	bt_self_component *self_component;
	const bt_component_filter *component_filter;

	self_component = bt_self_component_filter_as_self_component(
		self_component_filter);
	component_filter = bt_self_component_filter_as_component_filter(
		self_component_filter);

	component_class_finalize(self_component);

	/*
	 * Free the user data Python object attached to the port.  The
	 * corresponding incref was done by the `void *` typemap in
	 * native_bt_port.i.
	 */
	for (i = 0; i < bt_component_filter_get_input_port_count(component_filter); i++) {
		bt_self_component_port_input *port_input;

		port_input = bt_self_component_filter_borrow_input_port_by_index(
			self_component_filter, i);

		delete_port_input_user_data(port_input);
	}

	for (i = 0; i < bt_component_filter_get_output_port_count(component_filter); i++) {
		bt_self_component_port_output *port_output;

		port_output = bt_self_component_filter_borrow_output_port_by_index(
			self_component_filter, i);

		delete_port_output_user_data(port_output);
	}
}

static
void component_class_sink_finalize(bt_self_component_sink *self_component_sink)
{
	uint64_t i;
	bt_self_component *self_component;
	const bt_component_sink *component_sink;

	self_component = bt_self_component_sink_as_self_component(
		self_component_sink);
	component_sink = bt_self_component_sink_as_component_sink(
		self_component_sink);

	component_class_finalize(self_component);

	/*
	 * Free the user data Python object attached to the port.  The
	 * corresponding incref was done by the `void *` typemap in
	 * native_bt_port.i.
	 */
	for (i = 0; i < bt_component_sink_get_input_port_count(component_sink); i++) {
		bt_self_component_port_input *port_input;

		port_input = bt_self_component_sink_borrow_input_port_by_index(
			self_component_sink, i);

		delete_port_input_user_data(port_input);
	}
}

static
bt_message_iterator_class_can_seek_beginning_method_status
component_class_can_seek_beginning(
		bt_self_message_iterator *self_message_iterator, bt_bool *can_seek)
{
	PyObject *py_iter;
	PyObject *py_result = NULL;
	bt_message_iterator_class_can_seek_beginning_method_status status;
	py_iter = bt_self_message_iterator_get_data(self_message_iterator);

	BT_ASSERT(py_iter);

	py_result = PyObject_CallMethod(py_iter,
		"_bt_can_seek_beginning_from_native", NULL);
	if (!py_result) {
		status = py_exc_to_status_message_iterator_clear(self_message_iterator);
		goto end;
	}

	BT_ASSERT(PyBool_Check(py_result));
	*can_seek = PyObject_IsTrue(py_result);

	status = BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_result);

	return status;
}

static
bt_message_iterator_class_seek_beginning_method_status
component_class_seek_beginning(bt_self_message_iterator *self_message_iterator)
{
	PyObject *py_iter;
	PyObject *py_result;
	bt_message_iterator_class_seek_beginning_method_status status;

	py_iter = bt_self_message_iterator_get_data(self_message_iterator);
	BT_ASSERT(py_iter);

	py_result = PyObject_CallMethod(py_iter,
		"_bt_seek_beginning_from_native",
		NULL);
	if (!py_result) {
		status = py_exc_to_status_message_iterator_clear(self_message_iterator);
		goto end;
	}

	BT_ASSERT(py_result == Py_None);

	status = BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_result);

	return status;
}

static
bt_message_iterator_class_can_seek_ns_from_origin_method_status
component_class_can_seek_ns_from_origin(
		bt_self_message_iterator *self_message_iterator,
		int64_t ns_from_origin, bt_bool *can_seek)
{
	PyObject *py_iter;
	PyObject *py_result = NULL;
	bt_message_iterator_class_can_seek_ns_from_origin_method_status status;

	py_iter = bt_self_message_iterator_get_data(self_message_iterator);
	BT_ASSERT(py_iter);

	py_result = PyObject_CallMethod(py_iter,
		"_bt_can_seek_ns_from_origin_from_native", "L", ns_from_origin);
	if (!py_result) {
		status = py_exc_to_status_message_iterator_clear(self_message_iterator);
		goto end;
	}

	BT_ASSERT(PyBool_Check(py_result));
	*can_seek = PyObject_IsTrue(py_result);

	status = BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_result);

	return status;
}

static
bt_message_iterator_class_seek_ns_from_origin_method_status
component_class_seek_ns_from_origin(
		bt_self_message_iterator *self_message_iterator,
		int64_t ns_from_origin)
{
	PyObject *py_iter;
	PyObject *py_result;
	bt_message_iterator_class_seek_ns_from_origin_method_status status;

	py_iter = bt_self_message_iterator_get_data(self_message_iterator);
	BT_ASSERT(py_iter);

	py_result = PyObject_CallMethod(py_iter,
		"_bt_seek_ns_from_origin_from_native", "L", ns_from_origin);
	if (!py_result) {
		status = py_exc_to_status_message_iterator_clear(self_message_iterator);
		goto end;
	}


	BT_ASSERT(py_result == Py_None);

	status = BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_result);

	return status;
}

static
bt_component_class_port_connected_method_status component_class_port_connected(
		bt_self_component *self_component,
		void *self_component_port,
		swig_type_info *self_component_port_swig_type,
		bt_port_type self_component_port_type,
		const void *other_port,
		swig_type_info *other_port_swig_type)
{
	bt_component_class_port_connected_method_status status;
	PyObject *py_comp = NULL;
	PyObject *py_self_port_ptr = NULL;
	PyObject *py_other_port_ptr = NULL;
	PyObject *py_method_result = NULL;
	bt_logging_level log_level = get_self_component_log_level(
		self_component);

	py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT(py_comp);
	py_self_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_component_port),
		self_component_port_swig_type, 0);
	if (!py_self_port_ptr) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Failed to create a SWIG pointer object.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	py_other_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(other_port),
		other_port_swig_type, 0);
	if (!py_other_port_ptr) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Failed to create a SWIG pointer object.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_bt_port_connected_from_native", "(OiO)", py_self_port_ptr,
		self_component_port_type, py_other_port_ptr);
	if (!py_method_result) {
		status = py_exc_to_status_component_clear(self_component);
		goto end;
	}

	BT_ASSERT(py_method_result == Py_None);

	status = BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_self_port_ptr);
	Py_XDECREF(py_other_port_ptr);
	Py_XDECREF(py_method_result);

	return status;
}

static
bt_component_class_port_connected_method_status
component_class_source_output_port_connected(
		bt_self_component_source *self_component_source,
		bt_self_component_port_output *self_component_port_output,
		const bt_port_input *other_port_input)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);

	return component_class_port_connected(
		self_component,
		self_component_port_output,
		SWIGTYPE_p_bt_self_component_port_output,
		BT_PORT_TYPE_OUTPUT,
		other_port_input,
		SWIGTYPE_p_bt_port_input);
}

static
bt_component_class_port_connected_method_status
component_class_filter_input_port_connected(
		bt_self_component_filter *self_component_filter,
		bt_self_component_port_input *self_component_port_input,
		const bt_port_output *other_port_output)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);

	return component_class_port_connected(
		self_component,
		self_component_port_input,
		SWIGTYPE_p_bt_self_component_port_input,
		BT_PORT_TYPE_INPUT,
		other_port_output,
		SWIGTYPE_p_bt_port_output);
}

static
bt_component_class_port_connected_method_status
component_class_filter_output_port_connected(
		bt_self_component_filter *self_component_filter,
		bt_self_component_port_output *self_component_port_output,
		const bt_port_input *other_port_input)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);

	return component_class_port_connected(
		self_component,
		self_component_port_output,
		SWIGTYPE_p_bt_self_component_port_output,
		BT_PORT_TYPE_OUTPUT,
		other_port_input,
		SWIGTYPE_p_bt_port_input);
}

static
bt_component_class_port_connected_method_status
component_class_sink_input_port_connected(
		bt_self_component_sink *self_component_sink,
		bt_self_component_port_input *self_component_port_input,
		const bt_port_output *other_port_output)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);

	return component_class_port_connected(
		self_component,
		self_component_port_input,
		SWIGTYPE_p_bt_self_component_port_input,
		BT_PORT_TYPE_INPUT,
		other_port_output,
		SWIGTYPE_p_bt_port_output);
}

static
bt_component_class_sink_graph_is_configured_method_status
component_class_sink_graph_is_configured(
		bt_self_component_sink *self_component_sink)
{
	PyObject *py_comp = NULL;
	PyObject *py_method_result = NULL;
	bt_component_class_sink_graph_is_configured_method_status status;
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);

	py_comp = bt_self_component_get_data(self_component);

	py_method_result = PyObject_CallMethod(py_comp,
		"_bt_graph_is_configured_from_native", NULL);
	if (!py_method_result) {
		status = py_exc_to_status_component_clear(self_component);
		goto end;
	}

	BT_ASSERT(py_method_result == Py_None);

	status = BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK;;

end:
	Py_XDECREF(py_method_result);
	return status;
}

static
bt_component_class_query_method_status component_class_query(
		const bt_component_class *component_class,
		bt_self_component_class *self_component_class,
		bt_private_query_executor *priv_query_executor,
		const char *object, const bt_value *params, void *method_data,
		const bt_value **result)
{
	PyObject *py_cls = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_priv_query_exec_ptr = NULL;
	PyObject *py_query_func = NULL;
	PyObject *py_object = NULL;
	PyObject *py_results_addr = NULL;
	bt_component_class_query_method_status status = __BT_FUNC_STATUS_OK;
	const bt_query_executor *query_exec =
		bt_private_query_executor_as_query_executor_const(
			priv_query_executor);
	bt_logging_level log_level =
		bt_query_executor_get_logging_level(query_exec);

	/*
	 * If there's any `method_data`, assume this component class is
	 * getting queried from Python, so that `method_data` is a
	 * Python object to pass to the user's _user_query() method.
	 */
	BT_ASSERT(!method_data ||
			bt_bt2_is_python_component_class(component_class));

	py_cls = lookup_cc_ptr_to_py_cls(component_class);
	if (!py_cls) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", component_class);
		goto error;
	}

	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Failed to create a SWIG pointer object.");
		goto error;
	}

	py_priv_query_exec_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(priv_query_executor),
		SWIGTYPE_p_bt_private_query_executor, 0);
	if (!py_priv_query_exec_ptr) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Failed to create a SWIG pointer object.");
		goto error;
	}

	py_object = SWIG_FromCharPtr(object);
	if (!py_object) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Failed to create a Python string.");
		goto error;
	}

	/*
	 * We don't take any reference on `method_data` which, if not
	 * `NULL`, is assumed to be a `PyObject *`: the user's
	 * _user_query() function will eventually take a reference if
	 * needed. If `method_data` is `NULL`, then we pass `Py_None` as
	 * the initialization's Python object.
	 */
	py_results_addr = PyObject_CallMethod(py_cls,
		"_bt_query_from_native", "(OOOO)", py_priv_query_exec_ptr,
		py_object, py_params_ptr,
		method_data ? method_data : Py_None);
	if (!py_results_addr) {
		status = py_exc_to_status_component_class_clear(self_component_class,
			log_level);
		if (status < 0) {
			static const char *fmt =
				"Failed to call Python class's _bt_query_from_native() method: py-cls-addr=%p";
			BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING, log_level, BT_LOG_TAG,
				fmt, py_cls);
			BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(
				self_component_class, fmt, py_cls);
		}
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a BT value object (new
	 * reference).
	 */
	*result = PyLong_AsVoidPtr(py_results_addr);
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(*result);
	goto end;

error:
	PyErr_Clear();
	status = __BT_FUNC_STATUS_ERROR;

end:
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_priv_query_exec_ptr);
	Py_XDECREF(py_query_func);
	Py_XDECREF(py_object);
	Py_XDECREF(py_results_addr);
	return status;
}

static
bt_component_class_query_method_status component_class_source_query(
		bt_self_component_class_source *self_component_class_source,
		bt_private_query_executor *priv_query_executor,
		const char *object, const bt_value *params, void *method_data,
		const bt_value **result)
{
	const bt_component_class_source *component_class_source = bt_self_component_class_source_as_component_class_source(self_component_class_source);
	const bt_component_class *component_class = bt_component_class_source_as_component_class_const(component_class_source);
	bt_self_component_class *self_component_class = bt_self_component_class_source_as_self_component_class(self_component_class_source);

	return component_class_query(component_class, self_component_class,
		priv_query_executor, object, params, method_data, result);
}

static
bt_component_class_query_method_status component_class_filter_query(
		bt_self_component_class_filter *self_component_class_filter,
		bt_private_query_executor *priv_query_executor,
		const char *object, const bt_value *params, void *method_data,
		const bt_value **result)
{
	const bt_component_class_filter *component_class_filter = bt_self_component_class_filter_as_component_class_filter(self_component_class_filter);
	const bt_component_class *component_class = bt_component_class_filter_as_component_class_const(component_class_filter);
	bt_self_component_class *self_component_class = bt_self_component_class_filter_as_self_component_class(self_component_class_filter);

	return component_class_query(component_class, self_component_class,
		priv_query_executor, object, params, method_data, result);
}

static
bt_component_class_query_method_status component_class_sink_query(
		bt_self_component_class_sink *self_component_class_sink,
		bt_private_query_executor *priv_query_executor,
		const char *object, const bt_value *params, void *method_data,
		const bt_value **result)
{
	const bt_component_class_sink *component_class_sink = bt_self_component_class_sink_as_component_class_sink(self_component_class_sink);
	const bt_component_class *component_class = bt_component_class_sink_as_component_class_const(component_class_sink);
	bt_self_component_class *self_component_class = bt_self_component_class_sink_as_self_component_class(self_component_class_sink);

	return component_class_query(component_class, self_component_class,
		priv_query_executor, object, params, method_data, result);
}

static
bt_message_iterator_class_initialize_method_status
component_class_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_component_port_output)
{
	bt_message_iterator_class_initialize_method_status status = __BT_FUNC_STATUS_OK;
	PyObject *py_comp_cls = NULL;
	PyObject *py_iter_cls = NULL;
	PyObject *py_iter_ptr = NULL;
	PyObject *py_config_ptr = NULL;
	PyObject *py_component_port_output_ptr = NULL;
	PyObject *py_init_method_result = NULL;
	PyObject *py_iter = NULL;
	PyObject *py_comp;
	bt_self_component *self_component =
		bt_self_message_iterator_borrow_component(
			self_message_iterator);
	bt_logging_level log_level = get_self_component_log_level(
		self_component);

	py_comp = bt_self_component_get_data(self_component);

	/* Find user's Python message iterator class */
	py_comp_cls = PyObject_GetAttrString(py_comp, "__class__");
	if (!py_comp_cls) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Cannot get Python object's `__class__` attribute.");
		goto python_error;
	}

	py_iter_cls = PyObject_GetAttrString(py_comp_cls, "_iter_cls");
	if (!py_iter_cls) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Cannot get Python class's `_iter_cls` attribute.");
		goto python_error;
	}

	py_iter_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_message_iterator),
		SWIGTYPE_p_bt_self_message_iterator, 0);
	if (!py_iter_ptr) {
		const char *err = "Failed to create a SWIG pointer object.";

		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"%s", err);
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(
			self_message_iterator, err);
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
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"Failed to call Python class's __new__() method: "
			"py-cls-addr=%p", py_iter_cls);
		goto python_error;
	}

	/*
	 * Initialize object:
	 *
	 *     py_iter.__init__(config, self_output_port)
	 *
	 * through the _init_from_native helper static method.
	 *
	 * At this point, py_iter._ptr is set, so this initialization
	 * function has access to self._component (which gives it the
	 * user Python component object from which the iterator was
	 * created).
	 */
	py_config_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(config),
		SWIGTYPE_p_bt_self_message_iterator_configuration, 0);
	if (!py_config_ptr) {
		const char *err = "Failed to create a SWIG pointer object";

		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"%s", err);
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(
			self_message_iterator, err);
		goto error;
	}

	py_component_port_output_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(self_component_port_output),
		SWIGTYPE_p_bt_self_component_port_output, 0);
	if (!py_component_port_output_ptr) {
		const char *err = "Failed to create a SWIG pointer object.";

		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"%s", err);
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(
			self_message_iterator, err);
		goto error;
	}

	py_init_method_result = PyObject_CallMethod(py_iter,
		"_bt_init_from_native", "OO", py_config_ptr,
		py_component_port_output_ptr);
	if (!py_init_method_result) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_component,
			"User's __init__() method failed:");
		goto python_error;
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

python_error:
	/* Handling of errors that cause a Python exception to be set. */
	status = py_exc_to_status_message_iterator_clear(self_message_iterator);
	goto end;

error:
	/* Handling of errors that don't cause a Python exception to be set. */
	status = __BT_FUNC_STATUS_ERROR;

end:
	BT_ASSERT(!PyErr_Occurred());

	Py_XDECREF(py_comp_cls);
	Py_XDECREF(py_iter_cls);
	Py_XDECREF(py_iter_ptr);
	Py_XDECREF(py_component_port_output_ptr);
	Py_XDECREF(py_init_method_result);
	Py_XDECREF(py_iter);
	return status;
}

static
void component_class_message_iterator_finalize(
		bt_self_message_iterator *message_iterator)
{
	PyObject *py_message_iter = bt_self_message_iterator_get_data(
		message_iterator);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_message_iter);

	/* Call user's _user_finalize() method */
	py_method_result = PyObject_CallMethod(py_message_iter,
		"_user_finalize", NULL);
	if (!py_method_result) {
		bt_self_component *self_comp =
			bt_self_message_iterator_borrow_component(
				message_iterator);
		bt_logging_level log_level = get_self_component_log_level(
			self_comp);

		/*
		 * Ignore any exception raised by the _user_finalize() method
		 * because it won't change anything at this point: the component
		 * is being destroyed anyway.
		 */
		BT_COMP_LOG_CUR_LVL(BT_LOG_WARNING, log_level, self_comp,
			"User's _user_finalize() method raised an exception: ignoring:");
		logw_exception_clear(get_self_message_iterator_log_level(
			message_iterator));
	}

	Py_XDECREF(py_method_result);
	Py_DECREF(py_message_iter);
}

/* Valid for both sources and filters. */

static
bt_message_iterator_class_next_method_status
component_class_message_iterator_next(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status;
	PyObject *py_message_iter = bt_self_message_iterator_get_data(message_iterator);
	PyObject *py_method_result = NULL;

	BT_ASSERT_DBG(py_message_iter);
	py_method_result = PyObject_CallMethod(py_message_iter,
		"_bt_next_from_native", NULL);
	if (!py_method_result) {
		status = py_exc_to_status_message_iterator_clear(message_iterator);
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a native message
	 * object (which is now ours).
	 */
	msgs[0] = PyLong_AsVoidPtr(py_method_result);
	*count = 1;

	/* Overflow errors should never happen. */
	BT_ASSERT_DBG(!PyErr_Occurred());

	status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_method_result);
	return status;
}

static
bt_component_class_sink_consume_method_status
component_class_sink_consume(bt_self_component_sink *self_component_sink)
{
   	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	PyObject *py_comp = bt_self_component_get_data(self_component);
	PyObject *py_method_result = NULL;
	bt_component_class_sink_consume_method_status status;

	BT_ASSERT_DBG(py_comp);

	py_method_result = PyObject_CallMethod(py_comp,
		"_user_consume", NULL);
	if (!py_method_result) {
		status = py_exc_to_status_component_clear(self_component);
		goto end;
	}

	status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;

end:
	Py_XDECREF(py_method_result);
	return status;
}

static
int component_class_set_help_and_desc(
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
bt_message_iterator_class *create_message_iterator_class(void)
{
	bt_message_iterator_class *message_iterator_class;
	bt_message_iterator_class_set_method_status ret;

	message_iterator_class = bt_message_iterator_class_create(
		component_class_message_iterator_next);
	if (!message_iterator_class) {
		BT_LOGE_STR("Cannot create message iterator class.");
		goto end;
	}

	ret = bt_message_iterator_class_set_seek_beginning_methods(
		message_iterator_class, component_class_seek_beginning,
		component_class_can_seek_beginning);
	BT_ASSERT(ret == 0);
	ret = bt_message_iterator_class_set_seek_ns_from_origin_methods(
		message_iterator_class, component_class_seek_ns_from_origin,
		component_class_can_seek_ns_from_origin);
	BT_ASSERT(ret == 0);
	ret = bt_message_iterator_class_set_initialize_method(
		message_iterator_class, component_class_message_iterator_init);
	BT_ASSERT(ret == 0);
	ret = bt_message_iterator_class_set_finalize_method(
		message_iterator_class, component_class_message_iterator_finalize);
	BT_ASSERT(ret == 0);

end:
	return message_iterator_class;
}

static
bt_component_class_source *bt_bt2_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class_source *component_class_source = NULL;
	bt_message_iterator_class *message_iterator_class;
	bt_component_class *component_class;
	int ret;

	BT_ASSERT(py_cls);

	message_iterator_class = create_message_iterator_class();
	if (!message_iterator_class) {
		goto end;
	}

	component_class_source = bt_component_class_source_create(name,
		message_iterator_class);
	if (!component_class_source) {
		BT_LOGE_STR("Cannot create source component class.");
		goto end;
	}

	component_class = bt_component_class_source_as_component_class(component_class_source);

	if (component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_source_set_initialize_method(component_class_source, component_class_source_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_finalize_method(component_class_source, component_class_source_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_output_port_connected_method(component_class_source,
		component_class_source_output_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_query_method(component_class_source, component_class_source_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_get_supported_mip_versions_method(component_class_source, component_class_source_get_supported_mip_versions);
	BT_ASSERT(ret == 0);
	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	bt_message_iterator_class_put_ref(message_iterator_class);
	return component_class_source;
}

static
bt_component_class_filter *bt_bt2_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class_filter *component_class_filter = NULL;
	bt_message_iterator_class *message_iterator_class;
	bt_component_class *component_class;
	int ret;

	BT_ASSERT(py_cls);

	message_iterator_class = create_message_iterator_class();
	if (!message_iterator_class) {
		goto end;
	}

	component_class_filter = bt_component_class_filter_create(name,
		message_iterator_class);
	if (!component_class_filter) {
		BT_LOGE_STR("Cannot create filter component class.");
		goto end;
	}

	component_class = bt_component_class_filter_as_component_class(component_class_filter);

	if (component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_filter_set_initialize_method(component_class_filter, component_class_filter_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_finalize_method (component_class_filter, component_class_filter_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_input_port_connected_method(component_class_filter,
		component_class_filter_input_port_connected);
   	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_output_port_connected_method(component_class_filter,
		component_class_filter_output_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_query_method(component_class_filter, component_class_filter_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_get_supported_mip_versions_method(component_class_filter, component_class_filter_get_supported_mip_versions);
	BT_ASSERT(ret == 0);
	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	bt_message_iterator_class_put_ref(message_iterator_class);
	return component_class_filter;
}

static
bt_component_class_sink *bt_bt2_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class_sink *component_class_sink;
	bt_component_class *component_class;
	int ret;

	BT_ASSERT(py_cls);
	component_class_sink = bt_component_class_sink_create(name, component_class_sink_consume);

	if (!component_class_sink) {
		BT_LOGE_STR("Cannot create sink component class.");
		goto end;
	}

	component_class = bt_component_class_sink_as_component_class(component_class_sink);

	if (component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_sink_set_initialize_method(component_class_sink, component_class_sink_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_finalize_method(component_class_sink, component_class_sink_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_input_port_connected_method(component_class_sink,
		component_class_sink_input_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_graph_is_configured_method(component_class_sink,
		component_class_sink_graph_is_configured);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_query_method(component_class_sink, component_class_sink_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_sink_set_get_supported_mip_versions_method(component_class_sink, component_class_sink_get_supported_mip_versions);
	BT_ASSERT(ret == 0);
	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	return component_class_sink;
}
