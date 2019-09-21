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

/* Output argument typemap for self port output (always appends) */
%typemap(in, numinputs=0)
	(bt_self_component_port_input **)
	(bt_self_component_port_input *temp_self_port = NULL) {
	$1 = &temp_self_port;
}

%typemap(argout) bt_self_component_port_input ** {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_self_component_port_input, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for self port output (always appends) */
%typemap(in, numinputs=0)
	(bt_self_component_port_output **)
	(bt_self_component_port_output *temp_self_port = NULL) {
	$1 = &temp_self_port;
}

%typemap(argout) (bt_self_component_port_output **) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_self_component_port_output, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Typemaps used for user data attached to self component ports. */

/*
 * The user data Python object is kept as the user data of the port, we pass
 * the PyObject pointer directly to the port creation function.
 */
%typemap(in) void *user_data {
	$1 = $input;
}

/*
 * The port, if created successfully, now owns a reference to the Python object,
 * we reflect that here.
 */
%typemap(argout) void *user_data {
	if (PyLong_AsLong($result) == __BT_FUNC_STATUS_OK) {
		Py_INCREF($1);
	}
}

%include <babeltrace2/graph/component.h>
%include <babeltrace2/graph/self-component.h>

/*
 * This type map relies on the rather common "user_data" name, so don't pollute
 * the typemap namespace.
 */
%clear void *user_data;
