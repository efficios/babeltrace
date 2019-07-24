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

%include <babeltrace2/graph/component-class.h>
%include <babeltrace2/graph/component-class-const.h>
%include <babeltrace2/graph/component-class-source-const.h>
%include <babeltrace2/graph/component-class-source.h>
%include <babeltrace2/graph/component-class-filter-const.h>
%include <babeltrace2/graph/component-class-filter.h>
%include <babeltrace2/graph/component-class-sink-const.h>
%include <babeltrace2/graph/component-class-sink.h>
%include <babeltrace2/graph/self-component-class-source.h>
%include <babeltrace2/graph/self-component-class-filter.h>
%include <babeltrace2/graph/self-component-class-sink.h>

%{
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


/*
 * Useful Python objects.
 */

static PyObject *py_mod_bt2 = NULL;
static PyObject *py_mod_bt2_exc_error_type = NULL;
static PyObject *py_mod_bt2_exc_memory_error = NULL;
static PyObject *py_mod_bt2_exc_try_again_type = NULL;
static PyObject *py_mod_bt2_exc_stop_type = NULL;
static PyObject *py_mod_bt2_exc_unknown_object_type = NULL;

static
void bt_bt2_cc_init_from_bt2(void)
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
		PyObject_GetAttrString(py_mod_bt2, "_Error");
	BT_ASSERT(py_mod_bt2_exc_error_type);
	py_mod_bt2_exc_memory_error =
		PyObject_GetAttrString(py_mod_bt2, "_MemoryError");
	BT_ASSERT(py_mod_bt2_exc_memory_error);
	py_mod_bt2_exc_try_again_type =
		PyObject_GetAttrString(py_mod_bt2, "TryAgain");
	BT_ASSERT(py_mod_bt2_exc_try_again_type);
	py_mod_bt2_exc_stop_type =
		PyObject_GetAttrString(py_mod_bt2, "Stop");
	BT_ASSERT(py_mod_bt2_exc_stop_type);
	py_mod_bt2_exc_unknown_object_type =
		PyObject_GetAttrString(py_mod_bt2, "UnknownObject");
	BT_ASSERT(py_mod_bt2_exc_unknown_object_type);
}

static
void bt_bt2_cc_exit_handler(void)
{
	/*
	 * This is an exit handler (set by the bt2 package).
	 *
	 * We only give back the references that we took in
	 * bt_bt2_cc_init_from_bt2() here. The global variables continue
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
	Py_XDECREF(py_mod_bt2_exc_unknown_object_type);
}


/* Library destructor */

__attribute__((destructor))
static
void native_comp_class_dtor(void) {
	/* Destroy component class association hash table */
	if (bt_cc_ptr_to_py_cls) {
		BT_LOGD_STR("Destroying native component class to Python component class hash table.");
		g_hash_table_destroy(bt_cc_ptr_to_py_cls);
	}
}

static
void restore_current_thread_error_and_append_exception_chain_recursive(
		PyObject *py_exc_value,
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	PyObject *py_exc_cause_value;
	PyObject *py_exc_type = NULL;
	PyObject *py_exc_tb = NULL;
	GString *gstr = NULL;

	/* If this exception has a cause, handle that one first. */
	py_exc_cause_value = PyException_GetCause(py_exc_value);
	if (py_exc_cause_value) {
		restore_current_thread_error_and_append_exception_chain_recursive(
			py_exc_cause_value, self_component_class,
			self_component, self_message_iterator, module_name);
	}

	/*
	 * If the raised exception is a bt2._Error, restore the wrapped error.
	 */
	if (PyErr_GivenExceptionMatches(py_exc_value, py_mod_bt2_exc_error_type)) {
		PyObject *py_error_swig_ptr;
		const bt_error *error;
		int ret;

		/*
		 * We never raise a bt2._Error with a cause: it should be the
		 * end of the chain.
		 */
		BT_ASSERT(!py_exc_cause_value);

		/*
		 * We steal the error object from the exception, to move
		 * it back as the current thread's error.
		 */
		py_error_swig_ptr = PyObject_GetAttrString(py_exc_value, "_ptr");
		BT_ASSERT(py_error_swig_ptr);

		ret = PyObject_SetAttrString(py_exc_value, "_ptr", Py_None);
		BT_ASSERT(ret == 0);

		ret = SWIG_ConvertPtr(py_error_swig_ptr, (void **) &error,
			SWIGTYPE_p_bt_error, 0);
		BT_ASSERT(ret == 0);

		BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(error);

		Py_DECREF(py_error_swig_ptr);
	}

	py_exc_type = PyObject_Type(py_exc_value);
	py_exc_tb = PyException_GetTraceback(py_exc_value);

	gstr = bt_py_common_format_exception(py_exc_type, py_exc_value,
			py_exc_tb, BT_LOG_OUTPUT_LEVEL, false);
	if (!gstr) {
		/* bt_py_common_format_exception has already warned. */
		goto end;
	}

	if (self_component_class) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(
			self_component_class, "%s", gstr->str);
	} else if (self_component) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT(
			self_component, "%s", gstr->str);
	} else if (self_message_iterator) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(
			self_message_iterator, "%s", gstr->str);
	} else {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
			module_name, "%s", gstr->str);
	}

end:
	if (gstr) {
		g_string_free(gstr, TRUE);
	}

	Py_XDECREF(py_exc_cause_value);
	Py_XDECREF(py_exc_type);
	Py_XDECREF(py_exc_tb);
}

/*
 * If you have the following code:
 *
 * try:
 *     try:
 *         something_that_raises_bt2_error()
 *     except bt2._Error as e1:
 *         raise ValueError from e1
 * except ValueError as e2:
 *     raise TypeError from e2
 *
 * We will have the following exception chain:
 *
 *     TypeError -> ValueError -> bt2._Error
 *
 * Where the TypeError is the current exception (obtained from PyErr_Fetch).
 *
 * The bt2._Error contains a `struct bt_error *` that used to be the current
 * thread's error, at the moment the exception was raised.
 *
 * This function gets to the bt2._Error and restores the wrapped
 * `struct bt_error *` as the current thread's error.
 *
 * Then, for each exception in the chain, starting with the oldest one, it adds
 * an error cause to the current thread's error.
 */
static
void restore_bt_error_and_append_current_exception_chain(
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	BT_ASSERT(PyErr_Occurred());

	/* Used to access and restore the current exception. */
	PyObject *py_exc_type;
	PyObject *py_exc_value;
	PyObject *py_exc_tb;

	/* Fetch and normalize the Python exception. */
	PyErr_Fetch(&py_exc_type, &py_exc_value, &py_exc_tb);
	PyErr_NormalizeException(&py_exc_type, &py_exc_value, &py_exc_tb);
	BT_ASSERT(py_exc_type);
	BT_ASSERT(py_exc_value);
	BT_ASSERT(py_exc_tb);

	/*
	 * Set the exception's traceback so it's possible to get it using
	 * PyException_GetTraceback in
	 * restore_current_thread_error_and_append_exception_chain_recursive.
	 */
	PyException_SetTraceback(py_exc_value, py_exc_tb);

	restore_current_thread_error_and_append_exception_chain_recursive(py_exc_value,
		self_component_class, self_component, self_message_iterator,
		module_name);

	PyErr_Restore(py_exc_type, py_exc_value, py_exc_tb);
}

static inline
void log_exception_and_maybe_append_error(int log_level,
		bool append_error,
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	GString *gstr;

	BT_ASSERT(PyErr_Occurred());
	gstr = bt_py_common_format_current_exception(BT_LOG_OUTPUT_LEVEL);
	if (!gstr) {
		/* bt_py_common_format_current_exception() logs errors */
		goto end;
	}

	BT_LOG_WRITE(log_level, BT_LOG_TAG, "%s", gstr->str);

	if (append_error) {
		restore_bt_error_and_append_current_exception_chain(
			self_component_class, self_component,
			self_message_iterator, module_name);

	}

end:
	if (gstr) {
		g_string_free(gstr, TRUE);
	}
}

static inline
void loge_exception(const char *module_name)
{
	log_exception_and_maybe_append_error(BT_LOG_ERROR, true, NULL, NULL,
		NULL, module_name);
}

static
void loge_exception_message_iterator(
		bt_self_message_iterator *self_message_iterator)
{
	log_exception_and_maybe_append_error(BT_LOG_ERROR, true, NULL, NULL,
		self_message_iterator, NULL);
}

static inline
void logw_exception(void)
{
	log_exception_and_maybe_append_error(BT_LOG_WARNING, false, NULL, NULL,
		NULL, NULL);
}

static inline
int py_exc_to_status(bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	int status = __BT_FUNC_STATUS_OK;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
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
		/* Unknown exception: convert to general error */
		log_exception_and_maybe_append_error(BT_LOG_WARNING, true,
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
int py_exc_to_status_component_class(bt_self_component_class *self_component_class)
{
	return py_exc_to_status(self_component_class, NULL, NULL, NULL);
}

static
int py_exc_to_status_component(bt_self_component *self_component)
{
	return py_exc_to_status(NULL, self_component, NULL, NULL);
}

static
int py_exc_to_status_message_iterator(
		bt_self_message_iterator *self_message_iterator)
{
	return py_exc_to_status(NULL, NULL, self_message_iterator, NULL);
}

/* Component class proxy methods (delegate to the attached Python object) */

static
bt_component_class_init_method_status component_class_init(
		bt_self_component *self_component,
		void *self_component_v,
		swig_type_info *self_comp_cls_type_swig_type,
		const bt_value *params,
		void *init_method_data)
{
	const bt_component *component = bt_self_component_as_component(self_component);
	const bt_component_class *component_class = bt_component_borrow_class_const(component);
	bt_component_class_init_method_status status = __BT_FUNC_STATUS_OK;
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
	 *     py_comp = py_cls._bt_init_from_native(py_comp_ptr, py_params_ptr)
	 *
	 * _UserComponentType._bt_init_from_native() calls the Python
	 * component object's __init__() function.
	 */
	py_comp = PyObject_CallMethod(py_cls,
		"_bt_init_from_native", "(OO)", py_comp_ptr, py_params_ptr);
	if (!py_comp) {
		BT_LOGW("Failed to call Python class's _bt_init_from_native() method: "
			"py-cls-addr=%p", py_cls);
		status = py_exc_to_status_component(self_component);
		goto end;
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
	status = __BT_FUNC_STATUS_ERROR;

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

static
bt_component_class_init_method_status component_class_source_init(
		bt_self_component_source *self_component_source,
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
bt_component_class_init_method_status component_class_filter_init(
		bt_self_component_filter *self_component_filter,
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
bt_component_class_init_method_status component_class_sink_init(
		bt_self_component_sink *self_component_sink,
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
	BT_ASSERT(py_comp);

	/* Call user's _user_finalize() method */
	PyObject *py_method_result = PyObject_CallMethod(py_comp,
		"_user_finalize", NULL);

	if (PyErr_Occurred()) {
		BT_LOGW("User component's _user_finalize() method raised an exception: ignoring:");
		logw_exception();
	}

	/*
	 * Ignore any exception raised by the _user_finalize() method
	 * because it won't change anything at this point: the component
	 * is being destroyed anyway.
	 */
	PyErr_Clear();
	Py_XDECREF(py_method_result);
	Py_DECREF(py_comp);
}

static
void component_class_source_finalize(bt_self_component_source *self_component_source)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);
	component_class_finalize(self_component);
}

static
void component_class_filter_finalize(bt_self_component_filter *self_component_filter)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);
	component_class_finalize(self_component);
}

static
void component_class_sink_finalize(bt_self_component_sink *self_component_sink)
{
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);
	component_class_finalize(self_component);
}

static
bt_bool component_class_can_seek_beginning(
		bt_self_message_iterator *self_message_iterator)
{
	PyObject *py_iter;
	PyObject *py_result = NULL;
	bt_bool can_seek_beginning = false;

	py_iter = bt_self_message_iterator_get_data(self_message_iterator);
	BT_ASSERT(py_iter);

	py_result = PyObject_GetAttrString(py_iter, "_bt_can_seek_beginning_from_native");

	BT_ASSERT(!py_result || PyBool_Check(py_result));

	if (py_result) {
		can_seek_beginning = PyObject_IsTrue(py_result);
	} else {
		/*
		 * Once can_seek_beginning can report errors, convert the
		 * exception to a status.  For now, log and return false;
		 */
		loge_exception_message_iterator(self_message_iterator);
		PyErr_Clear();
	}

	Py_XDECREF(py_result);

	return can_seek_beginning;
}

static
bt_component_class_message_iterator_seek_beginning_method_status
component_class_seek_beginning(bt_self_message_iterator *self_message_iterator)
{
	PyObject *py_iter;
	PyObject *py_result;
	bt_component_class_message_iterator_seek_beginning_method_status status;

	py_iter = bt_self_message_iterator_get_data(self_message_iterator);
	BT_ASSERT(py_iter);
	py_result = PyObject_CallMethod(py_iter, "_bt_seek_beginning_from_native",
		NULL);
	BT_ASSERT(!py_result || py_result == Py_None);
	status = py_exc_to_status_message_iterator(self_message_iterator);
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

	py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT(py_comp);
	py_self_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_component_port),
		self_component_port_swig_type, 0);
	if (!py_self_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	py_other_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(other_port),
		other_port_swig_type, 0);
	if (!py_other_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		status = __BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_bt_port_connected_from_native", "(OiO)", py_self_port_ptr,
		self_component_port_type, py_other_port_ptr);
	BT_ASSERT(!py_method_result || py_method_result == Py_None);
	status = py_exc_to_status_component(self_component);

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
	bt_component_class_sink_graph_is_configured_method_status status = __BT_FUNC_STATUS_OK;
	bt_self_component *self_component = bt_self_component_sink_as_self_component(self_component_sink);

	py_comp = bt_self_component_get_data(self_component);
	py_method_result = PyObject_CallMethod(py_comp,
		"_bt_graph_is_configured_from_native", NULL);
	BT_ASSERT(!py_method_result || py_method_result == Py_None);
	status = py_exc_to_status_component(self_component);
	Py_XDECREF(py_method_result);
	return status;
}

static
bt_component_class_query_method_status component_class_query(
		const bt_component_class *component_class,
		bt_self_component_class *self_component_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		bt_logging_level log_level,
		const bt_value **result)
{
	PyObject *py_cls = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_query_exec_ptr = NULL;
	PyObject *py_query_func = NULL;
	PyObject *py_object = NULL;
	PyObject *py_results_addr = NULL;
	bt_component_class_query_method_status status = __BT_FUNC_STATUS_OK;

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
		"_bt_query_from_native", "(OOOi)", py_query_exec_ptr,
		py_object, py_params_ptr, (int) log_level);
	if (!py_results_addr) {
		BT_LOGW("Failed to call Python class's _bt_query_from_native() method: "
			"py-cls-addr=%p", py_cls);
		status = py_exc_to_status_component_class(self_component_class);
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
	Py_XDECREF(py_query_exec_ptr);
	Py_XDECREF(py_query_func);
	Py_XDECREF(py_object);
	Py_XDECREF(py_results_addr);
	return status;
}

static
bt_component_class_query_method_status component_class_source_query(
		bt_self_component_class_source *self_component_class_source,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		bt_logging_level log_level,
		const bt_value **result)
{
	const bt_component_class_source *component_class_source = bt_self_component_class_source_as_component_class_source(self_component_class_source);
	const bt_component_class *component_class = bt_component_class_source_as_component_class_const(component_class_source);
	bt_self_component_class *self_component_class = bt_self_component_class_source_as_self_component_class(self_component_class_source);

	return component_class_query(component_class, self_component_class, query_executor, object, params, log_level, result);
}

static
bt_component_class_query_method_status component_class_filter_query(
		bt_self_component_class_filter *self_component_class_filter,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		bt_logging_level log_level,
		const bt_value **result)
{
	const bt_component_class_filter *component_class_filter = bt_self_component_class_filter_as_component_class_filter(self_component_class_filter);
	const bt_component_class *component_class = bt_component_class_filter_as_component_class_const(component_class_filter);
	bt_self_component_class *self_component_class = bt_self_component_class_filter_as_self_component_class(self_component_class_filter);

	return component_class_query(component_class, self_component_class, query_executor, object, params, log_level, result);
}

static
bt_component_class_query_method_status component_class_sink_query(
		bt_self_component_class_sink *self_component_class_sink,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		bt_logging_level log_level,
		const bt_value **result)
{
	const bt_component_class_sink *component_class_sink = bt_self_component_class_sink_as_component_class_sink(self_component_class_sink);
	const bt_component_class *component_class = bt_component_class_sink_as_component_class_const(component_class_sink);
	bt_self_component_class *self_component_class = bt_self_component_class_sink_as_self_component_class(self_component_class_sink);

	return component_class_query(component_class, self_component_class, query_executor, object, params, log_level, result);
}

static
bt_component_class_message_iterator_init_method_status
component_class_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component *self_component,
		bt_self_component_port_output *self_component_port_output)
{
	bt_component_class_message_iterator_init_method_status status = __BT_FUNC_STATUS_OK;
	PyObject *py_comp_cls = NULL;
	PyObject *py_iter_cls = NULL;
	PyObject *py_iter_ptr = NULL;
	PyObject *py_component_port_output_ptr = NULL;
	PyObject *py_init_method_result = NULL;
	PyObject *py_iter = NULL;
	PyObject *py_comp;

	py_comp = bt_self_component_get_data(self_component);

	/* Find user's Python message iterator class */
	py_comp_cls = PyObject_GetAttrString(py_comp, "__class__");
	if (!py_comp_cls) {
		BT_LOGE_STR("Cannot get Python object's `__class__` attribute.");
		goto python_error;
	}

	py_iter_cls = PyObject_GetAttrString(py_comp_cls, "_iter_cls");
	if (!py_iter_cls) {
		BT_LOGE_STR("Cannot get Python class's `_iter_cls` attribute.");
		goto python_error;
	}

	py_iter_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_message_iterator),
		SWIGTYPE_p_bt_self_message_iterator, 0);
	if (!py_iter_ptr) {
		const char *err = "Failed to create a SWIG pointer object.";

		BT_LOGE_STR(err);
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
		BT_LOGE("Failed to call Python class's __new__() method: "
			"py-cls-addr=%p", py_iter_cls);
		goto python_error;
	}

	/*
	 * Initialize object:
	 *
	 *     py_iter.__init__(self_output_port)
	 *
	 * through the _init_for_native helper static method.
	 *
	 * At this point, py_iter._ptr is set, so this initialization
	 * function has access to self._component (which gives it the
	 * user Python component object from which the iterator was
	 * created).
	 */
	py_component_port_output_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr(self_component_port_output),
		SWIGTYPE_p_bt_self_component_port_output, 0);
	if (!py_component_port_output_ptr) {
		const char *err = "Failed to create a SWIG pointer object.";

		BT_LOGE_STR(err);
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(
			self_message_iterator, err);
		goto error;
	}

	py_init_method_result = PyObject_CallMethod(py_iter,
		"_bt_init_from_native", "O", py_component_port_output_ptr);
	if (!py_init_method_result) {
		BT_LOGE_STR("User's __init__() method failed:");
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
	status = py_exc_to_status_message_iterator(self_message_iterator);
	BT_ASSERT(status != __BT_FUNC_STATUS_OK);
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
bt_component_class_message_iterator_init_method_status
component_class_source_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component_source *self_component_source,
		bt_self_component_port_output *self_component_port_output)
{
	bt_self_component *self_component = bt_self_component_source_as_self_component(self_component_source);

	return component_class_message_iterator_init(self_message_iterator, self_component, self_component_port_output);
}

static
bt_component_class_message_iterator_init_method_status
component_class_filter_message_iterator_init(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component_filter *self_component_filter,
		bt_self_component_port_output *self_component_port_output)
{
	bt_self_component *self_component = bt_self_component_filter_as_self_component(self_component_filter);

	return component_class_message_iterator_init(self_message_iterator, self_component, self_component_port_output);
}

static
void component_class_message_iterator_finalize(
		bt_self_message_iterator *message_iterator)
{
	PyObject *py_message_iter = bt_self_message_iterator_get_data(message_iterator);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_message_iter);

	/* Call user's _user_finalize() method */
	py_method_result = PyObject_CallMethod(py_message_iter,
		"_user_finalize", NULL);

	if (PyErr_Occurred()) {
		BT_LOGW("User's _user_finalize() method raised an exception: ignoring:");
		logw_exception();
	}

	/*
	 * Ignore any exception raised by the _user_finalize() method
	 * because it won't change anything at this point: the component
	 * is being destroyed anyway.
	 */
	PyErr_Clear();
	Py_XDECREF(py_method_result);
	Py_DECREF(py_message_iter);
}

/* Valid for both sources and filters. */

static
bt_component_class_message_iterator_next_method_status
component_class_message_iterator_next(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_component_class_message_iterator_next_method_status status = __BT_FUNC_STATUS_OK;
	PyObject *py_message_iter = bt_self_message_iterator_get_data(message_iterator);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_message_iter);
	py_method_result = PyObject_CallMethod(py_message_iter,
		"_bt_next_from_native", NULL);
	if (!py_method_result) {
		status = py_exc_to_status_message_iterator(message_iterator);
		BT_ASSERT(status != __BT_FUNC_STATUS_OK);
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a native message
	 * object (which is now ours).
	 */
	msgs[0] = PyLong_AsVoidPtr(py_method_result);
	*count = 1;

	/* Clear potential overflow error; should never happen */
	BT_ASSERT(!PyErr_Occurred());
	goto end;

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

	BT_ASSERT(py_comp);
	py_method_result = PyObject_CallMethod(py_comp,
		"_user_consume", NULL);
	status = py_exc_to_status_component(self_component);
	if (!py_method_result && status == __BT_FUNC_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		BT_LOGE("User's _user_consume() method failed without raising an exception: "
			"status=%d", status);
		status = __BT_FUNC_STATUS_ERROR;
	}

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
bt_component_class_source *bt_bt2_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class_source *component_class_source;
	bt_component_class *component_class;
	int ret;

	BT_ASSERT(py_cls);
	component_class_source = bt_component_class_source_create(name,
		component_class_message_iterator_next);
	if (!component_class_source) {
		BT_LOGE_STR("Cannot create source component class.");
		goto end;
	}

	component_class = bt_component_class_source_as_component_class(component_class_source);

	if (component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_source_set_init_method(component_class_source, component_class_source_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_finalize_method(component_class_source, component_class_source_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_message_iterator_can_seek_beginning_method(component_class_source,
		component_class_can_seek_beginning);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_message_iterator_seek_beginning_method(component_class_source,
		component_class_seek_beginning);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_output_port_connected_method(component_class_source,
		component_class_source_output_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_query_method(component_class_source, component_class_source_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_message_iterator_init_method(
		component_class_source, component_class_source_message_iterator_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_message_iterator_finalize_method(
		component_class_source, component_class_message_iterator_finalize);
	BT_ASSERT(ret == 0);
	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	return component_class_source;
}

static
bt_component_class_filter *bt_bt2_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	bt_component_class *component_class;
	bt_component_class_filter *component_class_filter;
	int ret;

	BT_ASSERT(py_cls);
	component_class_filter = bt_component_class_filter_create(name,
		component_class_message_iterator_next);
	if (!component_class_filter) {
		BT_LOGE_STR("Cannot create filter component class.");
		goto end;
	}

	component_class = bt_component_class_filter_as_component_class(component_class_filter);

	if (component_class_set_help_and_desc(component_class, description, help)) {
		goto end;
	}

	ret = bt_component_class_filter_set_init_method(component_class_filter, component_class_filter_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_finalize_method (component_class_filter, component_class_filter_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_message_iterator_can_seek_beginning_method(component_class_filter,
		component_class_can_seek_beginning);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_message_iterator_seek_beginning_method(component_class_filter,
		component_class_seek_beginning);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_input_port_connected_method(component_class_filter,
		component_class_filter_input_port_connected);
   	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_output_port_connected_method(component_class_filter,
		component_class_filter_output_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_query_method(component_class_filter, component_class_filter_query);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_message_iterator_init_method(
		component_class_filter, component_class_filter_message_iterator_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_filter_set_message_iterator_finalize_method(
		component_class_filter, component_class_message_iterator_finalize);
	BT_ASSERT(ret == 0);
	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
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

	ret = bt_component_class_sink_set_init_method(component_class_sink, component_class_sink_init);
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
	register_cc_ptr_to_py_cls(component_class, py_cls);

end:
	return component_class_sink;
}
%}

struct bt_component_class_source *bt_bt2_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_filter *bt_bt2_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_sink *bt_bt2_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
void bt_bt2_cc_init_from_bt2(void);
void bt_bt2_cc_exit_handler(void);
