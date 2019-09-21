#ifndef BABELTRACE2_TRACE_IR_FIELD_H
#define BABELTRACE2_TRACE_IR_FIELD_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-tir-field Fields
@ingroup api-tir

@brief
    Containers of trace data.

<strong><em>Fields</em></strong> are containers of trace data: they are
found in \bt_p_ev and \bt_p_pkt.

Fields are instances of \bt_p_fc:

@image html field-class-zoom.png

Borrow the class of a field with bt_field_borrow_class() and
bt_field_borrow_class_const().

Fields are \ref api-tir "trace IR" data objects.

There are many types of fields. They can be divided into two main
categories:

<dl>
  <dt>Scalar</dt>
  <dd>
    Fields which contain a simple value.

    The scalar fields are:

    - \ref api-tir-field-bool "Boolean"
    - \ref api-tir-field-ba "Bit array"
    - \ref api-tir-field-int "Integer" (unsigned and signed)
    - \ref api-tir-field-enum "Enumeration" (unsigned and signed)
    - \ref api-tir-field-real "Real" (single-precision and double-precision)
    - \ref api-tir-field-string "String"
  </dd>

  <dt>Container</dt>
  <dd>
    Fields which contain other fields.

    The container fields are:

    - \ref api-tir-field-array "Array" (static and dynamic)
    - \ref api-tir-field-struct "Structure"
    - \ref api-tir-field-opt "Option"
    - \ref api-tir-field-var "Variant"
  </dd>
</dl>

@image html fc-to-field.png "Fields (green) are instances of field classes (orange)."

You cannot directly create a field: there are no
<code>bt_field_*_create()</code> functions. The \bt_name library
creates fields within an \bt_ev or a \bt_pkt from \bt_p_fc.
Therefore, to fill the payload fields of an \bt_ev, you must first
borrow the event's existing payload \bt_struct_field with
bt_event_borrow_payload_field() and then borrow each field, recursively,
to set their values.

The functions to borrow a field from an event or a packet are:

- bt_packet_borrow_context_field() and
  bt_packet_borrow_context_field_const()
- bt_event_borrow_common_context_field() and
  bt_event_borrow_common_context_field_const()
- bt_event_borrow_specific_context_field() and
  bt_event_borrow_specific_context_field_const()
- bt_event_borrow_payload_field() and
  bt_event_borrow_payload_field_const()

Some fields conceptually inherit other fields, eventually
making an inheritance hierarchy. For example, an \bt_enum_field
\em is an \bt_int_field. Therefore, an enumeration field holds an
integral value, just like an integer field does.

The complete field inheritance hierarchy is:

@image html all-fields.png

This is the same inheritance hierarchy as the \bt_fc inheritance
hierarchy.

In the illustration above:

- A field with a dark background is instantiated from a concrete \bt_fc,
  which you can directly create with a dedicated
  <code>bt_field_class_*_create()</code> function.

- A field with a pale background is an \em abstract field: it's not a
  concrete instance, but it can have properties, therefore there can be
  functions which apply to it.

  For example, bt_field_integer_signed_set_value() applies to any
  \bt_sint_field.

Fields are \ref api-fund-unique-object "unique objects": they belong
to an \bt_ev or to a \bt_pkt.

Some library functions \ref api-fund-freezing "freeze" fields on
success. The documentation of those functions indicate this
postcondition.

All the field types share the same C type, #bt_field.

Get the type enumerator of a field's class with bt_field_get_class_type().
Get whether or not a field class type conceptually \em is a given type
with the inline bt_field_class_type_is() function.

<h1>\anchor api-tir-field-bool Boolean field</h1>

A <strong><em>boolean field</em></strong> is a \bt_bool_fc instance.

A boolean field contains a boolean value (#BT_TRUE or #BT_FALSE).

Set the value of a boolean field with bt_field_bool_set_value().

Get the value of a boolean field with bt_field_bool_get_value().

<h1>\anchor api-tir-field-ba Bit array field</h1>

A <strong><em>bit array field</em></strong> is a \bt_ba_fc instance.

A bit array field contains a fixed-length array of bits. Its length
is \ref api-tir-fc-ba-prop-len "given by its class".

The bit array field API interprets the array as an unsigned integer
value: the least significant bit's index is 0.

For example, to get whether or not bit&nbsp;3 of a bit array field is
set:

@code
uint64_t value = bt_field_bit_array_get_value_as_integer(field);

if (value & (UINT64_C(1) << UINT64_C(3))) {
    // Bit 3 is set
}
@endcode

Set the bits of a bit array field with
bt_field_bit_array_set_value_as_integer().

Get the bits of a bit array field with
bt_field_bit_array_get_value_as_integer().

<h1>\anchor api-tir-field-int Integer fields</h1>

<strong><em>Integer fields</em></strong> are \bt_int_fc instances.

Integer fields contain integral values.

An integer field is an \em abstract field. The concrete integer fields
are:

<dl>
  <dt>Unsigned integer field</dt>
  <dd>
    An \bt_uint_fc instance.

    An unsigned integer field contains an unsigned integral value
    (\c uint64_t).

    Set the value of an unsigned integer field with
    bt_field_integer_unsigned_set_value().

    Get the value of an unsigned integer field with
    bt_field_integer_unsigned_get_value().
  </dd>

  <dt>Signed integer field</dt>
  <dd>
    A \bt_sint_fc instance.

    A signed integer field contains a signed integral value (\c int64_t).

    Set the value of a signed integer field with
    bt_field_integer_signed_set_value().

    Get the value of a signed integer field with
    bt_field_integer_signed_get_value().
  </dd>
</dl>

<h2>\anchor api-tir-field-enum Enumeration fields</h2>

<strong><em>Enumeration fields</em></strong> are \bt_enum_fc instances.

Enumeration fields \em are \bt_p_int_field: they contain integral
values.

An enumeration field is an \em abstract field.
The concrete enumeration fields are:

<dl>
  <dt>Unsigned enumeration field</dt>
  <dd>
    An \bt_uenum_fc instance.

    An unsigned enumeration field contains an unsigned integral value
    (\c uint64_t).

    Set the value of an unsigned enumeration field with
    bt_field_integer_unsigned_set_value().

    Get the value of an unsigned enumeration field with
    bt_field_integer_unsigned_get_value().

    Get all the enumeration field class labels mapped to the value of a
    given unsigned enumeration field with
    bt_field_enumeration_unsigned_get_mapping_labels().
  </dd>

  <dt>Signed enumeration field</dt>
  <dd>
    A \bt_senum_fc instance.

    A signed enumeration field contains a signed integral value
    (\c int64_t).

    Set the value of a signed enumeration field with
    bt_field_integer_signed_set_value().

    Get the value of a signed enumeration field with
    bt_field_integer_signed_get_value().

    Get all the enumeration field class labels mapped to the value of a
    given signed enumeration field with
    bt_field_enumeration_signed_get_mapping_labels().
  </dd>
</dl>

<h1>\anchor api-tir-field-real Real fields</h1>

<strong><em>Real fields</em></strong> are \bt_real_fc instances.

Real fields contain
<a href="https://en.wikipedia.org/wiki/Real_number">real</a>
values (\c float or \c double types).

A real field is an \em abstract field. The concrete real fields are:

<dl>
  <dt>Single-precision real field</dt>
  <dd>
    A single-precision real field class instance.

    A single-precision real field contains a \c float value.

    Set the value of a single-precision real field with
    bt_field_real_single_precision_set_value().

    Get the value of a single-precision real field with
    bt_field_real_single_precision_get_value().
  </dd>

  <dt>Double-precision real field</dt>
  <dd>
    A double-precision real field class instance.

    A double-precision real field contains a \c double value.

    Set the value of a double-precision real field with
    bt_field_real_double_precision_set_value().

    Get the value of a double-precision real field with
    bt_field_real_double_precision_get_value().
  </dd>
</dl>

<h1>\anchor api-tir-field-string String field</h1>

A <strong><em>string field</em></strong> is a \bt_string_fc instance.

A string field contains an UTF-8 string value.

Set the value of a string field with
bt_field_string_set_value().

Get the value of a string field with
bt_field_string_get_value().

Get the length of a string field with
bt_field_string_get_length().

Append a string to a string field's current value with
bt_field_string_append() and bt_field_string_append_with_length().

Clear a string field with bt_field_string_clear().

<h1>\anchor api-tir-field-array Array fields</h1>

<strong><em>Array fields</em></strong> are \bt_array_fc instances.

Array fields contain zero or more fields which have the same class.

An array field is an \em abstract field. The concrete array fields are:

<dl>
  <dt>Static array field</dt>
  <dd>
    A \bt_sarray_fc instance.

    A static array field contains a fixed number of fields. Its length
    is \ref api-tir-fc-sarray-prop-len "given by its class".
  </dd>

  <dt>Dynamic array field class</dt>
  <dd>
    A \bt_darray_fc instance.

    A dynamic array field contains a variable number of fields, that is,
    each instance of the same dynamic array field class can contain a
    different number of elements.

    Set a dynamic array field's length with
    bt_field_array_dynamic_set_length() before you borrow any of its
    fields.
  </dd>
</dl>

Get an array field's length with bt_field_array_get_length().

Borrow a field from an array field at a given index with
bt_field_array_borrow_element_field_by_index() and
bt_field_array_borrow_element_field_by_index_const().

<h1>\anchor api-tir-field-struct Structure field</h1>

A <strong><em>structure field</em></strong> is a \bt_struct_fc instance.

A structure field contains an ordered list of zero or more members. A
structure field member contains a field: it's an instance of a structure
field class member.

Borrow a member's field from a structure field at a given index with
bt_field_structure_borrow_member_field_by_index() and
bt_field_structure_borrow_member_field_by_index_const().

Borrow a member's field from a structure field by name with
bt_field_structure_borrow_member_field_by_name() and
bt_field_structure_borrow_member_field_by_name_const().

<h1>\anchor api-tir-field-opt Option field</h1>

An <strong><em>option field</em></strong> is an \bt_opt_fc instance.

An option field either does or doesn't contain a field.

Set whether or not an option field has a field with
bt_field_option_set_has_field().

Borrow the field from an option field with
bt_field_option_borrow_field() or
bt_field_option_borrow_field_const().

<h1>\anchor api-tir-field-var Variant field</h1>

A <strong><em>variant field</em></strong> is a \bt_var_fc instance.

A variant field has a single selected option amongst one or more
possible options. A variant field option contains a field: it's an
instance of a variant field class option.

Set the current option of a variant field with
bt_field_variant_select_option_by_index().

Get a variant field's selected option's index with
bt_field_variant_get_selected_option_index().

Borrow a variant field's selected option's field with
bt_field_variant_borrow_selected_option_field() and
bt_field_variant_borrow_selected_option_field_const().

Borrow the class of a variant field's selected
option with bt_field_variant_borrow_selected_option_class_const(),
bt_field_variant_with_selector_field_integer_unsigned_borrow_selected_option_class_const(),
and
bt_field_variant_with_selector_field_integer_signed_borrow_selected_option_class_const().
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_field bt_field;

@brief
    Field.

@}
*/

/*!
@name Type query
@{
*/

/*!
@brief
    Returns the type enumerator of the \ref api-tir-fc "class" of the
    field \bt_p{field}.

This function returns

@code
bt_field_class_get_type(bt_field_borrow_class(field))
@endcode

@param[in] field
    Field of which to get the class's type enumerator

@returns
    Type enumerator of the class of \bt_p{field}.

@bt_pre_not_null{field}

@sa bt_field_class_get_type() &mdash;
    Returns the type enumerator of a \bt_fc.
*/
extern bt_field_class_type bt_field_get_class_type(
		const bt_field *field);

/*! @} */

/*!
@name Class access
@{
*/

/*!
@brief
    Borrows the \ref api-tir-fc "class" of the field \bt_p{field}.

@param[in] field
    Field of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{field}.

@bt_pre_not_null{field}

@sa bt_field_borrow_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *bt_field_borrow_class(bt_field *field);

/*!
@brief
    Borrows the \ref api-tir-fc "class" of the field \bt_p{field}
    (\c const version).

See bt_field_borrow_class().
*/
extern const bt_field_class *bt_field_borrow_class_const(
		const bt_field *field);

/*! @} */

/*!
@name Boolean field
@{
*/

/*!
@brief
    Sets the value of the \bt_bool_field \bt_p{field} to
    \bt_p{value}.

@param[in] field
    Boolean field of which to set the value to \bt_p{value}.
@param[in] value
    New value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_bool_field{field}
@bt_pre_hot{field}

@sa bt_field_bool_get_value() &mdash;
    Returns the value of a boolean field.
*/
extern void bt_field_bool_set_value(bt_field *field, bt_bool value);

/*!
@brief
    Returns the value of the \bt_bool_field \bt_p{field}.

@param[in] field
    Boolean field of which to get the value.

@returns
    Value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_bool_field{field}

@sa bt_field_bool_set_value() &mdash;
    Sets the value of a boolean field.
*/
extern bt_bool bt_field_bool_get_value(const bt_field *field);

/*! @} */

/*!
@name Bit array field
@{
*/

/*!
@brief
    Sets the bits of the \bt_ba_field \bt_p{field} to the bits of
    \bt_p{bits}.

The least significant bit's index is 0.

See \bt_c_ba_field to learn more.

@param[in] field
    Bit array field of which to set the bits to \bt_p{bits}.
@param[in] bits
    New bits of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_ba_field{field}
@bt_pre_hot{field}

@sa bt_field_bit_array_get_value_as_integer() &mdash;
    Returns the bits of a bit array field as an integer.
*/
extern void bt_field_bit_array_set_value_as_integer(bt_field *field,
		uint64_t bits);

/*!
@brief
    Returns the bits of the \bt_ba_field \bt_p{field} as an
    unsigned integer.

The least significant bit's index is 0.

See \bt_c_ba_field to learn more.

@param[in] field
    Bit array field of which to get the bits.

@returns
    Bits of \bt_p{field} as an unsigned integer.

@bt_pre_not_null{field}
@bt_pre_is_ba_field{field}

@sa bt_field_bit_array_set_value_as_integer() &mdash;
    Sets the bits of a bit array field from an integer.
*/
extern uint64_t bt_field_bit_array_get_value_as_integer(
		const bt_field *field);

/*! @} */

/*!
@name Integer field
@{
*/

/*!
@brief
    Sets the value of the \bt_uint_field \bt_p{field} to
    \bt_p{value}.

@param[in] field
    Unsigned integer field of which to set the value to \bt_p{value}.
@param[in] value
    New value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_uint_field{field}
@bt_pre_hot{field}
@pre
    \bt_p{value} is within the
    \ref api-tir-fc-int-prop-size "field value range" of the
    class of \bt_p{field}.

@sa bt_field_integer_unsigned_get_value() &mdash;
    Returns the value of an unsigned integer field.
*/
extern void bt_field_integer_unsigned_set_value(bt_field *field,
		uint64_t value);

/*!
@brief
    Returns the value of the \bt_uint_field \bt_p{field}.

@param[in] field
    Unsigned integer field of which to get the value.

@returns
    Value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_uint_field{field}

@sa bt_field_integer_unsigned_set_value() &mdash;
    Sets the value of an unsigned integer field.
*/
extern uint64_t bt_field_integer_unsigned_get_value(
		const bt_field *field);

/*!
@brief
    Sets the value of the \bt_sint_field \bt_p{field} to
    \bt_p{value}.

@param[in] field
    Signed integer field of which to set the value to \bt_p{value}.
@param[in] value
    New value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_sint_field{field}
@bt_pre_hot{field}
@pre
    \bt_p{value} is within the
    \ref api-tir-fc-int-prop-size "field value range" of the
    class of \bt_p{field}.

@sa bt_field_integer_signed_get_value() &mdash;
    Returns the value of an signed integer field.
*/
extern void bt_field_integer_signed_set_value(bt_field *field,
		int64_t value);

/*!
@brief
    Returns the value of the \bt_sint_field \bt_p{field}.

@param[in] field
    Signed integer field of which to get the value.

@returns
    Value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_sint_field{field}

@sa bt_field_integer_signed_set_value() &mdash;
    Sets the value of an signed integer field.
*/
extern int64_t bt_field_integer_signed_get_value(const bt_field *field);

/*! @} */

/*!
@name Enumeration field
@{
*/

/*!
@brief
    Status codes for
    bt_field_enumeration_unsigned_get_mapping_labels() and
    bt_field_enumeration_signed_get_mapping_labels().
*/
typedef enum bt_field_enumeration_get_mapping_labels_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_enumeration_get_mapping_labels_status;

/*!
@brief
    Returns an array of all the labels of the mappings of the
    \ref api-tir-fc-enum "class" of the \bt_uenum_field \bt_p{field}
    of which the \bt_p_uint_rg contain the integral value
    of \bt_p{field}.

This function returns

@code
(bt_field_enumeration_get_mapping_labels_status)
bt_field_class_enumeration_unsigned_get_mapping_labels_for_value(
    bt_field_borrow_class_const(field),
    bt_field_integer_unsigned_get_value(field),
    labels, count)
@endcode

@param[in] field
    Unsigned enumeration field having the class from which to get the
    labels of the mappings of which the ranges contain its
    integral value.
@param[out] labels
    See
    bt_field_class_enumeration_unsigned_get_mapping_labels_for_value().
@param[out] count
    See
    bt_field_class_enumeration_unsigned_get_mapping_labels_for_value().

@retval #BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_OK
    Success.
@retval #BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field}
@bt_pre_is_uenum_field{field}
@bt_pre_not_null{labels}
@bt_pre_not_null{count}
*/
extern bt_field_enumeration_get_mapping_labels_status
bt_field_enumeration_unsigned_get_mapping_labels(const bt_field *field,
		bt_field_class_enumeration_mapping_label_array *labels,
		uint64_t *count);

/*!
@brief
    Returns an array of all the labels of the mappings of the
    \ref api-tir-fc-enum "class" of the \bt_senum_field \bt_p{field}
    of which the \bt_p_sint_rg contain the integral value
    of \bt_p{field}.

This function returns

@code
(bt_field_enumeration_get_mapping_labels_status)
bt_field_class_enumeration_signed_get_mapping_labels_for_value(
    bt_field_borrow_class_const(field),
    bt_field_integer_signed_get_value(field),
    labels, count)
@endcode

@param[in] field
    Signed enumeration field having the class from which to get the
    labels of the mappings of which the ranges contain its
    integral value.
@param[out] labels
    See
    bt_field_class_enumeration_signed_get_mapping_labels_for_value().
@param[out] count
    See
    bt_field_class_enumeration_signed_get_mapping_labels_for_value().

@retval #BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_OK
    Success.
@retval #BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field}
@bt_pre_is_senum_field{field}
@bt_pre_not_null{labels}
@bt_pre_not_null{count}
*/
extern bt_field_enumeration_get_mapping_labels_status
bt_field_enumeration_signed_get_mapping_labels(const bt_field *field,
		bt_field_class_enumeration_mapping_label_array *labels,
		uint64_t *count);

/*! @} */

/*!
@name Real field
@{
*/

/*!
@brief
    Sets the value of the \bt_sreal_field \bt_p{field} to
    \bt_p{value}.

@param[in] field
    Single-precision real field of which to set the value to
    \bt_p{value}.
@param[in] value
    New value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_sreal_field{field}
@bt_pre_hot{field}

@sa bt_field_real_single_precision_get_value() &mdash;
    Returns the value of a single-precision real field.
*/
extern void bt_field_real_single_precision_set_value(bt_field *field,
		float value);

/*!
@brief
    Returns the value of the \bt_sreal_field \bt_p{field}.

@param[in] field
    Single-precision real field of which to get the value.

@returns
    Value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_sreal_field{field}

@sa bt_field_real_single_precision_set_value() &mdash;
    Sets the value of a single-precision real field.
*/
extern float bt_field_real_single_precision_get_value(const bt_field *field);

/*!
@brief
    Sets the value of the \bt_dreal_field \bt_p{field} to
    \bt_p{value}.

@param[in] field
    Double-precision real field of which to set the value to
    \bt_p{value}.
@param[in] value
    New value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_dreal_field{field}
@bt_pre_hot{field}

@sa bt_field_real_double_precision_get_value() &mdash;
    Returns the value of a double-precision real field.
*/
extern void bt_field_real_double_precision_set_value(bt_field *field,
		double value);

/*!
@brief
    Returns the value of the \bt_dreal_field \bt_p{field}.

@param[in] field
    Double-precision real field of which to get the value.

@returns
    Value of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_dreal_field{field}

@sa bt_field_real_double_precision_set_value() &mdash;
    Sets the value of a double-precision real field.
*/
extern double bt_field_real_double_precision_get_value(const bt_field *field);

/*! @} */

/*!
@name String field
@{
*/

/*!
@brief
    Status codes for bt_field_string_set_value().
*/
typedef enum bt_field_string_set_value_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_STRING_SET_VALUE_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_STRING_SET_VALUE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_string_set_value_status;

/*!
@brief
    Sets the value of the \bt_string_field \bt_p{field} to
    a copy of \bt_p{value}.

@param[in] field
    String field of which to set the value to \bt_p{value}.
@param[in] value
    New value of \bt_p{field} (copied).

@retval #BT_FIELD_STRING_SET_VALUE_STATUS_OK
    Success.
@retval #BT_FIELD_STRING_SET_VALUE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field}
@bt_pre_is_string_field{field}
@bt_pre_hot{field}
@bt_pre_not_null{value}

@sa bt_field_string_get_value() &mdash;
    Returns the value of a string field.
@sa bt_field_string_append() &mdash;
    Appends a string to a string field.
@sa bt_field_string_clear() &mdash;
    Clears a string field.
*/
extern bt_field_string_set_value_status bt_field_string_set_value(
		bt_field *field, const char *value);

/*!
@brief
    Returns the length of the \bt_string_field \bt_p{field}.

@param[in] field
    String field of which to get the length.

@returns
    Length of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_string_field{field}
*/
extern uint64_t bt_field_string_get_length(const bt_field *field);

/*!
@brief
    Returns the value of the \bt_string_field \bt_p{field}.

@param[in] field
    String field of which to get the value.

@returns
    @parblock
    Value of \bt_p{field}.

    The returned pointer remains valid until \bt_p{field} is modified.
    @endparblock

@bt_pre_not_null{field}
@bt_pre_is_string_field{field}

@sa bt_field_string_set_value() &mdash;
    Sets the value of a string field.
*/
extern const char *bt_field_string_get_value(const bt_field *field);

/*!
@brief
    Status codes for bt_field_string_append() and
    bt_field_string_append_with_length().
*/
typedef enum bt_field_string_append_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_STRING_APPEND_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_STRING_APPEND_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_string_append_status;

/*!
@brief
    Appends a copy of the string \bt_p{value} to the current value of
    the \bt_string_field \bt_p{field}.

@attention
    If you didn't set the value of \bt_p{field} yet, you must call
    bt_field_string_clear() before you call this function.

@param[in] field
    String field to which to append the string \bt_p{value}.
@param[in] value
    String to append to \bt_p{field} (copied).

@retval #BT_FIELD_STRING_APPEND_STATUS_OK
    Success.
@retval #BT_FIELD_STRING_APPEND_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field}
@bt_pre_is_string_field{field}
@bt_pre_hot{field}
@bt_pre_not_null{value}

@sa bt_field_string_append_with_length() &mdash;
    Appends a string with a given length to a string field.
@sa bt_field_string_set_value() &mdash;
    Sets the value of a string field.
*/
extern bt_field_string_append_status bt_field_string_append(
		bt_field *field, const char *value);

/*!
@brief
    Appends a copy of the first \bt_p{length} bytes of the string
    \bt_p{value} to the current value of the \bt_string_field
    \bt_p{field}.

@attention
    If you didn't set the value of \bt_p{field} yet, you must call
    bt_field_string_clear() before you call this function.

@param[in] field
    String field to which to append the first \bt_p{length} bytes of
    the string \bt_p{value}.
@param[in] value
    String of which to append the first \bt_p{length} bytes to
    \bt_p{field} (copied).
@param[in] length
    Number of bytes of \bt_p{value} to append to \bt_p{field}.

@retval #BT_FIELD_STRING_APPEND_STATUS_OK
    Success.
@retval #BT_FIELD_STRING_APPEND_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field}
@bt_pre_is_string_field{field}
@bt_pre_hot{field}
@bt_pre_not_null{value}

@sa bt_field_string_append() &mdash;
    Appends a string to a string field.
@sa bt_field_string_set_value() &mdash;
    Sets the value of a string field.
*/
extern bt_field_string_append_status bt_field_string_append_with_length(
		bt_field *field, const char *value, uint64_t length);

/*!
@brief
    Clears the \bt_string_field \bt_p{field}, making its value an empty
    string.

@param[in] field
    String field to clear.

@bt_pre_not_null{field}
@bt_pre_is_string_field{field}
@bt_pre_hot{field}

@sa bt_field_string_set_value() &mdash;
    Sets the value of a string field.
*/
extern void bt_field_string_clear(bt_field *field);

/*! @} */

/*!
@name Array field
@{
*/

/*!
@brief
    Returns the length of the \bt_array_field \bt_p{field}.

@param[in] field
    Array field of which to get the length.

@returns
    Length of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_array_field{field}
*/
extern uint64_t bt_field_array_get_length(const bt_field *field);

/*!
@brief
    Borrows the field at index \bt_p{index} from the \bt_array_field
    \bt_p{field}.

@attention
    If \bt_p{field} is a dynamic array field, it must have a length
    (call bt_field_array_dynamic_set_length()) before you call this
    function.

@param[in] field
    Array field from which to borrow the field at index \bt_p{index}.
@param[in] index
    Index of the field to borrow from \bt_p{field}.

@returns
    @parblock
    \em Borrowed reference of the field of \bt_p{field} at index
    \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field} exists.
    @endparblock

@bt_pre_not_null{field}
@bt_pre_is_array_field{field}
@pre
    \bt_p{index} is less than the length of \bt_p{field} (as returned by
    bt_field_array_get_length()).

@sa bt_field_array_borrow_element_field_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_field *bt_field_array_borrow_element_field_by_index(
		bt_field *field, uint64_t index);

/*!
@brief
    Borrows the field at index \bt_p{index} from the \bt_array_field
    \bt_p{field} (\c const version).

See bt_field_array_borrow_element_field_by_index().
*/
extern const bt_field *
bt_field_array_borrow_element_field_by_index_const(
		const bt_field *field, uint64_t index);

/*!
@brief
    Status codes for bt_field_array_dynamic_set_length().
*/
typedef enum bt_field_array_dynamic_set_length_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_array_dynamic_set_length_status;

/*!
@brief
    Sets the length of the \bt_darray_field \bt_p{field}.

@param[in] field
    Dynamic array field of which to set the length.
@param[in] length
    New length of \bt_p{field}.

@retval #BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_OK
    Success.
@retval #BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field}
@bt_pre_is_darray_field{field}
@bt_pre_hot{field}
*/
extern bt_field_array_dynamic_set_length_status
bt_field_array_dynamic_set_length(bt_field *field, uint64_t length);

/*! @} */

/*!
@name Structure field
@{
*/

/*!
@brief
    Borrows the field of the member at index \bt_p{index} from the
    \bt_struct_field \bt_p{field}.

@param[in] field
    Structure field from which to borrow the field of the member at
    index \bt_p{index}.
@param[in] index
    Index of the member containing the field to borrow from
    \bt_p{field}.

@returns
    @parblock
    \em Borrowed reference of the field of the member of \bt_p{field} at
    index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field} exists.
    @endparblock

@bt_pre_not_null{field}
@bt_pre_is_struct_field{field}
@pre
    \bt_p{index} is less than the number of members in the
    \ref api-tir-fc-struct "class" of \bt_p{field} (as
    returned by bt_field_class_structure_get_member_count()).

@sa bt_field_structure_borrow_member_field_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_field *bt_field_structure_borrow_member_field_by_index(
		bt_field *field, uint64_t index);

/*!
@brief
    Borrows the field of the member at index \bt_p{index} from the
    \bt_struct_field \bt_p{field} (\c const version).

See bt_field_structure_borrow_member_field_by_index().
*/
extern const bt_field *
bt_field_structure_borrow_member_field_by_index_const(
		const bt_field *field, uint64_t index);

/*!
@brief
    Borrows the field of the member having the name \bt_p{name} from the
    \bt_struct_field \bt_p{field}.

If there's no member having the name \bt_p{name} in the
\ref api-tir-fc-struct "class" of \bt_p{field}, this function
returns \c NULL.

@param[in] field
    Structure field from which to borrow the field of the member having
    the name \bt_p{name}.
@param[in] name
    Name of the member containing the field to borrow from \bt_p{field}.

@returns
    @parblock
    \em Borrowed reference of the field of the member of \bt_p{field}
    having the name \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{field} exists.
    @endparblock

@bt_pre_not_null{field}
@bt_pre_is_struct_field{field}
@bt_pre_not_null{name}

@sa bt_field_structure_borrow_member_field_by_name_const() &mdash;
    \c const version of this function.
*/
extern bt_field *bt_field_structure_borrow_member_field_by_name(
		bt_field *field, const char *name);

/*!
@brief
    Borrows the field of the member having the name \bt_p{name} from the
    \bt_struct_field \bt_p{field} (\c const version).

See bt_field_structure_borrow_member_field_by_name().
*/
extern const bt_field *
bt_field_structure_borrow_member_field_by_name_const(
		const bt_field *field, const char *name);

/*! @} */

/*!
@name Option field
@{
*/

/*!
@brief
    Sets whether or not the \bt_opt_field \bt_p{field}
    has a field.

@param[in] field
    Option field of which to set whether or not it has a field.
@param[in] has_field
    #BT_TRUE to make \bt_p{field} have a field.

@bt_pre_not_null{field}
@bt_pre_is_opt_field{field}
*/
extern void bt_field_option_set_has_field(bt_field *field, bt_bool has_field);

/*!
@brief
    Borrows the field of the \bt_opt_field \bt_p{field}.

@attention
    You must call bt_field_option_set_has_field() before you call
    this function.

If \bt_p{field} has no field, this function returns \c NULL.

@param[in] field
    Option field from which to borrow the field.

@returns
    @parblock
    \em Borrowed reference of the field of \bt_p{field},
    or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{field} exists.
    @endparblock

@bt_pre_not_null{field}
@bt_pre_is_opt_field{field}

@sa bt_field_option_set_has_field() &mdash;
    Sets whether or not an option field has a field.
@sa bt_field_option_borrow_field_const() &mdash;
    \c const version of this function.
*/
extern bt_field *bt_field_option_borrow_field(bt_field *field);

/*!
@brief
    Borrows the field of the \bt_opt_field \bt_p{field}
    (\c const version).

See bt_field_option_borrow_field().
*/
extern const bt_field *
bt_field_option_borrow_field_const(const bt_field *field);

/*! @} */

/*!
@name Variant field
@{
*/

/*!
@brief
    Status code for bt_field_variant_select_option_by_index().
*/
typedef enum bt_field_variant_select_option_by_index_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_VARIANT_SELECT_OPTION_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_field_variant_select_option_by_index_status;

/*!
@brief
    Sets the selected option of the \bt_var_field \bt_p{field}
    to the option at index \bt_p{index}.

@param[in] field
    Variant field of which to set the selected option.
@param[in] index
    Index of the option to set as the selected option of \bt_p{field}.

@retval #BT_FIELD_VARIANT_SELECT_OPTION_STATUS_OK
    Success.

@bt_pre_not_null{field}
@bt_pre_is_var_field{field}
@pre
    \bt_p{index} is less than the number of options in the
    \ref api-tir-fc-var "class" of \bt_p{field} (as
    returned by bt_field_class_variant_get_option_count()).
*/
extern bt_field_variant_select_option_by_index_status
bt_field_variant_select_option_by_index(
		bt_field *field, uint64_t index);

/*!
@brief
    Borrows the field of the selected option of the \bt_var_field
    \bt_p{field}.

@attention
    You must call bt_field_variant_select_option_by_index() before
    you call this function.

@param[in] field
    Variant field from which to borrow the selected option's field.

@returns
    @parblock
    \em Borrowed reference of the field of the selected option of
    \bt_p{field}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{field} exists.
    @endparblock

@bt_pre_not_null{field}
@bt_pre_is_var_field{field}

@sa bt_field_variant_select_option_by_index() &mdash;
    Sets a variant field's selected option.
@sa bt_field_variant_get_selected_option_index() &mdash;
    Returns the index of a variant field's selected option.
@sa bt_field_variant_borrow_selected_option_field_const() &mdash;
    \c const version of this function.
*/
extern bt_field *bt_field_variant_borrow_selected_option_field(
		bt_field *field);

/*!
@brief
    Borrows the field of the selected option of the \bt_var_field
    \bt_p{field} (\c const version).

See bt_field_variant_borrow_selected_option_field().
*/
extern const bt_field *
bt_field_variant_borrow_selected_option_field_const(
		const bt_field *field);

/*!
@brief
    Returns the index of the selected option of the \bt_var_field
    \bt_p{field}.

@param[in] field
    Variant field of which to get the index of the selected option.

@returns
    Index of the selected option of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_var_field{field}

@sa bt_field_variant_borrow_selected_option_field_const() &mdash;
    Borrows the field of a variant field's selected option.
*/
extern uint64_t bt_field_variant_get_selected_option_index(
		const bt_field *field);

/*!
@brief
    Borrows the class of the selected option of the \bt_var_field
    \bt_p{field}.

This function returns

@code
bt_field_class_variant_borrow_option_by_index(
    bt_field_variant_get_selected_option_index(field))
@endcode

@param[in] field
    Variant field of which to get the selected option's class.

@returns
    Class of the selected option of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_var_field{field}
*/
extern const bt_field_class_variant_option *
bt_field_variant_borrow_selected_option_class_const(
		const bt_field *field);

/*!
@brief
    Borrows the class of the selected option of the \bt_var_field
    (with an unsigned integer selector field) \bt_p{field}.

This function returns

@code
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
    bt_field_variant_get_selected_option_index(field))
@endcode

@param[in] field
    Variant field of which to get the selected option's class.

@returns
    Class of the selected option of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_var_wuis_field{field}
*/
extern const bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_variant_with_selector_field_integer_unsigned_borrow_selected_option_class_const(
		const bt_field *field);

/*!
@brief
    Borrows the class of the selected option of the \bt_var_field
    (with a signed integer selector field) \bt_p{field}.

This function returns

@code
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
    bt_field_variant_get_selected_option_index(field))
@endcode

@param[in] field
    Variant field of which to get the selected option's class.

@returns
    Class of the selected option of \bt_p{field}.

@bt_pre_not_null{field}
@bt_pre_is_var_wsis_field{field}
*/
extern const bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_variant_with_selector_field_integer_signed_borrow_selected_option_class_const(
		const bt_field *field);

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_FIELD_H */
