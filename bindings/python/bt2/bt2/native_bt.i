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

#ifndef SWIGPYTHON
# error Unsupported output language
#endif

%module native_bt

%{
#define BT_LOG_TAG "PY-NATIVE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/assert-internal.h>

typedef const unsigned char *BTUUID;
%}

typedef int bt_bool;

/* For uint*_t/int*_t */
%include "stdint.i"

/* Remove `bt_` and `BT_` prefixes from function names and enumeration items */
%rename("%(strip:[bt_])s", %$isfunction) "";
%rename("%(strip:[BT_])s", %$isenumitem) "";

/* Output argument typemap for string output (always appends) */
%typemap(in, numinputs=0) const char **BTOUTSTR (char *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) const char **BTOUTSTR {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_Python_str_FromChar(*$1));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for field type output (always appends) */
%typemap(in, numinputs=0) struct bt_field_type **BTOUTFT (struct bt_field_type *temp_ft = NULL) {
	$1 = &temp_ft;
}

%typemap(argout) struct bt_field_type **BTOUTFT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_field_type, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for component output (always appends) */
%typemap(in, numinputs=0) struct bt_component **BTOUTCOMP (struct bt_component *temp_comp = NULL) {
	$1 = &temp_comp;
}

%typemap(argout) struct bt_component **BTOUTCOMP {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_component, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for connection output (always appends) */
%typemap(in, numinputs=0) struct bt_connection **BTOUTCONN (struct bt_connection *temp_conn = NULL) {
	$1 = &temp_conn;
}

%typemap(argout) struct bt_connection **BTOUTCONN {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_connection, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for private port output (always appends) */
%typemap(in, numinputs=0) struct bt_private_port **BTOUTPRIVPORT (struct bt_private_port *temp_priv_port = NULL) {
	$1 = &temp_priv_port;
}

%typemap(argout) struct bt_private_port **BTOUTPRIVPORT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_private_port, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0) struct bt_value **BTOUTVALUE (struct bt_value *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) struct bt_value **BTOUTVALUE {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_value, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for initialized uint64_t output parameter (always appends) */
%typemap(in, numinputs=0) uint64_t *OUTPUTINIT (uint64_t temp = -1ULL) {
	$1 = &temp;
}

%typemap(argout) uint64_t *OUTPUTINIT {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_unsigned_SS_long_SS_long((*$1)));
}

/* Output argument typemap for initialized unsigned int output parameter (always appends) */
%typemap(in, numinputs=0) unsigned int *OUTPUTINIT (unsigned int temp = -1) {
	$1 = &temp;
}

%typemap(argout) unsigned int *OUTPUTINIT {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_unsigned_SS_long_SS_long((uint64_t) (*$1)));
}

/* Input argument typemap for UUID bytes */
%typemap(in) BTUUID {
	$1 = (unsigned char *) PyBytes_AsString($input);
}

/* Output argument typemap for UUID bytes */
%typemap(out) BTUUID {
	if (!$1) {
		Py_INCREF(Py_None);
		$result = Py_None;
	} else {
		$result = PyBytes_FromStringAndSize((const char *) $1, 16);
	}
}

/*
 * Input and output argument typemaps for raw Python objects (direct).
 *
 * Those typemaps honor the convention of Python C function calls with
 * respect to reference counting: parameters are passed as borrowed
 * references, and objects are returned as new references. The wrapped
 * C function must ensure that the return value is always a new
 * reference, and never steal parameter references.
 */
%typemap(in) PyObject * {
	$1 = $input;
}

%typemap(out) PyObject * {
	$result = $1;
}

%{
static enum bt_notification_type *bt_py3_notif_types_from_py_list(
		PyObject *py_notif_types)
{
	enum bt_notification_type *notification_types = NULL;
	size_t i;

	BT_ASSERT(!PyErr_Occurred());

	if (py_notif_types == Py_None) {
		goto end;
	}

	BT_ASSERT(PyList_Check(py_notif_types));
	notification_types = g_new0(enum bt_notification_type,
		PyList_Size(py_notif_types) + 1);
	BT_ASSERT(notification_types);
	notification_types[PyList_Size(py_notif_types)] =
		BT_NOTIFICATION_TYPE_SENTINEL;

	for (i = 0; i < PyList_Size(py_notif_types); i++) {
		PyObject *item = PyList_GetItem(py_notif_types, i);
		long value;
		int overflow;

		BT_ASSERT(item);
		BT_ASSERT(PyLong_Check(item));
		value = PyLong_AsLongAndOverflow(item, &overflow);
		BT_ASSERT(overflow == 0);
		notification_types[i] = value;
	}

end:
	return notification_types;
}
%}

/* Per-module interface files */
%include "native_btccpriomap.i"
%include "native_btclockclass.i"
%include "native_btcomponent.i"
%include "native_btcomponentclass.i"
%include "native_btconnection.i"
%include "native_btctfwriter.i"
%include "native_btevent.i"
%include "native_bteventclass.i"
%include "native_btfields.i"
%include "native_btft.i"
%include "native_btgraph.i"
%include "native_btlogging.i"
%include "native_btnotification.i"
%include "native_btnotifiter.i"
%include "native_btpacket.i"
%include "native_btplugin.i"
%include "native_btport.i"
%include "native_btqueryexec.i"
%include "native_btref.i"
%include "native_btstream.i"
%include "native_btstreamclass.i"
%include "native_bttrace.i"
%include "native_btvalues.i"
%include "native_btversion.i"
