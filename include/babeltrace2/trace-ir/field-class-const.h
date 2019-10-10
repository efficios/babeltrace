#ifndef BABELTRACE2_TRACE_IR_FIELD_CLASS_CONST_H
#define BABELTRACE2_TRACE_IR_FIELD_CLASS_CONST_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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
 */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>
#include <stddef.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_field_class_type {
	BT_FIELD_CLASS_TYPE_BOOL						= 1ULL << 0,
	BT_FIELD_CLASS_TYPE_BIT_ARRAY						= 1ULL << 1,
	BT_FIELD_CLASS_TYPE_INTEGER						= 1ULL << 2,
	BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER					= (1ULL << 3) | BT_FIELD_CLASS_TYPE_INTEGER,
	BT_FIELD_CLASS_TYPE_SIGNED_INTEGER					= (1ULL << 4) | BT_FIELD_CLASS_TYPE_INTEGER,
	BT_FIELD_CLASS_TYPE_ENUMERATION						= 1ULL << 5,
	BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION				= BT_FIELD_CLASS_TYPE_ENUMERATION | BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER,
	BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION					= BT_FIELD_CLASS_TYPE_ENUMERATION | BT_FIELD_CLASS_TYPE_SIGNED_INTEGER,
	BT_FIELD_CLASS_TYPE_REAL						= 1ULL << 6,
	BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL				= (1ULL << 7) | BT_FIELD_CLASS_TYPE_REAL,
	BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL				= (1ULL << 8) | BT_FIELD_CLASS_TYPE_REAL,
	BT_FIELD_CLASS_TYPE_STRING						= 1ULL << 9,
	BT_FIELD_CLASS_TYPE_STRUCTURE						= 1ULL << 10,
	BT_FIELD_CLASS_TYPE_ARRAY						= 1ULL << 11,
	BT_FIELD_CLASS_TYPE_STATIC_ARRAY					= (1ULL << 12) | BT_FIELD_CLASS_TYPE_ARRAY,
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY					= (1ULL << 13) | BT_FIELD_CLASS_TYPE_ARRAY,
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD			= (1ULL << 14) | BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD			= (1ULL << 15) | BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,
	BT_FIELD_CLASS_TYPE_OPTION						= 1ULL << 16,
	BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD			= (1ULL << 17) | BT_FIELD_CLASS_TYPE_OPTION,
	BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD				= (1ULL << 18) | BT_FIELD_CLASS_TYPE_OPTION,
	BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD			= (1ULL << 19) | BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD,
	BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD			= (1ULL << 20) | BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD,
	BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 21) | BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD,
	BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 22) | BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD,
	BT_FIELD_CLASS_TYPE_VARIANT						= 1ULL << 23,
	BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD			= (1ULL << 24) | BT_FIELD_CLASS_TYPE_VARIANT,
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD				= (1ULL << 25) | BT_FIELD_CLASS_TYPE_VARIANT,
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD			= (1ULL << 26) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD,
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD	= (1ULL << 27) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD,
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 28) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD,

	/*
	 * Make sure the enumeration type is a 64-bit integer in case
	 * the project needs field class types in the future.
	 *
	 * This is not part of the API.
	 */
	__BT_FIELD_CLASS_TYPE_BIG_VALUE						= 1ULL << 62,
} bt_field_class_type;

typedef enum bt_field_class_integer_preferred_display_base {
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY		= 2,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL		= 8,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL		= 10,
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL	= 16,
} bt_field_class_integer_preferred_display_base;

extern bt_field_class_type bt_field_class_get_type(
		const bt_field_class *field_class);

static inline
bt_bool bt_field_class_type_is(const bt_field_class_type type,
		const bt_field_class_type type_to_check)
{
	return (type & type_to_check) == type_to_check;
}

extern const bt_value *bt_field_class_borrow_user_attributes_const(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_bit_array_get_length(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_integer_get_field_value_range(
		const bt_field_class *field_class);

extern bt_field_class_integer_preferred_display_base
bt_field_class_integer_get_preferred_display_base(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_enumeration_get_mapping_count(
		const bt_field_class *field_class);

extern const bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_label_const(
		const bt_field_class *field_class, const char *label);

extern const bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_label_const(
		const bt_field_class *field_class, const char *label);

static inline
const bt_field_class_enumeration_mapping *
bt_field_class_enumeration_unsigned_mapping_as_mapping_const(
		const bt_field_class_enumeration_unsigned_mapping *mapping)
{
	return __BT_UPCAST_CONST(bt_field_class_enumeration_mapping, mapping);
}

static inline
const bt_field_class_enumeration_mapping *
bt_field_class_enumeration_signed_mapping_as_mapping_const(
		const bt_field_class_enumeration_signed_mapping *mapping)
{
	return __BT_UPCAST_CONST(bt_field_class_enumeration_mapping, mapping);
}

extern const char *bt_field_class_enumeration_mapping_get_label(
		const bt_field_class_enumeration_mapping *mapping);

extern const bt_integer_range_set_unsigned *
bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
		const bt_field_class_enumeration_unsigned_mapping *mapping);

extern const bt_integer_range_set_signed *
bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
		const bt_field_class_enumeration_signed_mapping *mapping);

typedef enum bt_field_class_enumeration_get_mapping_labels_for_value_status {
	BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_class_enumeration_get_mapping_labels_for_value_status;

extern bt_field_class_enumeration_get_mapping_labels_for_value_status
bt_field_class_enumeration_unsigned_get_mapping_labels_for_value(
		const bt_field_class *field_class, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern bt_field_class_enumeration_get_mapping_labels_for_value_status
bt_field_class_enumeration_signed_get_mapping_labels_for_value(
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

extern const bt_value *bt_field_class_structure_member_borrow_user_attributes_const(
		const bt_field_class_structure_member *member);

extern const bt_field_class *
bt_field_class_array_borrow_element_field_class_const(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_array_static_get_length(
		const bt_field_class *field_class);

extern const bt_field_path *
bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
		const bt_field_class *field_class);

extern const bt_field_class *
bt_field_class_option_borrow_field_class_const(
		const bt_field_class *field_class);

extern const bt_field_path *
bt_field_class_option_with_selector_field_borrow_selector_field_path_const(
		const bt_field_class *field_class);

extern bt_bool
bt_field_class_option_with_selector_field_bool_selector_is_reversed(
		const bt_field_class *field_class);

extern const bt_integer_range_set_unsigned *
bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const(
		const bt_field_class *field_class);

extern const bt_integer_range_set_signed *
bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const(
		const bt_field_class *field_class);

extern uint64_t bt_field_class_variant_get_option_count(
		const bt_field_class *field_class);

extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

extern const bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

extern const bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

extern const bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

extern const char *bt_field_class_variant_option_get_name(
		const bt_field_class_variant_option *option);

extern const bt_field_class *
bt_field_class_variant_option_borrow_field_class_const(
		const bt_field_class_variant_option *option);

extern const bt_value *bt_field_class_variant_option_borrow_user_attributes_const(
		const bt_field_class_variant_option *option);

extern const bt_field_path *
bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
		const bt_field_class *field_class);

extern const bt_integer_range_set_unsigned *
bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const(
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *option);

static inline
const bt_field_class_variant_option *
bt_field_class_variant_with_selector_field_integer_unsigned_option_as_option_const(
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *option)
{
	return __BT_UPCAST_CONST(bt_field_class_variant_option, option);
}

extern const bt_integer_range_set_signed *
bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const(
		const bt_field_class_variant_with_selector_field_integer_signed_option *option);

static inline
const bt_field_class_variant_option *
bt_field_class_variant_with_selector_field_integer_signed_option_as_option_const(
		const bt_field_class_variant_with_selector_field_integer_signed_option *option)
{
	return __BT_UPCAST_CONST(bt_field_class_variant_option, option);
}

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

#endif /* BABELTRACE2_TRACE_IR_FIELD_CLASS_CONST_H */
