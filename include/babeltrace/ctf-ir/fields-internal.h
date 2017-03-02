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
#include <stdint.h>
#include <glib.h>

struct bt_ctf_stream_pos;

struct bt_ctf_field {
	struct bt_object base;
	struct bt_ctf_field_type *type;
	int payload_set;
	int frozen;
};

struct bt_ctf_field_integer {
	struct bt_ctf_field parent;
	union {
		int64_t signd;
		uint64_t unsignd;
	} payload;
};

struct bt_ctf_field_enumeration {
	struct bt_ctf_field parent;
	struct bt_ctf_field *payload;
};

struct bt_ctf_field_floating_point {
	struct bt_ctf_field parent;
	double payload;
};

struct bt_ctf_field_structure {
	struct bt_ctf_field parent;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct bt_ctf_field */
};

struct bt_ctf_field_variant {
	struct bt_ctf_field parent;
	struct bt_ctf_field *tag;
	struct bt_ctf_field *payload;
};

struct bt_ctf_field_array {
	struct bt_ctf_field parent;
	GPtrArray *elements; /* Array of pointers to struct bt_ctf_field */
};

struct bt_ctf_field_sequence {
	struct bt_ctf_field parent;
	struct bt_ctf_field *length;
	GPtrArray *elements; /* Array of pointers to struct bt_ctf_field */
};

struct bt_ctf_field_string {
	struct bt_ctf_field parent;
	GString *payload;
};

/* Validate that the field's payload is set (returns 0 if set). */
BT_HIDDEN
int bt_ctf_field_validate(struct bt_ctf_field *field);

/* Mark field payload as unset. */
BT_HIDDEN
int bt_ctf_field_reset(struct bt_ctf_field *field);

BT_HIDDEN
int bt_ctf_field_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order);

BT_HIDDEN
void bt_ctf_field_freeze(struct bt_ctf_field *field);

BT_HIDDEN
bool bt_ctf_field_is_set(struct bt_ctf_field *field);

#endif /* BABELTRACE_CTF_IR_FIELDS_INTERNAL_H */
