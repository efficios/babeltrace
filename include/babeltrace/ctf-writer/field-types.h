#ifndef BABELTRACE_CTF_WRITER_FIELD_TYPES_H
#define BABELTRACE_CTF_WRITER_FIELD_TYPES_H

/*
 * Babeltrace - CTF writer: Event Fields
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
#include <stddef.h>

#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_field;
struct bt_ctf_field_type;

enum bt_ctf_scope {
	/// Unknown, used for errors.
	BT_CTF_SCOPE_UNKNOWN			= -1,

	/// Trace packet header.
	BT_CTF_SCOPE_TRACE_PACKET_HEADER	= 1,

	/// Stream packet context.
	BT_CTF_SCOPE_STREAM_PACKET_CONTEXT	= 2,

	/// Stream event header.
	BT_CTF_SCOPE_STREAM_EVENT_HEADER	= 3,

	/// Stream event context.
	BT_CTF_SCOPE_STREAM_EVENT_CONTEXT	= 4,

	/// Event context.
	BT_CTF_SCOPE_EVENT_CONTEXT		= 5,

	/// Event payload.
	BT_CTF_SCOPE_EVENT_PAYLOAD		= 6,

	/// @cond DOCUMENT
	BT_CTF_SCOPE_ENV			= 0,
	BT_CTF_SCOPE_EVENT_FIELDS		= 6,
	/// @endcond
};

enum bt_ctf_field_type_id {
	BT_CTF_FIELD_TYPE_ID_UNKNOWN	= -1,
	BT_CTF_FIELD_TYPE_ID_INTEGER	= 0,
	BT_CTF_FIELD_TYPE_ID_FLOAT	= 1,
	BT_CTF_FIELD_TYPE_ID_ENUM	= 2,
	BT_CTF_FIELD_TYPE_ID_STRING	= 3,
	BT_CTF_FIELD_TYPE_ID_STRUCT	= 4,
	BT_CTF_FIELD_TYPE_ID_ARRAY	= 5,
	BT_CTF_FIELD_TYPE_ID_SEQUENCE	= 6,
	BT_CTF_FIELD_TYPE_ID_VARIANT	= 7,
	BT_CTF_FIELD_TYPE_ID_NR,
};

extern enum bt_ctf_field_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *field_type);

enum bt_ctf_byte_order {
	BT_CTF_BYTE_ORDER_UNKNOWN		= -1,
	BT_CTF_BYTE_ORDER_NATIVE		= 0,
	BT_CTF_BYTE_ORDER_UNSPECIFIED,
	BT_CTF_BYTE_ORDER_LITTLE_ENDIAN,
	BT_CTF_BYTE_ORDER_BIG_ENDIAN,
	BT_CTF_BYTE_ORDER_NETWORK,
};

enum bt_ctf_string_encoding {
	BT_CTF_STRING_ENCODING_UNKNOWN	= -1,
	BT_CTF_STRING_ENCODING_NONE,
	BT_CTF_STRING_ENCODING_UTF8,
	BT_CTF_STRING_ENCODING_ASCII,
};

/* Pre-2.0 CTF writer compatibility */
#define ctf_string_encoding	bt_ctf_string_encoding

extern int bt_ctf_field_type_get_alignment(
		struct bt_ctf_field_type *field_type);

extern int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *field_type,
		unsigned int alignment);

extern enum bt_ctf_byte_order bt_ctf_field_type_get_byte_order(
		struct bt_ctf_field_type *field_type);

extern int bt_ctf_field_type_set_byte_order(
		struct bt_ctf_field_type *field_type,
		enum bt_ctf_byte_order byte_order);

enum bt_ctf_integer_base {
	/// Unknown, used for errors.
	BT_CTF_INTEGER_BASE_UNKNOWN		= -1,

	/// Unspecified by the tracer.
	BT_CTF_INTEGER_BASE_UNSPECIFIED		= 0,

	/// Binary.
	BT_CTF_INTEGER_BASE_BINARY		= 2,

	/// Octal.
	BT_CTF_INTEGER_BASE_OCTAL		= 8,

	/// Decimal.
	BT_CTF_INTEGER_BASE_DECIMAL		= 10,

	/// Hexadecimal.
	BT_CTF_INTEGER_BASE_HEXADECIMAL		= 16,
};

extern struct bt_ctf_field_type *bt_ctf_field_type_integer_create(
		unsigned int size);

extern int bt_ctf_field_type_integer_get_size(
		struct bt_ctf_field_type *int_field_type);

extern int bt_ctf_field_type_integer_set_size(
		struct bt_ctf_field_type *int_field_type, unsigned int size);

extern bt_bool bt_ctf_field_type_integer_is_signed(
		struct bt_ctf_field_type *int_field_type);

/* Pre-2.0 CTF writer compatibility */
static inline
int bt_ctf_field_type_integer_get_signed(
		struct bt_ctf_field_type *int_field_type)
{
	return bt_ctf_field_type_integer_is_signed(int_field_type) ? 1 : 0;
}

extern int bt_ctf_field_type_integer_set_is_signed(
		struct bt_ctf_field_type *int_field_type, bt_bool is_signed);

/* Pre-2.0 CTF writer compatibility */
static inline
int bt_ctf_field_type_integer_set_signed(
		struct bt_ctf_field_type *int_field_type, int is_signed)
{
	return bt_ctf_field_type_integer_set_is_signed(int_field_type,
		is_signed ? BT_TRUE : BT_FALSE);
}

extern enum bt_ctf_integer_base bt_ctf_field_type_integer_get_base(
		struct bt_ctf_field_type *int_field_type);

extern int bt_ctf_field_type_integer_set_base(
		struct bt_ctf_field_type *int_field_type,
		enum bt_ctf_integer_base base);

extern enum bt_ctf_string_encoding bt_ctf_field_type_integer_get_encoding(
		struct bt_ctf_field_type *int_field_type);

extern int bt_ctf_field_type_integer_set_encoding(
		struct bt_ctf_field_type *int_field_type,
		enum bt_ctf_string_encoding encoding);

extern struct bt_ctf_clock_class *bt_ctf_field_type_integer_get_mapped_clock_class(
		struct bt_ctf_field_type *int_field_type);

extern int bt_ctf_field_type_integer_set_mapped_clock_class(
		struct bt_ctf_field_type *int_field_type,
		struct bt_ctf_clock_class *clock_class);

extern struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void);

extern int bt_ctf_field_type_floating_point_get_exponent_digits(
		struct bt_ctf_field_type *float_field_type);

extern int bt_ctf_field_type_floating_point_set_exponent_digits(
		struct bt_ctf_field_type *float_field_type,
		unsigned int exponent_size);

extern int bt_ctf_field_type_floating_point_get_mantissa_digits(
		struct bt_ctf_field_type *float_field_type);

extern int bt_ctf_field_type_floating_point_set_mantissa_digits(
		struct bt_ctf_field_type *float_field_type,
		unsigned int mantissa_sign_size);

extern struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(
		struct bt_ctf_field_type *int_field_type);

extern
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_get_container_field_type(
		struct bt_ctf_field_type *enum_field_type);

extern int64_t bt_ctf_field_type_enumeration_get_mapping_count(
		struct bt_ctf_field_type *enum_field_type);

extern int bt_ctf_field_type_enumeration_signed_get_mapping_by_index(
		struct bt_ctf_field_type *enum_field_type, uint64_t index,
		const char **name, int64_t *range_begin, int64_t *range_end);

extern int bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(
		struct bt_ctf_field_type *enum_field_type, uint64_t index,
		const char **name, uint64_t *range_begin,
		uint64_t *range_end);

extern int bt_ctf_field_type_enumeration_signed_add_mapping(
		struct bt_ctf_field_type *enum_field_type,
		const char *name, int64_t range_begin, int64_t range_end);

extern int bt_ctf_field_type_enumeration_unsigned_add_mapping(
		struct bt_ctf_field_type *enum_field_type,
		const char *name, uint64_t range_begin, uint64_t range_end);

/* Pre-2.0 CTF writer compatibility */
static inline
int bt_ctf_field_type_enumeration_add_mapping(
		struct bt_ctf_field_type *enumeration, const char *name,
		int64_t range_start, int64_t range_end)
{
	return bt_ctf_field_type_enumeration_signed_add_mapping(enumeration,
		name, range_start, range_end);
}

extern struct bt_ctf_field_type *bt_ctf_field_type_string_create(void);

extern enum bt_ctf_string_encoding bt_ctf_field_type_string_get_encoding(
		struct bt_ctf_field_type *string_field_type);

extern int bt_ctf_field_type_string_set_encoding(
		struct bt_ctf_field_type *string_field_type,
		enum bt_ctf_string_encoding encoding);

extern struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void);

extern int64_t bt_ctf_field_type_structure_get_field_count(
		struct bt_ctf_field_type *struct_field_type);

extern int bt_ctf_field_type_structure_get_field_by_index(
		struct bt_ctf_field_type *struct_field_type,
		const char **field_name, struct bt_ctf_field_type **field_type,
		uint64_t index);

/* Pre-2.0 CTF writer compatibility */
static inline
int bt_ctf_field_type_structure_get_field(struct bt_ctf_field_type *structure,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index)
{
	return bt_ctf_field_type_structure_get_field_by_index(structure,
		field_name, field_type, (uint64_t) index);
}

extern
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_field_type_by_name(
		struct bt_ctf_field_type *struct_field_type,
		const char *field_name);

extern int bt_ctf_field_type_structure_add_field(
		struct bt_ctf_field_type *struct_field_type,
		struct bt_ctf_field_type *field_type,
		const char *field_name);

extern struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_field_type,
		unsigned int length);

extern struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_field_type(
		struct bt_ctf_field_type *array_field_type);

extern int64_t bt_ctf_field_type_array_get_length(
		struct bt_ctf_field_type *array_field_type);

extern struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_field_type,
		const char *length_name);

extern struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_field_type(
		struct bt_ctf_field_type *sequence_field_type);

extern const char *bt_ctf_field_type_sequence_get_length_field_name(
		struct bt_ctf_field_type *sequence_field_type);

extern struct bt_ctf_field_type *bt_ctf_field_type_variant_create(
		struct bt_ctf_field_type *tag_field_type,
		const char *tag_name);

extern struct bt_ctf_field_type *bt_ctf_field_type_variant_get_tag_field_type(
		struct bt_ctf_field_type *variant_field_type);

extern const char *bt_ctf_field_type_variant_get_tag_name(
		struct bt_ctf_field_type *variant_field_type);

extern int bt_ctf_field_type_variant_set_tag_name(
		struct bt_ctf_field_type *variant_field_type,
		const char *tag_name);

extern int64_t bt_ctf_field_type_variant_get_field_count(
		struct bt_ctf_field_type *variant_field_type);

extern int bt_ctf_field_type_variant_get_field_by_index(
		struct bt_ctf_field_type *variant_field_type,
		const char **field_name,
		struct bt_ctf_field_type **field_type, uint64_t index);

extern
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_by_name(
		struct bt_ctf_field_type *variant_field_type,
		const char *field_name);

extern
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_from_tag(
		struct bt_ctf_field_type *variant_field_type,
		struct bt_ctf_field *tag_field);

extern int bt_ctf_field_type_variant_add_field(
		struct bt_ctf_field_type *variant_field_type,
		struct bt_ctf_field_type *field_type,
		const char *field_name);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_FIELD_TYPES_H */
