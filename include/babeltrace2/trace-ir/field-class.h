#ifndef BABELTRACE2_TRACE_IR_FIELD_CLASS_H
#define BABELTRACE2_TRACE_IR_FIELD_CLASS_H

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
#include <babeltrace2/trace-ir/field-class-const.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_field_class *bt_field_class_bool_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_integer_unsigned_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_integer_signed_create(
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

extern bt_field_class *bt_field_class_enumeration_unsigned_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_enumeration_signed_create(
		bt_trace_class *trace_class);

typedef enum bt_field_class_enumeration_add_mapping_status {
	BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_class_enumeration_add_mapping_status;

extern bt_field_class_enumeration_add_mapping_status
bt_field_class_enumeration_unsigned_add_mapping(
		bt_field_class *field_class, const char *label,
		const bt_integer_range_set_unsigned *range_set);

extern bt_field_class_enumeration_add_mapping_status
bt_field_class_enumeration_signed_add_mapping(
		bt_field_class *field_class, const char *label,
		const bt_integer_range_set_signed *range_set);

extern bt_field_class *bt_field_class_string_create(
		bt_trace_class *trace_class);

extern bt_field_class *bt_field_class_structure_create(
		bt_trace_class *trace_class);

typedef enum bt_field_class_structure_append_member_status {
	BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_class_structure_append_member_status;

extern bt_field_class_structure_append_member_status
bt_field_class_structure_append_member(
		bt_field_class *struct_field_class,
		const char *name, bt_field_class *field_class);

extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index(
		bt_field_class *field_class, uint64_t index);

extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name(
		bt_field_class *field_class, const char *name);

extern bt_field_class *bt_field_class_array_static_create(
		bt_trace_class *trace_class,
		bt_field_class *elem_field_class, uint64_t length);

extern bt_field_class *bt_field_class_array_dynamic_create(
		bt_trace_class *trace_class,
		bt_field_class *elem_field_class,
		bt_field_class *length_field_class);

extern bt_field_class *bt_field_class_array_borrow_element_field_class(
		bt_field_class *field_class);

extern bt_field_class *bt_field_class_option_create(
		bt_trace_class *trace_class,
		bt_field_class *content_field_class,
		bt_field_class *selector_field_class);

extern bt_field_class *bt_field_class_variant_create(
		bt_trace_class *trace_class,
		bt_field_class *selector_field_class);

typedef enum bt_field_class_variant_without_selector_append_option_status {
	BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_APPEND_OPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_APPEND_OPTION_STATUS_OK			= __BT_FUNC_STATUS_OK,
} bt_field_class_variant_without_selector_append_option_status;

extern bt_field_class_variant_without_selector_append_option_status
bt_field_class_variant_without_selector_append_option(
		bt_field_class *var_field_class, const char *name,
		bt_field_class *field_class);

typedef enum bt_field_class_variant_with_selector_append_option_status {
	BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_APPEND_OPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_APPEND_OPTION_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_class_variant_with_selector_append_option_status;

extern bt_field_class_variant_with_selector_append_option_status
bt_field_class_variant_with_selector_unsigned_append_option(
		bt_field_class *var_field_class, const char *name,
		bt_field_class *field_class,
		const bt_integer_range_set_unsigned *range_set);

extern bt_field_class_variant_with_selector_append_option_status
bt_field_class_variant_with_selector_signed_append_option(
		bt_field_class *var_field_class, const char *name,
		bt_field_class *field_class,
		const bt_integer_range_set_signed *range_set);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_FIELD_CLASS_H */
