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
struct bt_component_class;

/* Status */
enum bt_component_class_type {
	BT_COMPONENT_CLASS_TYPE_UNKNOWN = -1,
	BT_COMPONENT_CLASS_TYPE_SOURCE = 0,
	BT_COMPONENT_CLASS_TYPE_SINK = 1,
	BT_COMPONENT_CLASS_TYPE_FILTER = 2,
};

/* General functions */
const char *bt_component_class_get_name(
		struct bt_component_class *component_class);
const char *bt_component_class_get_description(
		struct bt_component_class *component_class);
const char *bt_component_class_get_help(
		struct bt_component_class *component_class);
enum bt_component_class_type bt_component_class_get_type(
		struct bt_component_class *component_class);

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

static PyObject *lookup_cc_ptr_to_py_cls(struct bt_component_class *bt_cc)
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
static PyObject *py_mod_bt2_exc_unsupported_feature_type = NULL;
static PyObject *py_mod_bt2_exc_try_again_type = NULL;
static PyObject *py_mod_bt2_exc_stop_type = NULL;
static PyObject *py_mod_bt2_exc_port_connection_refused_type = NULL;
static PyObject *py_mod_bt2_exc_notif_iter_canceled_type = NULL;
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
	py_mod_bt2_exc_unsupported_feature_type =
		PyObject_GetAttrString(py_mod_bt2, "UnsupportedFeature");
	BT_ASSERT(py_mod_bt2_exc_unsupported_feature_type);
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
	Py_XDECREF(py_mod_bt2_exc_unsupported_feature_type);
	Py_XDECREF(py_mod_bt2_exc_try_again_type);
	Py_XDECREF(py_mod_bt2_exc_stop_type);
	Py_XDECREF(py_mod_bt2_exc_port_connection_refused_type);
	Py_XDECREF(py_mod_bt2_exc_notif_iter_canceled_type);
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


/* Component class proxy methods (delegate to the attached Python object) */

static enum bt_notification_iterator_status bt_py3_exc_to_notif_iter_status(void)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
		goto end;
	}

	if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_unsupported_feature_type)) {
		status = BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_stop_type)) {
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_try_again_type)) {
		status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
	} else {
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
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
		status = BT_QUERY_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

static enum bt_component_status bt_py3_exc_to_component_status(void)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	PyObject *exc = PyErr_Occurred();

	if (!exc) {
		goto end;
	}

	if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_unsupported_feature_type)) {
		status = BT_COMPONENT_STATUS_UNSUPPORTED;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_try_again_type)) {
		status = BT_COMPONENT_STATUS_AGAIN;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_stop_type)) {
		status = BT_COMPONENT_STATUS_END;
	} else if (PyErr_GivenExceptionMatches(exc,
			py_mod_bt2_exc_port_connection_refused_type)) {
		status = BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION;
	} else {
		status = BT_COMPONENT_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

static enum bt_component_status bt_py3_cc_init(
		struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data)
{
	struct bt_component *comp =
		bt_component_from_private(priv_comp);
	struct bt_component_class *comp_cls = bt_component_get_class(comp);
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	PyObject *py_cls = NULL;
	PyObject *py_comp = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_comp_ptr = NULL;

	(void) init_method_data;
	BT_ASSERT(comp);
	BT_ASSERT(comp_cls);

	/*
	 * Get the user-defined Python class which created this
	 * component's class in the first place (borrowed
	 * reference).
	 */
	py_cls = lookup_cc_ptr_to_py_cls(comp_cls);
	if (!py_cls) {
		BT_LOGE("Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", comp_cls);
		goto error;
	}

	/* Parameters pointer -> SWIG pointer Python object */
	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	/* Private component pointer -> SWIG pointer Python object */
	py_comp_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(priv_comp),
		SWIGTYPE_p_bt_private_component, 0);
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
		BT_LOGE("Failed to call Python class's _init_from_native() method: "
			"py-cls-addr=%p", py_cls);
		goto error;
	}

	/*
	 * Our user Python component object is now fully created and
	 * initialized by the user. Since we just created it, this
	 * native component is its only (persistent) owner.
	 */
	bt_private_component_set_user_data(priv_comp, py_comp);
	py_comp = NULL;
	goto end;

error:
	status = BT_COMPONENT_STATUS_ERROR;

	/*
	 * Clear any exception: we're returning a bad status anyway. If
	 * this call originated from Python (creation from a plugin's
	 * component class, for example), then the user gets an
	 * appropriate creation error.
	 */
	PyErr_Clear();

end:
	bt_put(comp_cls);
	bt_put(comp);
	Py_XDECREF(py_comp);
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_comp_ptr);
	return status;
}

static void bt_py3_cc_finalize(struct bt_private_component *priv_comp)
{
	PyObject *py_comp = bt_private_component_get_user_data(priv_comp);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_comp);

	/* Call user's _finalize() method */
	py_method_result = PyObject_CallMethod(py_comp,
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

static enum bt_component_status bt_py3_cc_accept_port_connection(
		struct bt_private_component *priv_comp,
		struct bt_private_port *self_priv_port,
		struct bt_port *other_port)
{
	enum bt_component_status status;
	PyObject *py_comp = NULL;
	PyObject *py_self_port_ptr = NULL;
	PyObject *py_other_port_ptr = NULL;
	PyObject *py_method_result = NULL;

	py_comp = bt_private_component_get_user_data(priv_comp);
	BT_ASSERT(py_comp);
	py_self_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_priv_port),
		SWIGTYPE_p_bt_private_port, 0);
	if (!py_self_port_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_other_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(other_port),
		SWIGTYPE_p_bt_port, 0);
	if (!py_other_port_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_accept_port_connection_from_native", "(OO)", py_self_port_ptr,
		py_other_port_ptr);
	status = bt_py3_exc_to_component_status();
	if (!py_method_result && status == BT_COMPONENT_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		BT_LOGE("User's _accept_port_connection() method failed without raising an exception: "
			"status=%d", status);
		goto error;
	}

	if (status == BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION) {
		/*
		 * Looks like the user method raised
		 * PortConnectionRefused: accept this like if it
		 * returned False.
		 */
		goto end;
	} else if (status != BT_COMPONENT_STATUS_OK) {
		BT_LOGE("User's _accept_port_connection() raised an unexpected exception: "
			"status=%d", status);
		goto error;
	}

	BT_ASSERT(PyBool_Check(py_method_result));

	if (py_method_result == Py_True) {
		status = BT_COMPONENT_STATUS_OK;
	} else {
		status = BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION;
	}

	goto end;

error:
	status = BT_COMPONENT_STATUS_ERROR;

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

static void bt_py3_cc_port_connected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *self_priv_port,
		struct bt_port *other_port)
{
	PyObject *py_comp = NULL;
	PyObject *py_self_port_ptr = NULL;
	PyObject *py_other_port_ptr = NULL;
	PyObject *py_method_result = NULL;

	py_comp = bt_private_component_get_user_data(priv_comp);
	BT_ASSERT(py_comp);
	py_self_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(self_priv_port),
		SWIGTYPE_p_bt_private_port, 0);
	if (!py_self_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_other_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(other_port),
		SWIGTYPE_p_bt_port, 0);
	if (!py_other_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_port_connected_from_native", "(OO)", py_self_port_ptr,
		py_other_port_ptr);
	BT_ASSERT(py_method_result == Py_None);
	Py_XDECREF(py_self_port_ptr);
	Py_XDECREF(py_other_port_ptr);
	Py_XDECREF(py_method_result);
}

static void bt_py3_cc_port_disconnected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *priv_port)
{
	PyObject *py_comp = NULL;
	PyObject *py_port_ptr = NULL;
	PyObject *py_method_result = NULL;

	py_comp = bt_private_component_get_user_data(priv_comp);
	BT_ASSERT(py_comp);
	py_port_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(priv_port),
		SWIGTYPE_p_bt_private_port, 0);
	if (!py_port_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_method_result = PyObject_CallMethod(py_comp,
		"_port_disconnected_from_native", "(O)", py_port_ptr);
	BT_ASSERT(py_method_result == Py_None);
	Py_XDECREF(py_port_ptr);
	Py_XDECREF(py_method_result);
}

static struct bt_component_class_query_method_return bt_py3_cc_query(
		struct bt_component_class *comp_cls,
		struct bt_query_executor *query_exec,
		const char *object, struct bt_value *params)
{
	PyObject *py_cls = NULL;
	PyObject *py_params_ptr = NULL;
	PyObject *py_query_exec_ptr = NULL;
	PyObject *py_query_func = NULL;
	PyObject *py_object = NULL;
	PyObject *py_results_addr = NULL;
	struct bt_component_class_query_method_return ret = {
		.status = BT_QUERY_STATUS_OK,
		.result = NULL,
	};

	py_cls = lookup_cc_ptr_to_py_cls(comp_cls);
	if (!py_cls) {
		BT_LOGE("Cannot find Python class associated to native component class: "
			"comp-cls-addr=%p", comp_cls);
		goto error;
	}

	py_params_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(params),
		SWIGTYPE_p_bt_value, 0);
	if (!py_params_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	py_query_exec_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(query_exec),
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
	ret.status = bt_py3_exc_to_query_status();
	if (!py_results_addr && ret.status == BT_QUERY_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		BT_LOGE("_query_from_native() class method failed without raising an exception: "
			"status=%d", ret.status);
		ret.status = BT_QUERY_STATUS_ERROR;
		goto end;
	}

	if (ret.status != BT_QUERY_STATUS_OK) {
		goto end;
	}

	if (py_results_addr == Py_NotImplemented) {
		BT_LOGE_STR("User's _query() method is not implemented.");
		goto error;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a BT value object (new
	 * reference).
	 */
	ret.result = (void *) PyLong_AsUnsignedLongLong(py_results_addr);
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(ret.result);
	goto end;

error:
	BT_PUT(ret.result);
	PyErr_Clear();
	ret.status = BT_QUERY_STATUS_ERROR;
	BT_PUT(ret.result);

end:
	Py_XDECREF(py_params_ptr);
	Py_XDECREF(py_query_exec_ptr);
	Py_XDECREF(py_query_func);
	Py_XDECREF(py_object);
	Py_XDECREF(py_results_addr);
	return ret;
}

static enum bt_notification_iterator_status bt_py3_cc_notification_iterator_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	PyObject *py_comp_cls = NULL;
	PyObject *py_iter_cls = NULL;
	PyObject *py_iter_ptr = NULL;
	PyObject *py_init_method_result = NULL;
	PyObject *py_iter = NULL;
	struct bt_private_component *priv_comp =
		bt_private_connection_private_notification_iterator_get_private_component(
			priv_notif_iter);
	PyObject *py_comp;

	BT_ASSERT(priv_comp);
	py_comp = bt_private_component_get_user_data(priv_comp);

	/* Find user's Python notification iterator class */
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

	py_iter_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(priv_notif_iter),
		SWIGTYPE_p_bt_private_connection_private_notification_iterator, 0);
	if (!py_iter_ptr) {
		BT_LOGE_STR("Failed to create a SWIG pointer object.");
		goto error;
	}

	/*
	 * Create object with borrowed native notification iterator
	 * reference:
	 *
	 *     py_iter = py_iter_cls.__new__(py_iter_cls, py_iter_ptr)
	 */
	py_iter = PyObject_CallMethod(py_iter_cls, "__new__",
		"(OO)", py_iter_cls, py_iter_ptr);
	if (!py_iter) {
		BT_LOGE("Failed to call Python class's __new__() method: "
			"py-cls-addr=%p", py_iter_cls);
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
		goto error;
	}

	/*
	 * Since the Python code can never instantiate a user-defined
	 * notification iterator class, the native notification iterator
	 * object does NOT belong to a user Python notification iterator
	 * object (borrowed reference). However this Python object is
	 * owned by this native notification iterator object.
	 *
	 * In the Python world, the lifetime of the native notification
	 * iterator is managed by a _GenericNotificationIterator
	 * instance:
	 *
	 *     _GenericNotificationIterator instance:
	 *         owns a native bt_notification_iterator object (iter)
	 *             owns a _UserNotificationIterator instance (py_iter)
	 *                 self._ptr is a borrowed reference to the
	 *                 native bt_private_connection_private_notification_iterator
	 *                 object (iter)
	 */
	bt_private_connection_private_notification_iterator_set_user_data(priv_notif_iter,
		py_iter);
	py_iter = NULL;
	goto end;

error:
	status = bt_py3_exc_to_notif_iter_status();
	if (status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		/*
		 * Looks like there wasn't any exception from the Python
		 * side, but we're still in an error state here.
		 */
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

	/*
	 * Clear any exception: we're returning a bad status anyway. If
	 * this call originated from Python, then the user gets an
	 * appropriate creation error.
	 */
	PyErr_Clear();

end:
	bt_put(priv_comp);
	Py_XDECREF(py_comp_cls);
	Py_XDECREF(py_iter_cls);
	Py_XDECREF(py_iter_ptr);
	Py_XDECREF(py_init_method_result);
	Py_XDECREF(py_iter);
	return status;
}

static void bt_py3_cc_notification_iterator_finalize(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter)
{
	PyObject *py_notif_iter =
		bt_private_connection_private_notification_iterator_get_user_data(priv_notif_iter);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_notif_iter);

	/* Call user's _finalize() method */
	py_method_result = PyObject_CallMethod(py_notif_iter,
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
	Py_DECREF(py_notif_iter);
}

static struct bt_notification_iterator_next_method_return
bt_py3_cc_notification_iterator_next(
			struct bt_private_connection_private_notification_iterator *priv_notif_iter)
{
	struct bt_notification_iterator_next_method_return next_ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};
	PyObject *py_notif_iter =
		bt_private_connection_private_notification_iterator_get_user_data(priv_notif_iter);
	PyObject *py_method_result = NULL;

	BT_ASSERT(py_notif_iter);
	py_method_result = PyObject_CallMethod(py_notif_iter,
		"_next_from_native", NULL);
	if (!py_method_result) {
		next_ret.status = bt_py3_exc_to_notif_iter_status();
		BT_ASSERT(next_ret.status != BT_NOTIFICATION_ITERATOR_STATUS_OK);
		goto end;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a native notification
	 * object (which is now ours).
	 */
	next_ret.notification =
		(struct bt_notification *) PyLong_AsUnsignedLongLong(
			py_method_result);

	/* Clear potential overflow error; should never happen */
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(next_ret.notification);
	goto end;

end:
	Py_XDECREF(py_method_result);
	return next_ret;
}

static enum bt_component_status bt_py3_cc_sink_consume(
		struct bt_private_component *priv_comp)
{
	PyObject *py_comp = bt_private_component_get_user_data(priv_comp);
	PyObject *py_method_result = NULL;
	enum bt_component_status status;

	BT_ASSERT(py_comp);
	py_method_result = PyObject_CallMethod(py_comp,
		"_consume", NULL);
	status = bt_py3_exc_to_component_status();
	if (!py_method_result && status == BT_COMPONENT_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		BT_LOGE("User's _consume() method failed without raising an exception: "
			"status=%d", status);
		status = BT_COMPONENT_STATUS_ERROR;
	}

	Py_XDECREF(py_method_result);
	return status;
}


/* Component class creation functions (called from Python module) */

static int bt_py3_cc_set_optional_attrs_methods(struct bt_component_class *cc,
		const char *description, const char *help)
{
	int ret = 0;

	if (description) {
		ret = bt_component_class_set_description(cc, description);
		if (ret) {
			BT_LOGE("Cannot set component class's description: "
				"comp-cls-addr=%p", cc);
			goto end;
		}
	}

	if (help) {
		ret = bt_component_class_set_help(cc, help);
		if (ret) {
			BT_LOGE("Cannot set component class's help text: "
				"comp-cls-addr=%p", cc);
			goto end;
		}
	}

	ret = bt_component_class_set_init_method(cc, bt_py3_cc_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_finalize_method(cc, bt_py3_cc_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_accept_port_connection_method(cc,
		bt_py3_cc_accept_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_port_connected_method(cc,
		bt_py3_cc_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_port_disconnected_method(cc,
		bt_py3_cc_port_disconnected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_query_method(cc, bt_py3_cc_query);
	BT_ASSERT(ret == 0);

end:
	return ret;
}

static void bt_py3_cc_set_optional_iter_methods(struct bt_component_class *cc,
		int (*set_notif_iter_init_method)(struct bt_component_class *, bt_component_class_notification_iterator_init_method),
		int (*set_notif_iter_finalize_method)(struct bt_component_class *, bt_component_class_notification_iterator_finalize_method))
{
	int ret __attribute__((unused));

	ret = set_notif_iter_init_method(
		cc, bt_py3_cc_notification_iterator_init);
	BT_ASSERT(ret == 0);
	ret = set_notif_iter_finalize_method(
		cc, bt_py3_cc_notification_iterator_finalize);
	BT_ASSERT(ret == 0);
}

static struct bt_component_class *bt_py3_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	struct bt_component_class *cc;
	int ret;

	BT_ASSERT(py_cls);
	cc = bt_component_class_source_create(name,
		bt_py3_cc_notification_iterator_next);
	if (!cc) {
		BT_LOGE_STR("Cannot create source component class.");
		goto end;
	}

	ret = bt_py3_cc_set_optional_attrs_methods(cc, description, help);
	if (ret) {
		BT_LOGE_STR("Cannot set source component class's optional attributes and methods.");
		BT_PUT(cc);
		goto end;
	}

	bt_py3_cc_set_optional_iter_methods(cc,
		bt_component_class_source_set_notification_iterator_init_method,
		bt_component_class_source_set_notification_iterator_finalize_method);
	register_cc_ptr_to_py_cls(cc, py_cls);
	bt_component_class_freeze(cc);

end:
	return cc;
}

static struct bt_component_class *bt_py3_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	struct bt_component_class *cc;
	int ret;

	BT_ASSERT(py_cls);
	cc = bt_component_class_filter_create(name,
		bt_py3_cc_notification_iterator_next);
	if (!cc) {
		BT_LOGE_STR("Cannot create filter component class.");
		goto end;
	}

	ret = bt_py3_cc_set_optional_attrs_methods(cc, description, help);
	if (ret) {
		BT_LOGE_STR("Cannot set filter component class's optional attributes and methods.");
		BT_PUT(cc);
		goto end;
	}

	bt_py3_cc_set_optional_iter_methods(cc,
		bt_component_class_filter_set_notification_iterator_init_method,
		bt_component_class_filter_set_notification_iterator_finalize_method);
	register_cc_ptr_to_py_cls(cc, py_cls);
	bt_component_class_freeze(cc);

end:
	return cc;
}

static struct bt_component_class *bt_py3_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help)
{
	struct bt_component_class *cc;
	int ret;

	BT_ASSERT(py_cls);
	cc = bt_component_class_sink_create(name, bt_py3_cc_sink_consume);
	if (!cc) {
		BT_LOGE_STR("Cannot create sink component class.");
		goto end;
	}

	ret = bt_py3_cc_set_optional_attrs_methods(cc, description, help);
	if (ret) {
		BT_LOGE_STR("Cannot set sink component class's optional attributes and methods.");
		BT_PUT(cc);
		goto end;
	}

	register_cc_ptr_to_py_cls(cc, py_cls);
	bt_component_class_freeze(cc);

end:
	return cc;
}
%}

struct bt_component_class *bt_py3_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class *bt_py3_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class *bt_py3_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
void bt_py3_cc_init_from_bt2(void);
void bt_py3_cc_exit_handler(void);
