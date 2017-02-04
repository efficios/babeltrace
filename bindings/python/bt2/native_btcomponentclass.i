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

%{
#include <babeltrace/component/component-class.h>
#include <babeltrace/component/component-class-source.h>
#include <babeltrace/component/component-class-sink.h>
#include <babeltrace/component/component-class-filter.h>
#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/component/notification/notification.h>
#include <assert.h>
#include <glib.h>
%}

/* Types */
struct bt_component_class;

/* Status */
enum bt_component_class_type {
	BT_COMPONENT_CLASS_TYPE_UNKNOWN =	-1,
	BT_COMPONENT_CLASS_TYPE_SOURCE =	0,
	BT_COMPONENT_CLASS_TYPE_SINK =		1,
	BT_COMPONENT_CLASS_TYPE_FILTER =	2,
};

/* General functions */
const char *bt_component_class_get_name(
		struct bt_component_class *component_class);
const char *bt_component_class_get_description(
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
	g_hash_table_insert(bt_cc_ptr_to_py_cls, (gpointer) bt_cc,
		(gpointer) py_cls);
}

static PyObject *lookup_cc_ptr_to_py_cls(struct bt_component_class *bt_cc)
{
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
static PyObject *py_mod_bt2_values = NULL;
static PyObject *py_mod_bt2_values_create_from_ptr_func = NULL;

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
	assert(py_mod_bt2);
	py_mod_bt2_values = PyImport_ImportModule("bt2.values");
	assert(py_mod_bt2_values);
	py_mod_bt2_values_create_from_ptr_func =
		PyObject_GetAttrString(py_mod_bt2_values,
			"_create_from_ptr");
	assert(py_mod_bt2_values_create_from_ptr_func);
	py_mod_bt2_exc_error_type =
		PyObject_GetAttrString(py_mod_bt2, "Error");
	assert(py_mod_bt2_exc_error_type);
	py_mod_bt2_exc_unsupported_feature_type =
		PyObject_GetAttrString(py_mod_bt2, "UnsupportedFeature");
	py_mod_bt2_exc_try_again_type =
		PyObject_GetAttrString(py_mod_bt2, "TryAgain");
	py_mod_bt2_exc_stop_type =
		PyObject_GetAttrString(py_mod_bt2, "Stop");
	assert(py_mod_bt2_exc_stop_type);
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
	 * to modules are still loaded, so it should be safe to use them
	 * even without a strong reference.
	 *
	 * We cannot do this in the library's destructor because it
	 * gets executed once Python is already finalized.
	 */
	Py_XDECREF(py_mod_bt2_values_create_from_ptr_func);
	Py_XDECREF(py_mod_bt2_values);
	Py_XDECREF(py_mod_bt2);
	Py_XDECREF(py_mod_bt2_exc_error_type);
	Py_XDECREF(py_mod_bt2_exc_unsupported_feature_type);
	Py_XDECREF(py_mod_bt2_exc_try_again_type);
	Py_XDECREF(py_mod_bt2_exc_stop_type);
}


/* Library constructor and destructor */

__attribute__((constructor))
static void bt_py3_native_comp_class_ctor(void) {
	/* Initialize component class association hash table */
	bt_cc_ptr_to_py_cls = g_hash_table_new(g_direct_hash, g_direct_equal);
	assert(bt_cc_ptr_to_py_cls);
}

__attribute__((destructor))
static void bt_py3_native_comp_class_dtor(void) {
	/* Destroy component class association hash table */
	if (bt_cc_ptr_to_py_cls) {
		g_hash_table_destroy(bt_cc_ptr_to_py_cls);
	}
}


/* Converts a BT value object to a bt2.values object */

static PyObject *bt_py3_bt_value_from_ptr(struct bt_value *value)
{
	PyObject *py_create_from_ptr_func = NULL;
	PyObject *py_value = NULL;
	PyObject *py_ptr = NULL;

	py_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(value),
		SWIGTYPE_p_bt_value, 0);
	py_value = PyObject_CallFunctionObjArgs(
		py_mod_bt2_values_create_from_ptr_func, py_ptr, NULL);
	if (!py_value) {
		goto end;
	}

	/*
	 * _create_from_ptr() only wraps an existing reference. `value`
	 * is a borrowed reference here, so increment its reference
	 * count for this new owner.
	 */
	bt_get(value);

end:
	Py_XDECREF(py_ptr);
	PyErr_Clear();
	return py_value;
}


/* self.__init__(*args, **kwargs) */

static int bt_py3_call_self_init(PyObject *py_self, PyObject *py_args,
		PyObject *py_kwargs)
{
	int ret = 0;
	size_t i;
	PyObject *py_init_method_result = NULL;
	PyObject *py_init_method = NULL;
	PyObject *py_init_method_args = NULL;
	PyObject *py_class = NULL;

	py_class = PyObject_GetAttrString(py_self, "__class__");
	if (!py_class) {
		goto error;
	}

	py_init_method = PyObject_GetAttrString(py_class, "__init__");
	if (!py_init_method) {
		goto error;
	}

	/* Prepend py_self to the arguments (copy them to new tuple) */
	py_init_method_args = PyTuple_New(PyTuple_Size(py_args) + 1);
	if (!py_init_method_args) {
		goto error;
	}

	Py_INCREF(py_self);
	ret = PyTuple_SetItem(py_init_method_args, 0, py_self);
	if (ret < 0) {
		Py_DECREF(py_self);
		goto error;
	}

	for (i = 0; i < PyTuple_Size(py_args); i++) {
		PyObject *obj = PyTuple_GetItem(py_args, i);
		if (!obj) {
			goto error;
		}

		Py_INCREF(obj);
		ret = PyTuple_SetItem(py_init_method_args, i + 1, obj);
		if (ret < 0) {
			Py_DECREF(obj);
			goto error;
		}
	}

	py_init_method_result = PyObject_Call(py_init_method,
		py_init_method_args, py_kwargs);
	if (!py_init_method_result) {
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	Py_XDECREF(py_init_method_args);
	Py_XDECREF(py_init_method_result);
	Py_XDECREF(py_init_method);
	Py_XDECREF(py_class);
	return ret;
}


/* Component class proxy methods (delegate to the attached Python object) */

static enum bt_component_status bt_py3_cc_init(
		struct bt_component *component, struct bt_value *params,
		void *init_method_data)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	int ret;

	/*
	 * This function is either called from
	 * _UserComponentType.__call__() (when init_method_data contains
	 * the PyObject * currently creating this component) or from
	 * other code (init_method_data is NULL).
	 */
	if (init_method_data) {
		/*
		 * Called from _UserComponentType.__call__().
		 *
		 * The `params` argument is completely ignored here,
		 * because if the user creating the component object
		 * from the component class object wants parameters,
		 * they'll be in the keyword arguments anyway.
		 */
		PyObject *py_comp = init_method_data;
		PyObject *py_comp_ptr = NULL;
		PyObject *py_init_args = NULL;
		PyObject *py_init_kwargs = NULL;

		/*
		 * No need to take a Python reference on py_comp here:
		 * we store it as a _borrowed_ reference. When all the
		 * Python references are dropped, the object's __del__()
		 * method is called. This method calls this side
		 * (bt_py3_component_on_del()) to swap the ownership:
		 * this BT component becomes the only owner of the
		 * Python object.
		 */
		bt_component_set_private_data(component, py_comp);

		/*
		 * Set this BT component pointer as py_comp._ptr, and
		 * take a reference on it (on success, at the end).
		 *
		 * This reference is put in the Python component
		 * object's __del__() method.
		 */
		py_comp_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(component),
			SWIGTYPE_p_bt_component, 0);
		if (!py_comp_ptr) {
			PyErr_Clear();
			goto with_init_method_data_error;
		}

		ret = PyObject_SetAttrString(py_comp, "_ptr", py_comp_ptr);
		if (ret < 0) {
			PyErr_Clear();
			goto with_init_method_data_error;
		}

		/*
		 * _UserComponentType.__call__() puts the arguments and
		 * keyword arguments to use for the py_comp.__init__()
		 * call in py_comp._init_args and py_comp._init_kwargs.
		 *
		 * We need to get them and remove them from py_comp before
		 * calling py_comp.__init__().
		 */
		py_init_args = PyObject_GetAttrString(py_comp, "_init_args");
		if (!py_init_args) {
			PyErr_Clear();
			goto with_init_method_data_error;
		}

		py_init_kwargs = PyObject_GetAttrString(py_comp, "_init_kwargs");
		if (!py_init_kwargs) {
			PyErr_Clear();
			goto with_init_method_data_error;
		}

		ret = PyObject_DelAttrString(py_comp, "_init_args");
		if (ret < 0) {
			PyErr_Clear();
			goto with_init_method_data_error;
		}

		ret = PyObject_DelAttrString(py_comp, "_init_kwargs");
		if (ret < 0) {
			PyErr_Clear();
			goto with_init_method_data_error;
		}

		/* Ready to call py_comp.__init__() */
		ret = bt_py3_call_self_init(py_comp, py_init_args,
			py_init_kwargs);
		if (ret) {
			/*
			 * We do not clear any raised exception here
			 * so that the Python caller (the one who is
			 * instantiating the class) gets the exception
			 * from the __init__() method.
			 */
			goto with_init_method_data_error;
		}

		/*
		 * py_comp.__init__() went well: get the native BT
		 * component object reference for the Python object.
		 */
		bt_get(component);
		goto with_init_method_data_end;

with_init_method_data_error:
		status = BT_COMPONENT_STATUS_ERROR;

		/*
		 * Reset py_comp._ptr to None to signal the error to
		 * _UserComponentType.__call__(). For exceptions raised
		 * from py_comp.__init__(), this is not important,
		 * because __call__() will also raise before even
		 * checking py_comp._ptr.
		 */
		if (PyObject_HasAttrString(py_comp, "_ptr")) {
			PyObject *py_err_type, *py_err_value, *py_err_traceback;
			PyErr_Fetch(&py_err_type, &py_err_value,
				&py_err_traceback);
			Py_INCREF(Py_None);
			PyObject_SetAttrString(py_comp, "_ptr", Py_None);
			PyErr_Restore(py_err_type, py_err_value,
				py_err_traceback);
		}

		/*
		 * Important: remove py_comp from our the BT component's
		 * private data. Since we're failing,
		 * bt_py3_cc_destroy() is about to be called: it must
		 * not consider py_comp. py_comp's __del__() method
		 * will be called (because the instance exists), but
		 * since py_comp._ptr is None, it won't do anything.
		 */
		bt_component_set_private_data(component, NULL);

with_init_method_data_end:
		Py_XDECREF(py_comp_ptr);
		Py_XDECREF(py_init_args);
		Py_XDECREF(py_init_kwargs);
	} else {
		/*
		 * Not called from _UserComponentType.__call__().
		 *
		 * In this case we need to instantiate the user-defined
		 * Python class. To do this we call the user-defined
		 * class manually with this already-created BT component
		 * object pointer as the __comp_ptr keyword argument.
		 *
		 * We keep a reference, the only existing one, to the
		 * created Python object. bt_py3_component_on_del() will
		 * never be called by the object's __del__() method
		 * because _UserComponentType.__call__() marks the
		 * object as owned by the native BT component object.
		 */
		PyObject *py_cls = NULL;
		PyObject *py_comp = NULL;
		PyObject *py_params = NULL;
		PyObject *py_comp_ptr = NULL;
		PyObject *py_cls_ctor_args = NULL;
		PyObject *py_cls_ctor_kwargs = NULL;

		/*
		 * Get the user-defined Python class which created this
		 * component's class in the first place (borrowed
		 * reference).
		 */
		py_cls = lookup_cc_ptr_to_py_cls(bt_component_get_class(component));
		if (!py_cls) {
			goto without_init_method_data_error;
		}

		/* BT value object -> Python bt2.values object */
		py_params = bt_py3_bt_value_from_ptr(params);
		if (!py_params) {
			goto without_init_method_data_error;
		}

		/* Component pointer -> SWIG pointer Python object */
		py_comp_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(component),
			SWIGTYPE_p_bt_component, 0);
		if (!py_comp_ptr) {
			goto without_init_method_data_error;
		}

		/*
		 * Do the equivalent of this:
		 *
		 *     py_comp = py_cls(__comp_ptr=py_comp_ptr, params=py_params)
		 *
		 * _UserComponentType.__call__() calls the Python
		 * component object's __init__() function itself in this
		 * case.
		 */
		py_cls_ctor_args = PyTuple_New(0);
		if (!py_cls_ctor_args) {
			goto without_init_method_data_error;
		}

		py_cls_ctor_kwargs = PyDict_New();
		if (!py_cls_ctor_kwargs) {
			goto without_init_method_data_error;
		}

		ret = PyDict_SetItemString(py_cls_ctor_kwargs, "__comp_ptr",
			py_comp_ptr);
		if (ret < 0) {
			goto without_init_method_data_error;
		}

		ret = PyDict_SetItemString(py_cls_ctor_kwargs, "params",
			py_params);
		if (ret < 0) {
			goto without_init_method_data_error;
		}

		py_comp = PyObject_Call(py_cls, py_cls_ctor_args,
			py_cls_ctor_kwargs);
		if (!py_comp) {
			goto without_init_method_data_error;
		}

		/*
		 * Our user Python component object is now fully created
		 * and initialized by the user. Since we just created
		 * it, this BT component is its only (persistent) owner.
		 */
		bt_component_set_private_data(component, py_comp);
		py_comp = NULL;
		goto without_init_method_data_end;

without_init_method_data_error:
		status = BT_COMPONENT_STATUS_ERROR;

		/*
		 * Clear any exception: we're returning a bad status
		 * anyway. If this call originated from Python (creation
		 * from a plugin's component class, for example), then
		 * the user gets an appropriate creation error.
		 */
		PyErr_Clear();

without_init_method_data_end:
		Py_XDECREF(py_comp);
		Py_XDECREF(py_params);
		Py_XDECREF(py_comp_ptr);
		Py_XDECREF(py_cls_ctor_args);
		Py_XDECREF(py_cls_ctor_kwargs);
	}

	return status;
}

static void bt_py3_cc_destroy(struct bt_component *component)
{
	PyObject *py_comp = bt_component_get_private_data(component);
	PyObject *py_destroy_method_result = NULL;

	if (py_comp) {
		/*
		 * If we're here, this BT component is the only owner of
		 * its private user Python object. It is safe to
		 * decrement its reference count, and thus destroy it.
		 *
		 * We call its _destroy() method before doing so to notify
		 * the Python user.
		 */
		py_destroy_method_result = PyObject_CallMethod(py_comp,
			"_destroy", NULL);

		/*
		 * Ignore any exception raised by the _destroy() method
		 * because it won't change anything at this point: the
		 * component is being destroyed anyway.
		 */
		PyErr_Clear();
		Py_XDECREF(py_destroy_method_result);
		Py_DECREF(py_comp);
	}
}

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
	} else {
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

static enum bt_notification_iterator_status bt_py3_cc_notification_iterator_init(
		struct bt_component *component,
		struct bt_notification_iterator *iter, void *init_method_data)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	PyObject *py_comp_cls = NULL;
	PyObject *py_iter_cls = NULL;
	PyObject *py_iter_ptr = NULL;
	PyObject *py_init_method_result = NULL;
	PyObject *py_iter = NULL;
	PyObject *py_comp = bt_component_get_private_data(component);

	/* Find user's Python notification iterator class */
	py_comp_cls = PyObject_GetAttrString(py_comp, "__class__");
	if (!py_comp_cls) {
		goto error;
	}

	py_iter_cls = PyObject_GetAttrString(py_comp_cls, "_iter_cls");
	if (!py_iter_cls) {
		goto error;
	}

	/*
	 * Create object with borrowed BT notification iterator
	 * reference:
	 *
	 *     py_iter = py_iter_cls.__new__(py_iter_cls, py_iter_ptr)
	 */
	py_iter_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(iter),
		SWIGTYPE_p_bt_notification_iterator, 0);
	if (!py_iter_ptr) {
		goto error;
	}

	py_iter = PyObject_CallMethod(py_iter_cls, "__new__",
		"(OO)", py_iter_cls, py_iter_ptr);
	if (!py_iter) {
		goto error;
	}

	/*
	 * Initialize object:
	 *
	 *     py_iter.__init__()
	 *
	 * At this point, py_iter._ptr is set, so this initialization
	 * function has access to the py_iter.component property (which
	 * gives it the user Python component object from which the
	 * iterator was created).
	 */
	py_init_method_result = PyObject_CallMethod(py_iter, "__init__", NULL);
	if (!py_init_method_result) {
		goto error;
	}

	/*
	 * Since the Python code can never instantiate a user-defined
	 * notification iterator class, the BT notification iterator
	 * object does NOT belong to a user Python notification iterator
	 * object (borrowed reference). However this Python object is
	 * owned by this BT notification iterator object.
	 *
	 * In the Python world, the lifetime of the BT notification
	 * iterator is managed by a _GenericNotificationIterator
	 * instance:
	 *
	 *     _GenericNotificationIterator instance:
	 *         owns a native bt_notification_iterator object (iter)
	 *             owns a UserNotificationIterator instance (py_iter)
	 *                 self._ptr is a borrowed reference to the
	 *                 native bt_notification_iterator object (iter)
	 */
	bt_notification_iterator_set_private_data(iter, py_iter);
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
	Py_XDECREF(py_comp_cls);
	Py_XDECREF(py_iter_cls);
	Py_XDECREF(py_iter_ptr);
	Py_XDECREF(py_init_method_result);
	Py_XDECREF(py_iter);
	return status;
}

static void bt_py3_cc_notification_iterator_destroy(
		struct bt_notification_iterator *iter)
{
	PyObject *py_notif_iter =
		bt_notification_iterator_get_private_data(iter);
	PyObject *py_destroy_method_result = NULL;

	assert(py_notif_iter);
	py_destroy_method_result = PyObject_CallMethod(py_notif_iter,
		"_destroy", NULL);

	/*
	 * Ignore any exception raised by the _destroy() method because
	 * it won't change anything at this point. The notification
	 * iterator is being destroyed anyway.
	 */
	PyErr_Clear();
	Py_XDECREF(py_destroy_method_result);
	Py_DECREF(py_notif_iter);
}

static struct bt_notification *bt_py3_cc_notification_iterator_get(
		struct bt_notification_iterator *iter)
{
	PyObject *py_notif_iter =
		bt_notification_iterator_get_private_data(iter);
	PyObject *py_get_method_result = NULL;
	struct bt_notification *notif = NULL;

	assert(py_notif_iter);
	py_get_method_result = PyObject_CallMethod(py_notif_iter,
		"_get_from_bt", NULL);
	if (!py_get_method_result) {
		goto error;
	}

	/*
	 * The returned object, on success, is an integer object
	 * (PyLong) containing the address of a BT notification
	 * object (which is not ours).
	 */
	notif = (struct bt_notification *) PyLong_AsUnsignedLongLong(
		py_get_method_result);

	/* Clear potential overflow error; should never happen */
	if (PyErr_Occurred()) {
		goto error;
	}

	if (!notif) {
		goto error;
	}

	goto end;

error:
	BT_PUT(notif);

	/*
	 * Clear any exception: we're returning NULL anyway. If this
	 * call originated from Python, then the user gets an
	 * appropriate error.
	 */
	PyErr_Clear();

end:
	Py_XDECREF(py_get_method_result);
	return notif;
}

static enum bt_notification_iterator_status bt_py3_cc_notification_iterator_next(
		struct bt_notification_iterator *iter)
{
	enum bt_notification_iterator_status status;
	PyObject *py_notif_iter =
		bt_notification_iterator_get_private_data(iter);
	PyObject *py_next_method_result = NULL;

	assert(py_notif_iter);
	py_next_method_result = PyObject_CallMethod(py_notif_iter,
		"_next", NULL);
	status = bt_py3_exc_to_notif_iter_status();
	if (!py_next_method_result
			&& status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

	Py_XDECREF(py_next_method_result);
	return status;
}

static enum bt_notification_iterator_status bt_py3_cc_notification_iterator_seek_time(
		struct bt_notification_iterator *iter, int64_t time)
{
	enum bt_notification_iterator_status status;
	PyObject *py_notif_iter =
		bt_notification_iterator_get_private_data(iter);
	PyObject *py_seek_to_time_method_result = NULL;

	assert(py_notif_iter);
	py_seek_to_time_method_result = PyObject_CallMethod(py_notif_iter,
		"_seek_to_time", "(L)", time);
	status = bt_py3_exc_to_notif_iter_status();
	if (!py_seek_to_time_method_result
			&& status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

	Py_XDECREF(py_seek_to_time_method_result);
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
	} else {
		status = BT_COMPONENT_STATUS_ERROR;
	}

end:
	PyErr_Clear();
	return status;
}

static enum bt_component_status bt_py3_cc_sink_consume(
        	struct bt_component *component)
{
	PyObject *py_comp = bt_component_get_private_data(component);
	PyObject *py_consume_method_result = NULL;
	enum bt_component_status status;

	py_consume_method_result = PyObject_CallMethod(py_comp,
		"_consume", NULL);
	status = bt_py3_exc_to_component_status();
	if (!py_consume_method_result && status == BT_COMPONENT_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		status = BT_COMPONENT_STATUS_ERROR;
	}

	Py_XDECREF(py_consume_method_result);
	return status;
}

static enum bt_component_status bt_py3_cc_sink_filter_add_iterator(
        	struct bt_component *component,
        	struct bt_notification_iterator *iter)
{
	PyObject *py_comp = bt_component_get_private_data(component);
	PyObject *py_add_iterator_method_result = NULL;
	PyObject *py_notif_iter_ptr = NULL;
	enum bt_component_status status;

	py_notif_iter_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(iter),
		SWIGTYPE_p_bt_notification_iterator, 0);
	if (!py_notif_iter_ptr) {
		PyErr_Clear();
		status = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	bt_get(iter);
	py_add_iterator_method_result = PyObject_CallMethod(py_comp,
		"_add_iterator_from_bt", "(O)", py_notif_iter_ptr);
	status = bt_py3_exc_to_component_status();
	if (!py_add_iterator_method_result
			&& status == BT_COMPONENT_STATUS_OK) {
		/* Pretty sure this should never happen, but just in case */
		status = BT_COMPONENT_STATUS_ERROR;
	}

end:
	Py_XDECREF(py_notif_iter_ptr);
	Py_XDECREF(py_add_iterator_method_result);
	return status;
}


/* Component class creation functions (called from Python module) */

static int bt_py3_cc_set_optional_attrs_methods(struct bt_component_class *cc,
		const char *description)
{
	int ret = 0;

	if (description) {
		ret = bt_component_class_set_description(cc, description);
		if (ret) {
			goto end;
		}
	}

	ret = bt_component_class_set_init_method(cc, bt_py3_cc_init);
	if (ret) {
		goto end;
	}

	ret = bt_component_class_set_destroy_method(cc, bt_py3_cc_destroy);
	if (ret) {
		goto end;
	}

end:
	return ret;
}

static int bt_py3_cc_set_optional_iter_methods(struct bt_component_class *cc,
		bool has_seek_time,
		int (*set_notif_iter_init_method)(struct bt_component_class *, bt_component_class_notification_iterator_init_method),
		int (*set_notif_iter_destroy_method)(struct bt_component_class *, bt_component_class_notification_iterator_destroy_method),
		int (*set_notif_iter_seek_time_method)(struct bt_component_class *, bt_component_class_notification_iterator_seek_time_method))
{
	int ret;

	ret = set_notif_iter_init_method(
		cc, bt_py3_cc_notification_iterator_init);
	if (ret) {
		goto end;
	}

	ret = set_notif_iter_destroy_method(
		cc, bt_py3_cc_notification_iterator_destroy);
	if (ret) {
		goto end;
	}

	if (has_seek_time) {
		ret = set_notif_iter_seek_time_method(
			cc, bt_py3_cc_notification_iterator_seek_time);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static struct bt_component_class *bt_py3_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		bool has_seek_time)
{
	struct bt_component_class *cc;
	int ret;

	assert(py_cls);
	cc = bt_component_class_source_create(name,
		bt_py3_cc_notification_iterator_get,
		bt_py3_cc_notification_iterator_next);
	if (!cc) {
		goto end;
	}

	ret = bt_py3_cc_set_optional_attrs_methods(cc, description);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	ret = bt_py3_cc_set_optional_iter_methods(cc, has_seek_time,
		bt_component_class_source_set_notification_iterator_init_method,
		bt_component_class_source_set_notification_iterator_destroy_method,
		bt_component_class_source_set_notification_iterator_seek_time_method);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	register_cc_ptr_to_py_cls(cc, py_cls);
	bt_component_class_freeze(cc);

end:
	return cc;
}

static struct bt_component_class *bt_py3_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		bool has_seek_time)
{
	struct bt_component_class *cc;
	int ret;

	assert(py_cls);
	cc = bt_component_class_filter_create(name,
		bt_py3_cc_notification_iterator_get,
		bt_py3_cc_notification_iterator_next);
	if (!cc) {
		goto end;
	}

	ret = bt_py3_cc_set_optional_attrs_methods(cc, description);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	ret = bt_py3_cc_set_optional_iter_methods(cc, has_seek_time,
		bt_component_class_filter_set_notification_iterator_init_method,
		bt_component_class_filter_set_notification_iterator_destroy_method,
		bt_component_class_filter_set_notification_iterator_seek_time_method);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	ret = bt_component_class_filter_set_add_iterator_method(cc,
		bt_py3_cc_sink_filter_add_iterator);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	register_cc_ptr_to_py_cls(cc, py_cls);
	bt_component_class_freeze(cc);

end:
	return cc;
}

static struct bt_component_class *bt_py3_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description)
{
	struct bt_component_class *cc;
	int ret;

	assert(py_cls);
	cc = bt_component_class_sink_create(name, bt_py3_cc_sink_consume);
	if (!cc) {
		goto end;
	}

	ret = bt_py3_cc_set_optional_attrs_methods(cc, description);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	ret = bt_component_class_sink_set_add_iterator_method(cc,
		bt_py3_cc_sink_filter_add_iterator);
	if (ret) {
		BT_PUT(cc);
		goto end;
	}

	register_cc_ptr_to_py_cls(cc, py_cls);
	bt_component_class_freeze(cc);

end:
	return cc;
}


/* Component creation function (called from Python module) */

static void bt_py3_component_create(
		struct bt_component_class *comp_class, PyObject *py_self,
		const char *name)
{
	struct bt_component *component =
		bt_component_create_with_init_method_data(comp_class,
			name, NULL, py_self);

	/*
	 * Our component initialization function, bt_py3_cc_init(), sets
	 * a new reference to the created component in the user's Python
	 * object (py_self._ptr). This is where the reference is kept.
	 * We don't need the returned component in this case (if any,
	 * because it might be NULL if the creation failed, most
	 * probably because py_self.__init__() raised something).
	 */
	bt_put(component);
}


/* Component delete notification */

static void bt_py3_component_on_del(PyObject *py_comp)
{
	/*
	 * The Python component's __del__() function calls this to
	 * indicate that there's no more reference on it from the
	 * Python world.
	 *
	 * Since the BT component can continue to live once the Python
	 * component object is deleted, we increment the Python
	 * component's reference count so that it now only belong to the
	 * BT component. We will decrement this reference count once
	 * the BT component is destroyed in bt_py3_cc_destroy().
	 */
	assert(py_comp);
	Py_INCREF(py_comp);
}
%}

%exception bt_py3_component_create {
	$action
	if (PyErr_Occurred()) {
		/* Exception is already set by bt_py3_component_create() */
		SWIG_fail;
	}
}

struct bt_component_class *bt_py3_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		bool has_seek_time);
struct bt_component_class *bt_py3_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		bool has_seek_time);
struct bt_component_class *bt_py3_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description);
void bt_py3_component_create(
		struct bt_component_class *comp_class, PyObject *py_self,
		const char *name);
void bt_py3_component_on_del(PyObject *py_comp);
void bt_py3_cc_init_from_bt2(void);
void bt_py3_cc_exit_handler(void);
