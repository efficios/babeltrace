#ifndef BABELTRACE_CTF_IR_FIELDS_INTERNAL_H
#define BABELTRACE_CTF_IR_FIELDS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event Fields internal
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

#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/types.h>
#include <glib.h>

struct bt_ctf_field {
	struct bt_object base;
	struct bt_ctf_field_type *type;
	int payload_set;
	int frozen;
};

struct bt_ctf_field_integer {
	struct bt_ctf_field parent;
	struct definition_integer definition;
};

struct bt_ctf_field_enumeration {
	struct bt_ctf_field parent;
	struct bt_ctf_field *payload;
};

struct bt_ctf_field_floating_point {
	struct bt_ctf_field parent;
	struct definition_float definition;
	struct definition_integer sign, mantissa, exp;
};

struct bt_ctf_field_structure {
	struct bt_ctf_field parent;
	GHashTable *field_name_to_index;
	GPtrArray *fields; /* Array of pointers to struct bt_ctf_field */
};

struct bt_ctf_field_variant {
	struct bt_ctf_field parent;
	struct bt_ctf_field *tag;
	struct bt_ctf_field *payload;
};

struct bt_ctf_field_array {
	struct bt_ctf_field parent;
	GPtrArray *elements; /* Array of pointers to struct bt_ctf_field */
};

struct bt_ctf_field_sequence {
	struct bt_ctf_field parent;
	struct bt_ctf_field *length;
	GPtrArray *elements; /* Array of pointers to struct bt_ctf_field */
};

struct bt_ctf_field_string {
	struct bt_ctf_field parent;
	GString *payload;
};

/*
 * Set a field's value with an already allocated field instance.
 */
BT_HIDDEN
int bt_ctf_field_structure_set_field(struct bt_ctf_field *structure,
		const char *name, struct bt_ctf_field *value);

/* Validate that the field's payload is set (returns 0 if set). */
BT_HIDDEN
int bt_ctf_field_validate(struct bt_ctf_field *field);

/* Mark field payload as unset. */
BT_HIDDEN
int bt_ctf_field_reset(struct bt_ctf_field *field);

BT_HIDDEN
int bt_ctf_field_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos);

BT_HIDDEN
void bt_ctf_field_freeze(struct bt_ctf_field *field);

/*
 * bt_ctf_field_copy: get a field's deep copy.
 *
 * Get a field's deep copy. The created field copy shares the source's
 * associated field types.
 *
 * On success, the returned copy has its reference count set to 1.
 *
 * @param field Field instance.
 *
 * Returns the field copy on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_field_copy(struct bt_ctf_field *field);


/*
 * bt_ctf_field_is_integer: returns whether or not a given field
 *	is an integer type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is an integer type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_integer(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_floating_point: returns whether or not a given field
 *	is a floating point number type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is a floating point number type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_floating_point(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_enumeration: returns whether or not a given field
 *	is an enumeration type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is an enumeration type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_enumeration(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_string: returns whether or not a given field
 *	is a string type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is a string type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_string(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_structure: returns whether or not a given field
 *	is a structure type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is a structure type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_structure(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_array: returns whether or not a given field
 *	is an array type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is an array type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_array(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_sequence: returns whether or not a given field
 *	is a sequence type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is a sequence type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_sequence(struct bt_ctf_field *field);

/*
 * bt_ctf_field_is_variant: returns whether or not a given field
 *	is a variant type.
 *
 * @param field Field instance.
 *
 * Returns 1 if the field instance is a variant type, 0 otherwise.
 */
BT_HIDDEN
int bt_ctf_field_is_variant(struct bt_ctf_field *field);

/*
 * bt_ctf_field_structure_get_field_by_index: get a structure's field by index.
 *
 * Get the structure's field corresponding to the provided field name.
 * bt_ctf_field_put() must be called on the returned value.
 * The indexes are the same as those provided for bt_ctf_field_type_structure.
 *
 * @param structure Structure field instance.
 * @param index Index of the field in the provided structure.
 *
 * Returns a field instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_field_structure_get_field_by_index(
		struct bt_ctf_field *structure, int index);

/*
 * bt_ctf_field_sequence_get_length: get a sequence's length.
 *
 * Get the sequence's length field.
 *
 * @param sequence Sequence field instance.
 *
 * Returns a field instance on success, NULL if a length was never set.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_field_sequence_get_length(
		struct bt_ctf_field *sequence);

/*
 * bt_ctf_field_variant_get_current_field: get the current selected field of a
 *	variant.
 *
 * Return the variant's current selected field. This function, unlike
 * bt_ctf_field_variant_get_field(), does not create any field; it
 * returns NULL if there's no current selected field yet.
 *
 * @param variant Variant field instance.
 *
 * Returns a field instance on success, NULL on error or when there's no
 * current selected field.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_field_variant_get_current_field(
		struct bt_ctf_field *variant);

/*
 * bt_ctf_field_enumeration_get_mapping_name: get an enumeration field's mapping
 *	name.
 *
 * Return the enumeration's underlying container field (an integer).
 * bt_ctf_field_put() must be called on the returned value.
 *
 * @param enumeration Enumeration field instance.
 *
 * Returns a field instance on success, NULL on error.
 */
BT_HIDDEN
const char *bt_ctf_field_enumeration_get_mapping_name(
		struct bt_ctf_field *enumeration);

/*
 * bt_ctf_field_signed_integer_get_value: get a signed integer field's value
 *
 * Get a signed integer field's value.
 *
 * @param integer Signed integer field instance.
 * @param value Pointer to a signed integer where the value will be stored.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_signed_integer_get_value(struct bt_ctf_field *integer,
		int64_t *value);

/*
 * bt_ctf_field_unsigned_integer_get_value: get unsigned integer field's value
 *
 * Get an unsigned integer field's value.
 *
 * @param integer Unsigned integer field instance.
 * @param value Pointer to an unsigned integer where the value will be stored.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_unsigned_integer_get_value(struct bt_ctf_field *integer,
		uint64_t *value);

/*
 * bt_ctf_field_floating_point_get_value: get a floating point field's value
 *
 * Get a floating point field's value.
 *
 * @param floating_point Floating point field instance.
 * @param value Pointer to a double where the value will be stored.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_floating_point_get_value(struct bt_ctf_field *floating_point,
		double *value);

/*
 * bt_ctf_field_string_get_value: get a string field's value
 *
 * Get a string field's value.
 *
 * @param string_field String field instance.
 *
 * Returns the string's value, NULL if unset.
 */
BT_HIDDEN
const char *bt_ctf_field_string_get_value(struct bt_ctf_field *string_field);

/*
 * bt_ctf_field_string_append: append a string to a string field's
 * current value.
 *
 * Append a string to the current value of a string field. If the string
 * field was never set using bt_ctf_field_string_set_value(), it is
 * first set to an empty string, and then the concatenation happens.
 *
 * @param string_field String field instance.
 * @param value String to append to the current string field's value.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_string_append(struct bt_ctf_field *string_field,
		const char *value);

/*
 * bt_ctf_field_string_append_len: append a string of a given length to
 * a string field's current value.
 *
 * Append a string of a given length to the current value of a string
 * field. If the string field was never set using
 * bt_ctf_field_string_set_value(), it is first set to an empty string,
 * and then the concatenation happens.
 *
 * If a null byte is encountered before the given length, only the
 * substring before the first null byte is appended.
 *
 * @param string_field String field instance.
 * @param value String to append to the current string field's value.
 * @param length Length of string value to append.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_string_append_len(
		struct bt_ctf_field *string_field, const char *value,
		unsigned int length);

/*
 * bt_ctf_field_get_type_id: get a field's ctf_type_id.
 *
 * This is a helper function which avoids a call to
 * bt_ctf_field_get_type(), followed by a call to
 * bt_ctf_field_type_get_type_id(), followed by a call to
 * bt_ctf_put().
 *
 * @param field Field instance.
 *
 * Returns the field's ctf_type_id, CTF_TYPE_UNKNOWN on error.
 */
BT_HIDDEN
enum ctf_type_id bt_ctf_field_get_type_id(struct bt_ctf_field *field);

/*
 * bt_ctf_field_get_type: get a field's type
 *
 * @param field Field intance.
 *
 * Returns a field type instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_get_type(struct bt_ctf_field *field);

#endif /* BABELTRACE_CTF_IR_FIELDS_INTERNAL_H */
