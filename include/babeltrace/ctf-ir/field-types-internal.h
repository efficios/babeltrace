#ifndef BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H
#define BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H

/*
 * BabelTrace - CTF IR: Event field types internal
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

#include <stdint.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <glib.h>

typedef void (*type_freeze_func)(struct bt_ctf_field_type *);
typedef int (*type_serialize_func)(struct bt_ctf_field_type *,
		struct metadata_context *);

struct bt_ctf_field_type {
	struct bt_object base;
	enum bt_ctf_field_type_id id;
	unsigned int alignment;
	type_freeze_func freeze;
	type_serialize_func serialize;
	/*
	 * A type can't be modified once it is added to an event or after a
	 * a field has been instanciated from it.
	 */
	int frozen;

	/*
	 * This flag indicates if the field type is valid. A valid
	 * field type is _always_ frozen. All the nested field types of
	 * a valid field type are also valid (and thus frozen).
	 */
	int valid;
};

struct bt_ctf_field_type_integer {
	struct bt_ctf_field_type parent;
	struct bt_ctf_clock_class *mapped_clock;
	enum bt_ctf_byte_order user_byte_order;
	bt_bool is_signed;
	unsigned int size;
	enum bt_ctf_integer_base base;
	enum bt_ctf_string_encoding encoding;
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
	/* Only set during validation. */
	bt_bool has_overlapping_ranges;
};

enum bt_ctf_field_type_enumeration_mapping_iterator_type {
	ITERATOR_BY_NAME,
	ITERATOR_BY_SIGNED_VALUE,
	ITERATOR_BY_UNSIGNED_VALUE,
};

struct bt_ctf_field_type_enumeration_mapping_iterator {
	struct bt_object base;
	struct bt_ctf_field_type_enumeration *enumeration_type;
	enum bt_ctf_field_type_enumeration_mapping_iterator_type type;
	int index;
	union {
		GQuark name_quark;
		int64_t signed_value;
		uint64_t unsigned_value;
	} u;
};

struct bt_ctf_field_type_floating_point {
	struct bt_ctf_field_type parent;
	enum bt_ctf_byte_order user_byte_order;
	unsigned int exp_dig;
	unsigned int mant_dig;
};

struct structure_field {
	GQuark name;
	struct bt_ctf_field_type *type;
};

struct bt_ctf_field_type_structure {
	struct bt_ctf_field_type parent;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
};

struct bt_ctf_field_type_variant {
	struct bt_ctf_field_type parent;
	GString *tag_name;
	struct bt_ctf_field_type_enumeration *tag;
	struct bt_ctf_field_path *tag_field_path;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
};

struct bt_ctf_field_type_array {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *element_type;
	unsigned int length; /* Number of elements */
};

struct bt_ctf_field_type_sequence {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *element_type;
	GString *length_field_name;
	struct bt_ctf_field_path *length_field_path;
};

struct bt_ctf_field_type_string {
	struct bt_ctf_field_type parent;
	enum bt_ctf_string_encoding encoding;
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
int bt_ctf_field_type_variant_set_tag_field_path(struct bt_ctf_field_type *type,
		struct bt_ctf_field_path *path);

BT_HIDDEN
int bt_ctf_field_type_variant_set_tag_field_type(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *tag_type);

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

BT_HIDDEN
int64_t bt_ctf_field_type_get_field_count(struct bt_ctf_field_type *type);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_get_field_at_index(
		struct bt_ctf_field_type *type, int index);

BT_HIDDEN
int bt_ctf_field_type_get_field_index(struct bt_ctf_field_type *type,
		const char *name);

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H */
