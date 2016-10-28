#ifndef BABELTRACE_CTF_IR_FIELD_TYPES_H
#define BABELTRACE_CTF_IR_FIELD_TYPES_H

/*
 * BabelTrace - CTF IR: Event field types
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

#include <stdint.h>
#include <babeltrace/ctf/events.h>

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
 * bt_ctf_field_type_integer_set_encoding: set an integer type's encoding.
 *
 * An integer encoding may be set to signal that the integer must be printed as
 * a text character.
 *
 * @param integer Integer type.
 * @param encoding Integer output encoding, defaults to
 *	CTF_STRING_NONE
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_integer_set_encoding(
		struct bt_ctf_field_type *integer,
		enum ctf_string_encoding encoding);

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
 * bt_ctf_field_type_floating_point_create: create a floating point field type.
 *
 * Allocate a new floating point field type. The creation of a field type sets
 * its reference count to 1.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void);

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
 * bt_ctf_field_type_string_create: create a string field type.
 *
 * Allocate a new string field type. The creation of a field type sets
 * its reference count to 1.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_field_type_string_create(void);

/*
 * bt_ctf_field_type_string_set_encoding: set a string type's encoding.
 *
 * Set the string type's encoding.
 *
 * @param string_type String type.
 * @param encoding String field encoding, default CTF_STRING_ASCII.
 *	Valid values are CTF_STRING_ASCII and
 *	CTF_STRING_UTF8.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_type_string_set_encoding(
		struct bt_ctf_field_type *string_type,
		enum ctf_string_encoding encoding);

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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_H */
