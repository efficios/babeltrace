#ifndef BABELTRACE_TRACE_IR_FIELDS_CONST_H
#define BABELTRACE_TRACE_IR_FIELDS_CONST_H

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

#include <stdint.h>

/* For enum bt_field_class_type */
#include <babeltrace/trace-ir/field-class.h>

/* For bt_field, bt_field_class */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const bt_field_class *bt_field_borrow_class_const(
		const bt_field *field);

extern enum bt_field_class_type bt_field_get_class_type(
		const bt_field *field);

extern int64_t bt_field_signed_integer_get_value(const bt_field *field);

extern uint64_t bt_field_unsigned_integer_get_value(
		const bt_field *field);

extern double bt_field_real_get_value(const bt_field *field);

extern int bt_field_unsigned_enumeration_get_mapping_labels(
		const bt_field *field,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern int bt_field_signed_enumeration_get_mapping_labels(
		const bt_field *field,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count);

extern const char *bt_field_string_get_value(const bt_field *field);

extern uint64_t bt_field_string_get_length(const bt_field *field);

extern const bt_field *
bt_field_structure_borrow_member_field_by_index_const(
		const bt_field *field, uint64_t index);

extern const bt_field *
bt_field_structure_borrow_member_field_by_name_const(
		const bt_field *field, const char *name);

extern uint64_t bt_field_array_get_length(const bt_field *field);

extern const bt_field *
bt_field_array_borrow_element_field_by_index_const(
		const bt_field *field, uint64_t index);

extern uint64_t bt_field_variant_get_selected_option_field_index(
		const bt_field *field);

extern const bt_field *
bt_field_variant_borrow_selected_option_field_const(
		const bt_field *field);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_FIELDS_CONST_H */
