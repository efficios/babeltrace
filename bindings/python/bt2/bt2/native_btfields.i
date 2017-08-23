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
struct bt_ctf_field;

/* Common functions */
struct bt_ctf_field *bt_ctf_field_create(
		struct bt_ctf_field_type *type);
struct bt_ctf_field_type *bt_ctf_field_get_type(
		struct bt_ctf_field *field);
struct bt_ctf_field *bt_ctf_field_copy(struct bt_ctf_field *field);

/* Integer field functions */
int bt_ctf_field_signed_integer_get_value(struct bt_ctf_field *integer,
		int64_t *OUTPUT);
int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *integer,
		int64_t value);
int bt_ctf_field_unsigned_integer_get_value(struct bt_ctf_field *integer,
		uint64_t *OUTPUT);
int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *integer,
		uint64_t value);

/* Floating point number field functions */
int bt_ctf_field_floating_point_get_value(
		struct bt_ctf_field *floating_point, double *OUTPUT);
int bt_ctf_field_floating_point_set_value(
		struct bt_ctf_field *floating_point,
		double value);

/* Enumeration field functions */
struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
		struct bt_ctf_field *enumeration);
struct bt_ctf_field_type_enumeration_mapping_iterator *
bt_ctf_field_enumeration_get_mappings(struct bt_ctf_field *enum_field);

/* String field functions */
const char *bt_ctf_field_string_get_value(
		struct bt_ctf_field *string_field);
int bt_ctf_field_string_set_value(struct bt_ctf_field *string_field,
		const char *value);
int bt_ctf_field_string_append(struct bt_ctf_field *string_field,
		const char *value);
int bt_ctf_field_string_append_len(
		struct bt_ctf_field *string_field, const char *value,
		unsigned int length);

/* Structure field functions */
struct bt_ctf_field *bt_ctf_field_structure_get_field_by_index(
		struct bt_ctf_field *structure, int index);
struct bt_ctf_field *bt_ctf_field_structure_get_field_by_name(
		struct bt_ctf_field *struct_field, const char *name);
int bt_ctf_field_structure_set_field_by_name(struct bt_ctf_field *struct_field,
		const char *name, struct bt_ctf_field *field);

/* Array field functions */
struct bt_ctf_field *bt_ctf_field_array_get_field(
		struct bt_ctf_field *array, uint64_t index);

/* Sequence field functions */
struct bt_ctf_field *bt_ctf_field_sequence_get_length(
		struct bt_ctf_field *sequence);
int bt_ctf_field_sequence_set_length(struct bt_ctf_field *sequence,
		struct bt_ctf_field *length_field);
struct bt_ctf_field *bt_ctf_field_sequence_get_field(
		struct bt_ctf_field *sequence, uint64_t index);

/* Variant field functions */
struct bt_ctf_field *bt_ctf_field_variant_get_field(
		struct bt_ctf_field *variant, struct bt_ctf_field *tag);
struct bt_ctf_field *bt_ctf_field_variant_get_current_field(
		struct bt_ctf_field *variant);
struct bt_ctf_field *bt_ctf_field_variant_get_tag(
		struct bt_ctf_field *variant);
