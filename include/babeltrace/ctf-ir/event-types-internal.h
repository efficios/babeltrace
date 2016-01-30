#ifndef BABELTRACE_CTF_IR_EVENT_TYPES_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_TYPES_INTERNAL_H

/*
 * BabelTrace - CTF IR: Event types internal
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

#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/events.h>
#include <glib.h>

typedef void (*type_freeze_func)(struct bt_ctf_field_type *);
typedef int (*type_serialize_func)(struct bt_ctf_field_type *,
		struct metadata_context *);

enum bt_ctf_node {
	CTF_NODE_UNKNOWN = -1,
	CTF_NODE_ENV = 0,
	CTF_NODE_TRACE_PACKET_HEADER = 1,
	CTF_NODE_STREAM_PACKET_CONTEXT = 2,
	CTF_NODE_STREAM_EVENT_HEADER = 3,
	CTF_NODE_STREAM_EVENT_CONTEXT = 4,
	CTF_NODE_EVENT_CONTEXT = 5,
	CTF_NODE_EVENT_FIELDS = 6,
};

struct bt_ctf_field_path {
	enum bt_ctf_node root;
	/*
	 * Array of integers (int) indicating the index in either
	 * structures or variants that make-up the path to a field.
	 */
	GArray *path_indexes;
};

struct bt_ctf_field_type {
	struct bt_object base;
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
	struct bt_ctf_clock *mapped_clock;

	/*
	 * This is what the user sets and is never modified by internal
	 * code.
	 *
	 * This field must contain a `BT_CTF_BYTE_ORDER_*` value.
	 */
	enum bt_ctf_byte_order user_byte_order;
};

struct enumeration_mapping {
	union {
		uint64_t _unsigned;
		int64_t _signed;
	} range_start;

	union {
		uint64_t _unsigned;
		int64_t _signed;
	} range_end;
	GQuark string;
};

struct bt_ctf_field_type_enumeration {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *container;
	GPtrArray *entries; /* Array of ptrs to struct enumeration_mapping */
	struct declaration_enum declaration;
};

struct bt_ctf_field_type_floating_point {
	struct bt_ctf_field_type parent;
	struct declaration_float declaration;
	struct declaration_integer sign;
	struct declaration_integer mantissa;
	struct declaration_integer exp;

	/*
	 * This is what the user sets and is never modified by internal
	 * code.
	 *
	 * This field must contain a `BT_CTF_BYTE_ORDER_*` value.
	 */
	enum bt_ctf_byte_order user_byte_order;
};

struct structure_field {
	GQuark name;
	struct bt_ctf_field_type *type;
};

struct bt_ctf_field_type_structure {
	struct bt_ctf_field_type parent;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
	struct declaration_struct declaration;
};

struct bt_ctf_field_type_variant {
	struct bt_ctf_field_type parent;
	GString *tag_name;
	struct bt_ctf_field_type_enumeration *tag;
	struct bt_ctf_field_path *tag_path;
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
	struct bt_ctf_field_path *length_field_path;
	struct declaration_sequence declaration;
};

struct bt_ctf_field_type_string {
	struct bt_ctf_field_type parent;
	struct declaration_string declaration;
};

BT_HIDDEN
void bt_ctf_field_type_freeze(struct bt_ctf_field_type *type);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_signed(
		struct bt_ctf_field_type_variant *variant, int64_t tag_value);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_unsigned(
		struct bt_ctf_field_type_variant *variant, uint64_t tag_value);

BT_HIDDEN
int bt_ctf_field_type_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context);

BT_HIDDEN
int bt_ctf_field_type_validate(struct bt_ctf_field_type *type);

BT_HIDDEN
const char *bt_ctf_field_type_enumeration_get_mapping_name_unsigned(
		struct bt_ctf_field_type_enumeration *enumeration_type,
		uint64_t value);

BT_HIDDEN
const char *bt_ctf_field_type_enumeration_get_mapping_name_signed(
		struct bt_ctf_field_type_enumeration *enumeration_type,
		int64_t value);

/* Override field type's byte order only if it is set to "native" */
BT_HIDDEN
void bt_ctf_field_type_set_native_byte_order(
		struct bt_ctf_field_type *type, int byte_order);

/* Deep copy a field type */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_copy(
		struct bt_ctf_field_type *type);

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_path_create(void);

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_path_copy(
		struct bt_ctf_field_path *path);

BT_HIDDEN
void bt_ctf_field_path_destroy(struct bt_ctf_field_path *path);

BT_HIDDEN
int bt_ctf_field_type_structure_get_field_name_index(
		struct bt_ctf_field_type *structure, const char *name);

/* Replace an existing field's type in a structure */
BT_HIDDEN
int bt_ctf_field_type_structure_set_field_index(
		struct bt_ctf_field_type *structure,
		struct bt_ctf_field_type *field, int index);

BT_HIDDEN
int bt_ctf_field_type_variant_get_field_name_index(
		struct bt_ctf_field_type *variant, const char *name);

BT_HIDDEN
int bt_ctf_field_type_sequence_set_length_field_path(
		struct bt_ctf_field_type *type,
		struct bt_ctf_field_path *path);

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_type_sequence_get_length_field_path(
		struct bt_ctf_field_type *type);

BT_HIDDEN
int bt_ctf_field_type_variant_set_tag_field_path(struct bt_ctf_field_type *type,
		struct bt_ctf_field_path *path);

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_type_variant_get_tag_field_path(
		struct bt_ctf_field_type *type);

BT_HIDDEN
int bt_ctf_field_type_variant_set_tag(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *tag);

/* Replace an existing field's type in a variant */
BT_HIDDEN
int bt_ctf_field_type_variant_set_field_index(
		struct bt_ctf_field_type *variant,
		struct bt_ctf_field_type *field, int index);

BT_HIDDEN
int bt_ctf_field_type_array_set_element_type(struct bt_ctf_field_type *array,
		struct bt_ctf_field_type *element_type);

BT_HIDDEN
int bt_ctf_field_type_sequence_set_element_type(struct bt_ctf_field_type *array,
		struct bt_ctf_field_type *element_type);

#endif /* BABELTRACE_CTF_IR_EVENT_TYPES_INTERNAL_H */
