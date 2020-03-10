/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#include "logging/comp-logging.h"


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
void bt_bt2_init_from_bt2(void)
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
void bt_bt2_exit_handler(void)
{
	/*
	 * This is an exit handler (set by the bt2 package).
	 *
	 * We only give back the references that we took in
	 * bt_bt2_init_from_bt2() here. The global variables continue
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
