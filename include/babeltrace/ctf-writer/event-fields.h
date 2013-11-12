#ifndef BABELTRACE_CTF_WRITER_EVENT_FIELDS_H
#define BABELTRACE_CTF_WRITER_EVENT_FIELDS_H

/*
 * BabelTrace - CTF Writer: Event Fields
 *
 * Copyright 2013 EfficiOS Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field;
struct bt_ctf_field_type;

/*
 * bt_ctf_field_create: create an instance of a field.
 *
 * Allocate a new field of the type described by the bt_ctf_field_type
 * structure.The creation of a field sets its reference count to 1.
 *
 * @param type Field type to be instanciated.
 *
 * Returns an allocated field on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_field_create(
		struct bt_ctf_field_type *type);

/*
 * bt_ctf_field_structure_get_field: get a structure's field.
 *
 * Get the structure's field corresponding to the provided field name.
 * bt_ctf_field_put() must be called on the returned value.
 *
 * @param structure Structure field instance.
 * @param name Name of the field in the provided structure.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_field_structure_get_field(
		struct bt_ctf_field *structure, const char *name);

/*
 * bt_ctf_field_array_get_field: get an array's field at position "index".
 *
 * Return the array's field at position "index". bt_ctf_field_put() must be
 * called on the returned value.
 *
 * @param array Array field instance.
 * @param index Position of the array's desired element.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_field_array_get_field(
		struct bt_ctf_field *array, uint64_t index);

/*
 * bt_ctf_field_sequence_set_length: set a sequence's length.
 *
 * Set the sequence's length field.
 *
 * @param sequence Sequence field instance.
 * @param length_field Integer field instance indicating the sequence's length.
 *
 * Returns a field instance on success, NULL on error.
 */
extern int bt_ctf_field_sequence_set_length(struct bt_ctf_field *sequence,
		struct bt_ctf_field *length_field);

/*
 * bt_ctf_field_sequence_get_field: get a sequence's field at position "index".
 *
 * Return the sequence's field at position "index". The sequence's length must
 * have been set prior to calling this function using
 * bt_ctf_field_sequence_set_length().
 * bt_ctf_field_put() must be called on the returned value.
 *
 * @param array Sequence field instance.
 * @param index Position of the sequence's desired element.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_field_sequence_get_field(
		struct bt_ctf_field *sequence, uint64_t index);

/*
 * bt_ctf_field_variant_get_field: get a variant's selected field.
 *
 * Return the variant's selected field. The "tag" field is the selector enum
 * field. bt_ctf_field_put() must be called on the returned value.
 *
 * @param variant Variant field instance.
 * @param tag Selector enumeration field.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_field_variant_get_field(
		struct bt_ctf_field *variant, struct bt_ctf_field *tag);

/*
 * bt_ctf_field_enumeration_get_container: get an enumeration field's container.
 *
 * Return the enumeration's underlying container field (an integer).
 * bt_ctf_field_put() must be called on the returned value.
 *
 * @param enumeration Enumeration field instance.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
		struct bt_ctf_field *enumeration);

/*
 * bt_ctf_field_signed_integer_set_value: set a signed integer field's value
 *
 * Set a signed integer field's value. The value is checked to make sure it
 * can be stored in the underlying field.
 *
 * @param integer Signed integer field instance.
 * @param value Signed integer field value.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *integer,
		int64_t value);

/*
 * bt_ctf_field_unsigned_integer_set_value: set unsigned integer field's value
 *
 * Set an unsigned integer field's value. The value is checked to make sure it
 * can be stored in the underlying field.
 *
 * @param integer Unsigned integer field instance.
 * @param value Unsigned integer field value.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *integer,
		uint64_t value);

/*
 * bt_ctf_field_floating_point_set_value: set a floating point field's value
 *
 * Set a floating point field's value. The underlying type may not support the
 * double's full precision.
 *
 * @param floating_point Floating point field instance.
 * @param value Floating point field value.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_floating_point_set_value(
		struct bt_ctf_field *floating_point,
		double value);

/*
 * bt_ctf_field_string_set_value: set a string field's value
 *
 * Set a string field's value.
 *
 * @param string String field instance.
 * @param value String field value (will be copied).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_field_string_set_value(struct bt_ctf_field *string,
		const char *value);

/*
 * bt_ctf_field_get and bt_ctf_field_put: increment and decrement the
 * field's reference count.
 *
 * These functions ensure that the field won't be destroyed when it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a field.
 *
 * When the field's reference count is decremented to 0 by a bt_ctf_field_put,
 * the field is freed.
 *
 * @param field Field instance.
 */
extern void bt_ctf_field_get(struct bt_ctf_field *field);
extern void bt_ctf_field_put(struct bt_ctf_field *field);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_EVENT_FIELDS_H */
