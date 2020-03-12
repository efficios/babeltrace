#ifndef BABELTRACE2_TRACE_IR_FIELD_CLASS_H
#define BABELTRACE2_TRACE_IR_FIELD_CLASS_H

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
#include <stddef.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-tir-fc Field classes
@ingroup api-tir

@brief
    Classes of \bt_p_field.

<strong><em>Field classes</em></strong> are the classes of \bt_p_field:

@image html field-class-zoom.png

Field classes are \ref api-tir "trace IR" metadata objects.

There are many types of field classes. They can be divided into two main
categories:

<dl>
  <dt>Scalar</dt>
  <dd>
    Classes of fields which contain a simple value.

    The scalar field classes are:

    - \ref api-tir-fc-bool "Boolean"
    - \ref api-tir-fc-ba "Bit array"
    - \ref api-tir-fc-int "Integer" (unsigned and signed)
    - \ref api-tir-fc-enum "Enumeration" (unsigned and signed)
    - \ref api-tir-fc-real "Real" (single-precision and double-precision)
    - \ref api-tir-fc-string "String"
  </dd>

  <dt>Container</dt>
  <dd>
    Classes of fields which contain other fields.

    The container field classes are:

    - \ref api-tir-fc-array "Array" (static and dynamic)
    - \ref api-tir-fc-struct "Structure"
    - \ref api-tir-fc-opt "Option"
    - \ref api-tir-fc-var "Variant"
  </dd>
</dl>

@image html fc-to-field.png "Fields (green) are instances of field classes (orange)."

Some field classes conceptually inherit other field classes, eventually
making an inheritance hierarchy. For example, a \bt_sarray_fc
\em is an array field class. Therefore, a static array field class has
any property that an array field class has.

The complete field class inheritance hierarchy is:

@image html all-field-classes.png

In the illustration above:

- You can create any field class with a dark background with
  a dedicated <code>bt_field_class_*_create()</code> function.

- A field class with a pale background is an \em abstract field class:
  you cannot create it, but it has properties, therefore there are
  functions which apply to it.

  For example, bt_field_class_integer_set_preferred_display_base()
  applies to any \bt_int_fc.

Field classes are \ref api-fund-shared-object "shared objects": get a
new reference with bt_field_class_get_ref() and put an existing
reference with bt_field_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" field classes on
success. The documentation of those functions indicate this
postcondition.

All the field class types share the same C type, #bt_field_class.

Get the type enumerator of a field class with bt_field_class_get_type().
Get whether or not a field class type conceptually \em is a given type
with the inline bt_field_class_type_is() function.

The following table shows the available type enumerators and creation
functions for each type of \em concrete (non-abstract) field class:

<table>
  <tr>
    <th>Name
    <th>Type enumerator
    <th>Creation function
  <tr>
    <td><em>\ref api-tir-fc-bool "Boolean"</em>
    <td>#BT_FIELD_CLASS_TYPE_BOOL
    <td>bt_field_class_bool_create()
  <tr>
    <td><em>\ref api-tir-fc-ba "Bit array"</em>
    <td>#BT_FIELD_CLASS_TYPE_BIT_ARRAY
    <td>bt_field_class_bit_array_create()
  <tr>
    <td><em>Unsigned \ref api-tir-fc-int "integer"</em>
    <td>#BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER
    <td>bt_field_class_integer_unsigned_create()
  <tr>
    <td><em>Signed \ref api-tir-fc-int "integer"</em>
    <td>#BT_FIELD_CLASS_TYPE_SIGNED_INTEGER
    <td>bt_field_class_integer_signed_create()
  <tr>
    <td><em>Unsigned \ref api-tir-fc-enum "enumeration"</em>
    <td>#BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION
    <td>bt_field_class_enumeration_unsigned_create()
  <tr>
    <td><em>Signed \ref api-tir-fc-enum "enumeration"</em>
    <td>#BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION
    <td>bt_field_class_enumeration_signed_create()
  <tr>
    <td><em>Single-precision \ref api-tir-fc-real "real"</em>
    <td>#BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL
    <td>bt_field_class_real_single_precision_create()
  <tr>
    <td><em>Double-precision \ref api-tir-fc-real "real"</em>
    <td>#BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL
    <td>bt_field_class_real_double_precision_create()
  <tr>
    <td><em>\ref api-tir-fc-string "String"</em>
    <td>#BT_FIELD_CLASS_TYPE_STRING
    <td>bt_field_class_string_create()
  <tr>
    <td><em>Static \ref api-tir-fc-array "array"</em>
    <td>#BT_FIELD_CLASS_TYPE_STATIC_ARRAY
    <td>bt_field_class_array_static_create()
  <tr>
    <td><em>Dynamic \ref api-tir-fc-array "array" (no length field)</em>
    <td>#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD
    <td>bt_field_class_array_dynamic_create()
  <tr>
    <td><em>Dynamic \ref api-tir-fc-array "array" (with length field)</em>
    <td>#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD
    <td>bt_field_class_array_dynamic_create()
  <tr>
    <td><em>\ref api-tir-fc-struct "Structure"</em>
    <td>#BT_FIELD_CLASS_TYPE_STRUCTURE
    <td>bt_field_class_structure_create()
  <tr>
    <td><em>\ref api-tir-fc-opt "Option" (no selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD
    <td>bt_field_class_option_without_selector_create()
  <tr>
    <td><em>\ref api-tir-fc-opt "Option" (boolean selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD
    <td>bt_field_class_option_with_selector_field_bool_create()
  <tr>
    <td><em>\ref api-tir-fc-opt "Option" (unsigned integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD
    <td>bt_field_class_option_with_selector_field_integer_unsigned_create()
  <tr>
    <td><em>\ref api-tir-fc-opt "Option" (signed integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD
    <td>bt_field_class_option_with_selector_field_integer_signed_create()
  <tr>
    <td><em>\ref api-tir-fc-var "Variant" (no selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD
    <td>bt_field_class_variant_create()
  <tr>
    <td><em>\ref api-tir-fc-var "Variant" (unsigned integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD
    <td>bt_field_class_variant_create()
  <tr>
    <td><em>\ref api-tir-fc-var "Variant" (signed integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD
    <td>bt_field_class_variant_create()
</table>

You need a \bt_trace_cls to create a field class: create one from a
\bt_self_comp with bt_trace_class_create().

Outside the field class API, you can use field classes at four
locations, called <em>scopes</em>, within the trace IR API:

- To set the packet context field class of a \bt_stream_cls with
  bt_stream_class_set_packet_context_field_class().

- To set the event common context field class of a stream class with
  bt_stream_class_set_event_common_context_field_class().

- To set the specific context field class of an \bt_ev_cls with
  bt_event_class_set_specific_context_field_class().

- To set the payload field class of an event class with
  bt_event_class_set_payload_field_class().

When you call one of the four functions above:

- The passed field class must be a \bt_struct_fc.

- You must \em not have already called any of the four functions above
  with the passed field class or with any of its contained field
  classes.

- If any of the field classes recursively contained in the passed
  field class has a \ref api-tir-fc-link "link to another field class",
  it must honor the field class link rules.

Once you have called one of the four functions above, the passed field
class becomes \ref api-fund-freezing "frozen".

<h1>Common field class property</h1>

A field class has the following common property:

<dl>
  <dt>
    \anchor api-tir-fc-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the field class.

    User attributes are custom attributes attached to a field class.

    Use bt_field_class_set_user_attributes(),
    bt_field_class_borrow_user_attributes(), and
    bt_field_class_borrow_user_attributes_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-bool Boolean field class</h1>

@image html fc-bool.png

A <strong><em>boolean field class</em></strong> is the class
of \bt_p_bool_field.

A boolean field contains a boolean value (#BT_TRUE or #BT_FALSE).

Create a boolean field class with bt_field_class_bool_create().

A boolean field class has no specific properties.

<h1>\anchor api-tir-fc-ba Bit array field class</h1>

@image html fc-ba.png

A <strong><em>bit array field class</em></strong> is the class
of \bt_p_ba_field.

A bit array field contains a fixed-length array of bits.

Create a bit array field class with bt_field_class_bit_array_create().

A bit array field class has the following property:

<dl>
  <dt>
    \anchor api-tir-fc-ba-prop-len
    Length
  </dt>
  <dd>
    Number of bits contained in the instances (bit array fields) of
    the bit array field class.

    As of \bt_name_version_min_maj, the maximum length of a bit array
    field is 64.

    You cannot change the length once the bit array field class is
    created.

    Get a bit array field class's length with
    bt_field_class_bit_array_get_length().
  </dd>
</dl>

<h1>\anchor api-tir-fc-int Integer field classes</h1>

@image html fc-int.png

<strong><em>Integer field classes</em></strong> are classes
of \bt_p_int_field.

Integer fields contain integral values.

An integer field class is an \em abstract field class: you cannot create
one. The concrete integer field classes are:

<dl>
  <dt>Unsigned integer field class</dt>
  <dd>
    Its instances (unsigned integer fields) contain an unsigned integral
    value (\c uint64_t).

    Create with bt_field_class_integer_unsigned_create().
  </dd>

  <dt>Signed integer field class</dt>
  <dd>
    Its instances (signed integer fields) contain a signed integral
    value (\c int64_t).

    Create with bt_field_class_integer_signed_create().
  </dd>
</dl>

Integer field classes have the following common properties:

<dl>
  <dt>
    \anchor api-tir-fc-int-prop-size
    Field value range
  </dt>
  <dd>
    Expected range of values that the instances (integer fields)
    of the integer field class can contain.

    For example, if the field value range of an unsigned integer
    field class is [0,&nbsp;16383], then its unsigned integer fields
    can only contain the values from 0 to 16383.

    \bt_cp_sink_comp can benefit from this property to make some space
    optimizations when writing trace data.

    Use bt_field_class_integer_set_field_value_range() and
    bt_field_class_integer_get_field_value_range().
  </dd>

  <dt>
    \anchor api-tir-fc-int-prop-base
    Preferred display base
  </dt>
  <dd>
    Preferred base (2, 8, 10, or 16) to use when displaying the
    instances (integer fields) of the integer field class.

    Use bt_field_class_integer_set_preferred_display_base() and
    bt_field_class_integer_get_preferred_display_base().
  </dd>
</dl>

<h2>\anchor api-tir-fc-enum Enumeration field classes</h2>

@image html fc-enum.png

<strong><em>Enumeration field classes</em></strong> are classes
of \bt_p_enum_field.

Enumeration field classes \em are \bt_p_int_fc: they have the integer
field classes properties.

Enumeration fields \em are integer fields: they contain integral values.

Enumeration field classes associate labels (strings) to specific
ranges of integral values. This association is called an enumeration
field class <em>mapping</em>.

For example, if an enumeration field class maps the label \c SUGAR to
the integer ranges [1,&nbsp;19] and [25,&nbsp;31], then an instance
(enumeration field) of this field class with the value 15 or 28 has the
label <code>SUGAR</code>.

An enumeration field class is an \em abstract field class: you cannot
create one. The concrete enumeration field classes are:

<dl>
  <dt>Unsigned enumeration field class</dt>
  <dd>
    Its instances (unsigned enumeration fields) contain an unsigned
    value (\c uint64_t).

    Create with bt_field_class_enumeration_unsigned_create().
  </dd>

  <dt>Signed enumeration field class</dt>
  <dd>
    Its instances (signed enumeration fields) contain a signed value
    (\c int64_t).

    Create with bt_field_class_enumeration_signed_create().
  </dd>
</dl>

Enumeration field classes have the following common property:

<dl>
  <dt>
    \anchor api-tir-fc-enum-prop-mappings
    Mappings
  </dt>
  <dd>
    Set of mappings of the enumeration field class.

    An enumeration field class mapping is a label (string) and an
    \bt_int_rs.

    The integer ranges of a given mapping or of multiple mappings of
    the same enumeration field class can overlap. For example,
    an enumeration field class can have those two mappings:

    - <code>CALORIES</code>: [1,&nbsp;11], [15,&nbsp;37]
    - <code>SODIUM</code>: [7,&nbsp;13]

    In that case, the values 2 and 30 correpond to the label
    <code>CALORIES</code>, the value 12 to the label
    <code>SODIUM</code>, and the value 10 to the labels
    \c CALORIES \em and <code>SODIUM</code>.

    Two mappings of the same enumeration field class cannot have the
    same label.

    Add a mapping to an enumeration field class with
    bt_field_class_enumeration_unsigned_add_mapping() or
    bt_field_class_enumeration_signed_add_mapping().

    Get the number of mappings in an enumeration field class with
    bt_field_class_enumeration_get_mapping_count().

    Borrow a mapping from an enumeration field class with
    bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(),
    bt_field_class_enumeration_signed_borrow_mapping_by_index_const(),
    bt_field_class_enumeration_unsigned_borrow_mapping_by_label_const(),
    and
    bt_field_class_enumeration_signed_borrow_mapping_by_label_const().

    An enumeration field class mapping is a
    \ref api-fund-unique-object "unique object": it
    belongs to the enumeration field class which contains it.

    There are two enumeration field class mapping types, depending on
    the field class's type:
    #bt_field_class_enumeration_unsigned_mapping and
    #bt_field_class_enumeration_signed_mapping.

    There is also the #bt_field_class_enumeration_mapping type for
    common properties and operations (for example,
    bt_field_class_enumeration_mapping_get_label()).
    \ref api-fund-c-typing "Upcast" a specific enumeration field class
    mapping to the #bt_field_class_enumeration_mapping type with
    bt_field_class_enumeration_unsigned_mapping_as_mapping_const() or
    bt_field_class_enumeration_signed_mapping_as_mapping_const().

    Get all the enumeration field class labels mapped to a given integer
    value with
    bt_field_class_enumeration_unsigned_get_mapping_labels_for_value()
    and
    bt_field_class_enumeration_signed_get_mapping_labels_for_value().
  </dd>
</dl>

<h1>\anchor api-tir-fc-real Real field classes</h1>

@image html fc-real.png

<strong><em>Real field classes</em></strong> are classes
of \bt_p_real_field.

Real fields contain
<a href="https://en.wikipedia.org/wiki/Real_number">real</a>
values (\c float or \c double types).

A real field class is an \em abstract field class: you cannot create
one. The concrete real field classes are:

<dl>
  <dt>Single-precision real field class</dt>
  <dd>
    Its instances (single-precision real fields) contain a \c float
    value.

    Create with bt_field_class_real_single_precision_create().
  </dd>

  <dt>Double-precision real field class</dt>
  <dd>
    Its instances (double-precision real fields) contain a \c double
    value.

    Create with bt_field_class_real_double_precision_create().
  </dd>
</dl>

Real field classes have no specific properties.

<h1>\anchor api-tir-fc-string String field class</h1>

@image html fc-string.png

A <strong><em>string field class</em></strong> is the class
of \bt_p_string_field.

A string field contains an UTF-8 string value.

Create a string field class with bt_field_class_string_create().

A string field class has no specific properties.

<h1>\anchor api-tir-fc-array Array field classes</h1>

@image html fc-array.png

<strong><em>Array field classes</em></strong> are classes
of \bt_p_array_field.

Array fields contain zero or more fields which have the same class.

An array field class is an \em abstract field class: you cannot create
one. The concrete array field classes are:

<dl>
  <dt>Static array field class</dt>
  <dd>
    Its instances (static array fields) contain a fixed number of
    fields.

    Create with bt_field_class_array_static_create().

    A static array field class has the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-sarray-prop-len
        Length
      </dt>
      <dd>
        Number of fields contained in the instances (static array
        fields) of the static array field class.

        You cannot change the length once the static array field class
        is created.

        Get a static array field class's length with
        bt_field_class_array_static_get_length().
      </dd>
    </dl>
  </dd>

  <dt>Dynamic array field class</dt>
  <dd>
    Its instances (dynamic array fields) contain a variable number array
    of fields.

    There are two types of dynamic array field classes: without or
    with a length field. See
    \ref api-tir-fc-link "Field classes with links to other field classes"
    to learn more.

    @image html darray-link.png "Dynamic array field class with a length field."

    Create with bt_field_class_array_dynamic_create().

    A dynamic array field class with a length field has the
    specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-darray-prop-len-fp
        Length field path
      </dt>
      <dd>
        Field path of the linked length field class.

        Get a dynamic array field class's length field path with
        bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const().
      </dd>
    </dl>
  </dd>
</dl>

Array field classes have the following common property:

<dl>
  <dt>
    \anchor api-tir-fc-array-prop-elem-fc
    Element field class
  </dt>
  <dd>
    Class of the fields contained in the instances (array fields) of the
    array field class.

    You cannot change the element field class once the array field class
    is created.

    Borrow an array field class's element field class with
    Use bt_field_class_array_borrow_element_field_class() and
    bt_field_class_array_borrow_element_field_class_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-struct Structure field class</h1>

@image html fc-struct.png

A <strong><em>structure field class</em></strong> is the class
of a \bt_struct_field.

A structure field contains an ordered list of zero or more members. Each
structure field member is the instance of a structure field class
member. A structure field class member has a name, a field class,
and user attributes.

Create a structure field class with bt_field_class_structure_create().

A structure field class has the following specific property:

<dl>
  <dt>
    \anchor api-tir-fc-struct-prop-members
    Members
  </dt>
  <dd>
    Ordered list of members (zero or more) of the structure field class.

    Each member has:

    - A name, unique amongst all the member names of the same
      structure field class.
    - A field class.
    - User attributes.

    The instances (structure fields) of a structure field class have
    members which are instances of the corresponding structure field
    class members.

    Append a member to a structure field class with
    bt_field_class_structure_append_member().

    Borrow a member from a structure field class with
    bt_field_class_structure_borrow_member_by_index(),
    bt_field_class_structure_borrow_member_by_name(),
    bt_field_class_structure_borrow_member_by_index_const(), and
    bt_field_class_structure_borrow_member_by_name_const().

    A structure field class member is a
    \ref api-fund-unique-object "unique object": it
    belongs to the structure field class which contains it.

    The type of a structure field class member is
    #bt_field_class_structure_member.

    Get a structure field class member's name with
    bt_field_class_structure_member_get_name().

    Borrow a structure field class member's field class with
    bt_field_class_structure_member_borrow_field_class() and
    bt_field_class_structure_member_borrow_field_class_const().

    Set a structure field class member's user attributes with
    bt_field_class_structure_member_set_user_attributes().

    Borrow a structure field class member's user attributes with
    bt_field_class_structure_member_borrow_user_attributes() and
    bt_field_class_structure_member_borrow_user_attributes_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-opt Option field classes</h1>

@image html fc-opt.png

<strong><em>Option field classes</em></strong> are classes
of \bt_p_opt_field.

An option field either does or doesn't contain a field, called its
optional field.

An option field class is an \em abstract field class: you cannot create
one. An option field class either has a selector field (it's linked to a
selector field class; see
\ref api-tir-fc-link "Field classes with links to other field classes")
or none. Therefore, the concrete option field classes are:

<dl>
  <dt>Option field class without a selector field</dt>
  <dd>
    Create with bt_field_class_option_without_selector_create().

    An option field class without a selector field has no specific
    properties.
  </dd>

  <dt>Option field class with a boolean selector field</dt>
  <dd>
    The linked selector field of an option field class's instance
    (an option field) is a \bt_bool_field.

    Consequently, the option field class's selector field class is
    a \bt_bool_fc.

    @image html opt-link.png "Option field class with a boolean selector field."

    Create with bt_field_class_option_with_selector_field_bool_create().

    An option field class with a boolean selector field has the
    following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-opt-prop-sel-rev
        Selector is reversed?
      </dt>
      <dd>
        Whether or not the linked boolean selector field makes the
        option field class's instance (an option field) contain a field
        when it's #BT_TRUE or when it's #BT_FALSE.

        If this property is #BT_TRUE, then if the linked selector field
        has the value #BT_FALSE, the option field contains a field.

        Use
        bt_field_class_option_with_selector_field_bool_set_selector_is_reversed()
        and
        bt_field_class_option_with_selector_field_bool_selector_is_reversed().
      </dd>
    </dl>
  </dd>

  <dt>Option field class with an unsigned selector field</dt>
  <dd>
    The linked selector field of an option field class's instance
    (an option field) is an \bt_uint_field.

    Consequently, the option field class's selector field class is
    an \bt_uint_fc.

    Create with
    bt_field_class_option_with_selector_field_integer_unsigned_create().

    Pass an \bt_uint_rs on creation to set which values of the selector
    field can make the option field contain a field.

    An option field class with an unsigned integer selector field has
    the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-opt-prop-uint-rs
        Selector's unsigned integer ranges
      </dt>
      <dd>
        If the linked unsigned integer selector field of an option
        field class's instance (an option field) has a value which
        intersects with the selector's unsigned integer ranges, then
        the option field contains a field.

        You cannot change the selector's unsigned integer ranges once
        the option field class is created.

        Borrow the selector's unsigned integer ranges from an
        option field class with an unsigned integer selector field with
        bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const().
      </dd>
    </dl>
  </dd>

  <dt>Option field class with a signed selector field</dt>
  <dd>
    The linked selector field of an option field class's instance
    (an option field) is a \bt_sint_field.

    Consequently, the option field class's selector field class is
    a \bt_sint_fc.

    Create with
    bt_field_class_option_with_selector_field_integer_signed_create().

    Pass a \bt_sint_rs on creation to set which values of the selector
    field can make the option field contain a field.

    An option field class with a signed integer selector field has
    the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-opt-prop-sint-rs
        Selector's signed integer ranges
      </dt>
      <dd>
        If the linked signed integer selector field of an option
        field class's instance (an option field) has a value which
        intersects with the selector's signed integer ranges, then
        the option field contains a field.

        You cannot change the selector's signed integer ranges once
        the option field class is created.

        Borrow the selector's signed integer ranges from an
        option field class with a signed integer selector field with
        bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const().
      </dd>
    </dl>
  </dd>
</dl>

Option field classes with a selector have the following common
property:

<dl>
  <dt>
    \anchor api-tir-fc-opt-prop-sel-fp
    Selector field path
  </dt>
  <dd>
    Field path of the linked selector field class.

    Borrow such an option field class's selector field path with
    bt_field_class_option_with_selector_field_borrow_selector_field_path_const().
  </dd>
</dl>

Option field classes have the following common property:

<dl>
  <dt>
    \anchor api-tir-fc-opt-prop-fc
    Optional field class
  </dt>
  <dd>
    Class of the optional field of an instance (option field) of the
    option field class.

    You cannot change the optional field class once the option field
    class is created.

    Borrow an option field class's optional field class with
    Use bt_field_class_option_borrow_field_class() and
    bt_field_class_option_borrow_field_class_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-var Variant field classes</h1>

@image html fc-var.png

<strong><em>Variant field classes</em></strong> are classes
of \bt_p_var_field.

A variant field contains a field amongst one or more possible fields.

Variant field classes contain one or more options. Each variant field
class option has a name, a field class, user attributes, and integer
ranges, depending on the exact type.

A variant field class is an \em abstract field class: you cannot create
one. A variant field class either has a selector field (it's linked to a
selector field class; see
\ref api-tir-fc-link "Field classes with links to other field classes")
or none. Therefore, the concrete variant field classes are:

<dl>
  <dt>Variant field class without a selector field</dt>
  <dd>
    Create with bt_field_class_variant_create(), passing \c NULL as
    the selector field class.

    Append an option to such a variant field class with
    bt_field_class_variant_without_selector_append_option().

    A variant field class without a selector field has no specific
    properties.
  </dd>

  <dt>Variant field class with an unsigned selector field</dt>
  <dd>
    The linked selector field of a variant field class's instance
    (a variant field) is an \bt_uint_field.

    Consequently, the variant field class's selector field class is
    an \bt_uint_fc.

    @image html var-link.png "Variant field class with an unsigned integer selector field."

    Create with bt_field_class_variant_create(), passing an
    unsigned integer field class as the selector field class.

    Append an option to such a variant field class with
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option().

    Pass an \bt_uint_rs when you append an option to set which values of
    the selector field can make the variant field have a given
    current option.

    Borrow such a variant field class's option with
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const()
    and
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const().
  </dd>

  <dt>Variant field class with a signed selector field</dt>
  <dd>
    The linked selector field of a variant field class's instance
    (a variant field) is a \bt_sint_field.

    Consequently, the variant field class's selector field class is
    a \bt_sint_fc.

    Create with bt_field_class_variant_create(), passing a
    signed integer field class as the selector field class.

    Append an option to such a variant field class with
    bt_field_class_variant_with_selector_field_integer_signed_append_option().

    Pass a \bt_sint_rs when you append an option to set which values of
    the selector field can make the variant field have a given
    current option.

    Borrow such a variant field class's option with
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const()
    and
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const().
  </dd>
</dl>

Variant field classes with a selector have the following common
property:

<dl>
  <dt>
    \anchor api-tir-fc-var-prop-sel-fp
    Selector field path
  </dt>
  <dd>
    Field path of the linked selector field class.

    Borrow such a variant field class's selector field path with
    bt_field_class_variant_with_selector_field_borrow_selector_field_path_const().
  </dd>
</dl>

Variant field classes have the following common property:

<dl>
  <dt>
    \anchor api-tir-fc-var-prop-opts
    Options
  </dt>
  <dd>
    Options of the variant field class.

    Each option has:

    - A name, unique amongst all the option names of the same
      variant field class.
    - A field class.
    - User attributes.

    If the variant field class is linked to a selector field class, then
    each option also has an \bt_int_rs. In that case, the ranges of a
    given option cannot overlap any range of any other option.

    A variant field class must contain at least one option.

    Depending on whether or not the variant field class has a selector
    field class, append an option to a variant field class
    with bt_field_class_variant_without_selector_append_option(),
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option(),
    or
    bt_field_class_variant_with_selector_field_integer_signed_append_option().

    Get the number of options contained in a variant field class
    with bt_field_class_variant_get_option_count().

    A variant field class option is a
    \ref api-fund-unique-object "unique object": it
    belongs to the variant field class which contains it.

    Borrow any variant field class's option with
    bt_field_class_variant_borrow_option_by_index(),
    bt_field_class_variant_borrow_option_by_name(),
    bt_field_class_variant_borrow_option_by_index_const(), and
    bt_field_class_variant_borrow_option_by_name_const().

    Those functions return the common #bt_field_class_variant_option
    type. Get the name of a variant field class option with
    bt_field_class_variant_option_get_name(). Borrow a variant field
    class option's field class with
    bt_field_class_variant_option_borrow_field_class() and
    bt_field_class_variant_option_borrow_field_class_const().

    Borrow the option of a variant field classes with a selector field
    class with
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(),
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const(),
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(), or
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const(),
    depending on the selector field class's type.

    Those functions return the
    #bt_field_class_variant_with_selector_field_integer_unsigned_option or
    #bt_field_class_variant_with_selector_field_integer_signed_option type.
    \ref api-fund-c-typing "Upcast" those types to the
    #bt_field_class_variant_option type with
    bt_field_class_variant_with_selector_field_integer_unsigned_option_as_option_const()
    or
    bt_field_class_variant_with_selector_field_integer_signed_option_as_option_const().

    Borrow the option's ranges from a variant field class with a
    selector field class with
    bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const()
    or
    bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const().

    Set a variant field class option's user attributes with
    bt_field_class_variant_option_set_user_attributes().

    Borrow a variant field class option's user attributes with
    bt_field_class_variant_option_borrow_user_attributes() and
    bt_field_class_variant_option_borrow_user_attributes_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-link Field classes with links to other field classes</h1>

\bt_cp_darray_fc, \bt_p_opt_fc, and \bt_p_var_fc \em can have links to
other, preceding field classes.

When a field class&nbsp;A has a link to another field class&nbsp;B, then
an instance (\bt_field) of A has a link to an instance of B.

This feature exists so that the linked field can contain the value of a
dynamic property of the field. Those properties are:

<dl>
  <dt>\bt_c_darray_field</dt>
  <dd>
    The linked field, a \bt_uint_field, contains the \b length of the
    dynamic array field.
  </dd>

  <dt>\bt_c_opt_field</dt>
  <dd>
    The linked field, either a \bt_bool_field or an \bt_int_field,
    indicates whether or not the option field has a field.
  </dd>

  <dt>\bt_c_var_field</dt>
  <dd>
    The linked field, an \bt_int_field, indicates the variant field's
    current selected field.
  </dd>
</dl>

Having a linked field class is <em>optional</em>: you always set the
field properties with a dedicated function anyway. For example, even if
a dynamic array field is linked to a preceding length field, you must
still set its length with bt_field_array_dynamic_set_length(). In that
case, the value of the length field must match what you pass to
bt_field_array_dynamic_set_length().

The purpose of this feature is to eventually save space when a
\bt_sink_comp writes trace files: if the trace format can convey that
a preceding, existing field represents the length of a dynamic array
field, then the sink component doesn't need to write the dynamic array
field's length twice. This is the case of the
<a href="https://diamon.org/ctf/">Common Trace Format</a>, for
example.

@image html darray-link.png "A dynamic array field class linked to an unsigned integer field class."

To link a field class&nbsp;A to a preceding field class&nbsp;B, pass
field class&nbsp;B when you create field class&nbsp;A. For example, pass
the \bt_uint_fc to bt_field_class_array_dynamic_create() to create a
\bt_darray_fc with a length field.

When you call bt_stream_class_set_packet_context_field_class(),
bt_stream_class_set_event_common_context_field_class(),
bt_event_class_set_specific_context_field_class(), or
bt_event_class_set_payload_field_class() with a field class containing
a field class&nbsp;A with a linked field class&nbsp;B, the path to
B becomes available in A. The functions to borrow this field path are:

- bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const()
- bt_field_class_option_with_selector_field_borrow_selector_field_path_const()
- bt_field_class_variant_with_selector_field_borrow_selector_field_path_const()

A field path indicates how to reach a linked field from a given
root <em>scope</em>. The available scopes are:

<dl>
  <dt>#BT_FIELD_PATH_SCOPE_PACKET_CONTEXT</dt>
  <dd>
    Packet context field.

    See bt_packet_borrow_context_field_const().
  </dd>

  <dt>#BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT</dt>
  <dd>
    Event common context field.

    See bt_event_borrow_common_context_field_const().
  </dd>

  <dt>#BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT</dt>
  <dd>
    Event specific context field.

    See bt_event_borrow_specific_context_field_const().
  </dd>

  <dt>#BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD</dt>
  <dd>
    Event payload field.

    See bt_event_borrow_payload_field_const().
  </dd>
</dl>

The rules regarding field class&nbsp;A vs. field class&nbsp;B (linked
field class) are:

- If A is within a packet context field class, B must also be in the
  same packet context field class.

  See bt_stream_class_set_packet_context_field_class().

- If A is within a event common context field class, B must be in one
  of:

  - The same event common context field class.
  - The packet context field class of the same \bt_stream_cls.

  See bt_stream_class_set_event_common_context_field_class().

- If A is within an event specific context field class, B must be in
  one of:

  - The same event specific context field class.
  - The event common context field class of the same stream class.
  - The packet context field class of the same stream class.

  See bt_event_class_set_specific_context_field_class().

- If A is within an event payload field class, B must be in one of:

  - The same event payload field class.
  - The event specific context field class of the same \bt_ev_cls.
  - The event common context field class of the same stream class.
  - The packet context field class of the same stream class.

  See bt_event_class_set_payload_field_class().

- If both A and B are in the same scope, then:

  - The lowest common ancestor field class of A and B must be a
    \bt_struct_fc.

  - B must precede A.

    Considering that the members of a structure field class are ordered,
    then B must be part of a member that's before the member which
    contains A in their lowest common ancestor structure field class.

  - The path from the lowest common ancestor structure field class of A
    and B to A and to B must only contain structure field classes.

- If A is in a different scope than B, the path from the root scope of B
  to B must only contain structure field classes.
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_field_class bt_field_class;

@brief
    Field class.

@}
*/

/*!
@name Type query
@{
*/

/*!
@brief
    Field class type enumerators.
*/
typedef enum bt_field_class_type {
	/*!
	@brief
	    \bt_c_bool_fc.
	*/
	BT_FIELD_CLASS_TYPE_BOOL						= 1ULL << 0,

	/*!
	@brief
	    \bt_c_ba_fc.
	*/
	BT_FIELD_CLASS_TYPE_BIT_ARRAY						= 1ULL << 1,

	/*!
	@brief
	    \bt_c_int_fc.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_INTEGER						= 1ULL << 2,

	/*!
	@brief
	    \bt_c_uint_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_INTEGER.
	*/
	BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER					= (1ULL << 3) | BT_FIELD_CLASS_TYPE_INTEGER,

	/*!
	@brief
	    \bt_c_sint_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_INTEGER.
	*/
	BT_FIELD_CLASS_TYPE_SIGNED_INTEGER					= (1ULL << 4) | BT_FIELD_CLASS_TYPE_INTEGER,

	/*!
	@brief
	    \bt_c_enum_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_INTEGER.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_ENUMERATION						= 1ULL << 5,

	/*!
	@brief
	    \bt_c_uenum_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_ENUMERATION
	and #BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER.
	*/
	BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION				= BT_FIELD_CLASS_TYPE_ENUMERATION | BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER,

	/*!
	@brief
	    \bt_c_senum_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_ENUMERATION
	and #BT_FIELD_CLASS_TYPE_SIGNED_INTEGER.
	*/
	BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION					= BT_FIELD_CLASS_TYPE_ENUMERATION | BT_FIELD_CLASS_TYPE_SIGNED_INTEGER,

	/*!
	@brief
	    \bt_c_real_fc.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_REAL						= 1ULL << 6,

	/*!
	@brief
	    Single-precision \bt_real_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_REAL.
	*/
	BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL				= (1ULL << 7) | BT_FIELD_CLASS_TYPE_REAL,

	/*!
	@brief
	    Double-precision \bt_real_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_REAL.
	*/
	BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL				= (1ULL << 8) | BT_FIELD_CLASS_TYPE_REAL,

	/*!
	@brief
	    \bt_c_string_fc..
	*/
	BT_FIELD_CLASS_TYPE_STRING						= 1ULL << 9,

	/*!
	@brief
	    \bt_c_struct_fc.
	*/
	BT_FIELD_CLASS_TYPE_STRUCTURE						= 1ULL << 10,

	/*!
	@brief
	    \bt_c_array_fc.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_ARRAY						= 1ULL << 11,

	/*!
	@brief
	    \bt_c_sarray_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_ARRAY.
	*/
	BT_FIELD_CLASS_TYPE_STATIC_ARRAY					= (1ULL << 12) | BT_FIELD_CLASS_TYPE_ARRAY,

	/*!
	@brief
	    \bt_c_darray_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_ARRAY.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY					= (1ULL << 13) | BT_FIELD_CLASS_TYPE_ARRAY,

	/*!
	@brief
	    \bt_c_darray_fc (without a length field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY.
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD			= (1ULL << 14) | BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,

	/*!
	@brief
	    \bt_c_darray_fc (with a length field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY.
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD			= (1ULL << 15) | BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,

	/*!
	@brief
	    \bt_c_opt_fc.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_OPTION						= 1ULL << 16,

	/*!
	@brief
	    \bt_c_opt_fc (without a selector field).
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD			= (1ULL << 17) | BT_FIELD_CLASS_TYPE_OPTION,

	/*!
	@brief
	    \bt_c_opt_fc (with a selector field).

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_OPTION.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD				= (1ULL << 18) | BT_FIELD_CLASS_TYPE_OPTION,

	/*!
	@brief
	    \bt_c_opt_fc (with a boolean selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD			= (1ULL << 19) | BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (with an integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD			= (1ULL << 20) | BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (with an unsigned integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 21) | BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (with a signed integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 22) | BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_var_fc.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_VARIANT						= 1ULL << 23,

	/*!
	@brief
	    \bt_c_var_fc (without a selector field).
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD			= (1ULL << 24) | BT_FIELD_CLASS_TYPE_VARIANT,

	/*!
	@brief
	    \bt_c_var_fc (with a selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD				= (1ULL << 25) | BT_FIELD_CLASS_TYPE_VARIANT,

	/*!
	@brief
	    \bt_c_var_fc (with an integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD			= (1ULL << 26) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (with an unsigned integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD	= (1ULL << 27) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (with a signed integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 28) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD,

	/*
	 * Make sure the enumeration type is a 64-bit integer in case
	 * the project needs field class types in the future.
	 *
	 * This is not part of the API.
	 */
	__BT_FIELD_CLASS_TYPE_BIG_VALUE						= 1ULL << 62,
} bt_field_class_type;

/*!
@brief
    Returns the type enumerator of the field class \bt_p{field_class}.

@param[in] field_class
    Field class of which to get the type enumerator

@returns
    Type enumerator of \bt_p{field_class}.

@bt_pre_not_null{field_class}

@sa bt_field_class_type_is() &mdash;
    Returns whether or not the type of a field class conceptually is a
    given type.
*/
extern bt_field_class_type bt_field_class_get_type(
		const bt_field_class *field_class);

/*!
@brief
    Returns whether or not the field class type \bt_p{type} conceptually
    \em is the field class type \bt_p{other_type}.

For example, an \bt_uint_fc conceptually \em is an integer field class,
so

@code
bt_field_class_type_is(BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER, BT_FIELD_CLASS_TYPE_INTEGER)
@endcode

returns #BT_TRUE.

@param[in] type
    Field class type to check against \bt_p{other_type}.
@param[in] other_type
    Field class type against which to check \bt_p{type}.

@returns
    #BT_TRUE if \bt_p{type} conceptually \em is \bt_p{other_type}.

@sa bt_field_class_get_type() &mdash;
    Returns the type enumerator of a field class.
*/
static inline
bt_bool bt_field_class_type_is(const bt_field_class_type type,
		const bt_field_class_type other_type)
{
	return (type & other_type) == other_type;
}

/*! @} */

/*!
@name Common property
@{
*/

/*!
@brief
    Sets the user attributes of the field class \bt_p{field_class} to
    \bt_p{user_attributes}.

See the \ref api-tir-fc-prop-user-attrs "user attributes" property.

@note
    When you create a field class with one of the
    <code>bt_field_class_*_create()</code> functions, the field class's
    initial user attributes is an empty \bt_map_val. Therefore you can
    borrow it with bt_field_class_borrow_user_attributes() and fill it
    directly instead of setting a new one with this function.

@param[in] field_class
    Field class of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_field_class_borrow_user_attributes() &mdash;
    Borrows the user attributes of a field class.
*/
extern void bt_field_class_set_user_attributes(
		bt_field_class *field_class, const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the field class \bt_p{field_class}.

See the \ref api-tir-fc-prop-user-attrs "user attributes" property.

@note
    When you create a field class with one of the
    <code>bt_field_class_*_create()</code> functions, the field class's
    initial user attributes is an empty \bt_map_val.

@param[in] field_class
    Field class from which to borrow the user attributes.

@returns
    User attributes of \bt_p{field_class} (a \bt_map_val).

@bt_pre_not_null{field_class}

@sa bt_field_class_set_user_attributes() &mdash;
    Sets the user attributes of a field class.
@sa bt_field_class_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_field_class_borrow_user_attributes(
		bt_field_class *field_class);

/*!
@brief
    Borrows the user attributes of the field class \bt_p{field_class}
    (\c const version).

See bt_field_class_borrow_user_attributes().
*/
extern const bt_value *bt_field_class_borrow_user_attributes_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Boolean field class
@{
*/

/*!
@brief
    Creates a \bt_bool_fc from the trace class \bt_p{trace_class}.

On success, the returned boolean field class has the following
property value:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a boolean field class.

@returns
    New boolean field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_bool_create(
		bt_trace_class *trace_class);

/*!
@}
*/

/*!
@name Bit array field class
@{
*/

/*!
@brief
    Creates a \bt_ba_fc with the length \bt_p{length} from the trace
    class \bt_p{trace_class}.

On success, the returned bit array field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-ba-prop-len "Length"
    <td>\bt_p{length}
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a bit array field class.
@param[in] length
    Length (number of bits) of the instances of the bit array field
    class to create.

@returns
    New bit array field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@pre
    0 < \bt_p{length} ≤ 64.
*/
extern bt_field_class *bt_field_class_bit_array_create(
		bt_trace_class *trace_class, uint64_t length);

/*!
@brief
    Returns the length of the \bt_ba_fc \bt_p{field_class}.

See the \ref api-tir-fc-ba-prop-len "length" property.

@param[in] field_class
    Bit array field class of which to get the length.

@returns
    Length of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_ba_fc{field_class}
*/
extern uint64_t bt_field_class_bit_array_get_length(
		const bt_field_class *field_class);

/*!
@}
*/

/*!
@name Integer field class
@{
*/

/*!
@brief
    Sets the field value range of the \bt_int_fc \bt_p{field_class}
    to \bt_p{n}.

See the \ref api-tir-fc-int-prop-size "field value range" property.

@param[in] field_class
    Integer field class of which to set the field value range to
    \bt_p{n}.
@param[in] n
    @parblock
    \em N in:

    <dl>
      <dt>Unsigned integer field class</dt>
      <dd>[0,&nbsp;2<sup><em>N</em></sup>&nbsp;-&nbsp;1]</dd>

      <dt>Signed integer field class</dt>
      <dd>[-2<sup><em>N</em></sup>,&nbsp;2<sup><em>N</em></sup>&nbsp;-&nbsp;1]</dd>
    </dl>
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_int_fc{field_class}
@pre
    \bt_p{n} ⩽ 64.

@sa bt_field_class_integer_get_field_value_range() &mdash;
    Returns the field value range of an integer field class.
*/
extern void bt_field_class_integer_set_field_value_range(
		bt_field_class *field_class, uint64_t n);

/*!
@brief
    Returns the field value range of the \bt_int_fc \bt_p{field_class}.

See the \ref api-tir-fc-int-prop-size "field value range" property.

@param[in] field_class
    Integer field class of which to get the field value range.

@returns
    @parblock
    Field value range of \bt_p{field_class}, that is, \em N in:

    <dl>
      <dt>Unsigned integer field class</dt>
      <dd>[0,&nbsp;2<sup><em>N</em></sup>&nbsp;-&nbsp;1]</dd>

      <dt>Signed integer field class</dt>
      <dd>[-2<sup><em>N</em></sup>,&nbsp;2<sup><em>N</em></sup>&nbsp;-&nbsp;1]</dd>
    </dl>
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_int_fc{field_class}

@sa bt_field_class_integer_set_field_value_range() &mdash;
    Sets the field value range of an integer field class.
*/
extern uint64_t bt_field_class_integer_get_field_value_range(
		const bt_field_class *field_class);

/*!
@brief
    Integer field class preferred display bases.
*/
typedef enum bt_field_class_integer_preferred_display_base {
	/*!
	@brief
	    Binary (2).
	*/
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY		= 2,

	/*!
	@brief
	    Octal (8).
	*/
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL		= 8,

	/*!
	@brief
	    Decimal (10).
	*/
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL		= 10,

	/*!
	@brief
	    Hexadecimal (16).
	*/
	BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL	= 16,
} bt_field_class_integer_preferred_display_base;

/*!
@brief
    Sets the preferred display base of the \bt_int_fc \bt_p{field_class}
    to \bt_p{preferred_display_base}.

See the \ref api-tir-fc-int-prop-base "preferred display base" property.

@param[in] field_class
    Integer field class of which to set the preferred display base to
    \bt_p{preferred_display_base}.
@param[in] preferred_display_base
    New preferred display base of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_int_fc{field_class}

@sa bt_field_class_integer_get_preferred_display_base() &mdash;
    Returns the preferred display base of an integer field class.
*/
extern void bt_field_class_integer_set_preferred_display_base(
		bt_field_class *field_class,
		bt_field_class_integer_preferred_display_base preferred_display_base);

/*!
@brief
    Returns the preferred display base of the \bt_int_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-int-prop-base "preferred display base" property.

@param[in] field_class
    Integer field class of which to get the preferred display base.

@retval #BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY
    2 (binary)
@retval #BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL
    8 (octal)
@retval #BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
    10 (decimal)
@retval #BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL
    16 (hexadecimal)

@bt_pre_not_null{field_class}
@bt_pre_is_int_fc{field_class}

@sa bt_field_class_integer_set_preferred_display_base() &mdash;
    Sets the preferred display base of an integer field class.
*/
extern bt_field_class_integer_preferred_display_base
bt_field_class_integer_get_preferred_display_base(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Unsigned integer field class
@{
*/

/*!
@brief
    Creates an \bt_uint_fc from the trace class \bt_p{trace_class}.

On success, the returned unsigned integer field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-int-prop-size "Field value range"
    <td>[0,&nbsp;2<sup>64</sup>&nbsp;-&nbsp;1]
  <tr>
    <td>\ref api-tir-fc-int-prop-base "Preferred display base"
    <td>#BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create an unsigned integer field class.

@returns
    New unsigned integer field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_integer_unsigned_create(
		bt_trace_class *trace_class);

/*! @} */

/*!
@name Signed integer field class
@{
*/

/*!
@brief
    Creates an \bt_sint_fc from the trace class \bt_p{trace_class}.

On success, the returned signed integer field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-int-prop-size "Field value range"
    <td>[-2<sup>63</sup>,&nbsp;2<sup>63</sup>&nbsp;-&nbsp;1]
  <tr>
    <td>\ref api-tir-fc-int-prop-base "Preferred display base"
    <td>#BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a signed integer field class.

@returns
    New signed integer field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_integer_signed_create(
		bt_trace_class *trace_class);

/*! @} */

/*!
@name Single-precision real field class
@{
*/

/*!
@brief
    Creates a single-precision \bt_real_fc from the trace class
    \bt_p{trace_class}.

On success, the returned single-precision real field class has the
following property value:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a single-preicision real
    field class.

@returns
    New single-precision real field class reference, or \c NULL on
    memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_real_single_precision_create(
		bt_trace_class *trace_class);

/*! @} */

/*!
@name Double-precision real field class
@{
*/

/*!
@brief
    Creates a double-precision \bt_real_fc from the trace class
    \bt_p{trace_class}.

On success, the returned double-precision real field class has the
following property value:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a double-preicision real
    field class.

@returns
    New double-precision real field class reference, or \c NULL on
    memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_real_double_precision_create(
		bt_trace_class *trace_class);

/*! @} */

/*!
@name Enumeration field class
@{
*/

/*!
@brief
    Array of \c const \bt_enum_fc labels.

Returned by bt_field_class_enumeration_unsigned_get_mapping_labels_for_value()
and bt_field_class_enumeration_signed_get_mapping_labels_for_value().
*/
typedef char const * const *bt_field_class_enumeration_mapping_label_array;

/*!
@brief
    Status codes for
    bt_field_class_enumeration_unsigned_get_mapping_labels_for_value()
    and
    bt_field_class_enumeration_signed_get_mapping_labels_for_value().
*/
typedef enum bt_field_class_enumeration_get_mapping_labels_for_value_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_enumeration_get_mapping_labels_for_value_status;

/*!
@brief
    Status codes for bt_field_class_enumeration_unsigned_add_mapping()
    and bt_field_class_enumeration_signed_add_mapping().
*/
typedef enum bt_field_class_enumeration_add_mapping_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_enumeration_add_mapping_status;

/*! @} */

/*!
@name Enumeration field class mapping
@{
*/

/*!
@typedef struct bt_field_class_enumeration_mapping bt_field_class_enumeration_mapping;

@brief
    Enumeration field class mapping.
*/

/*!
@brief
    Returns the number of mappings contained in the \bt_enum_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] field_class
    Enumeration field class of which to get the number of contained
    mappings.

@returns
    Number of contained mappings in \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_enum_fc{field_class}
*/
extern uint64_t bt_field_class_enumeration_get_mapping_count(
		const bt_field_class *field_class);

/*!
@brief
    Returns the label of the \bt_enum_fc mapping \bt_p{mapping}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] mapping
    Enumeration field class mapping of which to get the label.

@returns
    @parblock
    Label of \bt_p{mapping}.

    The returned pointer remains valid as long as \bt_p{mapping} exists.
    @endparblock

@bt_pre_not_null{mapping}
*/
extern const char *bt_field_class_enumeration_mapping_get_label(
		const bt_field_class_enumeration_mapping *mapping);

/*! @} */

/*!
@name Unsigned enumeration field class
@{
*/

/*!
@brief
    Creates an \bt_uenum_fc from the trace class \bt_p{trace_class}.

On success, the returned unsigned enumeration field class has the
following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-int-prop-size "Field value range"
    <td>[0,&nbsp;2<sup>64</sup>&nbsp;-&nbsp;1]
  <tr>
    <td>\ref api-tir-fc-int-prop-base "Preferred display base"
    <td>#BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
  <tr>
    <td>\ref api-tir-fc-enum-prop-mappings "Mappings"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create an unsigned enumeration field
    class.

@returns
    New unsigned enumeration field class reference, or \c NULL on memory
    error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_enumeration_unsigned_create(
		bt_trace_class *trace_class);

/*!
@brief
    Adds a mapping to the \bt_uenum_fc \bt_p{field_class} having the
    label \bt_p{label} and the unsigned integer ranges \bt_p{ranges}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] field_class
    Unsigned enumeration field class to which to add a mapping having
    the label \bt_p{label} and the integer ranges \bt_p{ranges}.
@param[in] label
    Label of the mapping to add to \bt_p{field_class} (copied).
@param[in] ranges
    Unsigned integer ranges of the mapping to add to
    \bt_p{field_class}.

@retval #BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_uenum_fc{field_class}
@bt_pre_not_null{label}
@pre
    \bt_p{field_class} has no mapping with the label \bt_p{label}.
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more unsigned integer ranges.
*/
extern bt_field_class_enumeration_add_mapping_status
bt_field_class_enumeration_unsigned_add_mapping(
		bt_field_class *field_class, const char *label,
		const bt_integer_range_set_unsigned *ranges);

/*!
@brief
    Borrows the mapping at index \bt_p{index} from the
    \bt_uenum_fc \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] field_class
    Unsigned enumeration field class from which to borrow the mapping at
    index \bt_p{index}.
@param[in] index
    Index of the mapping to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the mapping of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_uenum_fc{field_class}
@pre
    \bt_p{index} is less than the number of mappings in
    \bt_p{field_class} (as returned by
    bt_field_class_enumeration_get_mapping_count()).

@sa bt_field_class_enumeration_get_mapping_count() &mdash;
    Returns the number of mappings contained in an
    enumeration field class.
*/
extern const bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
		const bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the mapping having the label \bt_p{label} from the
    \bt_uenum_fc \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

If there's no mapping having the label \bt_p{label} in
\bt_p{field_class}, this function returns \c NULL.

@param[in] field_class
    Unsigned enumeration field class from which to borrow the mapping
    having the label \bt_p{label}.
@param[in] label
    Label of the mapping to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the mapping of
    \bt_p{field_class} having the label \bt_p{label}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_uenum_fc{field_class}
@bt_pre_not_null{label}
*/
extern const bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_label_const(
		const bt_field_class *field_class, const char *label);

/*!
@brief
    Returns an array of all the labels of the mappings of the
    \bt_uenum_fc \bt_p{field_class} of which the \bt_p_uint_rg contain
    the integral value \bt_p{value}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

This function sets \bt_p{*labels} to the resulting array and
\bt_p{*count} to the number of labels in \bt_p{*labels}.

On success, if there's no mapping ranges containing the value
\bt_p{value}, \bt_p{*count} is 0.

@param[in] field_class
    Unsigned enumeration field class from which to get the labels of the
    mappings of which the ranges contain \bt_p{value}.
@param[in] value
    Value for which to get the mapped labels in \bt_p{field_class}.
@param[out] labels
    @parblock
    <strong>On success</strong>, \bt_p{*labels}
    is an array of labels of the mappings of \bt_p{field_class}
    containing \bt_p{value}.

    The number of labels in \bt_p{*labels} is \bt_p{*count}.

    The array is owned by \bt_p{field_class} and remains valid as long
    as:

    - \bt_p{field_class} is not modified.
    - You don't call this function again with \bt_p{field_class}.
    @endparblock
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of labels
    in \bt_p{*labels} (can be 0).

@retval #BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_is_uenum_fc{field_class}
@bt_pre_not_null{labels}
@bt_pre_not_null{count}
*/
extern bt_field_class_enumeration_get_mapping_labels_for_value_status
bt_field_class_enumeration_unsigned_get_mapping_labels_for_value(
		const bt_field_class *field_class, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *labels,
		uint64_t *count);

/*! @} */

/*!
@name Unsigned enumeration field class mapping
@{
*/

/*!
@typedef struct bt_field_class_enumeration_unsigned_mapping bt_field_class_enumeration_unsigned_mapping;

@brief
    Unsigned enumeration field class mapping.
*/

/*!
@brief
    Borrows the \bt_p_uint_rg from the \bt_uenum_fc mapping
    \bt_p{mapping}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] mapping
    Unsigned enumeration field class mapping from which to borrow the
    unsigned integer ranges.

@returns
    Unsigned integer ranges of \bt_p{mapping}.

@bt_pre_not_null{mapping}
*/
extern const bt_integer_range_set_unsigned *
bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
		const bt_field_class_enumeration_unsigned_mapping *mapping);

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_uenum_fc mapping
    \bt_p{mapping} to the common #bt_field_class_enumeration_mapping
    type.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] mapping
    @parblock
    Unsigned enumeration field class mapping to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{mapping} as a common enumeration field class mapping.
*/
static inline
const bt_field_class_enumeration_mapping *
bt_field_class_enumeration_unsigned_mapping_as_mapping_const(
		const bt_field_class_enumeration_unsigned_mapping *mapping)
{
	return __BT_UPCAST_CONST(bt_field_class_enumeration_mapping, mapping);
}

/*! @} */

/*!
@name Signed enumeration field class
@{
*/

/*!
@brief
    Creates a \bt_senum_fc from the trace class \bt_p{trace_class}.

On success, the returned signed enumeration field class has the
following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-int-prop-size "Field value range"
    <td>[-2<sup>63</sup>,&nbsp;2<sup>63</sup>&nbsp;-&nbsp;1]
  <tr>
    <td>\ref api-tir-fc-int-prop-base "Preferred display base"
    <td>#BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
  <tr>
    <td>\ref api-tir-fc-enum-prop-mappings "Mappings"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a signed enumeration field
    class.

@returns
    New signed enumeration field class reference, or \c NULL on memory
    error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_enumeration_signed_create(
		bt_trace_class *trace_class);

/*!
@brief
    Adds a mapping to the \bt_senum_fc \bt_p{field_class} having the
    label \bt_p{label} and the \bt_p_sint_rg \bt_p{ranges}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] field_class
    Signed enumeration field class to which to add a mapping having
    the label \bt_p{label} and the integer ranges \bt_p{ranges}.
@param[in] label
    Label of the mapping to add to \bt_p{field_class} (copied).
@param[in] ranges
    Signed integer ranges of the mapping to add to
    \bt_p{field_class}.

@retval #BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_senum_fc{field_class}
@bt_pre_not_null{label}
@pre
    \bt_p{field_class} has no mapping with the label \bt_p{label}.
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more signed integer ranges.
*/
extern bt_field_class_enumeration_add_mapping_status
bt_field_class_enumeration_signed_add_mapping(
		bt_field_class *field_class, const char *label,
		const bt_integer_range_set_signed *ranges);

/*!
@brief
    Borrows the mapping at index \bt_p{index} from the
    \bt_senum_fc \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] field_class
    Signed enumeration field class from which to borrow the mapping at
    index \bt_p{index}.
@param[in] index
    Index of the mapping to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the mapping of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_senum_fc{field_class}
@pre
    \bt_p{index} is less than the number of mappings in
    \bt_p{field_class} (as returned by
    bt_field_class_enumeration_get_mapping_count()).

@sa bt_field_class_enumeration_get_mapping_count() &mdash;
    Returns the number of mappings contained in an
    enumeration field class.
*/
extern const bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
		const bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the mapping having the label \bt_p{label} from the
    \bt_senum_fc \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

If there's no mapping having the label \bt_p{label} in
\bt_p{field_class}, this function returns \c NULL.

@param[in] field_class
    Signed enumeration field class from which to borrow the mapping
    having the label \bt_p{label}.
@param[in] label
    Label of the mapping to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the mapping of
    \bt_p{field_class} having the label \bt_p{label}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_senum_fc{field_class}
@bt_pre_not_null{label}
*/
extern const bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_label_const(
		const bt_field_class *field_class, const char *label);

/*!
@brief
    Returns an array of all the labels of the mappings of the
    \bt_senum_fc \bt_p{field_class} of which the \bt_p_sint_rg contain
    the integral value \bt_p{value}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

This function sets \bt_p{*labels} to the resulting array and
\bt_p{*count} to the number of labels in \bt_p{*labels}.

On success, if there's no mapping ranges containing the value
\bt_p{value}, \bt_p{*count} is 0.

@param[in] field_class
    Signed enumeration field class from which to get the labels of the
    mappings of which the ranges contain \bt_p{value}.
@param[in] value
    Value for which to get the mapped labels in \bt_p{field_class}.
@param[out] labels
    @parblock
    <strong>On success</strong>, \bt_p{*labels}
    is an array of labels of the mappings of \bt_p{field_class}
    containing \bt_p{value}.

    The number of labels in \bt_p{*labels} is \bt_p{*count}.

    The array is owned by \bt_p{field_class} and remains valid as long
    as:

    - \bt_p{field_class} is not modified.
    - You don't call this function again with \bt_p{field_class}.
    @endparblock
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of labels
    in \bt_p{*labels} (can be 0).

@retval #BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_is_senum_fc{field_class}
@bt_pre_not_null{labels}
@bt_pre_not_null{count}
*/
extern bt_field_class_enumeration_get_mapping_labels_for_value_status
bt_field_class_enumeration_signed_get_mapping_labels_for_value(
		const bt_field_class *field_class, int64_t value,
		bt_field_class_enumeration_mapping_label_array *labels,
		uint64_t *count);

/*! @} */

/*!
@name Signed enumeration field class mapping
@{
*/

/*!
@typedef struct bt_field_class_enumeration_signed_mapping bt_field_class_enumeration_signed_mapping;

@brief
    Signed enumeration field class mapping.
*/

/*!
@brief
    Borrows the \bt_p_sint_rg from the \bt_senum_fc mapping
    \bt_p{mapping}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] mapping
    Signed enumeration field class mapping from which to borrow the
    signed integer ranges.

@returns
    Signed integer ranges of \bt_p{mapping}.

@bt_pre_not_null{mapping}
*/
extern const bt_integer_range_set_signed *
bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
		const bt_field_class_enumeration_signed_mapping *mapping);

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_senum_fc mapping
    \bt_p{mapping} to the common #bt_field_class_enumeration_mapping
    type.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

@param[in] mapping
    @parblock
    Signed enumeration field class mapping to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{mapping} as a common enumeration field class mapping.
*/
static inline
const bt_field_class_enumeration_mapping *
bt_field_class_enumeration_signed_mapping_as_mapping_const(
		const bt_field_class_enumeration_signed_mapping *mapping)
{
	return __BT_UPCAST_CONST(bt_field_class_enumeration_mapping, mapping);
}

/*! @} */

/*!
@name String field class
@{
*/

/*!
@brief
    Creates a \bt_string_fc from the trace class \bt_p{trace_class}.

On success, the returned string field class has the following property
value:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a string field class.

@returns
    New string field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_string_create(
		bt_trace_class *trace_class);

/*! @} */

/*!
@name Array field class
@{
*/

/*!
@brief
    Borrows the element field class from the \bt_array_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-array-prop-elem-fc "element field class"
property.

@param[in] field_class
    Array field class from which to borrow the element field class.

@returns
    Element field class of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_array_fc{field_class}

@sa bt_field_class_array_borrow_element_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *bt_field_class_array_borrow_element_field_class(
		bt_field_class *field_class);

/*!
@brief
    Borrows the element field class from the \bt_array_fc
    \bt_p{field_class} (\c const version).

See bt_field_class_array_borrow_element_field_class().
*/
extern const bt_field_class *
bt_field_class_array_borrow_element_field_class_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Static array field class
@{
*/

/*!
@brief
    Creates a \bt_sarray_fc having the element field class
    \bt_p{element_field_class} and the length \bt_p{length} from the
    trace class \bt_p{trace_class}.

On success, the returned static array field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-array-prop-elem-fc "Element field class"
    <td>\bt_p{element_field_class}
  <tr>
    <td>\ref api-tir-fc-sarray-prop-len "Length"
    <td>\bt_p{length}
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a static array field class.
@param[in] element_field_class
    Class of the element fields of the instances of the static array
    field class to create.
@param[in] length
    Length of the instances of the static array field class to create.

@returns
    New static array field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{element_field_class}
bt_pre_fc_not_in_tc{element_field_class}

@bt_post_success_frozen{element_field_class}
*/
extern bt_field_class *bt_field_class_array_static_create(
		bt_trace_class *trace_class,
		bt_field_class *element_field_class, uint64_t length);

/*!
@brief
    Returns the length of the \bt_sarray_fc \bt_p{field_class}.

See the \ref api-tir-fc-sarray-prop-len "length" property.

@param[in] field_class
    Static array field class of which to get the length.

@returns
    Length of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_sarray_fc{field_class}
*/
extern uint64_t bt_field_class_array_static_get_length(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Dynamic array field class
@{
*/

/*!
@brief
    Creates a \bt_darray_fc having the element field class
    \bt_p{element_field_class} from the trace class \bt_p{trace_class}.

If \bt_p{length_field_class} is not \c NULL, then the created dynamic
array field class has a linked length field class.
See
\ref api-tir-fc-link "Field classes with links to other field classes"
to learn more.

On success, the returned dynamic array field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-array-prop-elem-fc "Element field class"
    <td>\bt_p{element_field_class}
  <tr>
    <td>\ref api-tir-fc-darray-prop-len-fp "Length field path"
    <td>
      \em None (if \bt_p{length_field_class} is not \c NULL, this
      property becomes available when the returned field class becomes
      part of an \bt_ev_cls or of a \bt_stream_cls)
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a dynamic array field class.
@param[in] element_field_class
    Class of the element fields of the instances of the dynamic array
    field class to create.
@param[in] length_field_class
    @parblock
    Linked length field class of the dynamic array field class to
    create.

    Can be \c NULL.
    @endparblock

@returns
    New dynamic array field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{element_field_class}
@bt_pre_fc_not_in_tc{element_field_class}
@pre
    <strong>If \bt_p{length_field_class} is not \c NULL</strong>,
    \bt_p{length_field_class} is an \bt_uint_fc.

@bt_post_success_frozen{element_field_class}
@bt_post_success_frozen{length_field_class}
*/
extern bt_field_class *bt_field_class_array_dynamic_create(
		bt_trace_class *trace_class,
		bt_field_class *element_field_class,
		bt_field_class *length_field_class);

/*! @} */

/*!
@name Dynamic array field class with length field
@{
*/

/*!
@brief
    Borrows the length field path from the \bt_darray_fc (with a length
    field) \bt_p{field_class}.

See the \ref api-tir-fc-darray-prop-len-fp "length field path" property.

This property is only available when a \bt_struct_fc containing
(recursively) \bt_p{field_class} is passed to one of:

- bt_stream_class_set_packet_context_field_class()
- bt_stream_class_set_event_common_context_field_class()
- bt_event_class_set_specific_context_field_class()
- bt_event_class_set_payload_field_class()

In the meantime, this function returns \c NULL.

@param[in] field_class
    Dynamic array field class from which to borrow the length
    field path.

@returns
    Length field path of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_darray_wl_fc{field_class}
*/
extern const bt_field_path *
bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Structure field class
@{
*/

/*!
@brief
    Creates a \bt_struct_fc from the trace class \bt_p{trace_class}.

On success, the returned structure field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-struct-prop-members "Members"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a structure field class.

@returns
    New structure field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
*/
extern bt_field_class *bt_field_class_structure_create(
    bt_trace_class *trace_class);

/*!
@brief
    Status codes for bt_field_class_structure_append_member().
*/
typedef enum bt_field_class_structure_append_member_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK    = __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_MEMORY_ERROR  = __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_structure_append_member_status;

/*!
@brief
    Appends a member to the \bt_struct_fc \bt_p{field_class} having the
    name \bt_p{name} and the field class \bt_p{member_field_class}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@param[in] field_class
    Structure field class to which to append a member having
    the name \bt_p{name} and the field class \bt_p{member_field_class}.
@param[in] name
    Name of the member to append to \bt_p{field_class} (copied).
@param[in] member_field_class
    Field class of the member to append to \bt_p{field_class}.

@retval #BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_struct_fc{field_class}
@pre
    \bt_p{field_class} has no member with the name \bt_p{name}.
@bt_pre_not_null{name}
@bt_pre_not_null{member_field_class}
@bt_pre_fc_not_in_tc{member_field_class}

@bt_post_success_frozen{member_field_class}
*/
extern bt_field_class_structure_append_member_status
bt_field_class_structure_append_member(
    bt_field_class *field_class,
    const char *name, bt_field_class *member_field_class);

/*!
@brief
    Returns the number of members contained in the \bt_struct_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@param[in] field_class
    Structure field class of which to get the number of contained
    members.

@returns
    Number of contained members in \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
*/
extern uint64_t bt_field_class_structure_get_member_count(
    const bt_field_class *field_class);

/*!
@brief
    Borrows the member at index \bt_p{index} from the
    \bt_struct_fc \bt_p{field_class}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@param[in] field_class
    Structure field class from which to borrow the member at
    index \bt_p{index}.
@param[in] index
    Index of the member to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the member of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@pre
    \bt_p{index} is less than the number of members in
    \bt_p{field_class} (as returned by
    bt_field_class_structure_get_member_count()).

@sa bt_field_class_structure_get_member_count() &mdash;
    Returns the number of members contained in a structure field class.
@sa bt_field_class_structure_borrow_member_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index(
    bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the member at index \bt_p{index} from the
    \bt_struct_fc \bt_p{field_class} (\c const version).

See bt_field_class_structure_borrow_member_by_index().
*/
extern const bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index_const(
    const bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the member having the name \bt_p{name} from the
    \bt_struct_fc \bt_p{field_class}.

See the \ref api-tir-fc-struct-prop-members "members" property.

If there's no member having the name \bt_p{name} in
\bt_p{field_class}, this function returns \c NULL.

@param[in] field_class
    Structure field class from which to borrow the member having the
    name \bt_p{name}.
@param[in] name
    Name of the member to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the member of
    \bt_p{field_class} having the name \bt_p{name}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_structure_borrow_member_by_name_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name(
    bt_field_class *field_class, const char *name);

/*!
@brief
    Borrows the member having the name \bt_p{name} from the
    \bt_struct_fc \bt_p{field_class} (\c const version).

See bt_field_class_structure_borrow_member_by_name().
*/
extern const bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name_const(
    const bt_field_class *field_class, const char *name);

/*! @} */

/*!
@name Structure field class member
@{
*/

/*!
@typedef struct bt_field_class_structure_member bt_field_class_structure_member;

@brief
    Structure field class member.
*/

/*!
@brief
    Returns the name of the \bt_struct_fc member \bt_p{member}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@param[in] member
    Structure field class member of which to get the name.

@returns
    @parblock
    Name of \bt_p{member}.

    The returned pointer remains valid as long as \bt_p{member} exists.
    @endparblock

@bt_pre_not_null{member}
*/
extern const char *bt_field_class_structure_member_get_name(
    const bt_field_class_structure_member *member);

/*!
@brief
    Borrows the field class from the \bt_struct_fc member
    \bt_p{member}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@param[in] member
    Structure field class member from which to borrow the field class.

@returns
    Field class of \bt_p{member}.

@bt_pre_not_null{member}

@sa bt_field_class_structure_member_borrow_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *
bt_field_class_structure_member_borrow_field_class(
    bt_field_class_structure_member *member);

/*!
@brief
    Borrows the field class from the \bt_struct_fc member
    \bt_p{member} (\c const version).

See bt_field_class_structure_member_borrow_field_class().
*/
extern const bt_field_class *
bt_field_class_structure_member_borrow_field_class_const(
    const bt_field_class_structure_member *member);

/*!
@brief
    Sets the user attributes of the \bt_struct_fc member \bt_p{member}
    to \bt_p{user_attributes}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@note
    When you append a member to a structure field class with
    bt_field_class_structure_append_member(), the member's
    initial user attributes is an empty \bt_map_val. Therefore you can
    borrow it with
    bt_field_class_structure_member_borrow_user_attributes() and fill it
    directly instead of setting a new one with this function.

@param[in] member
    Structure field class member of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{member}.

@bt_pre_not_null{member}
@bt_pre_hot{member}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_field_class_structure_member_borrow_user_attributes() &mdash;
    Borrows the user attributes of a structure field class member.
*/
extern void bt_field_class_structure_member_set_user_attributes(
    bt_field_class_structure_member *member,
    const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the \bt_struct_fc member
    \bt_p{member}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@note
    When you append a member to a structure field class with
    bt_field_class_structure_append_member(), the member's
    initial user attributes is an empty \bt_map_val.

@param[in] member
    Structure field class member from which to borrow the user
    attributes.

@returns
    User attributes of \bt_p{member} (a \bt_map_val).

@bt_pre_not_null{member}

@sa bt_field_class_structure_member_set_user_attributes() &mdash;
    Sets the user attributes of a structure field class member.
@sa bt_field_class_structure_member_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *
bt_field_class_structure_member_borrow_user_attributes(
    bt_field_class_structure_member *member);

/*!
@brief
    Borrows the user attributes of the \bt_struct_fc member
    \bt_p{member} (\c const version).

See bt_field_class_structure_member_borrow_user_attributes().
*/
extern const bt_value *
bt_field_class_structure_member_borrow_user_attributes_const(
    const bt_field_class_structure_member *member);

/*! @} */

/*!
@name Option field class
@{
*/

/*!
@brief
    Borrows the optional field class from the \bt_opt_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-opt-prop-fc "optional field class" property.

@param[in] field_class
    Option field class from which to borrow the optional field class.

@returns
    Optional field class of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_opt_fc{field_class}

@sa bt_field_class_option_borrow_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *bt_field_class_option_borrow_field_class(
		bt_field_class *field_class);

/*!
@brief
    Borrows the optional field class from the \bt_opt_fc
    \bt_p{field_class} (\c const version).

See bt_field_class_option_borrow_field_class().
*/
extern const bt_field_class *
bt_field_class_option_borrow_field_class_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Option field class without a selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (without a selector field) having the optional
    field class \bt_p{optional_field_class} from the trace class
    \bt_p{trace_class}.

On success, the returned option field class has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-opt-prop-fc "Optional field class"
    <td>\bt_p{optional_field_class}
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create an option field class.
@param[in] optional_field_class
    Class of the optional fields of the instances of the option field
    class to create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}

@bt_post_success_frozen{optional_field_class}
*/
extern bt_field_class *bt_field_class_option_without_selector_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class);

/*! @} */

/*!
@name Option field class with a selector field
@{
*/

/*!
@brief
    Borrows the selector field path from the \bt_opt_fc (with a selector
    field) \bt_p{field_class}.

See the \ref api-tir-fc-opt-prop-sel-fp "selector field path" property.

This property is only available when a \bt_struct_fc containing
(recursively) \bt_p{field_class} is passed to one of:

- bt_stream_class_set_packet_context_field_class()
- bt_stream_class_set_event_common_context_field_class()
- bt_event_class_set_specific_context_field_class()
- bt_event_class_set_payload_field_class()

In the meantime, this function returns \c NULL.

@param[in] field_class
    Option field class from which to borrow the selector field path.

@returns
    Selector field path of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_opt_ws_fc{field_class}
*/
extern const bt_field_path *
bt_field_class_option_with_selector_field_borrow_selector_field_path_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Option field class with a boolean selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (with a boolean selector field) having the
    optional field class \bt_p{optional_field_class} from the trace
    class \bt_p{trace_class}.

On success, the returned option field class has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-opt-prop-fc "Optional field class"
    <td>\bt_p{optional_field_class}
  <tr>
    <td>\ref api-tir-fc-opt-prop-sel-fp "Selector field path"
    <td>
      \em None (this property becomes available when the returned field
      class becomes part of an \bt_ev_cls or of a \bt_stream_cls)
  <tr>
    <td>\ref api-tir-fc-opt-prop-sel-rev "Selector is reversed?"
    <td>#BT_FALSE
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create an option field class.
@param[in] optional_field_class
    Class of the optional fields of the instances of the option field
    class to create.
@param[in] selector_field_class
    Linked selector field class of the option field class to create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_not_null{selector_field_class}
@pre
    \bt_p{selector_field_class} is a \bt_bool_fc.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}
*/
extern bt_field_class *bt_field_class_option_with_selector_field_bool_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		bt_field_class *selector_field_class);

/*!
@brief
    Sets whether or not the selector of the \bt_opt_fc (with a boolean
    selector field) \bt_p{field_class} is reversed.

See the \ref api-tir-fc-opt-prop-sel-rev "selector is reversed?"
property.

@param[in] field_class
    Option field class of which to set whether or not its selector
    is reversed.
@param[in] selector_is_reversed
    #BT_TRUE to make \bt_p{field_class} have a reversed selector.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_opt_wbs_fc{field_class}

@sa bt_field_class_option_with_selector_field_bool_selector_is_reversed() &mdash;
    Returns whether or not the selector of an option field class (with
    a boolean selector field) is reversed.
*/
extern void
bt_field_class_option_with_selector_field_bool_set_selector_is_reversed(
		bt_field_class *field_class, bt_bool selector_is_reversed);

/*!
@brief
    Returns whether or not the selector of the
    \bt_opt_fc (with a boolean selector field) is reversed.

See the \ref api-tir-fc-opt-prop-sel-rev "selector is reversed?"
property.

@param[in] field_class
    Option field class of which to get whether or not its selector is
    reversed.

@returns
    #BT_TRUE if the selector of \bt_p{field_class} is reversed.

@bt_pre_not_null{field_class}
@bt_pre_is_opt_wbs_fc{field_class}

@sa bt_field_class_option_with_selector_field_bool_set_selector_is_reversed() &mdash;
    Sets whether or not the selector of an option field class (with
    a boolean selector field) is reversed.
*/
extern bt_bool
bt_field_class_option_with_selector_field_bool_selector_is_reversed(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Option field class with an unsigned integer selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (with an unsigned integer selector field)
    having the optional field class \bt_p{optional_field_class} from the
    trace class \bt_p{trace_class}.

On success, the returned option field class has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-opt-prop-fc "Optional field class"
    <td>\bt_p{optional_field_class}
  <tr>
    <td>\ref api-tir-fc-opt-prop-sel-fp "Selector field path"
    <td>
      \em None (this property becomes available when the returned field
      class becomes part of an \bt_ev_cls or of a \bt_stream_cls)
  <tr>
    <td>\ref api-tir-fc-opt-prop-uint-rs "Selector's unsigned integer ranges"
    <td>\bt_p{ranges}
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create an option field class.
@param[in] optional_field_class
    Class of the optional fields of the instances of the option field
    class to create.
@param[in] selector_field_class
    Linked selector field class of the option field class to create.
@param[in] ranges
    Selector's unsigned integer ranges of the option field class to
    create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_not_null{selector_field_class}
@pre
    \bt_p{selector_field_class} is a \bt_uint_fc.
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more \bt_p_uint_rg.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}
@bt_post_success_frozen{ranges}
*/
extern bt_field_class *
bt_field_class_option_with_selector_field_integer_unsigned_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		bt_field_class *selector_field_class,
		const bt_integer_range_set_unsigned *ranges);

/*!
@brief
    Borrows the \bt_p_uint_rg from the \bt_opt_fc (with an unsigned
    integer selector field) \bt_p{field_class}.

See the
\ref api-tir-fc-opt-prop-uint-rs "selector's unsigned integer ranges"
property.

@param[in] field_class
    Option field class from which to borrow the unsigned integer ranges.

@returns
    Unsigned integer ranges of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_opt_wuis_fc{field_class}
*/
extern const bt_integer_range_set_unsigned *
bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Option field class with a signed integer selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (with a signed integer selector field)
    having the optional field class \bt_p{optional_field_class} from the
    trace class \bt_p{trace_class}.

On success, the returned option field class has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-opt-prop-fc "Optional field class"
    <td>\bt_p{optional_field_class}
  <tr>
    <td>\ref api-tir-fc-opt-prop-sel-fp "Selector field path"
    <td>
      \em None (this property becomes available when the returned field
      class becomes part of an \bt_ev_cls or of a \bt_stream_cls)
  <tr>
    <td>\ref api-tir-fc-opt-prop-sint-rs "Selector's signed integer ranges"
    <td>\bt_p{ranges}
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create an option field class.
@param[in] optional_field_class
    Class of the optional fields of the instances of the option field
    class to create.
@param[in] selector_field_class
    Linked selector field class of the option field class to create.
@param[in] ranges
    Selector's signed integer ranges of the option field class to
    create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_not_null{selector_field_class}
@pre
    \bt_p{selector_field_class} is a \bt_uint_fc.
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more \bt_p_uint_rg.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}
@bt_post_success_frozen{ranges}
*/
extern bt_field_class *
bt_field_class_option_with_selector_field_integer_signed_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		bt_field_class *selector_field_class,
		const bt_integer_range_set_signed *ranges);

/*!
@brief
    Borrows the \bt_p_sint_rg from the \bt_opt_fc (with a signed
    integer selector field) \bt_p{field_class}.

See the
\ref api-tir-fc-opt-prop-sint-rs "selector's signed integer ranges"
property.

@param[in] field_class
    Option field class from which to borrow the signed integer ranges.

@returns
    Signed integer ranges of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_opt_wsis_fc{field_class}
*/
extern const bt_integer_range_set_signed *
bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Variant field class
@{
*/

/*!
@brief
    Creates a \bt_var_fc from the trace class \bt_p{trace_class}.

If \bt_p{selector_field_class} is not \c NULL, then the created variant
field class has a linked selector field class.
See
\ref api-tir-fc-link "Field classes with links to other field classes"
to learn more.

On success, the returned variant field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-var-prop-sel-fp "Selector field path"
    <td>
      \em None (if \bt_p{selector_field_class} is not \c NULL, this
      property becomes available when the returned field class becomes
      part of an \bt_ev_cls or of a \bt_stream_cls)
  <tr>
    <td>\ref api-tir-fc-var-prop-opts "Options"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a variant field class.
@param[in] selector_field_class
    @parblock
    Linked selector field class of the variant field class to create.

    Can be \c NULL.
    @endparblock

@returns
    New variant field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@pre
    <strong>If \bt_p{selector_field_class} is not \c NULL</strong>,
    \bt_p{selector_field_class} is an \bt_int_fc.

@bt_post_success_frozen{element_field_class}
@bt_post_success_frozen{selector_field_class}
*/
extern bt_field_class *bt_field_class_variant_create(
		bt_trace_class *trace_class,
		bt_field_class *selector_field_class);

/*!
@brief
    Returns the number of options contained in the \bt_var_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class of which to get the number of contained
    options.

@returns
    Number of contained options in \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_var_fc{field_class}
*/
extern uint64_t bt_field_class_variant_get_option_count(
		const bt_field_class *field_class);

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class from which to borrow the option at
    index \bt_p{index}.
@param[in] index
    Index of the option to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the option of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_fc{field_class}
@pre
    \bt_p{index} is less than the number of options in
    \bt_p{field_class} (as returned by
    bt_field_class_variant_get_option_count()).

@sa bt_field_class_variant_get_option_count() &mdash;
    Returns the number of options contained in a variant field class.
@sa bt_field_class_variant_borrow_option_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index(
		bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc \bt_p{field_class} (\c const version).

See bt_field_class_variant_borrow_option_by_index().
*/
extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

If there's no option having the name \bt_p{name} in
\bt_p{field_class}, this function returns \c NULL.

@param[in] field_class
    Variant field class from which to borrow the option having the
    name \bt_p{name}.
@param[in] name
    Name of the option to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the option of
    \bt_p{field_class} having the name \bt_p{name}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_variant_borrow_option_by_name_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name(
		bt_field_class *field_class, const char *name);

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc \bt_p{field_class} (\c const version).

See bt_field_class_variant_borrow_option_by_name().
*/
extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

/*! @} */

/*!
@name Variant field class option
@{
*/

/*!
@typedef struct bt_field_class_variant_option bt_field_class_variant_option;

@brief
    Variant field class option.
*/

/*!
@brief
    Returns the name of the \bt_var_fc option \bt_p{option}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] option
    Variant field class option of which to get the name.

@returns
    @parblock
    Name of \bt_p{option}.

    The returned pointer remains valid as long as \bt_p{option} exists.
    @endparblock

@bt_pre_not_null{option}
*/
extern const char *bt_field_class_variant_option_get_name(
		const bt_field_class_variant_option *option);

/*!
@brief
    Borrows the field class from the \bt_var_fc option
    \bt_p{option}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] option
    Variant field class option from which to borrow the field class.

@returns
    Field class of \bt_p{option}.

@bt_pre_not_null{option}

@sa bt_field_class_variant_option_borrow_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *bt_field_class_variant_option_borrow_field_class(
		bt_field_class_variant_option *option);

/*!
@brief
    Borrows the field class from the \bt_var_fc option
    \bt_p{option} (\c const version).

See bt_field_class_variant_option_borrow_field_class().
*/
extern const bt_field_class *
bt_field_class_variant_option_borrow_field_class_const(
		const bt_field_class_variant_option *option);

/*!
@brief
    Sets the user attributes of the \bt_var_fc option \bt_p{option}
    to \bt_p{user_attributes}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@note
    When you append an option to a variant field class with
    bt_field_class_variant_without_selector_append_option(),
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option(),
    or
    bt_field_class_variant_with_selector_field_integer_signed_append_option(),
    the option's initial user attributes is an empty \bt_map_val.
    Therefore you can borrow it with
    bt_field_class_variant_option_borrow_user_attributes() and fill it
    directly instead of setting a new one with this function.

@param[in] option
    Variant field class option of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{option}.

@bt_pre_not_null{option}
@bt_pre_hot{option}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_field_class_variant_option_borrow_user_attributes() &mdash;
    Borrows the user attributes of a variant field class option.
*/
extern void bt_field_class_variant_option_set_user_attributes(
		bt_field_class_variant_option *option,
		const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the \bt_var_fc option \bt_p{option}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@note
    When you append an option to a variant field class with
    bt_field_class_variant_without_selector_append_option(),
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option(),
    or
    bt_field_class_variant_with_selector_field_integer_signed_append_option(),
    the option's initial user attributes is an empty \bt_map_val.

@param[in] option
    Variant field class option from which to borrow the user
    attributes.

@returns
    User attributes of \bt_p{option} (a \bt_map_val).

@bt_pre_not_null{option}

@sa bt_field_class_variant_option_set_user_attributes() &mdash;
    Sets the user attributes of a variant field class option.
@sa bt_field_class_variant_option_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_field_class_variant_option_borrow_user_attributes(
		bt_field_class_variant_option *option);

/*!
@brief
    Borrows the user attributes of the \bt_var_fc option \bt_p{option}
    (\c const version).

See bt_field_class_variant_option_borrow_user_attributes().
*/
extern const bt_value *bt_field_class_variant_option_borrow_user_attributes_const(
		const bt_field_class_variant_option *option);

/*! @} */

/*!
@name Variant field class without a selector field
@{
*/

/*!
@brief
    Status codes for
    bt_field_class_variant_without_selector_append_option().
*/
typedef enum bt_field_class_variant_without_selector_append_option_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_FIELD_APPEND_OPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_variant_without_selector_append_option_status;

/*!
@brief
    Appends an option to the \bt_var_fc (without a selector field)
    \bt_p{field_class} having the name \bt_p{name} and the
    field class \bt_p{option_field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class to which to append an option having
    the name \bt_p{name} and the field class \bt_p{option_field_class}.
@param[in] name
    Name of the option to append to \bt_p{field_class} (copied).
@param[in] option_field_class
    Field class of the option to append to \bt_p{field_class}.

@retval #BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_FIELD_APPEND_OPTION_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_var_wos_fc{field_class}
@pre
    \bt_p{field_class} has no option with the name \bt_p{name}.
@bt_pre_not_null{name}
@bt_pre_not_null{option_field_class}
@bt_pre_fc_not_in_tc{option_field_class}

@bt_post_success_frozen{option_field_class}
*/
extern bt_field_class_variant_without_selector_append_option_status
bt_field_class_variant_without_selector_append_option(
		bt_field_class *field_class, const char *name,
		bt_field_class *option_field_class);

/*! @} */

/*!
@name Variant field class with a selector field
@{
*/

/*!
@brief
    Status codes for
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option()
    and
    bt_field_class_variant_with_selector_field_integer_signed_append_option().
*/
typedef enum bt_field_class_variant_with_selector_field_integer_append_option_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_variant_with_selector_field_integer_append_option_status;

/*!
@brief
    Borrows the selector field path from the \bt_var_fc (with a selector
    field) \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-sel-fp "selector field path" property.

This property is only available when a \bt_struct_fc containing
(recursively) \bt_p{field_class} is passed to one of:

- bt_stream_class_set_packet_context_field_class()
- bt_stream_class_set_event_common_context_field_class()
- bt_event_class_set_specific_context_field_class()
- bt_event_class_set_payload_field_class()

In the meantime, this function returns \c NULL.

@param[in] field_class
    Variant field class from which to borrow the selector field path.

@returns
    Selector field path of \bt_p{field_class}.

@bt_pre_not_null{field_class}
@bt_pre_is_var_ws_fc{field_class}
*/
extern const bt_field_path *
bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
		const bt_field_class *field_class);

/*! @} */

/*!
@name Variant field class with an unsigned integer selector field
@{
*/

/*!
@brief
    Appends an option to the \bt_var_fc (with an unsigned integer
    selector field) \bt_p{field_class} having the name \bt_p{name},
    the field class \bt_p{option_field_class}, and the
    \bt_p_uint_rg \bt_p{ranges}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class to which to append an option having
    the name \bt_p{name}, the field class \bt_p{option_field_class},
    and the unsigned integer ranges \bt_p{ranges}.
@param[in] name
    Name of the option to append to \bt_p{field_class} (copied).
@param[in] option_field_class
    Field class of the option to append to \bt_p{field_class}.
@param[in] ranges
    Unsigned integer ranges of the option to append to
    \bt_p{field_class}.

@retval #BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_var_wuis_fc{field_class}
@pre
    \bt_p{field_class} has no option with the name \bt_p{name}.
@bt_pre_not_null{name}
@bt_pre_not_null{option_field_class}
@bt_pre_fc_not_in_tc{option_field_class}
@bt_pre_not_null{ŗanges}
@pre
    \bt_p{ranges} contains one or more unsigned integer ranges.
@pre
    The unsigned integer ranges in \bt_p{ranges} do not overlap
    any unsigned integer range of any existing option in
    \bt_p{field_class}.

@bt_post_success_frozen{option_field_class}
*/
extern bt_field_class_variant_with_selector_field_integer_append_option_status
bt_field_class_variant_with_selector_field_integer_unsigned_append_option(
		bt_field_class *field_class, const char *name,
		bt_field_class *option_field_class,
		const bt_integer_range_set_unsigned *ranges);

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc (with an unsigned integer selector field)
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class from which to borrow the option at
    index \bt_p{index}.
@param[in] index
    Index of the option to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the option of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_wuis_fc{field_class}
@pre
    \bt_p{index} is less than the number of options in
    \bt_p{field_class} (as returned by
    bt_field_class_variant_get_option_count()).

@sa bt_field_class_variant_get_option_count() &mdash;
    Returns the number of options contained in a variant field class.
*/
extern const bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc (with an unsigned integer selector field)
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

If there's no option having the name \bt_p{name} in
\bt_p{field_class}, this function returns \c NULL.

@param[in] field_class
    Variant field class from which to borrow the option having the
    name \bt_p{name}.
@param[in] name
    Name of the option to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the option of
    \bt_p{field_class} having the name \bt_p{name}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_wuis_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_variant_borrow_option_by_name_const() &mdash;
    Borrows an option by name from a variant field class.
*/
extern const bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

/*! @} */

/*!
@name Variant field class (with an unsigned integer selector field) option
@{
*/

/*!
@typedef struct bt_field_class_variant_with_selector_field_integer_unsigned_option bt_field_class_variant_with_selector_field_integer_unsigned_option;

@brief
    Variant field class (with an unsigned integer selector field) option.
*/

/*!
@brief
    Borrows the \bt_p_uint_rg from the \bt_var_fc (with an unsigned
    integer selector field) option \bt_p{option}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] option
    Variant field class option from which to borrow the
    unsigned integer ranges.

@returns
    Unsigned integer ranges of \bt_p{option}.

@bt_pre_not_null{option}
*/
extern const bt_integer_range_set_unsigned *
bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const(
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *option);

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_var_fc (with an
    unsigned integer selector field) option \bt_p{option} to the
    common #bt_field_class_variant_option type.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] option
    @parblock
    Variant field class option to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{option} as a common variant field class option.
*/
static inline
const bt_field_class_variant_option *
bt_field_class_variant_with_selector_field_integer_unsigned_option_as_option_const(
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *option)
{
	return __BT_UPCAST_CONST(bt_field_class_variant_option, option);
}

/*! @} */

/*!
@name Variant field class with a signed integer selector field
@{
*/

/*!
@brief
    Appends an option to the \bt_var_fc (with a signed integer
    selector field) \bt_p{field_class} having the name \bt_p{name},
    the field class \bt_p{option_field_class}, and the
    \bt_p_sint_rg \bt_p{ranges}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class to which to append an option having
    the name \bt_p{name} and the field class \bt_p{option_field_class},
    and the signed integer ranges \bt_p{ranges}.
@param[in] name
    Name of the option to append to \bt_p{field_class} (copied).
@param[in] option_field_class
    Field class of the option to append to \bt_p{field_class}.
@param[in] ranges
    Signed integer ranges of the option to append to
    \bt_p{field_class}.

@retval #BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_var_wsis_fc{field_class}
@pre
    \bt_p{field_class} has no option with the name \bt_p{name}.
@bt_pre_not_null{name}
@bt_pre_not_null{option_field_class}
@bt_pre_fc_not_in_tc{option_field_class}
@bt_pre_not_null{ŗanges}
@pre
    \bt_p{ranges} contains one or more signed integer ranges.
@pre
    The signed integer ranges in \bt_p{ranges} do not overlap with
    any signed integer range of any existing option in
    \bt_p{field_class}.

@bt_post_success_frozen{option_field_class}
*/
extern bt_field_class_variant_with_selector_field_integer_append_option_status
bt_field_class_variant_with_selector_field_integer_signed_append_option(
		bt_field_class *field_class, const char *name,
		bt_field_class *option_field_class,
		const bt_integer_range_set_signed *ranges);

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc (with a signed integer selector field)
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class from which to borrow the option at
    index \bt_p{index}.
@param[in] index
    Index of the option to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the option of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_wsis_fc{field_class}
@pre
    \bt_p{index} is less than the number of options in
    \bt_p{field_class} (as returned by
    bt_field_class_variant_get_option_count()).

@sa bt_field_class_variant_get_option_count() &mdash;
    Returns the number of options contained in a variant field class.
*/
extern const bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index);

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc (with a signed integer selector field)
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

If there's no option having the name \bt_p{name} in
\bt_p{field_class}, this function returns \c NULL.

@param[in] field_class
    Variant field class from which to borrow the option having the
    name \bt_p{name}.
@param[in] name
    Name of the option to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the option of
    \bt_p{field_class} having the name \bt_p{name}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    is not modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_wsis_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_variant_borrow_option_by_name_const() &mdash;
    Borrows an option by name from a variant field class.
*/
extern const bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name);

/*! @} */

/*!
@name Variant field class (with a signed integer selector field) option
@{
*/

/*!
@typedef struct bt_field_class_variant_with_selector_field_integer_signed_option bt_field_class_variant_with_selector_field_integer_signed_option;

@brief
    Variant field class (with a signed integer selector field) option.
*/

/*!
@brief
    Borrows the \bt_p_sint_rg from the \bt_var_fc (with a signed
    integer selector field) option \bt_p{option}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] option
    Variant field class option from which to borrow the
    signed integer ranges.

@returns
    Signed integer ranges of \bt_p{option}.

@bt_pre_not_null{option}
*/
extern const bt_integer_range_set_signed *
bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const(
		const bt_field_class_variant_with_selector_field_integer_signed_option *option);

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_var_fc (with a signed
    integer selector field) option \bt_p{option} to the
    common #bt_field_class_variant_option type.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] option
    @parblock
    Variant field class option to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{option} as a common variant field class option.
*/
static inline
const bt_field_class_variant_option *
bt_field_class_variant_with_selector_field_integer_signed_option_as_option_const(
		const bt_field_class_variant_with_selector_field_integer_signed_option *option)
{
	return __BT_UPCAST_CONST(bt_field_class_variant_option, option);
}

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the field class \bt_p{field_class}.

@param[in] field_class
    @parblock
    Field class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_field_class_put_ref() &mdash;
    Decrements the reference count of a field class.
*/
extern void bt_field_class_get_ref(const bt_field_class *field_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the field class \bt_p{field_class}.

@param[in] field_class
    @parblock
    Field class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_field_class_get_ref() &mdash;
    Increments the reference count of a field class.
*/
extern void bt_field_class_put_ref(const bt_field_class *field_class);

/*!
@brief
    Decrements the reference count of the field class
    \bt_p{_field_class}, and then sets \bt_p{_field_class} to \c NULL.

@param _field_class
    @parblock
    Field class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_field_class}
*/
#define BT_FIELD_CLASS_PUT_REF_AND_RESET(_field_class)	\
	do {						\
		bt_field_class_put_ref(_field_class);	\
		(_field_class) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the field class \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a field class reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_FIELD_CLASS_MOVE_REF(_dst, _src)	\
	do {					\
		bt_field_class_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_FIELD_CLASS_H */
