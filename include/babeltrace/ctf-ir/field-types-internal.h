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
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <glib.h>

typedef void (*type_freeze_func)(struct bt_field_type *);
typedef int (*type_serialize_func)(struct bt_field_type *,
		struct metadata_context *);

struct bt_field_type {
	struct bt_object base;
	enum bt_field_type_id id;
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

struct bt_field_type_integer {
	struct bt_field_type parent;
	struct bt_clock_class *mapped_clock;
	enum bt_byte_order user_byte_order;
	bt_bool is_signed;
	unsigned int size;
	enum bt_integer_base base;
	enum bt_string_encoding encoding;
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

struct bt_field_type_enumeration {
	struct bt_field_type parent;
	struct bt_field_type *container;
	GPtrArray *entries; /* Array of ptrs to struct enumeration_mapping */
	/* Only set during validation. */
	bt_bool has_overlapping_ranges;
};

enum bt_field_type_enumeration_mapping_iterator_type {
	ITERATOR_BY_NAME,
	ITERATOR_BY_SIGNED_VALUE,
	ITERATOR_BY_UNSIGNED_VALUE,
};

struct bt_field_type_enumeration_mapping_iterator {
	struct bt_object base;
	struct bt_field_type_enumeration *enumeration_type;
	enum bt_field_type_enumeration_mapping_iterator_type type;
	int index;
	union {
		GQuark name_quark;
		int64_t signed_value;
		uint64_t unsigned_value;
	} u;
};

struct bt_field_type_floating_point {
	struct bt_field_type parent;
	enum bt_byte_order user_byte_order;
	unsigned int exp_dig;
	unsigned int mant_dig;
};

struct structure_field {
	GQuark name;
	struct bt_field_type *type;
};

struct bt_field_type_structure {
	struct bt_field_type parent;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
};

struct bt_field_type_variant {
	struct bt_field_type parent;
	GString *tag_name;
	struct bt_field_type_enumeration *tag;
	struct bt_field_path *tag_field_path;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
};

struct bt_field_type_array {
	struct bt_field_type parent;
	struct bt_field_type *element_type;
	unsigned int length; /* Number of elements */
};

struct bt_field_type_sequence {
	struct bt_field_type parent;
	struct bt_field_type *element_type;
	GString *length_field_name;
	struct bt_field_path *length_field_path;
};

struct bt_field_type_string {
	struct bt_field_type parent;
	enum bt_string_encoding encoding;
};

BT_HIDDEN
void bt_field_type_freeze(struct bt_field_type *type);

BT_HIDDEN
struct bt_field_type *bt_field_type_variant_get_field_type_signed(
		struct bt_field_type_variant *variant, int64_t tag_value);

BT_HIDDEN
struct bt_field_type *bt_field_type_variant_get_field_type_unsigned(
		struct bt_field_type_variant *variant, uint64_t tag_value);

BT_HIDDEN
int bt_field_type_serialize(struct bt_field_type *type,
		struct metadata_context *context);

BT_HIDDEN
int bt_field_type_validate(struct bt_field_type *type);

BT_HIDDEN
int bt_field_type_structure_get_field_name_index(
		struct bt_field_type *structure, const char *name);

BT_HIDDEN
int bt_field_type_structure_replace_field(struct bt_field_type *type,
		const char *field_name, struct bt_field_type *field_type);

BT_HIDDEN
int bt_field_type_variant_get_field_name_index(
		struct bt_field_type *variant, const char *name);

BT_HIDDEN
int bt_field_type_sequence_set_length_field_path(
		struct bt_field_type *type,
		struct bt_field_path *path);

BT_HIDDEN
int bt_field_type_variant_set_tag_field_path(struct bt_field_type *type,
		struct bt_field_path *path);

BT_HIDDEN
int bt_field_type_variant_set_tag_field_type(struct bt_field_type *type,
		struct bt_field_type *tag_type);

BT_HIDDEN
int bt_field_type_array_set_element_type(struct bt_field_type *array,
		struct bt_field_type *element_type);

BT_HIDDEN
int bt_field_type_sequence_set_element_type(struct bt_field_type *array,
		struct bt_field_type *element_type);

BT_HIDDEN
int64_t bt_field_type_get_field_count(struct bt_field_type *type);

BT_HIDDEN
struct bt_field_type *bt_field_type_get_field_at_index(
		struct bt_field_type *type, int index);

BT_HIDDEN
int bt_field_type_get_field_index(struct bt_field_type *type,
		const char *name);

BT_HIDDEN
int bt_field_type_integer_set_mapped_clock_class_no_check(
		struct bt_field_type *int_field_type,
		struct bt_clock_class *clock_class);

static inline
const char *bt_field_type_id_string(enum bt_field_type_id type_id)
{
	switch (type_id) {
	case BT_FIELD_TYPE_ID_UNKNOWN:
		return "BT_FIELD_TYPE_ID_UNKNOWN";
	case BT_FIELD_TYPE_ID_INTEGER:
		return "BT_FIELD_TYPE_ID_INTEGER";
	case BT_FIELD_TYPE_ID_FLOAT:
		return "BT_FIELD_TYPE_ID_FLOAT";
	case BT_FIELD_TYPE_ID_ENUM:
		return "BT_FIELD_TYPE_ID_ENUM";
	case BT_FIELD_TYPE_ID_STRING:
		return "BT_FIELD_TYPE_ID_STRING";
	case BT_FIELD_TYPE_ID_STRUCT:
		return "BT_FIELD_TYPE_ID_STRUCT";
	case BT_FIELD_TYPE_ID_ARRAY:
		return "BT_FIELD_TYPE_ID_ARRAY";
	case BT_FIELD_TYPE_ID_SEQUENCE:
		return "BT_FIELD_TYPE_ID_SEQUENCE";
	case BT_FIELD_TYPE_ID_VARIANT:
		return "BT_FIELD_TYPE_ID_VARIANT";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_byte_order_string(enum bt_byte_order bo)
{
	switch (bo) {
	case BT_BYTE_ORDER_UNKNOWN:
		return "BT_BYTE_ORDER_UNKNOWN";
	case BT_BYTE_ORDER_UNSPECIFIED:
		return "BT_BYTE_ORDER_UNSPECIFIED";
	case BT_BYTE_ORDER_NATIVE:
		return "BT_BYTE_ORDER_NATIVE";
	case BT_BYTE_ORDER_LITTLE_ENDIAN:
		return "BT_BYTE_ORDER_LITTLE_ENDIAN";
	case BT_BYTE_ORDER_BIG_ENDIAN:
		return "BT_BYTE_ORDER_BIG_ENDIAN";
	case BT_BYTE_ORDER_NETWORK:
		return "BT_BYTE_ORDER_NETWORK";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_string_encoding_string(enum bt_string_encoding encoding)
{
	switch (encoding) {
	case BT_STRING_ENCODING_UNKNOWN:
		return "BT_STRING_ENCODING_UNKNOWN";
	case BT_STRING_ENCODING_NONE:
		return "BT_STRING_ENCODING_NONE";
	case BT_STRING_ENCODING_UTF8:
		return "BT_STRING_ENCODING_UTF8";
	case BT_STRING_ENCODING_ASCII:
		return "BT_STRING_ENCODING_ASCII";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_integer_base_string(enum bt_integer_base base)
{
	switch (base) {
	case BT_INTEGER_BASE_UNKNOWN:
		return "BT_INTEGER_BASE_UNKNOWN";
	case BT_INTEGER_BASE_UNSPECIFIED:
		return "BT_INTEGER_BASE_UNSPECIFIED";
	case BT_INTEGER_BASE_BINARY:
		return "BT_INTEGER_BASE_BINARY";
	case BT_INTEGER_BASE_OCTAL:
		return "BT_INTEGER_BASE_OCTAL";
	case BT_INTEGER_BASE_DECIMAL:
		return "BT_INTEGER_BASE_DECIMAL";
	case BT_INTEGER_BASE_HEXADECIMAL:
		return "BT_INTEGER_BASE_HEXADECIMAL";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_scope_string(enum bt_scope scope)
{
	switch (scope) {
	case BT_SCOPE_UNKNOWN:
		return "BT_SCOPE_UNKNOWN";
	case BT_SCOPE_TRACE_PACKET_HEADER:
		return "BT_SCOPE_TRACE_PACKET_HEADER";
	case BT_SCOPE_STREAM_PACKET_CONTEXT:
		return "BT_SCOPE_STREAM_PACKET_CONTEXT";
	case BT_SCOPE_STREAM_EVENT_HEADER:
		return "BT_SCOPE_STREAM_EVENT_HEADER";
	case BT_SCOPE_STREAM_EVENT_CONTEXT:
		return "BT_SCOPE_STREAM_EVENT_CONTEXT";
	case BT_SCOPE_EVENT_CONTEXT:
		return "BT_SCOPE_EVENT_CONTEXT";
	case BT_SCOPE_EVENT_PAYLOAD:
		return "BT_SCOPE_EVENT_PAYLOAD";
	case BT_SCOPE_ENV:
		return "BT_SCOPE_ENV";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H */
