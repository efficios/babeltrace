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

%{
typedef const unsigned char *BTUUID;
%}

#ifndef SWIGPYTHON
# error Unsupported output language
#endif

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
%typemap(in, numinputs=0) struct bt_ctf_field_type **BTOUTFT (struct bt_ctf_field_type *temp_ft = NULL) {
	$1 = &temp_ft;
}

%typemap(argout) struct bt_ctf_field_type **BTOUTFT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_ctf_field_type, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
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

/* Per-module interface files */
%include "native_btclockclass.i"
%include "native_btevent.i"
%include "native_bteventclass.i"
%include "native_btfields.i"
%include "native_btft.i"
%include "native_btpacket.i"
%include "native_btref.i"
%include "native_btstream.i"
%include "native_btstreamclass.i"
%include "native_bttrace.i"
%include "native_btvalues.i"
%include "native_btctfwriter.i"
%include "native_btcomponentclass.i"
%include "native_btcomponent.i"
%include "native_btnotifiter.i"
%include "native_btnotification.i"
%include "native_btplugin.i"
