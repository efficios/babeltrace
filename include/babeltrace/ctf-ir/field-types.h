#ifndef BABELTRACE_CTF_IR_FIELD_TYPES_H
#define BABELTRACE_CTF_IR_FIELD_TYPES_H

/*
 * BabelTrace - CTF IR: Event field types
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

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_bool */
#include <babeltrace/types.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_field_type;
struct bt_field_path;
struct bt_field_type_signed_enumeration_mapping_ranges;
struct bt_field_type_unsigned_enumeration_mapping_ranges;

typedef const char * const *bt_field_type_enumeration_mapping_label_array;

enum bt_field_type_id {
	BT_FIELD_TYPE_ID_UNSIGNED_INTEGER,
	BT_FIELD_TYPE_ID_SIGNED_INTEGER,
	BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION,
	BT_FIELD_TYPE_ID_SIGNED_ENUMERATION,
	BT_FIELD_TYPE_ID_REAL,
	BT_FIELD_TYPE_ID_STRING,
	BT_FIELD_TYPE_ID_STRUCTURE,
	BT_FIELD_TYPE_ID_STATIC_ARRAY,
	BT_FIELD_TYPE_ID_DYNAMIC_ARRAY,
	BT_FIELD_TYPE_ID_VARIANT,
};

enum bt_field_type_integer_preferred_display_base {
	BT_FIELD_TYPE_INTEGER_PREFERRED_DISPLAY_BASE_BINARY,
	BT_FIELD_TYPE_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL,
	BT_FIELD_TYPE_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
	BT_FIELD_TYPE_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL,
};

extern enum bt_field_type_id bt_field_type_get_type_id(
		struct bt_field_type *field_type);

extern struct bt_field_type *bt_field_type_unsigned_integer_create(void);

extern struct bt_field_type *bt_field_type_signed_integer_create(void);

extern uint64_t bt_field_type_integer_get_field_value_range(
		struct bt_field_type *field_type);

extern int bt_field_type_integer_set_field_value_range(
		struct bt_field_type *field_type, uint64_t size);

extern enum bt_field_type_integer_preferred_display_base
bt_field_type_integer_get_preferred_display_base(
		struct bt_field_type *field_type);

extern int bt_field_type_integer_set_preferred_display_base(
		struct bt_field_type *field_type,
		enum bt_field_type_integer_preferred_display_base base);

extern struct bt_field_type *bt_field_type_real_create(void);

extern bt_bool bt_field_type_real_is_single_precision(
		struct bt_field_type *field_type);

extern int bt_field_type_real_set_is_single_precision(
		struct bt_field_type *field_type,
		bt_bool is_single_precision);

extern struct bt_field_type *bt_field_type_unsigned_enumeration_create(void);

extern struct bt_field_type *bt_field_type_signed_enumeration_create(void);

extern uint64_t bt_field_type_enumeration_get_mapping_count(
		struct bt_field_type *field_type);

extern void bt_field_type_unsigned_enumeration_borrow_mapping_by_index(
		struct bt_field_type *field_type, uint64_t index,
		const char **label,
		struct bt_field_type_unsigned_enumeration_mapping_ranges **ranges);

extern void bt_field_type_signed_enumeration_borrow_mapping_by_index(
		struct bt_field_type *field_type, uint64_t index,
		const char **label,
		struct bt_field_type_signed_enumeration_mapping_ranges **ranges);

extern uint64_t bt_field_type_unsigned_enumeration_mapping_ranges_get_range_count(
		struct bt_field_type_unsigned_enumeration_mapping_ranges *ranges);

extern uint64_t bt_field_type_signed_enumeration_mapping_ranges_get_range_count(
		struct bt_field_type_signed_enumeration_mapping_ranges *ranges);

extern void bt_field_type_unsigned_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_type_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, uint64_t *lower, uint64_t *upper);

extern void bt_field_type_signed_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_type_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, int64_t *lower, int64_t *upper);

extern int bt_field_type_unsigned_enumeration_get_mapping_labels_by_value(
		struct bt_field_type *field_type, uint64_t value,
		bt_field_type_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern int bt_field_type_signed_enumeration_get_mapping_labels_by_value(
		struct bt_field_type *field_type, int64_t value,
		bt_field_type_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern int bt_field_type_unsigned_enumeration_map_range(
		struct bt_field_type *field_type, const char *label,
		uint64_t range_lower, uint64_t range_upper);

extern int bt_field_type_signed_enumeration_map_range(
		struct bt_field_type *field_type, const char *label,
		int64_t range_lower, int64_t range_upper);

extern struct bt_field_type *bt_field_type_string_create(void);

extern struct bt_field_type *bt_field_type_structure_create(void);

extern uint64_t bt_field_type_structure_get_member_count(
		struct bt_field_type *field_type);

extern void bt_field_type_structure_borrow_member_by_index(
		struct bt_field_type *struct_field_type, uint64_t index,
		const char **name, struct bt_field_type **field_type);

extern
struct bt_field_type *bt_field_type_structure_borrow_member_field_type_by_name(
		struct bt_field_type *field_type, const char *name);

extern int bt_field_type_structure_append_member(
		struct bt_field_type *struct_field_type, const char *name,
		struct bt_field_type *field_type);

extern struct bt_field_type *bt_field_type_static_array_create(
		struct bt_field_type *elem_field_type,
		uint64_t length);

extern struct bt_field_type *bt_field_type_dynamic_array_create(
		struct bt_field_type *elem_field_type);

extern struct bt_field_type *bt_field_type_array_borrow_element_field_type(
		struct bt_field_type *field_type);

extern uint64_t bt_field_type_static_array_get_length(
		struct bt_field_type *field_type);

extern struct bt_field_path *
bt_field_type_dynamic_array_borrow_length_field_path(
		struct bt_field_type *field_type);

extern int bt_field_type_dynamic_array_set_length_field_type(
		struct bt_field_type *field_type,
		struct bt_field_type *length_field_type);

extern struct bt_field_type *bt_field_type_variant_create(void);

extern struct bt_field_path *
bt_field_type_variant_borrow_selector_field_path(
		struct bt_field_type *field_type);

extern int bt_field_type_variant_set_selector_field_type(
		struct bt_field_type *field_type,
		struct bt_field_type *selector_field_type);

extern uint64_t bt_field_type_variant_get_option_count(
		struct bt_field_type *field_type);

extern void bt_field_type_variant_borrow_option_by_index(
		struct bt_field_type *variant_field_type, uint64_t index,
		const char **name, struct bt_field_type **field_type);

extern
struct bt_field_type *bt_field_type_variant_borrow_option_field_type_by_name(
		struct bt_field_type *field_type,
		const char *name);

extern int bt_field_type_variant_append_option(
		struct bt_field_type *var_field_type,
		const char *name, struct bt_field_type *field_type);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_H */
