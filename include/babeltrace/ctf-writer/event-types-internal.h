#ifndef BABELTRACE_CTF_WRITER_EVENT_TYPES_INTERNAL_H
#define BABELTRACE_CTF_WRITER_EVENT_TYPES_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Event types internal
 *
 * Copyright 2013 EfficiOS Inc.
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

#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/ref-internal.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/events.h>
#include <glib.h>

typedef void(*type_freeze_func)(struct bt_ctf_field_type *);
typedef int(*type_serialize_func)(struct bt_ctf_field_type *,
		struct metadata_context *);

struct bt_ctf_field_type {
	struct bt_ctf_ref ref_count;
	struct bt_declaration *declaration;
	type_freeze_func freeze;
	type_serialize_func serialize;
	/*
	 * A type can't be modified once it is added to an event or after a
	 * a field has been instanciated from it.
	 */
	int frozen;
};

struct bt_ctf_field_type_integer {
	struct bt_ctf_field_type parent;
	struct declaration_integer declaration;
};

struct enumeration_mapping {
	int64_t range_start;
	int64_t range_end;
	GQuark string;
};

struct bt_ctf_field_type_enumeration {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *container;
	GPtrArray *entries; /* Array of pointers to struct enum_mapping */
	struct declaration_enum declaration;
};

struct bt_ctf_field_type_floating_point {
	struct bt_ctf_field_type parent;
	struct declaration_float declaration;
	struct declaration_integer sign;
	struct declaration_integer mantissa;
	struct declaration_integer exp;
};

struct structure_field {
	GQuark name;
	struct bt_ctf_field_type *type;
};

struct bt_ctf_field_type_structure {
	struct bt_ctf_field_type parent;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
	struct declaration_enum declaration;
};

struct bt_ctf_field_type_variant {
	struct bt_ctf_field_type parent;
	GString *tag_name;
	struct bt_ctf_field_type_enumeration *tag;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
	struct declaration_variant declaration;
};

struct bt_ctf_field_type_array {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *element_type;
	unsigned int length; /* Number of elements */
	struct declaration_array declaration;
};

struct bt_ctf_field_type_sequence {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *element_type;
	GString *length_field_name;
	struct declaration_sequence declaration;
};

struct bt_ctf_field_type_string {
	struct bt_ctf_field_type parent;
	struct declaration_string declaration;
};

BT_HIDDEN
void bt_ctf_field_type_freeze(struct bt_ctf_field_type *type);

BT_HIDDEN
enum ctf_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_type(
		struct bt_ctf_field_type_structure *structure,
		const char *name);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(
		struct bt_ctf_field_type_array *array);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(
		struct bt_ctf_field_type_sequence *sequence);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type(
		struct bt_ctf_field_type_variant *variant, int64_t tag_value);

BT_HIDDEN
int bt_ctf_field_type_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context);

BT_HIDDEN
int bt_ctf_field_type_validate(struct bt_ctf_field_type *type);

#endif /* BABELTRACE_CTF_WRITER_EVENT_TYPES_INTERNAL_H */
