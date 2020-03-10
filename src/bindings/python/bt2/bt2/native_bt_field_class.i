/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
