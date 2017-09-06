#ifndef BABELTRACE_CTF_IR_FIELDS_H
#define BABELTRACE_CTF_IR_FIELDS_H

/*
 * Babeltrace - CTF IR: Event Fields
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

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_field_type;

/**
@defgroup ctfirfields CTF IR fields
@ingroup ctfir
@brief CTF IR fields.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>field</em></strong> is an object which holds a
concrete value, and which is described by a @ft.

In the CTF IR hierarchy, you can set the root fields of two objects:

- \ref ctfirpacket
  - Trace packet header field: bt_ctf_packet_set_header().
  - Stream packet context field: bt_ctf_packet_set_context().
- \ref ctfirevent
  - Stream event header field: bt_ctf_event_set_header().
  - Stream event context field: bt_ctf_event_set_stream_event_context().
  - Event context field: bt_ctf_event_set_event_context().
  - Event payload field: bt_ctf_event_set_payload_field().

There are two categories of fields:

- <strong>Basic fields</strong>:
  - @intfield: contains an integral value.
  - @floatfield: contains a floating point number value.
  - @enumfield: contains an integer field which contains an integral
    value.
  - @stringfield: contains a string value.
- <strong>Compound fields</strong>:
  - @structfield: contains an ordered list of named fields
    (possibly with different @fts).
  - @arrayfield: contains an ordered list of fields which share
    the same field type.
  - @seqfield: contains an ordered list of fields which share
    the same field type.
  - @varfield: contains a single, current field.

You can create a field object from a @ft object with
bt_ctf_field_create(). The enumeration and compound fields create their
contained fields with the following getters if such fields do not exist
yet:

- bt_ctf_field_enumeration_get_container()
- bt_ctf_field_structure_get_field_by_name()
- bt_ctf_field_array_get_field()
- bt_ctf_field_sequence_get_field()
- bt_ctf_field_variant_get_field()

If you already have a field object, you can also assign it to a specific
name within a @structfield with
bt_ctf_field_structure_set_field_by_name().

You can get a reference to the @ft which was used to create a field with
bt_ctf_field_get_type(). You can get the
\link #bt_ctf_field_type_id type ID\endlink of this field type directly with
bt_ctf_field_get_type_id().

You can get a deep copy of a field with bt_ctf_field_copy(). The field
copy, and its contained field copies if it's the case, have the same
field type as the originals.

As with any Babeltrace object, CTF IR field objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

The functions which freeze CTF IR \link ctfirpacket packet\endlink and
\link ctfirevent event\endlink objects also freeze their root field
objects. You cannot modify a frozen field object: it is considered
immutable, except for \link refs reference counting\endlink.

@sa ctfirfieldtypes

@file
@brief CTF IR fields type and functions.
@sa ctfirfields

@addtogroup ctfirfields
@{
*/

/**
@struct bt_ctf_field
@brief A CTF IR field.
@sa ctfirfields
*/
struct bt_ctf_field;
struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field_type;
struct bt_ctf_field_type_enumeration_mapping_iterator;

/**
@name Creation and parent field type access functions
@{
*/

/**
@brief  Creates an uninitialized @field described by the @ft
	\p field_type.

On success, \p field_type becomes the parent of the created field
object.

On success, this function creates an \em uninitialized field: it has
no value. You need to set the value of the created field with one of the
its specific setters.

@param[in] field_type	Field type which describes the field to create.
@returns		Created field object, or \c NULL on error.

@prenotnull{field_type}
@postsuccessrefcountret1
@postsuccessfrozen{field_type}
*/
extern struct bt_ctf_field *bt_ctf_field_create(
		struct bt_ctf_field_type *field_type);

/**
@brief	Returns the parent @ft of the @field \p field.

This function returns a reference to the field type which was used to
create the field object in the first place with bt_ctf_field_create().

@param[in] field	Field of which to get the parent field type.
@returns		Parent field type of \p event,
			or \c NULL on error.

@prenotnull{field}
@postrefcountsame{field}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field_type *bt_ctf_field_get_type(
	struct bt_ctf_field *field);

/** @} */

/**
@name Type information
@{
*/

/**
@brief	Returns the type ID of the @ft of the @field \p field.

@param[in] field	Field of which to get the type ID of its
			parent field type..
@returns		Type ID of the parent field type of \p field,
			or #BT_CTF_FIELD_TYPE_ID_UNKNOWN on error.

@prenotnull{field}
@postrefcountsame{field}

@sa #bt_ctf_field_type_id: CTF IR field type ID.
@sa bt_ctf_field_is_integer(): Returns whether or not a given field is a
	@intfield.
@sa bt_ctf_field_is_floating_point(): Returns whether or not a given
	field is a @floatfield.
@sa bt_ctf_field_is_enumeration(): Returns whether or not a given field
	is a @enumfield.
@sa bt_ctf_field_is_string(): Returns whether or not a given field is a
	@stringfield.
@sa bt_ctf_field_is_structure(): Returns whether or not a given field is
	a @structfield.
@sa bt_ctf_field_is_array(): Returns whether or not a given field is a
	@arrayfield.
@sa bt_ctf_field_is_sequence(): Returns whether or not a given field is
	a @seqfield.
@sa bt_ctf_field_is_variant(): Returns whether or not a given field is a
	@varfield.
*/
extern enum bt_ctf_field_type_id bt_ctf_field_get_type_id(struct bt_ctf_field *field);

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
extern int bt_ctf_field_signed_integer_get_value(struct bt_ctf_field *integer,
		int64_t *value);

/**
@brief	Returns whether or not the @field \p field is a @intfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is an integer field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_integer(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @floatfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is a floating point number fiel
			#BT_FALSE or 0 otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_floating_point(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @enumfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is an enumeration field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_enumeration(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @stringfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is a string field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_string(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @structfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is a structure field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_structure(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @arrayfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is an array field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_array(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @seqfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is a sequence field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_sequence(struct bt_ctf_field *field);

/**
@brief	Returns whether or not the @field \p field is a @varfield.

@param[in] field	Field to check (can be \c NULL).
@returns                #BT_TRUE if \p field is a variant field, or
			#BT_FALSE otherwise (including if \p field is
			\c NULL).

@prenotnull{field}
@postrefcountsame{field}

@sa bt_ctf_field_get_type_id(): Returns the type ID of a given
	field's type.
*/
extern bt_bool bt_ctf_field_is_variant(struct bt_ctf_field *field);

/** @} */

/**
@name Misc. functions
@{
*/

/**
@brief	Creates a \em deep copy of the @field \p field.

You can copy a frozen field: the resulting copy is <em>not frozen</em>.

@param[in] field	Field to copy.
@returns		Deep copy of \p field on success,
			or \c NULL on error.

@prenotnull{field}
@postrefcountsame{field}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned field is not frozen.
*/
extern struct bt_ctf_field *bt_ctf_field_copy(struct bt_ctf_field *field);

/** @} */

/** @} */

/**
@defgroup ctfirintfield CTF IR integer field
@ingroup ctfirfields
@brief CTF IR integer field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>integer field</em></strong> is a @field which
holds a signed or unsigned integral value, and which is described by
a @intft.

An integer field object is considered \em unsigned if
bt_ctf_field_type_integer_get_signed() on its parent field type returns
0. Otherwise it is considered \em signed. You \em must use
bt_ctf_field_unsigned_integer_get_value() and
bt_ctf_field_unsigned_integer_set_value() with an unsigned integer
field, and bt_ctf_field_signed_integer_get_value() and
bt_ctf_field_signed_integer_set_value() with a signed integer field.

After you create an integer field with bt_ctf_field_create(), you
\em must set an integral value with
bt_ctf_field_unsigned_integer_set_value() or
bt_ctf_field_signed_integer_set_value() before you can get the
field's value with bt_ctf_field_unsigned_integer_get_value() or
bt_ctf_field_signed_integer_get_value().

@sa ctfirintfieldtype
@sa ctfirfields

@addtogroup ctfirintfield
@{
*/

/**
@brief	Returns the signed integral value of the @intfield
	\p integer_field.

@param[in] integer_field	Integer field of which to get the
				signed integral value.
@param[out] value		Returned signed integral value of
				\p integer_field.
@returns			0 on success, or a negative value on
				error, including if \p integer_field
				has no integral value yet.

@prenotnull{integer_field}
@prenotnull{value}
@preisintfield{integer_field}
@pre bt_ctf_field_type_integer_get_signed() returns 1 for the parent
	@ft of \p integer_field.
@pre \p integer_field contains a signed integral value previously
	set with bt_ctf_field_signed_integer_set_value().
@postrefcountsame{integer_field}

@sa bt_ctf_field_signed_integer_set_value(): Sets the signed integral
	value of a given integer field.
*/
extern int bt_ctf_field_signed_integer_get_value(
		struct bt_ctf_field *integer_field, int64_t *value);

/**
@brief	Sets the signed integral value of the @intfield
	\p integer_field to \p value.

@param[in] integer_field	Integer field of which to set
				the signed integral value.
@param[in] value		New signed integral value of
				\p integer_field.
@returns			0 on success, or a negative value on error.

@prenotnull{integer_field}
@preisintfield{integer_field}
@prehot{integer_field}
@pre bt_ctf_field_type_integer_get_signed() returns 1 for the parent
	@ft of \p integer_field.
@postrefcountsame{integer_field}

@sa bt_ctf_field_signed_integer_get_value(): Returns the signed integral
	value of a given integer field.
*/
extern int bt_ctf_field_signed_integer_set_value(
		struct bt_ctf_field *integer_field, int64_t value);

/**
@brief	Returns the unsigned integral value of the @intfield
	\p integer_field.

@param[in] integer_field	Integer field of which to get the
				unsigned integral value.
@param[out] value		Returned unsigned integral value of
				\p integer_field.
@returns			0 on success, or a negative value on
				error, including if \p integer_field
				has no integral value yet.

@prenotnull{integer_field}
@prenotnull{value}
@preisintfield{integer_field}
@pre bt_ctf_field_type_integer_get_signed() returns 0 for the parent
	@ft of \p integer_field.
@pre \p integer_field contains an unsigned integral value previously
	set with bt_ctf_field_unsigned_integer_set_value().
@postrefcountsame{integer_field}

@sa bt_ctf_field_unsigned_integer_set_value(): Sets the unsigned
	integral value of a given integer field.
*/
extern int bt_ctf_field_unsigned_integer_get_value(
		struct bt_ctf_field *integer_field, uint64_t *value);

/**
@brief	Sets the unsigned integral value of the @intfield
	\p integer_field to \p value.

@param[in] integer_field	Integer field of which to set
				the unsigned integral value.
@param[in] value		New unsigned integral value of
				\p integer_field.
@returns			0 on success, or a negative value on error.

@prenotnull{integer_field}
@preisintfield{integer_field}
@prehot{integer_field}
@pre bt_ctf_field_type_integer_get_signed() returns 0 for the parent
	@ft of \p integer_field.
@postrefcountsame{integer_field}

@sa bt_ctf_field_unsigned_integer_get_value(): Returns the unsigned
	integral value of a given integer field.
*/
extern int bt_ctf_field_unsigned_integer_set_value(
		struct bt_ctf_field *integer_field, uint64_t value);

/** @} */

/**
@defgroup ctfirfloatfield CTF IR floating point number field
@ingroup ctfirfields
@brief CTF IR floating point number field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>floating point number field</em></strong> is a
@field which holds a floating point number value, and which is
described by a @floatft.

After you create a floating point number field with bt_ctf_field_create(), you
\em must set a floating point number value with
bt_ctf_field_floating_point_set_value() before you can get the
field's value with bt_ctf_field_floating_point_get_value().

@sa ctfirfloatfieldtype
@sa ctfirfields

@addtogroup ctfirfloatfield
@{
*/

/**
@brief	Returns the floating point number value of the @floatfield
	\p float_field.

@param[in] float_field	Floating point number field of which to get the
			floating point number value.
@param[out] value	Returned floating point number value of
			\p float_field.
@returns                0 on success, or a negative value on error,
			including if \p float_field has no floating
			point number value yet.

@prenotnull{float_field}
@prenotnull{value}
@preisfloatfield{float_field}
@pre \p float_field contains a floating point number value previously
	set with bt_ctf_field_floating_point_set_value().
@postrefcountsame{float_field}

@sa bt_ctf_field_floating_point_set_value(): Sets the floating point
	number value of a given floating point number field.
*/
extern int bt_ctf_field_floating_point_get_value(
		struct bt_ctf_field *float_field, double *value);

/**
@brief	Sets the floating point number value of the @floatfield
	\p float_field to \p value.

@param[in] float_field	Floating point number field of which to set
			the floating point number value.
@param[in] value	New floating point number value of
			\p float_field.
@returns		0 on success, or a negative value on error.

@prenotnull{float_field}
@preisfloatfield{float_field}
@prehot{float_field}
@postrefcountsame{float_field}

@sa bt_ctf_field_floating_point_get_value(): Returns the floating point
	number value of a given floating point number field.
*/
extern int bt_ctf_field_floating_point_set_value(
		struct bt_ctf_field *float_field,
		double value);

/** @} */

/**
@defgroup ctfirenumfield CTF IR enumeration field
@ingroup ctfirfields
@brief CTF IR enumeration field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>enumeration field</em></strong> is a @field which
holds a @intfield, and which is described by a @enumft.

To set the current integral value of an enumeration field, you need to
get its wrapped @intfield with bt_ctf_field_enumeration_get_container(),
and then set the integral value with either
bt_ctf_field_signed_integer_set_value() or
bt_ctf_field_unsigned_integer_set_value().

Once you set the integral value of an enumeration field by following the
previous paragraph, you can get the mappings containing this value in
their range with bt_ctf_field_enumeration_get_mappings(). This function
returns a @enumftiter.

@sa ctfirenumfieldtype
@sa ctfirfields

@addtogroup ctfirenumfield
@{
*/

/**
@brief  Returns the @intfield, potentially creating it, wrapped by the
	@enumfield \p enum_field.

This function creates the @intfield to return if it does not currently
exist.

@param[in] enum_field	Enumeration field of which to get the wrapped
			integer field.
@returns		Integer field wrapped by \p enum_field, or
			\c NULL on error.

@prenotnull{enum_field}
@preisenumfield{enum_field}
@postrefcountsame{enum_field}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
		struct bt_ctf_field *enum_field);

/**
@brief	Returns a @enumftiter on all the mappings of the field type of
	\p enum_field which contain the current integral value of the
	@enumfield \p enum_field in their range.

This function is the equivalent of using
bt_ctf_field_type_enumeration_find_mappings_by_unsigned_value() or
bt_ctf_field_type_enumeration_find_mappings_by_signed_value() with the
current integral value of \p enum_field.

@param[in] enum_field	Enumeration field of which to get the mappings
			containing the current integral value of \p
			enum_field in their range.
@returns                @enumftiter on the set of mappings of the field
			type of \p enum_field which contain the current
			integral value of \p enum_field in their range,
			or \c NULL if no mappings were found or on
			error.

@prenotnull{enum_field}
@preisenumfield{enum_field}
@pre The wrapped integer field of \p enum_field contains an integral
	value.
@postrefcountsame{enum_field}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned @enumftiter can iterate
	on at least one mapping.
*/
extern struct bt_ctf_field_type_enumeration_mapping_iterator *
bt_ctf_field_enumeration_get_mappings(struct bt_ctf_field *enum_field);

/** @} */

/**
@defgroup ctfirstringfield CTF IR string field
@ingroup ctfirfields
@brief CTF IR string field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>string field</em></strong> is a @field which holds
a string value, and which is described by a @stringft.

Use bt_ctf_field_string_set_value() to set the current string value
of a string field object. You can also use bt_ctf_field_string_append()
and bt_ctf_field_string_append_len() to append a string to the current
value of a string field.

After you create a string field with bt_ctf_field_create(), you
\em must set a string value with
bt_ctf_field_string_set_value(), bt_ctf_field_string_append(), or
bt_ctf_field_string_append_len() before you can get the
field's value with bt_ctf_field_string_get_value().

@sa ctfirstringfieldtype
@sa ctfirfields

@addtogroup ctfirstringfield
@{
*/

/**
@brief	Returns the string value of the @stringfield \p string_field.

On success, \p string_field remains the sole owner of the returned
value.

@param[in] string_field	String field field of which to get the
			string value.
@returns                String value, or \c NULL on error.

@prenotnull{string_field}
@prenotnull{value}
@preisstringfield{string_field}
@pre \p string_field contains a string value previously
	set with bt_ctf_field_string_set_value(),
	bt_ctf_field_string_append(), or
	bt_ctf_field_string_append_len().
@postrefcountsame{string_field}

@sa bt_ctf_field_string_set_value(): Sets the string value of a given
	string field.
*/
extern const char *bt_ctf_field_string_get_value(
		struct bt_ctf_field *string_field);

/**
@brief	Sets the string value of the @stringfield \p string_field to
	\p value.

@param[in] string_field	String field of which to set
			the string value.
@param[in] value	New string value of \p string_field (copied
			on success).
@returns		0 on success, or a negative value on error.

@prenotnull{string_field}
@prenotnull{value}
@preisstringfield{string_field}
@prehot{string_field}
@postrefcountsame{string_field}

@sa bt_ctf_field_string_get_value(): Returns the string value of a
	given string field.
*/
extern int bt_ctf_field_string_set_value(struct bt_ctf_field *string_field,
		const char *value);

/**
@brief	Appends the string \p value to the current string value of
	the @stringfield \p string_field.

This function is the equivalent of:

@code
bt_ctf_field_string_append_len(string_field, value, strlen(value));
@endcode

@param[in] string_field	String field of which to append \p value to
			its current value.
@param[in] value	String to append to the current string value
			of \p string_field (copied on success).
@returns		0 on success, or a negative value on error.

@prenotnull{string_field}
@prenotnull{value}
@preisstringfield{string_field}
@prehot{string_field}
@postrefcountsame{string_field}

@sa bt_ctf_field_string_set_value(): Sets the string value of a given
	string field.
*/
extern int bt_ctf_field_string_append(struct bt_ctf_field *string_field,
		const char *value);

/**
@brief	Appends the first \p length characters of \p value to the
	current string value of the @stringfield \p string_field.

If \p string_field has no current string value, this function first
sets an empty string as the string value of \p string_field and then
appends the first \p length characters of \p value.

@param[in] string_field	String field of which to append the first
			\p length characters of \p value to
			its current value.
@param[in] value	String containing the characters to append to
			the current string value of \p string_field
			(copied on success).
@param[in] length	Number of characters of \p value to append to
			the current string value of \p string_field.
@returns		0 on success, or a negative value on error.

@prenotnull{string_field}
@prenotnull{value}
@preisstringfield{string_field}
@prehot{string_field}
@postrefcountsame{string_field}

@sa bt_ctf_field_string_set_value(): Sets the string value of a given
	string field.
*/
extern int bt_ctf_field_string_append_len(
		struct bt_ctf_field *string_field, const char *value,
		unsigned int length);

/** @} */

/**
@defgroup ctfirstructfield CTF IR structure field
@ingroup ctfirfields
@brief CTF IR structure field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>structure field</em></strong> is a @field which
contains an ordered list of zero or more named @fields which can be
different @fts, and which is described by a @structft.

To set the value of a specific field of a structure field, you need to
first get the field with bt_ctf_field_structure_get_field_by_name() or
bt_ctf_field_structure_get_field_by_index(). If you already have a
field object, you can assign it to a specific name within a structure
field with bt_ctf_field_structure_set_field_by_name().

@sa ctfirstructfieldtype
@sa ctfirfields

@addtogroup ctfirstructfield
@{
*/

/**
@brief  Returns the @field named \p name, potentially creating it,
	in the @structfield \p struct_field.

This function creates the @field to return if it does not currently
exist.

@param[in] struct_field	Structure field of which to get the field
			named \p name.
@param[in] name		Name of the field to get from \p struct_field.
@returns		Field named \p name in \p struct_field, or
			\c NULL on error.

@prenotnull{struct_field}
@prenotnull{name}
@preisstructfield{struct_field}
@postrefcountsame{struct_field}
@postsuccessrefcountretinc

@sa bt_ctf_field_structure_get_field_by_index(): Returns the field of a
	given structure field by index.
@sa bt_ctf_field_structure_set_field_by_name(): Sets the field of a
	given structure field by name.
*/
extern struct bt_ctf_field *bt_ctf_field_structure_get_field_by_name(
		struct bt_ctf_field *struct_field, const char *name);

/* Pre-2.0 CTF writer compatibility */
#define bt_ctf_field_structure_get_field bt_ctf_field_structure_get_field_by_name

/**
@brief  Returns the @field at index \p index in the @structfield
	\p struct_field.

@param[in] struct_field	Structure field of which to get the field
			at index \p index.
@param[in] index	Index of the field to get in \p struct_field.
@returns		Field at index \p index in \p struct_field, or
			\c NULL on error.

@prenotnull{struct_field}
@preisstructfield{struct_field}
@pre \p index is lesser than the number of fields contained in the
	parent field type of \p struct_field (see
	bt_ctf_field_type_structure_get_field_count()).
@postrefcountsame{struct_field}
@postsuccessrefcountretinc

@sa bt_ctf_field_structure_get_field_by_name(): Returns the field of a
	given structure field by name.
@sa bt_ctf_field_structure_set_field_by_name(): Sets the field of a
	given structure field by name.
*/
extern struct bt_ctf_field *bt_ctf_field_structure_get_field_by_index(
		struct bt_ctf_field *struct_field, uint64_t index);

/**
@brief	Sets the field of the @structfield \p struct_field named \p name
	to the @field \p field.

If \p struct_field already contains a field named \p name, then its
reference count is decremented, and \p field replaces it.

The field type of \p field, as returned by bt_ctf_field_get_type(),
\em must be equivalent to the field type returned by
bt_ctf_field_type_structure_get_field_type_by_name() with the field
type of \p struct_field and the same name, \p name.

bt_ctf_trace_get_packet_header_type() for the parent trace class of
\p packet.

@param[in] struct_field	Structure field of which to set the field
			named \p name.
@param[in] name		Name of the field to set in \p struct_field.
@param[in] field	Field named \p name to set in \p struct_field.
@returns		0 on success, or -1 on error.

@prenotnull{struct_field}
@prenotnull{name}
@prenotnull{field}
@prehot{struct_field}
@preisstructfield{struct_field}
@pre \p field has a field type equivalent to the field type returned by
	bt_ctf_field_type_structure_get_field_type_by_name() for the
	field type of \p struct_field with the name \p name.
@postrefcountsame{struct_field}
@post <strong>On success, if there's an existing field in
	\p struct_field named \p name</strong>, its reference count is
	decremented.
@postsuccessrefcountinc{field}

@sa bt_ctf_field_structure_get_field_by_index(): Returns the field of a
	given structure field by index.
@sa bt_ctf_field_structure_get_field_by_name(): Returns the field of a
	given structure field by name.
*/
extern int bt_ctf_field_structure_set_field_by_name(
		struct bt_ctf_field *struct_field,
		const char *name, struct bt_ctf_field *field);

/** @} */

/**
@defgroup ctfirarrayfield CTF IR array field
@ingroup ctfirfields
@brief CTF IR array field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>array field</em></strong> is a @field which
contains an ordered list of zero or more @fields sharing the same @ft,
and which is described by a @arrayft.

To set the value of a specific field of an array field, you need to
first get the field with bt_ctf_field_array_get_field().

@sa ctfirarrayfieldtype
@sa ctfirfields

@addtogroup ctfirarrayfield
@{
*/

/**
@brief  Returns the @field at index \p index, potentially creating it,
	in the @arrayfield \p array_field.

This function creates the @field to return if it does not currently
exist.

@param[in] array_field	Array field of which to get the field
			at index \p index.
@param[in] index	Index of the field to get in \p array_field.
@returns		Field at index \p index in \p array_field, or
			\c NULL on error.

@prenotnull{array_field}
@preisarrayfield{array_field}
@pre \p index is lesser than bt_ctf_field_type_array_get_length() called
	on the field type of \p array_field.
@postrefcountsame{array_field}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field *bt_ctf_field_array_get_field(
		struct bt_ctf_field *array_field, uint64_t index);

/** @} */

/**
@defgroup ctfirseqfield CTF IR sequence field
@ingroup ctfirfields
@brief CTF IR sequence field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>sequence field</em></strong> is a @field which
contains an ordered list of zero or more @fields sharing the same @ft,
and which is described by a @seqft.

Before you can get a specific field of a sequence field with
bt_ctf_field_sequence_get_field(), you need to set its current length
@intfield with bt_ctf_field_sequence_set_length(). The integral value of
the length field of a sequence field indicates the number of fields
it contains.

@sa ctfirseqfieldtype
@sa ctfirfields

@addtogroup ctfirseqfield
@{
*/

/**
@brief  Returns the @field at index \p index, potentially creating it,
	in the @seqfield \p sequence_field.

This function creates the @field to return if it does not currently
exist.

@param[in] sequence_field	Sequence field of which to get the field
				at index \p index.
@param[in] index		Index of the field to get in
				\p sequence_field.
@returns			Field at index \p index in
				\p sequence_field, or \c NULL on error.

@prenotnull{sequence_field}
@preisseqfield{sequence_field}
@pre \p sequence_field has a length field previously set with
	bt_ctf_field_sequence_set_length().
@pre \p index is lesser than the current integral value of the current
	length field of \p sequence_field (see
	bt_ctf_field_sequence_get_length()).
@postrefcountsame{sequence_field}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field *bt_ctf_field_sequence_get_field(
		struct bt_ctf_field *sequence_field, uint64_t index);

/**
@brief  Returns the length @intfield of the @seqfield \p sequence_field.

The current integral value of the returned length field indicates the
number of fields contained in \p sequence_field.

@param[in] sequence_field	Sequence field of which to get the
				length field.
@returns			Length field of \p sequence_field, or
				\c NULL on error.

@prenotnull{sequence_field}
@preisseqfield{sequence_field}
@pre \p sequence_field has a length field previously set with
	bt_ctf_field_sequence_set_length().
@postrefcountsame{sequence_field}
@postsuccessrefcountretinc
@post <strong>On success</strong>, the returned field is a @intfield.

@sa bt_ctf_field_sequence_set_length(): Sets the length field of a given
	sequence field.
*/
extern struct bt_ctf_field *bt_ctf_field_sequence_get_length(
		struct bt_ctf_field *sequence_field);

/**
@brief	Sets the length @intfield of the @seqfield \p sequence_field
	to \p length_field.

The current integral value of \p length_field indicates the number of
fields contained in \p sequence_field.

@param[in] sequence_field	Sequence field of which to set the
				length field.
@param[in] length_field		Length field of \p sequence_field.
@returns			0 on success, or a negative value on error.

@prenotnull{sequence_field}
@prenotnull{length_field}
@preisseqfield{sequence_field}
@preisintfield{length_field}
@prehot{sequence_field}
@postrefcountsame{sequence_field}
@postsuccessrefcountinc{length_field}

@sa bt_ctf_field_sequence_get_length(): Returns the length field of a
	given sequence field.
*/
extern int bt_ctf_field_sequence_set_length(struct bt_ctf_field *sequence_field,
		struct bt_ctf_field *length_field);

/** @} */

/**
@defgroup ctfirvarfield CTF IR variant field
@ingroup ctfirfields
@brief CTF IR variant field.

@code
#include <babeltrace/ctf-ir/fields.h>
@endcode

A CTF IR <strong><em>variant field</em></strong> is a @field which
contains a current @field amongst one or more choices, and which is
described by a @varft.

Use bt_ctf_field_variant_get_field() to get the @field selected by
a specific tag @enumfield. Once you call this function, you can call
bt_ctf_field_variant_get_current_field() afterwards to get this last
field again.

@sa ctfirvarfieldtype
@sa ctfirfields

@addtogroup ctfirvarfield
@{
*/

/**
@brief  Returns the @field, potentially creating it, selected by the
	tag @intfield \p tag_field in the @varfield \p variant_field.

This function creates the @field to return if it does not currently
exist.

Once you call this function, you can call
bt_ctf_field_variant_get_current_field() to get the same field again,
and you can call bt_ctf_field_variant_get_tag() to get \p tag_field.

@param[in] variant_field	Variant field of which to get the field
				selected by \p tag_field.
@param[in] tag_field		Tag field.
@returns			Field selected by \p tag_field in
				\p variant_field, or \c NULL on error.

@prenotnull{variant_field}
@prenotnull{tag_field}
@preisvarfield{variant_field}
@preisenumfield{tag_field}
@postrefcountsame{variant_field}
@postsuccessrefcountinc{tag_field}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field *bt_ctf_field_variant_get_field(
		struct bt_ctf_field *variant_field,
		struct bt_ctf_field *tag_field);

/**
@brief  Returns the currently selected @field of the @varfield
	\p variant_field.

@param[in] variant_field	Variant field of which to get the
				currently selected field.
@returns			Currently selected field of
				\p variant_field, or \c NULL if there's
				no selected field or on error.

@prenotnull{variant_field}
@preisvarfield{variant_field}
@pre \p variant_field contains has a current selected field previously
	set with bt_ctf_field_variant_get_field().
@postrefcountsame{variant_field}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field *bt_ctf_field_variant_get_current_field(
		struct bt_ctf_field *variant_field);

/**
@brief  Returns the tag @enumfield of the @varfield \p variant_field.

@param[in] variant_field	Variant field of which to get the
				tag field.
@returns			Tag field of \p variant_field, or
				\c NULL on error.

@prenotnull{variant_field}
@preisvarfield{variant_field}
@pre \p variant_field contains has a current selected field previously
	set with bt_ctf_field_variant_get_field().
@postrefcountsame{variant_field}
@postsuccessrefcountretinc
@post <strong>On success</strong>, the returned field is a @enumfield.
*/
extern struct bt_ctf_field *bt_ctf_field_variant_get_tag(
		struct bt_ctf_field *variant_field);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_FIELDS_H */
