#ifndef BABELTRACE2_TRACE_IR_FIELD_H
#define BABELTRACE2_TRACE_IR_FIELD_H

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

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void bt_field_bool_set_value(bt_field *field, bt_bool value);

extern void bt_field_integer_signed_set_value(bt_field *field,
		int64_t value);

extern void bt_field_integer_unsigned_set_value(bt_field *field,
		uint64_t value);

extern void bt_field_real_set_value(bt_field *field, double value);

typedef enum bt_field_string_set_value_status {
	BT_FIELD_STRING_SET_VALUE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_STRING_SET_VALUE_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_string_set_value_status;

extern bt_field_string_set_value_status bt_field_string_set_value(
		bt_field *field, const char *value);

typedef enum bt_field_string_append_status {
	BT_FIELD_STRING_APPEND_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_STRING_APPEND_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_string_append_status;

extern bt_field_string_append_status bt_field_string_append(
		bt_field *field, const char *value);

extern bt_field_string_append_status bt_field_string_append_with_length(
		bt_field *field, const char *value, uint64_t length);

extern void bt_field_string_clear(bt_field *field);

extern bt_field *bt_field_structure_borrow_member_field_by_index(
		bt_field *field, uint64_t index);

extern bt_field *bt_field_structure_borrow_member_field_by_name(
		bt_field *field, const char *name);

extern bt_field *bt_field_array_borrow_element_field_by_index(
		bt_field *field, uint64_t index);

typedef enum bt_field_array_dynamic_set_length_status {
	BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_array_dynamic_set_length_status;

extern bt_field_array_dynamic_set_length_status
bt_field_array_dynamic_set_length(
		bt_field *field, uint64_t length);

extern void bt_field_option_set_has_field(bt_field *field, bt_bool has_field);

extern bt_field *bt_field_option_borrow_field(bt_field *field);

typedef enum bt_field_variant_select_option_field_by_index_status {
	BT_FIELD_VARIANT_SELECT_OPTION_FIELD_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_field_variant_select_option_field_by_index_status;

extern bt_field_variant_select_option_field_by_index_status
bt_field_variant_select_option_field_by_index(
		bt_field *field, uint64_t index);

extern bt_field *bt_field_variant_borrow_selected_option_field(
		bt_field *field);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_FIELD_H */
