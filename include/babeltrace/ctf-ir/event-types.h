#ifndef BABELTRACE_CTF_IR_EVENT_TYPES_H
#define BABELTRACE_CTF_IR_EVENT_TYPES_H

/*
 * BabelTrace - CTF IR: Event Types
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/ctf/events.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field_type;
struct bt_ctf_field;
struct bt_ctf_field_path;

enum bt_ctf_integer_base {
	BT_CTF_INTEGER_BASE_UNKNOWN = -1,
	BT_CTF_INTEGER_BASE_BINARY = 2,
	BT_CTF_INTEGER_BASE_OCTAL = 8,
	BT_CTF_INTEGER_BASE_DECIMAL = 10,
	BT_CTF_INTEGER_BASE_HEXADECIMAL = 16,
};

enum bt_ctf_byte_order {
	BT_CTF_BYTE_ORDER_UNKNOWN = -1,
	/*
	 * Note that native, in the context of the CTF specification, is defined
	 * as "the byte order described in the trace" and does not mean that the
	 * host's endianness will be used.
	 */
	BT_CTF_BYTE_ORDER_NATIVE = 0,
	BT_CTF_BYTE_ORDER_LITTLE_ENDIAN,
	BT_CTF_BYTE_ORDER_BIG_ENDIAN,
	BT_CTF_BYTE_ORDER_NETWORK,
};

enum bt_ctf_string_encoding {
	BT_CTF_STRING_ENCODING_NONE = CTF_STRING_NONE,
	BT_CTF_STRING_ENCODING_UTF8 = CTF_STRING_UTF8,
	BT_CTF_STRING_ENCODING_ASCII = CTF_STRING_ASCII,
	BT_CTF_STRING_ENCODING_UNKNOWN = CTF_STRING_UNKNOWN,
};

enum bt_ctf_scope {
	BT_CTF_SCOPE_UNKNOWN = -1,
	BT_CTF_SCOPE_ENV = 0,
	BT_CTF_SCOPE_TRACE_PACKET_HEADER = 1,
	BT_CTF_SCOPE_STREAM_PACKET_CONTEXT = 2,
	BT_CTF_SCOPE_STREAM_EVENT_HEADER = 3,
	BT_CTF_SCOPE_STREAM_EVENT_CONTEXT = 4,
	BT_CTF_SCOPE_EVENT_CONTEXT = 5,
	BT_CTF_SCOPE_EVENT_FIELDS = 6,
};

/*
 * bt_ctf_field_type_integer_create: create an integer field type.
 *
 * Allocate a new integer field type of the given size. The creation of a field
 * type sets its reference count to 1.
 *
 * @param size Integer field type size/length in bits.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_integer_create(
		unsigned int size);

/*
 * bt_ctf_field_type_integer_get_size: get an integer type's size.
 *
 * Get an integer type's size.
 *
 * @param integer Integer type.
 *
 * Returns the integer type's size, a negative value on error.
 */
extern int bt_ctf_field_type_integer_get_size(
		struct bt_ctf_field_type *integer);

/*
 * bt_ctf_field_type_integer_get_signed: get an integer type's signedness.
 *
 * Get an integer type's signedness attribute.
 *
 * @param integer Integer type.
 *
 * Returns the integer's signedness, a negative value on error.
 */
extern int bt_ctf_field_type_integer_get_signed(
		struct bt_ctf_field_type *integer);

/*
 * bt_ctf_field_type_integer_set_signed: set an integer type's signedness.
 *
 * Set an integer type's signedness attribute.
 *
 * @param integer Integer type.
 * @param is_signed Integer's signedness, defaults to FALSE.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_integer_set_signed(
		struct bt_ctf_field_type *integer, int is_signed);

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
extern enum bt_ctf_integer_base bt_ctf_field_type_integer_get_base(
		struct bt_ctf_field_type *integer);

/*
 * bt_ctf_field_type_integer_set_base: set an integer type's base.
 *
 * Set an integer type's base used to pretty-print the resulting trace.
 *
 * @param integer Integer type.
 * @param base Integer base, defaults to BT_CTF_INTEGER_BASE_DECIMAL.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_integer_set_base(struct bt_ctf_field_type *integer,
		enum bt_ctf_integer_base base);

/*
 * bt_ctf_field_type_integer_get_encoding: get an integer type's encoding.
 *
 * @param integer Integer type.
 *
 * Returns the string field's encoding on success,
 * BT_CTF_STRING_ENCODING_UNKNOWN on error.
 */
extern enum bt_ctf_string_encoding bt_ctf_field_type_integer_get_encoding(
		struct bt_ctf_field_type *integer);

/*
 * bt_ctf_field_type_integer_set_encoding: set an integer type's encoding.
 *
 * An integer encoding may be set to signal that the integer must be printed as
 * a text character.
 *
 * @param integer Integer type.
 * @param encoding Integer output encoding, defaults to
 *	BT_CTF_STRING_ENCODING_NONE
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_integer_set_encoding(
		struct bt_ctf_field_type *integer,
		enum bt_ctf_string_encoding encoding);

/**
 * bt_ctf_field_type_integer_get_mapped_clock: get an integer type's mapped clock.
 *
 * @param integer Integer type.
 *
 * Returns the integer's mapped clock (if any), NULL on error.
 */
extern struct bt_ctf_clock *bt_ctf_field_type_integer_get_mapped_clock(
		struct bt_ctf_field_type *integer);

/**
 * bt_ctf_field_type_integer_set_mapped_clock: set an integer type's mapped clock.
 *
 * @param integer Integer type.
 * @param clock Clock to map.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_integer_set_mapped_clock(
		struct bt_ctf_field_type *integer,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_field_type_enumeration_create: create an enumeration field type.
 *
 * Allocate a new enumeration field type with the given underlying type. The
 * creation of a field type sets its reference count to 1.
 * The resulting enumeration will share the integer_container_type's ownership
 * by increasing its reference count.
 *
 * @param integer_container_type Underlying integer type of the enumeration
 *	type.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(
		struct bt_ctf_field_type *integer_container_type);

/*
 * bt_ctf_field_type_enumeration_get_container_type: get underlying container.
 *
 * Get the enumeration type's underlying integer container type.
 *
 * @param enumeration Enumeration type.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_get_container_type(
		struct bt_ctf_field_type *enumeration);

/*
 * bt_ctf_field_type_enumeration_add_mapping: add an enumeration mapping.
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
extern int bt_ctf_field_type_enumeration_add_mapping(
		struct bt_ctf_field_type *enumeration, const char *name,
		int64_t range_start, int64_t range_end);

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
extern int bt_ctf_field_type_enumeration_add_mapping_unsigned(
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
extern int bt_ctf_field_type_enumeration_get_mapping_count(
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
extern int bt_ctf_field_type_enumeration_get_mapping(
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
extern int bt_ctf_field_type_enumeration_get_mapping_unsigned(
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
extern int bt_ctf_field_type_enumeration_get_mapping_index_by_name(
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
extern int bt_ctf_field_type_enumeration_get_mapping_index_by_value(
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
extern int bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(
		struct bt_ctf_field_type *enumeration, uint64_t value);

/*
 * bt_ctf_field_type_floating_point_create: create a floating point field type.
 *
 * Allocate a new floating point field type. The creation of a field type sets
 * its reference count to 1.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void);

/*
 * bt_ctf_field_type_floating_point_get_exponent_digits: get exponent digit
 *	count.
 *
 * @param floating_point Floating point type.
 *
 * Returns the exponent digit count on success, a negative value on error.
 */
extern int bt_ctf_field_type_floating_point_get_exponent_digits(
		struct bt_ctf_field_type *floating_point);

/*
 * bt_ctf_field_type_floating_point_set_exponent_digits: set exponent digit
 *	count.
 *
 * Set the number of exponent digits to use to store the floating point field.
 * The only values currently supported are FLT_EXP_DIG and DBL_EXP_DIG.
 *
 * @param floating_point Floating point type.
 * @param exponent_digits Number of digits to allocate to the exponent (defaults
 *	to FLT_EXP_DIG).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_floating_point_set_exponent_digits(
		struct bt_ctf_field_type *floating_point,
		unsigned int exponent_digits);

/*
 * bt_ctf_field_type_floating_point_get_mantissa_digits: get mantissa digit
 * count.
 *
 * @param floating_point Floating point type.
 *
 * Returns the mantissa digit count on success, a negative value on error.
 */
extern int bt_ctf_field_type_floating_point_get_mantissa_digits(
		struct bt_ctf_field_type *floating_point);

/*
 * bt_ctf_field_type_floating_point_set_mantissa_digits: set mantissa digit
 *	count.
 *
 * Set the number of mantissa digits to use to store the floating point field.
 * The only values currently supported are FLT_MANT_DIG and DBL_MANT_DIG.
 *
 * @param floating_point Floating point type.
 * @param mantissa_digits Number of digits to allocate to the mantissa (defaults
 *	to FLT_MANT_DIG).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_floating_point_set_mantissa_digits(
		struct bt_ctf_field_type *floating_point,
		unsigned int mantissa_digits);

/*
 * bt_ctf_field_type_structure_create: create a structure field type.
 *
 * Allocate a new structure field type. The creation of a field type sets
 * its reference count to 1.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void);

/*
 * bt_ctf_field_type_structure_add_field: add a field to a structure.
 *
 * Add a field of type "field_type" to the structure. The structure will share
 * field_type's ownership by increasing its reference count.
 *
 * @param structure Structure type.
 * @param field_type Type of the field to add to the structure type.
 * @param field_name Name of the structure's new field (will be copied).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_structure_add_field(
		struct bt_ctf_field_type *structure,
		struct bt_ctf_field_type *field_type,
		const char *field_name);

/*
 * bt_ctf_field_type_structure_get_field_count: Get the number of fields defined
 *	in the structure.
 *
 * @param structure Structure type.
 *
 * Returns the field count on success, a negative value on error.
 */
extern int bt_ctf_field_type_structure_get_field_count(
		struct bt_ctf_field_type *structure);

/*
 * bt_ctf_field_type_structure_get_field: get a structure's field type and name.
 *
 * @param structure Structure type.
 * @param field_type Pointer to a const char* where the field's name will
 *	be returned.
 * @param field_type Pointer to a bt_ctf_field_type* where the field's type will
 *	be returned.
 * @param index Index of field.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_structure_get_field(
		struct bt_ctf_field_type *structure,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index);

/*
 * bt_ctf_field_type_structure_get_field_type_by_name: get a structure field's
 *	type by name.
 *
 * @param structure Structure type.
 * @param field_name Name of the structure's field.
 *
 * Returns a field type instance on success, NULL on error.
 */
extern
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_field_type_by_name(
		struct bt_ctf_field_type *structure, const char *field_name);

/*
 * bt_ctf_field_type_variant_create: create a variant field type.
 *
 * Allocate a new variant field type. The creation of a field type sets
 * its reference count to 1. tag_name must be the name of an enumeration
 * field declared in the same scope as this variant.
 *
 * @param enum_tag Type of the variant's tag/selector (must be an enumeration).
 * @param tag_name Name of the variant's tag/selector field (will be copied).
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_variant_create(
		struct bt_ctf_field_type *enum_tag, const char *tag_name);

/*
 * bt_ctf_field_type_variant_get_tag_type: get a variant's tag type.
 *
 * @param variant Variant type.
 *
 * Returns a field type instance on success, NULL if unset.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_variant_get_tag_type(
		struct bt_ctf_field_type *variant);

/*
 * bt_ctf_field_type_variant_get_tag_name: get a variant's tag name.
 *
 * @param variant Variant type.
 *
 * Returns the tag field's name, NULL if unset.
 */
extern const char *bt_ctf_field_type_variant_get_tag_name(
		struct bt_ctf_field_type *variant);

/*
 * bt_ctf_field_type_variant_set_tag_name: set a variant's tag name.
 *
 * @param variant Variant type.
 * @param name Tag field name.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_variant_set_tag_name(
		struct bt_ctf_field_type *variant, const char *name);

/*
 * bt_ctf_field_type_variant_add_field: add a field to a variant.
 *
 * Add a field of type "field_type" to the variant. The variant will share
 * field_type's ownership by increasing its reference count. The "field_name"
 * will be copied. field_name must match a mapping in the tag/selector
 * enumeration.
 *
 * @param variant Variant type.
 * @param field_type Type of the variant type's new field.
 * @param field_name Name of the variant type's new field (will be copied).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_variant_add_field(
		struct bt_ctf_field_type *variant,
		struct bt_ctf_field_type *field_type,
		const char *field_name);

/*
 * bt_ctf_field_type_variant_get_field_type_by_name: get variant field's type.
 *
 * @param structure Variant type.
 * @param field_name Name of the variant's field.
 *
 * Returns a field type instance on success, NULL on error.
 */
extern
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
extern
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
extern int bt_ctf_field_type_variant_get_field_count(
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
extern int bt_ctf_field_type_variant_get_field(
		struct bt_ctf_field_type *variant, const char **field_name,
		struct bt_ctf_field_type **field_type, int index);

/*
 * bt_ctf_field_type_array_create: create an array field type.
 *
 * Allocate a new array field type. The creation of a field type sets
 * its reference count to 1.
 *
 * @param element_type Array's element type.
 * @oaram length Array type's length.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_type, unsigned int length);

/*
 * bt_ctf_field_type_array_get_element_type: get an array's element type.
 *
 * @param array Array type.
 *
 * Returns a field type instance on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(
		struct bt_ctf_field_type *array);

/*
 * bt_ctf_field_type_array_get_length: get an array's length.
 *
 * @param array Array type.
 *
 * Returns the array's length on success, a negative value on error.
 */
extern int64_t bt_ctf_field_type_array_get_length(
		struct bt_ctf_field_type *array);

/*
 * bt_ctf_field_type_sequence_create: create a sequence field type.
 *
 * Allocate a new sequence field type. The creation of a field type sets
 * its reference count to 1. "length_field_name" must match an integer field
 * declared in the same scope.
 *
 * @param element_type Sequence's element type.
 * @param length_field_name Name of the sequence's length field (will be
 *	copied).
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_type,
		const char *length_field_name);

/*
 * bt_ctf_field_type_sequence_get_element_type: get a sequence's element type.
 *
 * @param sequence Sequence type.
 *
 * Returns a field type instance on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(
		struct bt_ctf_field_type *sequence);

/*
 * bt_ctf_field_type_sequence_get_length_field_name: get length field name.
 *
 * @param sequence Sequence type.
 *
 * Returns the sequence's length field on success, NULL on error.
 */
extern const char *bt_ctf_field_type_sequence_get_length_field_name(
		struct bt_ctf_field_type *sequence);

/*
 * bt_ctf_field_type_string_create: create a string field type.
 *
 * Allocate a new string field type. The creation of a field type sets
 * its reference count to 1.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_string_create(void);

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
extern enum bt_ctf_string_encoding bt_ctf_field_type_string_get_encoding(
		struct bt_ctf_field_type *string_type);

/*
 * bt_ctf_field_type_string_set_encoding: set a string type's encoding.
 *
 * Set the string type's encoding.
 *
 * @param string_type String type.
 * @param encoding String field encoding, default BT_CTF_STRING_ENCODING_ASCII.
 *	Valid values are BT_CTF_STRING_ENCODING_ASCII and
 *	BT_CTF_STRING_ENCODING_UTF8.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_string_set_encoding(
		struct bt_ctf_field_type *string_type,
		enum bt_ctf_string_encoding encoding);

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
extern int bt_ctf_field_type_get_alignment(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_set_alignment: set a field type's alignment.
 *
 * Set the field type's alignment.
 *
 * @param type Field type.
 * @param alignment Type's alignment. Defaults to 1 (bit-aligned). However,
 *	some types, such as structures and string, may impose other alignment
 *	constraints.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type,
		unsigned int alignment);

/*
 * bt_ctf_field_type_get_byte_order: get a field type's byte order.
 *
 * @param type Field type.
 *
 * Returns the field type's byte order on success, a negative value on error.
 */
extern enum bt_ctf_byte_order bt_ctf_field_type_get_byte_order(
		struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_set_byte_order: set a field type's byte order.
 *
 * Set the field type's byte order.
 *
 * @param type Field type.
 * @param byte_order Field type's byte order. Defaults to
 * BT_CTF_BYTE_ORDER_NATIVE; the trace's endianness.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *type,
		enum bt_ctf_byte_order byte_order);

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
extern struct bt_ctf_field_path *bt_ctf_field_type_variant_get_tag_field_path(
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
extern struct bt_ctf_field_path *bt_ctf_field_type_sequence_get_length_field_path(
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
extern int bt_ctf_field_type_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b);

/*
 * bt_ctf_field_type_get_type_id: get a field type's ctf_type_id.
 *
 * @param type Field type.
 *
 * Returns the field type's ctf_type_id, CTF_TYPE_UNKNOWN on error.
 */
extern enum ctf_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_integer: returns whether or not a given field
 *	type is an integer type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is an integer type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_integer(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_floating_point: returns whether or not a given field
 *	type is a floating point number type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a floating point number type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_floating_point(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_enumeration: returns whether or not a given field
 *	type is an enumeration type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is an enumeration type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_enumeration(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_string: returns whether or not a given field
 *	type is a string type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a string type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_string(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_structure: returns whether or not a given field
 *	type is a structure type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a structure type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_structure(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_array: returns whether or not a given field
 *	type is an array type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is an array type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_array(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_sequence: returns whether or not a given field
 *	type is a sequence type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a sequence type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_sequence(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_is_variant: returns whether or not a given field
 *	type is a variant type.
 *
 * @param type Field type.
 *
 * Returns 1 if the field type is a variant type, 0 otherwise.
 */
extern int bt_ctf_field_type_is_variant(struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_type_get and bt_ctf_field_type_put: increment and decrement
 * the field type's reference count.
 *
 * You may also use bt_ctf_get() and bt_ctf_put() with field type objects.
 *
 * These functions ensure that the field type won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a field type.
 *
 * When the field type's reference count is decremented to 0 by a
 * bt_ctf_field_type_put, the field type is freed.
 *
 * @param type Field type.
 */
extern void bt_ctf_field_type_get(struct bt_ctf_field_type *type);
extern void bt_ctf_field_type_put(struct bt_ctf_field_type *type);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_TYPES_H */
