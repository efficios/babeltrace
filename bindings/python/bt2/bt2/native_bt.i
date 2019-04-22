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
#include <babeltrace/property.h>
#include <babeltrace/assert-internal.h>

typedef const uint8_t *bt_uuid;
%}

typedef int bt_bool;

/* For uint*_t/int*_t */
%include "stdint.i"

/*
 * Remove `bt_` and `BT_` prefixes from function names, global variables and
 * enumeration items
 */
%rename("%(strip:[bt_])s", %$isfunction) "";
%rename("%(strip:[bt_])s", %$isvariable) "";
%rename("%(strip:[BT_])s", %$isenumitem) "";

/* Output argument typemap for string output (always appends) */
%typemap(in, numinputs=0) (const char **OUT) (char *temp_value) {
	$1 = &temp_value;
}

%typemap(argout) (const char **OUT) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_Python_str_FromChar(*$1));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0) (bt_value **OUT) (struct bt_value *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) (bt_value **OUT) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_value, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for initialized uint64_t output parameter (always appends) */
%typemap(in, numinputs=0) (uint64_t *OUT) (uint64_t temp) {
	$1 = &temp;
}

%typemap(argout) uint64_t *OUT {
	$result = SWIG_Python_AppendOutput(resultobj,
			SWIG_From_unsigned_SS_long_SS_long((*$1)));
}

/* Output argument typemap for initialized int64_t output parameter (always appends) */
%typemap(in, numinputs=0) (int64_t *OUT) (int64_t temp) {
	$1 = &temp;
}

%typemap(argout) (int64_t *OUT) {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_long_SS_long((*$1)));
}

/* Output argument typemap for initialized unsigned int output parameter (always appends) */
%typemap(in, numinputs=0) (unsigned int *OUT) (unsigned int temp) {
	$1 = &temp;
}

%typemap(argout) (unsigned int *OUT) {
	$result = SWIG_Python_AppendOutput(resultobj,
			SWIG_From_unsigned_SS_long_SS_long((uint64_t) (*$1)));
}
/* Output argument typemap for initialized double output parameter (always appends) */
%typemap(in, numinputs=0) (double *OUT) (double temp) {
	$1 = &temp;
}

%typemap(argout) (double *OUT) {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_int((*$1)));
}

/* Input argument typemap for UUID bytes */
%typemap(in) bt_uuid {
	$1 = (unsigned char *) PyBytes_AsString($input);
}

/* Output argument typemap for UUID bytes */
%typemap(out) bt_uuid {
	if (!$1) {
		Py_INCREF(Py_None);
		$result = Py_None;
	} else {
		$result = PyBytes_FromStringAndSize((const char *) $1, 16);
	}
}

/* Input argument typemap for bt_bool */
%typemap(in) bt_bool {
	$1 = PyObject_IsTrue($input);
}

/* Output argument typemap for bt_bool */
%typemap(out) bt_bool {
	if ($1 > 0) {
		$result = Py_True;
	} else {
		$result = Py_False;
	}
	Py_INCREF($result);
	return $result;
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

/* From property.h */

typedef enum bt_property_availability {
	BT_PROPERTY_AVAILABILITY_AVAILABLE,
	BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE,
} bt_property_availability;

/* Per-module interface files */
%include "native_bt_clock_class.i"
%include "native_bt_clock_snapshot.i"
%include "native_bt_component.i"
%include "native_bt_component_class.i"
%include "native_bt_connection.i"
%include "native_bt_event.i"
%include "native_bt_event_class.i"
%include "native_bt_field.i"
%include "native_bt_field_class.i"
%include "native_bt_field_path.i"
%include "native_bt_graph.i"
%include "native_bt_logging.i"
%include "native_bt_message.i"
%include "native_bt_notifier.i"
%include "native_bt_packet.i"
%include "native_bt_plugin.i"
%include "native_bt_port.i"
%include "native_bt_query_exec.i"
%include "native_bt_stream.i"
%include "native_bt_stream_class.i"
%include "native_bt_trace.i"
%include "native_bt_trace_class.i"
%include "native_bt_value.i"
%include "native_bt_version.i"
