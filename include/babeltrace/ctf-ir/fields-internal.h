#ifndef BABELTRACE_CTF_IR_FIELDS_INTERNAL_H
#define BABELTRACE_CTF_IR_FIELDS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event Fields internal
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
 */

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <glib.h>

struct bt_stream_pos;

struct bt_field {
	struct bt_object base;
	struct bt_field_type *type;
	bool payload_set;
	bool frozen;
};

struct bt_field_integer {
	struct bt_field parent;
	union {
		int64_t signd;
		uint64_t unsignd;
	} payload;
};

struct bt_field_enumeration {
	struct bt_field parent;
	struct bt_field *payload;
};

struct bt_field_floating_point {
	struct bt_field parent;
	double payload;
};

struct bt_field_structure {
	struct bt_field parent;
	GPtrArray *fields; /* Array of pointers to struct bt_field */
};

struct bt_field_variant {
	struct bt_field parent;
	struct bt_field *tag;
	struct bt_field *payload;
};

struct bt_field_array {
	struct bt_field parent;
	GPtrArray *elements; /* Array of pointers to struct bt_field */
};

struct bt_field_sequence {
	struct bt_field parent;
	struct bt_field *length;
	GPtrArray *elements; /* Array of pointers to struct bt_field */
};

struct bt_field_string {
	struct bt_field parent;
	GString *payload;
};

BT_HIDDEN
int bt_field_serialize_recursive(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order);

/* Validate that the field's payload is set (returns 0 if set). */
BT_HIDDEN
int _bt_field_validate_recursive(struct bt_field *field);

BT_HIDDEN
void _bt_field_freeze_recursive(struct bt_field *field);

BT_HIDDEN
bt_bool _bt_field_is_set_recursive(struct bt_field *field);

BT_HIDDEN
void _bt_field_reset_recursive(struct bt_field *field);

BT_HIDDEN
void _bt_field_set(struct bt_field *field, bool val);

#ifdef BT_DEV_MODE
# define bt_field_validate_recursive		_bt_field_validate_recursive
# define bt_field_freeze_recursive		_bt_field_freeze_recursive
# define bt_field_is_set_recursive		_bt_field_is_set_recursive
# define bt_field_reset_recursive		_bt_field_reset_recursive
# define bt_field_set				_bt_field_set
#else
# define bt_field_validate_recursive(_field)	(-1)
# define bt_field_freeze_recursive(_field)
# define bt_field_is_set_recursive(_field)	(BT_FALSE)
# define bt_field_reset_recursive(_field)		(BT_TRUE)
# define bt_field_set(_field, _val)
#endif

BT_HIDDEN
int64_t bt_field_sequence_get_int_length(struct bt_field *field);

#endif /* BABELTRACE_CTF_IR_FIELDS_INTERNAL_H */
