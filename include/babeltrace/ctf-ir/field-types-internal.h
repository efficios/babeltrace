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

enum bt_ctf_ir_scope {
	BT_CTF_SCOPE_UNKNOWN = -1,
	BT_CTF_SCOPE_ENV = 0,
	BT_CTF_SCOPE_TRACE_PACKET_HEADER = 1,
	BT_CTF_SCOPE_STREAM_PACKET_CONTEXT = 2,
	BT_CTF_SCOPE_STREAM_EVENT_HEADER = 3,
	BT_CTF_SCOPE_STREAM_EVENT_CONTEXT = 4,
	BT_CTF_SCOPE_EVENT_CONTEXT = 5,
	BT_CTF_SCOPE_EVENT_FIELDS = 6,
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

	/*
	 * This flag indicates if the field type is valid. A valid
	 * field type is _always_ frozen. All the nested field types of
	 * a valid field type are also valid (and thus frozen).
	 */
	int valid;
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

	/*
	 * The `declaration` field above contains 3 pointers pointing
	 * to the fields below. This avoids unnecessary dynamic
	 * allocations.
	 */
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
	struct bt_ctf_field_path *tag_field_path;
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
int bt_ctf_field_type_get_field_count(struct bt_ctf_field_type *type);

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_get_field_at_index(
		struct bt_ctf_field_type *type, int index);

BT_HIDDEN
int bt_ctf_field_type_get_field_index(struct bt_ctf_field_type *type,
		const char *name);

/*
 * bt_ctf_field_type_integer_get_size: get an integer type's size.
 *
 * Get an integer type's size.
 *
 * @param integer Integer type.
 *
 * Returns the integer type's size, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_integer_get_size(struct bt_ctf_field_type *integer);

/*
 * bt_ctf_field_type_integer_get_base: get an integer type's base.
 *
 * Get an integer type's base used to pretty-print the resulting trace.
 *
 * @param integer Integer type.
 *
 * Returns the integer type's base on success, BT_CTF_INTEGER_BASE_UNKNOWN on
 *	error.
 */
BT_HIDDEN
enum bt_ctf_integer_base bt_ctf_field_type_integer_get_base(
		struct bt_ctf_field_type *integer);

/*
 * bt_ctf_field_type_integer_get_encoding: get an integer type's encoding.
 *
 * @param integer Integer type.
 *
 * Returns the string field's encoding on success,
 * BT_CTF_STRING_ENCODING_UNKNOWN on error.
 */
BT_HIDDEN
enum bt_ctf_string_encoding bt_ctf_field_type_integer_get_encoding(
		struct bt_ctf_field_type *integer);

/**
 * bt_ctf_field_type_integer_get_mapped_clock: get an integer type's mapped clock.
 *
 * @param integer Integer type.
 *
 * Returns the integer's mapped clock (if any), NULL on error.
 */
BT_HIDDEN
struct bt_ctf_clock *bt_ctf_field_type_integer_get_mapped_clock(
		struct bt_ctf_field_type *integer);

/**
 * bt_ctf_field_type_integer_set_mapped_clock: set an integer type's mapped clock.
 *
 * @param integer Integer type.
 * @param clock Clock to map.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_integer_set_mapped_clock(
		struct bt_ctf_field_type *integer,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_field_type_enumeration_get_container_type: get underlying container.
 *
 * Get the enumeration type's underlying integer container type.
 *
 * @param enumeration Enumeration type.
 *
 * Returns an allocated field type on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_get_container_type(
		struct bt_ctf_field_type *enumeration);

/*
 * bt_ctf_field_type_enumeration_add_mapping_unsigned: add an enumeration
 *	mapping.
 *
 * Add a mapping to the enumeration. The range's values are inclusive.
 *
 * @param enumeration Enumeration type.
 * @param name Enumeration mapping name (will be copied).
 * @param range_start Enumeration mapping range start.
 * @param range_end Enumeration mapping range end.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_add_mapping_unsigned(
		struct bt_ctf_field_type *enumeration, const char *name,
		uint64_t range_start, uint64_t range_end);

/*
 * bt_ctf_field_type_enumeration_get_mapping_count: Get the number of mappings
 *	defined in the enumeration.
 *
 * @param enumeration Enumeration type.
 *
 * Returns the mapping count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_count(
		struct bt_ctf_field_type *enumeration);

/*
 * bt_ctf_field_type_enumeration_get_mapping: get an enumeration mapping.
 *
 * @param enumeration Enumeration type.
 * @param index Index of mapping.
 * @param name Pointer where the mapping's name will be returned (valid for
 *	the lifetime of the enumeration).
 * @param range_start Pointer where the enumeration mapping's range start will
 *	be returned.
 * @param range_end Pointer where the enumeration mapping's range end will
 *	be returned.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping(
		struct bt_ctf_field_type *enumeration, int index,
		const char **name, int64_t *range_start, int64_t *range_end);

/*
 * bt_ctf_field_type_enumeration_get_mapping_unsigned: get a mapping.
 *
 * @param enumeration Enumeration type.
 * @param index Index of mapping.
 * @param name Pointer where the mapping's name will be returned (valid for
 *	the lifetime of the enumeration).
 * @param range_start Pointer where the enumeration mapping's range start will
 *	be returned.
 * @param range_end Pointer where the enumeration mapping's range end will
 *	be returned.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_unsigned(
		struct bt_ctf_field_type *enumeration, int index,
		const char **name, uint64_t *range_start,
		uint64_t *range_end);

/*
 * bt_ctf_field_type_enumeration_get_mapping_index_by_name: get an enumerations'
 *	mapping index by name.
 *
 * @param enumeration Enumeration type.
 * @param name Mapping name.
 *
 * Returns mapping index on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_index_by_name(
		struct bt_ctf_field_type *enumeration, const char *name);

/*
 * bt_ctf_field_type_enumeration_get_mapping_index_by_value: get an
 *	enumerations' mapping index by value.
 *
 * @param enumeration Enumeration type.
 * @param value Value.
 *
 * Returns mapping index on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_index_by_value(
		struct bt_ctf_field_type *enumeration, int64_t value);

/*
 * bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value: get an
 *	enumerations' mapping index by value.
 *
 * @param enumeration Enumeration type.
 * @param value Value.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(
		struct bt_ctf_field_type *enumeration, uint64_t value);

/*
 * bt_ctf_field_type_floating_point_get_exponent_digits: get exponent digit
 *	count.
 *
 * @param floating_point Floating point type.
 *
 * Returns the exponent digit count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_floating_point_get_exponent_digits(
		struct bt_ctf_field_type *floating_point);

/*
 * bt_ctf_field_type_floating_point_get_mantissa_digits: get mantissa digit
 * count.
 *
 * @param floating_point Floating point type.
 *
 * Returns the mantissa digit count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_floating_point_get_mantissa_digits(
		struct bt_ctf_field_type *floating_point);

/*
 * bt_ctf_field_type_structure_get_field_count: Get the number of fields defined
 *	in the structure.
 *
 * @param structure Structure type.
 *
 * Returns the field count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_structure_get_field_count(
		struct bt_ctf_field_type *structure);

/*
 * bt_ctf_field_type_structure_get_field_type_by_name: get a structure field's
 *	type by name.
 *
 * @param structure Structure type.
 * @param field_name Name of the structure's field.
 *
 * Returns a field type instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_field_type_by_name(
		struct bt_ctf_field_type *structure, const char *field_name);

/*
 * bt_ctf_field_type_variant_get_tag_type: get a variant's tag type.
 *
 * @param variant Variant type.
 *
 * Returns a field type instance on success, NULL if unset.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_tag_type(
		struct bt_ctf_field_type *variant);

/*
 * bt_ctf_field_type_variant_get_tag_name: get a variant's tag name.
 *
 * @param variant Variant type.
 *
 * Returns the tag field's name, NULL if unset.
 */
BT_HIDDEN
const char *bt_ctf_field_type_variant_get_tag_name(
		struct bt_ctf_field_type *variant);

/*
 * bt_ctf_field_type_variant_set_tag_name: set a variant's tag name.
 *
 * @param variant Variant type.
 * @param name Tag field name.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_variant_set_tag_name(
		struct bt_ctf_field_type *variant, const char *name);

/*
 * bt_ctf_field_type_variant_get_field_type_by_name: get variant field's type.
 *
 * @param structure Variant type.
 * @param field_name Name of the variant's field.
 *
 * Returns a field type instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_by_name(
		struct bt_ctf_field_type *variant, const char *field_name);

/*
 * bt_ctf_field_type_variant_get_field_type_from_tag: get variant field's type.
 *
 * @param variant Variant type.
 * @param tag Type tag (enum).
 *
 * Returns a field type instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_from_tag(
		struct bt_ctf_field_type *variant, struct bt_ctf_field *tag);

/*
 * bt_ctf_field_type_variant_get_field_count: Get the number of fields defined
 *	in the variant.
 *
 * @param variant Variant type.
 *
 * Returns the field count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_variant_get_field_count(
		struct bt_ctf_field_type *variant);

/*
 * bt_ctf_field_type_variant_get_field: get a variant's field name and type.
 *
 * @param variant Variant type.
 * @param field_type Pointer to a const char* where the field's name will
 *	be returned.
 * @param field_type Pointer to a bt_ctf_field_type* where the field's type will
 *	be returned.
 * @param index Index of field.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_variant_get_field(
		struct bt_ctf_field_type *variant, const char **field_name,
		struct bt_ctf_field_type **field_type, int index);

/*
 * bt_ctf_field_type_array_get_element_type: get an array's element type.
 *
 * @param array Array type.
 *
 * Returns a field type instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(
		struct bt_ctf_field_type *array);

/*
 * bt_ctf_field_type_array_get_length: get an array's length.
 *
 * @param array Array type.
 *
 * Returns the array's length on success, a negative value on error.
 */
BT_HIDDEN
int64_t bt_ctf_field_type_array_get_length(struct bt_ctf_field_type *array);

/*
 * bt_ctf_field_type_sequence_get_element_type: get a sequence's element type.
 *
 * @param sequence Sequence type.
 *
 * Returns a field type instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(
		struct bt_ctf_field_type *sequence);

/*
 * bt_ctf_field_type_sequence_get_length_field_name: get length field name.
 *
 * @param sequence Sequence type.
 *
 * Returns the sequence's length field on success, NULL on error.
 */
BT_HIDDEN
const char *bt_ctf_field_type_sequence_get_length_field_name(
		struct bt_ctf_field_type *sequence);

/*
 * bt_ctf_field_type_string_get_encoding: get a string type's encoding.
 *
 * Get the string type's encoding.
 *
 * @param string_type String type.
 *
 * Returns the string's encoding on success, a BT_CTF_STRING_ENCODING_UNKNOWN
 * on error.
 */
BT_HIDDEN
enum bt_ctf_string_encoding bt_ctf_field_type_string_get_encoding(
		struct bt_ctf_field_type *string_type);

/*
 * bt_ctf_field_type_get_alignment: get a field type's alignment.
 *
 * Get the field type's alignment.
 *
 * @param type Field type.
 *
 * Returns the field type's alignment on success, a negative value on error and
 * 0 if the alignment is undefined (as in the case of a variant).
 */
BT_HIDDEN
int bt_ctf_field_type_get_alignment(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_get_byte_order: get a field type's byte order.
 *
 * @param type Field type.
 *
 * Returns the field type's byte order on success, a negative value on error.
 */
BT_HIDDEN
enum bt_ctf_byte_order bt_ctf_field_type_get_byte_order(
		struct bt_ctf_field_type *type);


/*
 * bt_ctf_field_type_variant_get_tag_field_path: get a variant's tag's field
 *	path.
 *
 * Get the variant's tag's field path.
 *
 * @param type Field type.
 *
 * Returns the field path on success, NULL on error or if no field path is set.
 */
BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_type_variant_get_tag_field_path(
		struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_sequence_get_length_field_path: get a sequence's length's
 *	field path.
 *
 * Get the sequence's length's field path.
 *
 * @param type Field type.
 *
 * Returns the field path on success, NULL on error or if no field path is set.
 */
BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_type_sequence_get_length_field_path(
		struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_compare: compare two field types recursively
 *
 * Compare two field types recursively.
 *
 * The registered tag field type of a variant field type is ignored:
 * only the tag strings are compared.
 *
 * @param type_a Field type A.
 * @param type_b Field type B.
 *
 * Returns 0 if both field types are semantically equivalent, a positive
 * value if they are not equivalent, or a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_type_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b);

/*
 * bt_ctf_field_type_get_type_id: get a field type's bt_ctf_type_id.
 *
 * @param type Field type.
 *
 * Returns the field type's bt_ctf_type_id, CTF_TYPE_UNKNOWN on error.
 */
BT_HIDDEN
enum ctf_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_integer: returns whether or not a given field
 *	type is an integer type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is an integer type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_integer(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_floating_point: returns whether or not a given field
 *	type is a floating point number type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a floating point number type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_floating_point(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_enumeration: returns whether or not a given field
 *	type is an enumeration type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is an enumeration type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_enumeration(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_string: returns whether or not a given field
 *	type is a string type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a string type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_string(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_structure: returns whether or not a given field
 *	type is a structure type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a structure type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_structure(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_array: returns whether or not a given field
 *	type is an array type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is an array type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_array(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_sequence: returns whether or not a given field
 *	type is a sequence type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a sequence type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_sequence(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_variant: returns whether or not a given field
 *	type is a variant type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a variant type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_type_is_variant(struct bt_ctf_field_type *type);

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H */
