#ifndef BABELTRACE_TRACE_IR_FIELD_CLASSES_H
#define BABELTRACE_TRACE_IR_FIELD_CLASSES_H

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

/* For bt_bool, bt_field_class */
#include <babeltrace/types.h>

/*
 * For enum bt_field_class_status,
 * enum bt_field_class_integer_preferred_display_base
 */
#include <babeltrace/trace-ir/field-class-const.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_field_class *bt_field_class_unsigned_integer_create(void);

extern bt_field_class *bt_field_class_signed_integer_create(void);

extern void bt_field_class_integer_set_field_value_range(
		bt_field_class *field_class, uint64_t size);

extern void bt_field_class_integer_set_preferred_display_base(
		bt_field_class *field_class,
		enum bt_field_class_integer_preferred_display_base base);

extern bt_field_class *bt_field_class_real_create(void);

extern void bt_field_class_real_set_is_single_precision(
		bt_field_class *field_class,
		bt_bool is_single_precision);

extern bt_field_class *bt_field_class_unsigned_enumeration_create(void);

extern bt_field_class *bt_field_class_signed_enumeration_create(void);

extern enum bt_field_class_status bt_field_class_unsigned_enumeration_map_range(
		bt_field_class *field_class, const char *label,
		uint64_t range_lower, uint64_t range_upper);

extern enum bt_field_class_status bt_field_class_signed_enumeration_map_range(
		bt_field_class *field_class, const char *label,
		int64_t range_lower, int64_t range_upper);

extern bt_field_class *bt_field_class_string_create(void);

extern bt_field_class *bt_field_class_structure_create(void);

extern enum bt_field_class_status bt_field_class_structure_append_member(
		bt_field_class *struct_field_class,
		const char *name, bt_field_class *field_class);

extern bt_field_class *bt_field_class_static_array_create(
		bt_field_class *elem_field_class, uint64_t length);

extern bt_field_class *bt_field_class_dynamic_array_create(
		bt_field_class *elem_field_class);

extern enum bt_field_class_status
bt_field_class_dynamic_array_set_length_field_class(
		bt_field_class *field_class,
		bt_field_class *length_field_class);

extern bt_field_class *bt_field_class_variant_create(void);

extern enum bt_field_class_status
bt_field_class_variant_set_selector_field_class(bt_field_class *field_class,
		bt_field_class *selector_field_class);

extern enum bt_field_class_status bt_field_class_variant_append_option(
		bt_field_class *var_field_class,
		const char *name, bt_field_class *field_class);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_FIELD_CLASSES_H */
