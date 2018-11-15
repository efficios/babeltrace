#ifndef BABELTRACE_TRACE_IR_FIELD_CLASSES_H
#define BABELTRACE_TRACE_IR_FIELD_CLASSES_H

/*
 * BabelTrace - Trace IR: Event field classes
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_object_get_ref() */
#include <babeltrace/object.h>

/* For bt_bool */
#include <babeltrace/types.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_field_class;
struct bt_field_path;
struct bt_field_class_signed_enumeration_mapping_ranges;
struct bt_field_class_unsigned_enumeration_mapping_ranges;

typedef const char * const *bt_field_class_enumeration_mapping_label_array;

enum bt_field_class_type {
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
};

enum bt_field_class_integer_preferred_display_base {
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL,
};

extern enum bt_field_class_type bt_field_class_get_type(
		struct bt_field_class *field_class);

extern struct bt_field_class *bt_field_class_unsigned_integer_create(void);

extern struct bt_field_class *bt_field_class_signed_integer_create(void);

extern uint64_t bt_field_class_integer_get_field_value_range(
		struct bt_field_class *field_class);

extern int bt_field_class_integer_set_field_value_range(
		struct bt_field_class *field_class, uint64_t size);

extern enum bt_field_class_integer_preferred_display_base
bt_field_class_integer_get_preferred_display_base(
		struct bt_field_class *field_class);

extern int bt_field_class_integer_set_preferred_display_base(
		struct bt_field_class *field_class,
		enum bt_field_class_integer_preferred_display_base base);

extern struct bt_field_class *bt_field_class_real_create(void);

extern bt_bool bt_field_class_real_is_single_precision(
		struct bt_field_class *field_class);

extern int bt_field_class_real_set_is_single_precision(
		struct bt_field_class *field_class,
		bt_bool is_single_precision);

extern struct bt_field_class *bt_field_class_unsigned_enumeration_create(void);

extern struct bt_field_class *bt_field_class_signed_enumeration_create(void);

extern uint64_t bt_field_class_enumeration_get_mapping_count(
		struct bt_field_class *field_class);

extern void bt_field_class_unsigned_enumeration_borrow_mapping_by_index(
		struct bt_field_class *field_class, uint64_t index,
		const char **label,
		struct bt_field_class_unsigned_enumeration_mapping_ranges **ranges);

extern void bt_field_class_signed_enumeration_borrow_mapping_by_index(
		struct bt_field_class *field_class, uint64_t index,
		const char **label,
		struct bt_field_class_signed_enumeration_mapping_ranges **ranges);

extern uint64_t bt_field_class_unsigned_enumeration_mapping_ranges_get_range_count(
		struct bt_field_class_unsigned_enumeration_mapping_ranges *ranges);

extern uint64_t bt_field_class_signed_enumeration_mapping_ranges_get_range_count(
		struct bt_field_class_signed_enumeration_mapping_ranges *ranges);

extern void bt_field_class_unsigned_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_class_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, uint64_t *lower, uint64_t *upper);

extern void bt_field_class_signed_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_class_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, int64_t *lower, int64_t *upper);

extern int bt_field_class_unsigned_enumeration_get_mapping_labels_by_value(
		struct bt_field_class *field_class, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern int bt_field_class_signed_enumeration_get_mapping_labels_by_value(
		struct bt_field_class *field_class, int64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern int bt_field_class_unsigned_enumeration_map_range(
		struct bt_field_class *field_class, const char *label,
		uint64_t range_lower, uint64_t range_upper);

extern int bt_field_class_signed_enumeration_map_range(
		struct bt_field_class *field_class, const char *label,
		int64_t range_lower, int64_t range_upper);

extern struct bt_field_class *bt_field_class_string_create(void);

extern struct bt_field_class *bt_field_class_structure_create(void);

extern uint64_t bt_field_class_structure_get_member_count(
		struct bt_field_class *field_class);

extern void bt_field_class_structure_borrow_member_by_index(
		struct bt_field_class *struct_field_class, uint64_t index,
		const char **name, struct bt_field_class **field_class);

extern
struct bt_field_class *bt_field_class_structure_borrow_member_field_class_by_name(
		struct bt_field_class *field_class, const char *name);

extern int bt_field_class_structure_append_member(
		struct bt_field_class *struct_field_class, const char *name,
		struct bt_field_class *field_class);

extern struct bt_field_class *bt_field_class_static_array_create(
		struct bt_field_class *elem_field_class,
		uint64_t length);

extern struct bt_field_class *bt_field_class_dynamic_array_create(
		struct bt_field_class *elem_field_class);

extern struct bt_field_class *bt_field_class_array_borrow_element_field_class(
		struct bt_field_class *field_class);

extern uint64_t bt_field_class_static_array_get_length(
		struct bt_field_class *field_class);

extern struct bt_field_path *
bt_field_class_dynamic_array_borrow_length_field_path(
		struct bt_field_class *field_class);

extern int bt_field_class_dynamic_array_set_length_field_class(
		struct bt_field_class *field_class,
		struct bt_field_class *length_field_class);

extern struct bt_field_class *bt_field_class_variant_create(void);

extern struct bt_field_path *
bt_field_class_variant_borrow_selector_field_path(
		struct bt_field_class *field_class);

extern int bt_field_class_variant_set_selector_field_class(
		struct bt_field_class *field_class,
		struct bt_field_class *selector_field_class);

extern uint64_t bt_field_class_variant_get_option_count(
		struct bt_field_class *field_class);

extern void bt_field_class_variant_borrow_option_by_index(
		struct bt_field_class *variant_field_class, uint64_t index,
		const char **name, struct bt_field_class **field_class);

extern
struct bt_field_class *bt_field_class_variant_borrow_option_field_class_by_name(
		struct bt_field_class *field_class,
		const char *name);

extern int bt_field_class_variant_append_option(
		struct bt_field_class *var_field_class,
		const char *name, struct bt_field_class *field_class);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_FIELD_CLASSES_H */
