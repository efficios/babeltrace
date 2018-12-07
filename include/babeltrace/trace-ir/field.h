#ifndef BABELTRACE_TRACE_IR_FIELDS_H
#define BABELTRACE_TRACE_IR_FIELDS_H

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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_field;
struct bt_field_class;

extern void bt_field_signed_integer_set_value(struct bt_field *field,
		int64_t value);

extern void bt_field_unsigned_integer_set_value(struct bt_field *field,
		uint64_t value);

extern void bt_field_real_set_value(struct bt_field *field, double value);

extern int bt_field_string_set_value(struct bt_field *field, const char *value);

extern int bt_field_string_append(struct bt_field *field, const char *value);

extern int bt_field_string_append_with_length(struct bt_field *field,
		const char *value, uint64_t length);

extern int bt_field_string_clear(struct bt_field *field);

extern struct bt_field *bt_field_structure_borrow_member_field_by_index(
		struct bt_field *field, uint64_t index);

extern struct bt_field *bt_field_structure_borrow_member_field_by_name(
		struct bt_field *field, const char *name);

extern struct bt_field *bt_field_array_borrow_element_field_by_index(
		struct bt_field *field, uint64_t index);

extern int bt_field_dynamic_array_set_length(struct bt_field *field,
		uint64_t length);

extern int bt_field_variant_select_option_field(struct bt_field *field,
		uint64_t index);

extern struct bt_field *bt_field_variant_borrow_selected_option_field(
		struct bt_field *field);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_FIELDS_H */
