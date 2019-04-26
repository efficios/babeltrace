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

%typemap(in, numinputs=0)
	(bt_field_class_enumeration_mapping_label_array *LABELARRAY, uint64_t *LABELCOUNT)
	(bt_field_class_enumeration_mapping_label_array temp_array, uint64_t temp_label_count = 0) {
	$1 = &temp_array;
	$2 = &temp_label_count;
}

%typemap(argout)
	(bt_field_class_enumeration_mapping_label_array *LABELARRAY, uint64_t *LABELCOUNT) {
	if (*$1) {
		PyObject *py_label_list = PyList_New(*$2);
		for (int i = 0; i < *$2; i++) {
			PyList_SET_ITEM(py_label_list, i, PyUnicode_FromString((*$1)[i]));
		}

		$result = SWIG_Python_AppendOutput($result, py_label_list);
	} else {
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0)
	(const bt_field_class_signed_enumeration_mapping_ranges **ENUM_RANGE_MAPPING)
	(bt_field_class_signed_enumeration_mapping_ranges *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout)
	(const bt_field_class_signed_enumeration_mapping_ranges **ENUM_RANGE_MAPPING) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_field_class_signed_enumeration_mapping_ranges, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0)
	(const bt_field_class_unsigned_enumeration_mapping_ranges **ENUM_RANGE_MAPPING)
	(bt_field_class_unsigned_enumeration_mapping_ranges *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout)
	(const bt_field_class_unsigned_enumeration_mapping_ranges **ENUM_RANGE_MAPPING ) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_field_class_unsigned_enumeration_mapping_ranges, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* From field-class-const.h */

typedef enum bt_field_class_status {
	BT_FIELD_CLASS_STATUS_OK = 0,
	BT_FIELD_CLASS_STATUS_NOMEM = -12,
} bt_field_class_status;

typedef enum bt_field_class_type {
	BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER,
	BT_FIELD_CLASS_TYPE_SIGNED_INTEGER,
	BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION,
	BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION,
	BT_FIELD_CLASS_TYPE_REAL,
	BT_FIELD_CLASS_TYPE_STRING,
	BT_FIELD_CLASS_TYPE_STRUCTURE,
	BT_FIELD_CLASS_TYPE_STATIC_ARRAY,
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,
	BT_FIELD_CLASS_TYPE_VARIANT,
} bt_field_class_type;

typedef enum bt_field_class_integer_preferred_display_base {
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL,
} bt_field_class_integer_preferred_display_base;

extern bt_field_class_type bt_field_class_get_type(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_integer_get_field_value_range(
		const bt_field_class *field_class);

extern bt_field_class_integer_preferred_display_base
bt_field_class_integer_get_preferred_display_base(
		const bt_field_class *field_class);

extern bt_bool bt_field_class_real_is_single_precision(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_enumeration_get_mapping_count(
		const bt_field_class *field_class);

extern const bt_field_class_unsigned_enumeration_mapping *
bt_field_class_unsigned_enumeration_borrow_mapping_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_signed_enumeration_mapping *
bt_field_class_signed_enumeration_borrow_mapping_by_index_const(
		const bt_field_class *field_class, uint64_t index);

const bt_field_class_enumeration_mapping *
bt_field_class_unsigned_enumeration_mapping_as_mapping_const(
		const bt_field_class_unsigned_enumeration_mapping *mapping);

const bt_field_class_enumeration_mapping *
bt_field_class_signed_enumeration_mapping_as_mapping_const(
		const bt_field_class_signed_enumeration_mapping *mapping);

extern const char *bt_field_class_enumeration_mapping_get_label(
		const bt_field_class_enumeration_mapping *mapping);

extern uint64_t bt_field_class_enumeration_mapping_get_range_count(
		const bt_field_class_enumeration_mapping *mapping);

extern void
bt_field_class_unsigned_enumeration_mapping_get_range_by_index(
		const bt_field_class_unsigned_enumeration_mapping *mapping,
		uint64_t index, uint64_t *OUT, uint64_t *OUT);

extern void
bt_field_class_signed_enumeration_mapping_get_range_by_index(
		const bt_field_class_signed_enumeration_mapping *mapping,
		uint64_t index, int64_t *OUT, int64_t *OUT);

extern bt_field_class_status
bt_field_class_unsigned_enumeration_get_mapping_labels_by_value(
		const bt_field_class *field_class, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *LABELARRAY,
		uint64_t *LABELCOUNT);

extern bt_field_class_status
bt_field_class_signed_enumeration_get_mapping_labels_by_value(
		const bt_field_class *field_class, int64_t value,
		bt_field_class_enumeration_mapping_label_array *LABELARRAY,
		uint64_t *LABELCOUNT);

extern uint64_t bt_field_class_structure_get_member_count(
		const bt_field_class *field_class);

extern const bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name_const(
		const bt_field_class *field_class, const char *name);

extern const char *bt_field_class_structure_member_get_name(
		const bt_field_class_structure_member *member);

extern const bt_field_class *
bt_field_class_structure_member_borrow_field_class_const(
		const bt_field_class_structure_member *member);

extern const bt_field_class *
bt_field_class_array_borrow_element_field_class_const(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_static_array_get_length(
		const bt_field_class *field_class);

extern const bt_field_path *
bt_field_class_dynamic_array_borrow_length_field_path_const(
		const bt_field_class *field_class);

extern const bt_field_path *
bt_field_class_variant_borrow_selector_field_path_const(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_variant_get_option_count(
		const bt_field_class *field_class);

extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

extern const char *bt_field_class_variant_option_get_name(
		const bt_field_class_variant_option *option);

extern const bt_field_class *
bt_field_class_variant_option_borrow_field_class_const(
		const bt_field_class_variant_option *option);

extern void bt_field_class_get_ref(const bt_field_class *field_class);

extern void bt_field_class_put_ref(const bt_field_class *field_class);

/* From field-class.h */

extern bt_field_class *bt_field_class_unsigned_integer_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_signed_integer_create(
		bt_trace_class *trace_class);

extern void bt_field_class_integer_set_field_value_range(
		bt_field_class *field_class, uint64_t size);

extern void bt_field_class_integer_set_preferred_display_base(
		bt_field_class *field_class,
		bt_field_class_integer_preferred_display_base base);

extern bt_field_class *bt_field_class_real_create(bt_trace_class *trace_class);

extern void bt_field_class_real_set_is_single_precision(
		bt_field_class *field_class,
		bt_bool is_single_precision);

extern bt_field_class *bt_field_class_unsigned_enumeration_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_signed_enumeration_create(
		bt_trace_class *trace_class);

extern bt_field_class_status bt_field_class_unsigned_enumeration_map_range(
		bt_field_class *field_class, const char *label,
		uint64_t range_lower, uint64_t range_upper);

extern bt_field_class_status bt_field_class_signed_enumeration_map_range(
		bt_field_class *field_class, const char *label,
		int64_t range_lower, int64_t range_upper);

extern bt_field_class *bt_field_class_string_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_structure_create(
		bt_trace_class *trace_class);

extern bt_field_class_status bt_field_class_structure_append_member(
		bt_field_class *struct_field_class,
		const char *name, bt_field_class *field_class);

extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index(
		bt_field_class *field_class, uint64_t index);

extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name(
		bt_field_class *field_class, const char *name);

extern bt_field_class *bt_field_class_static_array_create(
		bt_trace_class *trace_class,
		bt_field_class *elem_field_class, uint64_t length);

extern bt_field_class *bt_field_class_dynamic_array_create(
		bt_trace_class *trace_class,
		bt_field_class *elem_field_class);

extern bt_field_class *bt_field_class_array_borrow_element_field_class(
		bt_field_class *field_class);

extern bt_field_class_status
bt_field_class_dynamic_array_set_length_field_class(
		bt_field_class *field_class,
		bt_field_class *length_field_class);

extern bt_field_class *bt_field_class_variant_create(
		bt_trace_class *trace_class);

extern bt_field_class_status
bt_field_class_variant_set_selector_field_class(bt_field_class *field_class,
		bt_field_class *selector_field_class);

extern bt_field_class_status bt_field_class_variant_append_option(
		bt_field_class *var_field_class,
		const char *name, bt_field_class *field_class);

extern bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index(
		bt_field_class *field_class, uint64_t index);

extern bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name(
		bt_field_class *field_class, char *name);

extern bt_field_class *bt_field_class_variant_option_borrow_field_class(
		bt_field_class_variant_option *option);
