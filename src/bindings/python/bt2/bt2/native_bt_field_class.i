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

/* Parameter names seem to be required for multi-argument typemaps to match. */
%typemap(in, numinputs=0)
	(bt_field_class_enumeration_mapping_label_array *labels, uint64_t *count)
	(bt_field_class_enumeration_mapping_label_array temp_array, uint64_t temp_label_count = 0) {
	$1 = &temp_array;
	$2 = &temp_label_count;
}

%typemap(argout)
	(bt_field_class_enumeration_mapping_label_array *labels, uint64_t *count) {
	if (*$1) {
		PyObject *py_label_list = PyList_New(*$2);
		uint64_t i;

		for (i = 0; i < *$2; i++) {
			PyList_SET_ITEM(py_label_list, i, PyUnicode_FromString((*$1)[i]));
		}

		$result = SWIG_Python_AppendOutput($result, py_label_list);
	} else {
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

%include <babeltrace2/trace-ir/field-class.h>
