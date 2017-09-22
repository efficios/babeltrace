/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Type */
struct bt_field_type;

/* Common enumerations */
enum bt_scope {
	BT_SCOPE_UNKNOWN		= -1,
	BT_SCOPE_TRACE_PACKET_HEADER	= 1,
	BT_SCOPE_STREAM_PACKET_CONTEXT	= 2,
	BT_SCOPE_STREAM_EVENT_HEADER	= 3,
	BT_SCOPE_STREAM_EVENT_CONTEXT	= 4,
	BT_SCOPE_EVENT_CONTEXT		= 5,
	BT_SCOPE_EVENT_PAYLOAD		= 6,
	BT_SCOPE_ENV			= 0,
	BT_SCOPE_EVENT_FIELDS		= 6,
};

enum bt_field_type_id {
	BT_FIELD_TYPE_ID_UNKNOWN	= -1,
	BT_FIELD_TYPE_ID_INTEGER	= 0,
	BT_FIELD_TYPE_ID_FLOAT		= 1,
	BT_FIELD_TYPE_ID_ENUM		= 2,
	BT_FIELD_TYPE_ID_STRING		= 3,
	BT_FIELD_TYPE_ID_STRUCT		= 4,
	BT_FIELD_TYPE_ID_ARRAY		= 6,
	BT_FIELD_TYPE_ID_SEQUENCE	= 7,
	BT_FIELD_TYPE_ID_VARIANT	= 5,
	BT_FIELD_TYPE_ID_NR		= 8,
};

enum bt_byte_order {
	BT_BYTE_ORDER_UNKNOWN	= -1,
	BT_BYTE_ORDER_NATIVE	= 0,
	BT_BYTE_ORDER_UNSPECIFIED,
	BT_BYTE_ORDER_LITTLE_ENDIAN,
	BT_BYTE_ORDER_BIG_ENDIAN,
	BT_BYTE_ORDER_NETWORK,
};

/**
@brief	String encoding of a @ft.
*/
enum bt_string_encoding {
	BT_STRING_ENCODING_UNKNOWN	= -1,
	BT_STRING_ENCODING_NONE,
	BT_STRING_ENCODING_UTF8,
	BT_STRING_ENCODING_ASCII,
};

/* Common functions */
enum bt_field_type_id bt_field_type_get_type_id(
		struct bt_field_type *field_type);
int bt_field_type_get_alignment(
		struct bt_field_type *field_type);
int bt_field_type_set_alignment(struct bt_field_type *field_type,
		unsigned int alignment);
enum bt_byte_order bt_field_type_get_byte_order(
		struct bt_field_type *field_type);
int bt_field_type_set_byte_order(
		struct bt_field_type *field_type,
		enum bt_byte_order byte_order);
int bt_field_type_compare(struct bt_field_type *field_type_a,
		struct bt_field_type *field_type_b);
struct bt_field_type *bt_field_type_copy(
		struct bt_field_type *field_type);

/* Integer field type base enumeration */
enum bt_integer_base {
	BT_INTEGER_BASE_UNKNOWN		= -1,
	BT_INTEGER_BASE_UNSPECIFIED	= 0,
	BT_INTEGER_BASE_BINARY		= 2,
	BT_INTEGER_BASE_OCTAL		= 8,
	BT_INTEGER_BASE_DECIMAL		= 10,
	BT_INTEGER_BASE_HEXADECIMAL	= 16,
};

/* Integer field type functions */
struct bt_field_type *bt_field_type_integer_create(
		unsigned int size);
int bt_field_type_integer_get_size(
		struct bt_field_type *int_field_type);
int bt_field_type_integer_set_size(
		struct bt_field_type *int_field_type, unsigned int size);
int bt_field_type_integer_is_signed(
		struct bt_field_type *int_field_type);
int bt_field_type_integer_set_is_signed(
		struct bt_field_type *int_field_type, int is_signed);
enum bt_integer_base bt_field_type_integer_get_base(
		struct bt_field_type *int_field_type);
int bt_field_type_integer_set_base(
		struct bt_field_type *int_field_type,
		enum bt_integer_base base);
enum bt_string_encoding bt_field_type_integer_get_encoding(
		struct bt_field_type *int_field_type);
int bt_field_type_integer_set_encoding(
		struct bt_field_type *int_field_type,
		enum bt_string_encoding encoding);
struct bt_clock_class *bt_field_type_integer_get_mapped_clock_class(
		struct bt_field_type *int_field_type);
int bt_field_type_integer_set_mapped_clock_class(
		struct bt_field_type *int_field_type,
		struct bt_clock_class *clock_class);

/* Floating point number field type functions */
struct bt_field_type *bt_field_type_floating_point_create(void);
int bt_field_type_floating_point_get_exponent_digits(
		struct bt_field_type *float_field_type);
int bt_field_type_floating_point_set_exponent_digits(
		struct bt_field_type *float_field_type,
		unsigned int exponent_size);
int bt_field_type_floating_point_get_mantissa_digits(
		struct bt_field_type *float_field_type);
int bt_field_type_floating_point_set_mantissa_digits(
		struct bt_field_type *float_field_type,
		unsigned int mantissa_sign_size);

/* Enumeration field type functions */
struct bt_field_type *bt_field_type_enumeration_create(
		struct bt_field_type *int_field_type);
struct bt_field_type *bt_field_type_enumeration_get_container_type(
		struct bt_field_type *enum_field_type);
int64_t bt_field_type_enumeration_get_mapping_count(
		struct bt_field_type *enum_field_type);
int bt_field_type_enumeration_get_mapping_signed(
		struct bt_field_type *enum_field_type, int index,
		const char **BTOUTSTR, int64_t *OUTPUT, int64_t *OUTPUT);
int bt_field_type_enumeration_get_mapping_unsigned(
		struct bt_field_type *enum_field_type, int index,
		const char **BTOUTSTR, uint64_t *OUTPUT,
		uint64_t *OUTPUT);
int bt_field_type_enumeration_add_mapping_signed(
		struct bt_field_type *enum_field_type, const char *name,
		int64_t range_begin, int64_t range_end);
int bt_field_type_enumeration_add_mapping_unsigned(
		struct bt_field_type *enum_field_type, const char *name,
		uint64_t range_begin, uint64_t range_end);
struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_name(
		struct bt_field_type *enum_field_type,
		const char *name);
struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_signed_value(
		struct bt_field_type *enum_field_type,
		int64_t value);
struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_unsigned_value(
		struct bt_field_type *enum_field_type,
		uint64_t value);

/* Enumeration field type mapping iterator functions */
int bt_field_type_enumeration_mapping_iterator_get_signed(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **BTOUTSTR, int64_t *OUTPUT, int64_t *OUTPUT);
int bt_field_type_enumeration_mapping_iterator_get_unsigned(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **BTOUTSTR, uint64_t *OUTPUT, uint64_t *OUTPUT);
int bt_field_type_enumeration_mapping_iterator_next(
		struct bt_field_type_enumeration_mapping_iterator *iter);

/* String field type functions */
struct bt_field_type *bt_field_type_string_create(void);
enum bt_string_encoding bt_field_type_string_get_encoding(
		struct bt_field_type *string_field_type);
int bt_field_type_string_set_encoding(
		struct bt_field_type *string_field_type,
		enum bt_string_encoding encoding);

/* Structure field type functions */
struct bt_field_type *bt_field_type_structure_create(void);
int64_t bt_field_type_structure_get_field_count(
		struct bt_field_type *struct_field_type);
int bt_field_type_structure_get_field_by_index(
		struct bt_field_type *struct_field_type,
		const char **BTOUTSTR, struct bt_field_type **BTOUTFT,
		uint64_t index);
struct bt_field_type *bt_field_type_structure_get_field_type_by_name(
		struct bt_field_type *struct_field_type,
		const char *field_name);
int bt_field_type_structure_add_field(
		struct bt_field_type *struct_field_type,
		struct bt_field_type *field_type,
		const char *field_name);

/* Array field type functions */
struct bt_field_type *bt_field_type_array_create(
		struct bt_field_type *element_field_type,
		unsigned int length);
struct bt_field_type *bt_field_type_array_get_element_type(
		struct bt_field_type *array_field_type);
int64_t bt_field_type_array_get_length(
		struct bt_field_type *array_field_type);

/* Sequence field type functions */
struct bt_field_type *bt_field_type_sequence_create(
		struct bt_field_type *element_field_type,
		const char *length_name);
struct bt_field_type *bt_field_type_sequence_get_element_type(
		struct bt_field_type *sequence_field_type);
const char *bt_field_type_sequence_get_length_field_name(
		struct bt_field_type *sequence_field_type);
struct bt_field_path *bt_field_type_sequence_get_length_field_path(
		struct bt_field_type *sequence_field_type);

/* Variant field type functions */
struct bt_field_type *bt_field_type_variant_create(
		struct bt_field_type *tag_field_type,
		const char *tag_name);
struct bt_field_type *bt_field_type_variant_get_tag_type(
		struct bt_field_type *variant_field_type);
const char *bt_field_type_variant_get_tag_name(
		struct bt_field_type *variant_field_type);
int bt_field_type_variant_set_tag_name(
		struct bt_field_type *variant_field_type,
		const char *tag_name);
struct bt_field_path *bt_field_type_variant_get_tag_field_path(
		struct bt_field_type *variant_field_type);
int64_t bt_field_type_variant_get_field_count(
		struct bt_field_type *variant_field_type);
int bt_field_type_variant_get_field_by_index(
		struct bt_field_type *variant_field_type,
		const char **BTOUTSTR,
		struct bt_field_type **BTOUTFT, uint64_t index);
struct bt_field_type *bt_field_type_variant_get_field_type_by_name(
		struct bt_field_type *variant_field_type,
		const char *field_name);
int bt_field_type_variant_add_field(
		struct bt_field_type *variant_field_type,
		struct bt_field_type *field_type,
		const char *field_name);
