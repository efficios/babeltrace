#ifndef BABELTRACE_TRACE_IR_FIELD_CLASSES_CONST_H
#define BABELTRACE_TRACE_IR_FIELD_CLASSES_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

/*
 * For bt_bool, bt_field_class, bt_field_path,
 * bt_field_class_enumeration_mapping,
 * bt_field_class_unsigned_enumeration_mapping,
 * bt_field_class_signed_enumeration_mapping,
 * bt_field_class_enumeration_mapping_label_array, __BT_UPCAST_CONST
 */
#include <babeltrace/types.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

static inline
const bt_field_class_enumeration_mapping *
bt_field_class_unsigned_enumeration_mapping_as_mapping_const(
		const bt_field_class_unsigned_enumeration_mapping *mapping)
{
	return __BT_UPCAST_CONST(bt_field_class_enumeration_mapping, mapping);
}

static inline
const bt_field_class_enumeration_mapping *
bt_field_class_signed_enumeration_mapping_as_mapping_const(
		const bt_field_class_signed_enumeration_mapping *mapping)
{
	return __BT_UPCAST_CONST(bt_field_class_enumeration_mapping, mapping);
}

extern const char *bt_field_class_enumeration_mapping_get_label(
		const bt_field_class_enumeration_mapping *mapping);

extern uint64_t bt_field_class_enumeration_mapping_get_range_count(
		const bt_field_class_enumeration_mapping *mapping);

extern void
bt_field_class_unsigned_enumeration_mapping_get_range_by_index(
		const bt_field_class_unsigned_enumeration_mapping *mapping,
		uint64_t index, uint64_t *lower, uint64_t *upper);

extern void
bt_field_class_signed_enumeration_mapping_get_range_by_index(
		const bt_field_class_signed_enumeration_mapping *mapping,
		uint64_t index, int64_t *lower, int64_t *upper);

extern bt_field_class_status
bt_field_class_unsigned_enumeration_get_mapping_labels_by_value(
		const bt_field_class *field_class, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern bt_field_class_status
bt_field_class_signed_enumeration_get_mapping_labels_by_value(
		const bt_field_class *field_class, int64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

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

#define BT_FIELD_CLASS_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_field_class_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_FIELD_CLASS_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_field_class_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)


#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_FIELD_CLASSES_CONST_H */
