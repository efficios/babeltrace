/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_TRACE_IR_FIELD_CLASS_H
#define BABELTRACE2_TRACE_IR_FIELD_CLASS_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

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
    - \ref api-tir-fc-blob "BLOB" (static and dynamic; \bt_avail_since{1})
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

Since Babeltrace&nbsp;2.1, get the effective \bt_mip (MIP) version of
the trace processing \bt_graph containing the \bt_comp from which a
field class was created with bt_field_class_get_graph_mip_version().

The following table shows the available type enumerators and creation
functions for each type of \em concrete (non-abstract) field class:

<table>
  <tr>
    <th>Name
    <th>Type enumerator
    <th>Creation function(s)
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
    <td><em>Static \ref api-tir-fc-blob "BLOB" (\bt_avail_since{1})</em>
    <td>#BT_FIELD_CLASS_TYPE_STATIC_BLOB
    <td>bt_field_class_blob_static_create()
  <tr>
    <td>
      <em>Dynamic \ref api-tir-fc-blob "BLOB"
      (instances without a linked length field; \bt_avail_since{1})</em>
    <td>#BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD
    <td>bt_field_class_blob_dynamic_without_length_field_location_create()
  <tr>
    <td>
      <em>Dynamic \ref api-tir-fc-blob "BLOB"
      (instances with a linked length field; \bt_avail_since{1})</em>
    <td>#BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD
    <td>bt_field_class_blob_dynamic_with_length_field_location_create()
  <tr>
    <td><em>Static \ref api-tir-fc-array "array"</em>
    <td>#BT_FIELD_CLASS_TYPE_STATIC_ARRAY
    <td>bt_field_class_array_static_create()
  <tr>
    <td>
      <em>Dynamic \ref api-tir-fc-array "array"
      (instances without a linked length field)</em>
    <td>#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD
    <td>
      - bt_field_class_array_dynamic_create()
      - bt_field_class_array_dynamic_without_length_field_location_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>Dynamic \ref api-tir-fc-array "array"
      (instances with a linked length field)</em>
    <td>#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD
    <td>
      - bt_field_class_array_dynamic_create()
      - bt_field_class_array_dynamic_with_length_field_location_create()
        (\bt_avail_since{1})
  <tr>
    <td><em>\ref api-tir-fc-struct "Structure"</em>
    <td>#BT_FIELD_CLASS_TYPE_STRUCTURE
    <td>bt_field_class_structure_create()
  <tr>
    <td>
      <em>\ref api-tir-fc-opt "Option"
      (instances without a linked selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD
    <td>
      - bt_field_class_option_without_selector_create()
      - bt_field_class_option_without_selector_field_location_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>\ref api-tir-fc-opt "Option"
      (instances with a linked boolean selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD
    <td>
      - bt_field_class_option_with_selector_field_bool_create()
      - bt_field_class_option_with_selector_field_location_bool_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>\ref api-tir-fc-opt "Option"
      (instances with a linked unsigned integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD
    <td>
      - bt_field_class_option_with_selector_field_integer_unsigned_create()
      - bt_field_class_option_with_selector_field_location_integer_unsigned_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>\ref api-tir-fc-opt "Option"
      (instances with a linked signed integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD
    <td>
      - bt_field_class_option_with_selector_field_integer_signed_create()
      - bt_field_class_option_with_selector_field_location_integer_signed_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>\ref api-tir-fc-var "Variant"
      (instances without a linked selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD
    <td>
      - bt_field_class_variant_create()
      - bt_field_class_variant_without_selector_field_location_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>\ref api-tir-fc-var "Variant"
      (instances with a linked unsigned integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD
    <td>
      - bt_field_class_variant_create()
      - bt_field_class_variant_with_selector_field_location_integer_unsigned_create()
        (\bt_avail_since{1})
  <tr>
    <td>
      <em>\ref api-tir-fc-var "Variant"
      (instances with a linked signed integer selector field)</em>
    <td>#BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD
    <td>
      - bt_field_class_variant_create()
      - bt_field_class_variant_with_selector_field_location_integer_signed_create()
        (\bt_avail_since{1})
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

- If an instance of any of the field classes recursively contained in
  the passed field class has a
  \ref api-tir-fc-link "link to an anterior field", then it must
  honor the field link rules.

Once you have called one of the four functions above, the passed field
class becomes \ref api-fund-freezing "frozen".

@sa \ref guide-mip-0-to-mip-1

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
    field is&nbsp;64.

    You cannot change the length once the bit array field class is
    created.

    Get the length of instances of a bit array field class with
    bt_field_class_bit_array_get_length().
  </dd>

  <dt>
    \anchor api-tir-fc-ba-prop-flags
    \bt_dt_opt Flags
    (only available when the field class was created from a
    \bt_trace_cls which was created
    from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;1; \bt_avail_since{1})
  </dt>
  <dd>
    Set of flags of the bit array field class.

    A bit array field class flag is a label (string) and an
    \bt_uint_rs, a set of bit index ranges. In the value of an
    instance (a \bt_ba_field) returned by
    bt_field_bit_array_get_value_as_integer(), the index of the
    least significant bit is&nbsp;0.

    The integer ranges of a given flag or of multiple flags of
    the same bit array field class can overlap. For example,
    a bit array field class can have those two flags:

    - <code>GALLERY</code>: [1,&nbsp;4], [8,&nbsp;14]
    - <code>LUNCH</code>: [3,&nbsp;6]

    In that case, the bit indexes&nbsp;2 and&nbsp;12 correspond to the
    label <code>GALLERY</code>, the bit index&nbsp;5 to the label
    <code>LUNCH</code>, and the bit index&nbsp;3 to the labels
    \c GALLERY \em and <code>LUNCH</code>.

    Given some bit flag field value (as an integer), a flag is said to
    be <strong><em>active</em></strong> when <em>at least one</em> of
    its bit indexes is the index of a set bit of the value. For example,
    given the 8-bit bit array field value

    @code{.unparsed}
    [true, false, true, true, false, true, false, true]
    @endcode

    where the first element is the first bit of the bit array field (the
    value as an integer being&nbsp;0xad):

    - A flag targeting bits&nbsp;1,&nbsp;3, and&nbsp;6 would be active because
      bit&nbsp;3 is set.

    - A flag targeting bits&nbsp;1 and&nbsp;4 wouldn't be active because both
      bits are cleared.

    Two flags of the same bit array field class cannot have the
    same label.

    Add a flag to a bit array field class with
    bt_field_class_bit_array_add_flag().

    Get the number of flags in a bit array field class with
    bt_field_class_bit_array_get_flag_count().

    Borrow a flag from a bit array field class with
    bt_field_class_bit_array_borrow_flag_by_index_const()
    and
    bt_field_class_bit_array_borrow_flag_by_label_const().

    A bit array field class flag is a
    \ref api-fund-unique-object "unique object": it
    belongs to the bit array field class which contains it.

    The type of a bit array field class flag is
    #bt_field_class_bit_array_flag.

    Get the label of a bit array field class flag with
    bt_field_class_bit_array_flag_get_label().

    Borrow the bit index range set of a bit array field class flag
    with bt_field_class_bit_array_flag_borrow_index_ranges_const().

    Get the labels of all the active flags of a bit array field class
    for the set bits of some integral value with
    bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer().
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
    can only contain the values from&nbsp;0 to&nbsp;16383.

    \bt_cp_sink_comp can benefit from this property to make some space
    optimizations when writing trace data.

    Use bt_field_class_integer_set_field_value_range() and
    bt_field_class_integer_get_field_value_range().
  </dd>

  <dt>
    \anchor api-tir-fc-int-prop-hints
    Field value hints (only available when the field class was created
    from a \bt_trace_cls which was created
    from a \bt_comp which belongs to a trace processing \bt_graph with
    the effective \bt_mip version&nbsp;1; \bt_avail_since{1})
  </dt>
  <dd>
    Hints about the values of the instances (integer fields)
    integer field class.

    As of \bt_name_version_min_maj, the only available hint is
    #BT_FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL which means that
    instances are expected to hold small integer values.

    Use bt_field_class_integer_set_field_value_hints(),
    bt_field_class_integer_get_field_value_hints(), and
    bt_field_class_integer_has_field_value_hint().
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
(enumeration field) of this field class with the value&nbsp;15
or&nbsp;28 has the label <code>SUGAR</code>.

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

    In that case, the values&nbsp;2 and&nbsp;30 correspond to the label
    <code>CALORIES</code>, the value&nbsp;12 to the label
    <code>SODIUM</code>, and the value&nbsp;10 to the labels
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
    the type of the field class:
    #bt_field_class_enumeration_unsigned_mapping and
    #bt_field_class_enumeration_signed_mapping.

    There's also the #bt_field_class_enumeration_mapping type for
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

<h1>\anchor api-tir-fc-blob BLOB field classes (\bt_avail_since{1})</h1>

@image html fc-blob.png

<strong><em>BLOB field classes</em></strong> are
classes of \bt_p_blob_field.

BLOB fields contain zero or more bytes of binary data.

@note
    BLOB field classes are only available when the \bt_trace_cls from
    which you create them was created from a \bt_comp which belongs
    to a trace processing \bt_graph with the effective \bt_mip
    version&nbsp;1 (\bt_avail_since{1}).

A BLOB field class is an \em abstract field class: you cannot create
one. The concrete BLOB field classes are:

<dl>
  <dt>Static BLOB field class</dt>
  <dd>
    Its instances (static BLOB fields) contain a fixed number of bytes.

    Create with bt_field_class_blob_static_create().

    A static BLOB field class has the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-sblob-prop-len
        Length
      </dt>
      <dd>
        Number of bytes contained in the instances (static BLOB
        fields) of the static BLOB field class.

        You cannot change the length once the static BLOB field class is
        created.

        Get the length of instances of a static BLOB field class with
        bt_field_class_blob_static_get_length().
      </dd>
    </dl>
  </dd>

  <dt>Dynamic BLOB field class</dt>
  <dd>
    Its instances (dynamic BLOB fields) contain a variable number
    of bytes.

    There are two types of dynamic BLOB field classes: instances
    without or with a linked length field. See
    \ref api-tir-fc-link "Fields with links to other fields"
    to learn more.

    @image html dblob-link.png "A dynamic blob field linked to an unsigned integer length field."

    Create with
    bt_field_class_blob_dynamic_without_length_field_location_create()
    or bt_field_class_blob_dynamic_with_length_field_location_create().

    The class of a dynamic BLOB field with a linked length field has
    the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-dblob-prop-len-fl
        Length field location
      </dt>
      <dd>
        \bt_c_field_loc to locate the linked length field of an
        instance.

        Borrow the length field location of a dynamic BLOB field class
        with
        bt_field_class_blob_dynamic_with_length_field_borrow_length_field_location_const().
      </dd>
    </dl>
  </dd>
</dl>

BLOB field classes have the following common property:

<dl>
  <dt>
    \anchor api-tir-fc-blob-prop-media-type
    Media type
  </dt>
  <dd>
    <a href="https://datatracker.ietf.org/doc/html/rfc2046">IANA media type</a>
    of instances of the BLOB field class.

    Use bt_field_class_blob_set_media_type() and
    bt_field_class_blob_get_media_type().
  </dd>
</dl>

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

        Get the length of instances of a static array field class with
        bt_field_class_array_static_get_length().
      </dd>
    </dl>
  </dd>

  <dt>Dynamic array field class</dt>
  <dd>
    Its instances (dynamic array fields) contain a variable number
    of fields.

    There are two types of dynamic array field classes: instances
    without or with a linked length field. See
    \ref api-tir-fc-link "Fields with links to other fields"
    to learn more.

    @image html darray-link.png "A dynamic array field linked to an unsigned integer length field."

    Create with, depending on the effective \bt_mip (MIP) version of the
    trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>bt_field_class_array_dynamic_create().</dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>
        bt_field_class_array_dynamic_without_length_field_location_create()
        or
        bt_field_class_array_dynamic_with_length_field_location_create().
      </dd>
    </dl>

    The class of a dynamic array field with a linked length field has
    one of the following specific properties, depending on the
    effective MIP version of the trace processing graph:

    <dl>
      <dt>
        \anchor api-tir-fc-darray-prop-len-fp
        MIP&nbsp;0: Length field path
      </dt>
      <dd>
        \bt_c_field_path to locate the linked length field of
        an instance.

        Borrow the length field path of a dynamic array field class with
        bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const().
      </dd>

      <dt>
        \anchor api-tir-fc-darray-prop-len-fl
        MIP&nbsp;1: Length field location (\bt_avail_since{1})
      </dt>
      <dd>
        \bt_c_field_loc to locate the linked length field of an
        instance.

        Borrow the length field location of a dynamic array field class
        with
        bt_field_class_array_dynamic_with_length_field_borrow_length_field_location_const().
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

    Borrow the element field class of an array field class with
    bt_field_class_array_borrow_element_field_class() and
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

    Get the name of a structure field class member with
    bt_field_class_structure_member_get_name().

    Borrow the field class of a structure field class member with
    bt_field_class_structure_member_borrow_field_class() and
    bt_field_class_structure_member_borrow_field_class_const().

    Set the user attributes of a structure field class member with
    bt_field_class_structure_member_set_user_attributes().

    Borrow the user attributes of a structure field class member with
    bt_field_class_structure_member_borrow_user_attributes() and
    bt_field_class_structure_member_borrow_user_attributes_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-opt Option field classes</h1>

@image html fc-opt.png

<strong><em>Option field classes</em></strong> are classes
of \bt_p_opt_field.

An option field either does or doesn't \em contain a field, called its
optional field.

An option field class is an \em abstract field class: you cannot create
one. An instance of an option field class either has a linked selector
field (see \ref api-tir-fc-link "Fields with links to other fields")
or none. Therefore, the concrete option field classes are:

<dl>
  <dt>Option field class (instances without a linked selector field)</dt>
  <dd>
    Create with, depending on the effective \bt_mip (MIP) version
    of the trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>bt_field_class_option_without_selector_create()</dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_option_without_selector_field_location_create()</dd>
    </dl>

    The class of an option field without a linked selector field has no
    specific properties.
  </dd>

  <dt>Option field class (instances with a linked boolean selector field)</dt>
  <dd>
    The linked selector field of an option field class instance
    (an option field) is a \bt_bool_field.

    @image html opt-link.png "Option field class with a boolean selector field."

    Create with, depending on the effective \bt_mip (MIP) version:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>bt_field_class_option_with_selector_field_bool_create()</dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_option_with_selector_field_location_bool_create()</dd>
    </dl>

    The class of an option field with a linked boolean selector field
    has the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-opt-prop-sel-rev
        Selector is reversed?
      </dt>
      <dd>
        Whether or not the linked boolean selector field make the
        option field class instance (an option field) \em contain a
        field when it's #BT_TRUE or when it's #BT_FALSE.

        If this property is #BT_TRUE, then if the linked selector field
        has the value #BT_FALSE, the option field contains a field.

        Use
        bt_field_class_option_with_selector_field_bool_set_selector_is_reversed()
        and
        bt_field_class_option_with_selector_field_bool_selector_is_reversed().
      </dd>
    </dl>
  </dd>

  <dt>Option field class (instances with a linked unsigned integer selector field)</dt>
  <dd>
    The linked selector field of an option field class instance
    (an option field) is an \bt_uint_field.

    Create with, depending on the effective \bt_mip (MIP) version
    of the trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>bt_field_class_option_with_selector_field_integer_unsigned_create()</dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_option_with_selector_field_location_integer_unsigned_create()</dd>
    </dl>

    Pass an \bt_uint_rs on creation to set which values of the selector
    field can make the option field contain a field.

    The class of an option field with a linked unsigned integer selector
    field has the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-opt-prop-uint-rs
        Unsigned integer ranges of the selector
      </dt>
      <dd>
        If the linked unsigned integer selector field of an option
        field class instance (an option field) has a value which
        intersects with the unsigned integer ranges of the selector,
        then the option field \em contains a field.

        You cannot change the unsigned integer ranges of the selector
        once the option field class is created.

        Borrow the unsigned integer ranges of the selector from such an
        option field class with
        bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const().
      </dd>
    </dl>
  </dd>

  <dt>Option field class (instances with a linked signed integer selector field)</dt>
  <dd>
    The linked selector field of an option field class instance
    (an option field) is a \bt_sint_field.

    Create with, depending on the effective \bt_mip (MIP) version of
    the trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>bt_field_class_option_with_selector_field_integer_signed_create()</dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_option_with_selector_field_location_integer_signed_create()</dd>
    </dl>

    Pass a \bt_sint_rs on creation to set which values of the selector
    field can make the option field contain a field.

    The class of an option field class with a linked signed integer
    selector field has the following specific property:

    <dl>
      <dt>
        \anchor api-tir-fc-opt-prop-sint-rs
        Signed integer ranges of the selector
      </dt>
      <dd>
        If the linked signed integer selector field of an option
        field class instance (an option field) has a value which
        intersects with the signed integer ranges of the selector, then
        the option field \em contains a field.

        You cannot change the signed integer ranges of the selector once
        the option field class is created.

        Borrow the signed integer ranges of the selector from such an
        option field class with
        bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const().
      </dd>
    </dl>
  </dd>
</dl>

The classes of option fields with a linked selector field have one of
the following common properties, depending on the effective
\bt_mip (MIP) version of the trace processing \bt_graph:

<dl>
  <dt>
    \anchor api-tir-fc-opt-prop-sel-fp
    MIP&nbsp;0: Selector field path
  </dt>
  <dd>
    \bt_c_field_path to locate the linked selector field of an instance.

    Borrow the selector field path of such an option field class with
    bt_field_class_option_with_selector_field_borrow_selector_field_path_const().
  </dd>

  <dt>
    \anchor api-tir-fc-opt-prop-sel-fl
    MIP&nbsp;1: Selector field location (\bt_avail_since{1})
  </dt>
  <dd>
    \bt_c_field_loc to locate the linked selector field of an instance.

    Borrow the selector field location of such an option field class
    with
    bt_field_class_option_with_selector_field_borrow_selector_field_location_const().
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

    Borrow the optional field class of an option field class with
    bt_field_class_option_borrow_field_class() and
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
one. An instance of a variant field class either has a linked selector
field (see \ref api-tir-fc-link "Fields with links to other fields")
or none. Therefore, the concrete variant field classes are:

<dl>
  <dt>Variant field class (instances without a linked selector field)</dt>
  <dd>
    Create with, depending on the effective \bt_mip (MIP) version
    of the trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>
        bt_field_class_variant_create(), passing \c NULL as
        the selector field class.
      </dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_variant_without_selector_field_location_create().</dd>
    </dl>

    Append an option to such a variant field class with
    bt_field_class_variant_without_selector_append_option().

    The class of a variant field without a linked selector field has no
    specific properties.
  </dd>

  <dt>Variant field class (instances with a linked unsigned selector field)</dt>
  <dd>
    The linked selector field of a variant field class instance
    (a variant field) is an \bt_uint_field.

    @image html var-link.png "A variant field linked to an unsigned integer selector field."

    Create with, depending on the effective \bt_mip (MIP) version of the
    trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>
        bt_field_class_variant_create(), passing an unsigned integer
        field class as the selector field class.
      </dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_variant_with_selector_field_location_integer_unsigned_create().</dd>
    </dl>

    Append an option to such a variant field class with
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option().

    Pass an \bt_uint_rs when you append an option to set which values of
    the selector field can make the variant field have a given
    current option.

    Borrow an option of such a variant field class with
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const()
    and
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const().
  </dd>

  <dt>Variant field class (instances with a linked signed selector field)</dt>
  <dd>
    The linked selector field of a variant field class instance
    (a variant field) is a \bt_sint_field.

    Create with, depending on the effective \bt_mip (MIP) version of the
    trace processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>
        bt_field_class_variant_create(), passing an signed integer
        field class as the selector field class.
      </dt>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>bt_field_class_variant_with_selector_field_location_integer_signed_create().</dd>
    </dl>

    Append an option to such a variant field class with
    bt_field_class_variant_with_selector_field_integer_signed_append_option().

    Pass a \bt_sint_rs when you append an option to set which values of
    the selector field can make the variant field have a given
    current option.

    Borrow an option of such a variant field class with
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const()
    and
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const().
  </dd>
</dl>

The classes of variant fields with a linked selector field have one of
the following common properties, depending on the effective
\bt_mip (MIP) version of the trace processing \bt_graph:

<dl>
  <dt>
    \anchor api-tir-fc-var-prop-sel-fp
    MIP&nbsp;0: Selector field path
  </dt>
  <dd>
    \bt_c_field_path to locate the linked selector field of an instance.

    Borrow the selector field path of such a variant field class with
    bt_field_class_variant_with_selector_field_borrow_selector_field_path_const().
  </dd>

  <dt>
    \anchor api-tir-fc-var-prop-sel-fl
    MIP&nbsp;1: Selector field location (\bt_avail_since{1})
  </dt>
  <dd>
    \bt_c_field_loc to locate the linked selector field of an instance.

    Borrow the selector field location of such a variant field class
    with
    bt_field_class_variant_with_selector_field_borrow_selector_field_location_const().
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

      The name is optional when the effective \bt_mip version of the
      trace processing \bt_graph is&nbsp;1 (\bt_avail_since{1}).

    - A field class.

    - User attributes.

    If an instance of the variant field class is linked to a selector
    field, then each option also has an \bt_int_rs. In that case, the
    ranges of a given option cannot overlap any range of any other
    option.

    A variant field class must contain at least one option.

    Depending on whether or not an instance of the variant field class
    has a linked selector field, append an option to a variant field
    class with bt_field_class_variant_without_selector_append_option(),
    bt_field_class_variant_with_selector_field_integer_unsigned_append_option(),
    or
    bt_field_class_variant_with_selector_field_integer_signed_append_option().

    Get the number of options contained in a variant field class
    with bt_field_class_variant_get_option_count().

    A variant field class option is a
    \ref api-fund-unique-object "unique object": it
    belongs to the variant field class which contains it.

    Borrow an option of any variant field class with
    bt_field_class_variant_borrow_option_by_index(),
    bt_field_class_variant_borrow_option_by_name(),
    bt_field_class_variant_borrow_option_by_index_const(), and
    bt_field_class_variant_borrow_option_by_name_const().

    Those functions return the common #bt_field_class_variant_option
    type. Get the name of a variant field class option with
    bt_field_class_variant_option_get_name(). Borrow the field class
    of a variant field class option with
    bt_field_class_variant_option_borrow_field_class() and
    bt_field_class_variant_option_borrow_field_class_const().

    Borrow the option of the class of a variant field with a linked
    selector field with
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(),
    bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const(),
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(), or
    bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const(),
    depending on the type of the selector field class.

    Those functions return the
    #bt_field_class_variant_with_selector_field_integer_unsigned_option or
    #bt_field_class_variant_with_selector_field_integer_signed_option type.
    \ref api-fund-c-typing "Upcast" those types to the
    #bt_field_class_variant_option type with
    bt_field_class_variant_with_selector_field_integer_unsigned_option_as_option_const()
    or
    bt_field_class_variant_with_selector_field_integer_signed_option_as_option_const().

    Borrow ranges of an option from the class of a variant field
    with a linked selector field with
    bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const()
    or
    bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const().

    Set the user attributes of a variant field class option with
    bt_field_class_variant_option_set_user_attributes().

    Borrow the user attributes of a variant field class option with
    bt_field_class_variant_option_borrow_user_attributes() and
    bt_field_class_variant_option_borrow_user_attributes_const().
  </dd>
</dl>

<h1>\anchor api-tir-fc-link Fields with links to other fields</h1>

An instance of a \bt_dblob_fc (\bt_avail_since{1}), a \bt_darray_fc, a
\bt_opt_fc, or a \bt_var_fc \em may have a link to another, anterior
field within the same \bt_pkt or \bt_ev.

This feature exists so that the linked field can contain the value of a
dynamic property of the "dependent" field. Those properties are:

<dl>
  <dt>\bt_c_dblob_field (\bt_avail_since{1})</dt>
  <dd>
    The linked field, a \bt_uint_field, contains the \b length (number
    of bytes) of the dynamic BLOB field.
  </dd>

  <dt>\bt_c_darray_field</dt>
  <dd>
    The linked field, a \bt_uint_field, contains the \b length (number
    of elements) of the dynamic array field.
  </dd>

  <dt>\bt_c_opt_field</dt>
  <dd>
    The linked field, either a \bt_bool_field or an \bt_int_field,
    indicates whether or not the option field has a field.
  </dd>

  <dt>\bt_c_var_field</dt>
  <dd>
    The linked field, an \bt_int_field, indicates the current selected
    field of a variant field.
  </dd>
</dl>

Having a linked field is <em>optional</em>: you always set the field
properties with a dedicated function anyway. For example, even if a
dynamic array field is linked to an anterior length field, you must
still set its integral length with bt_field_array_dynamic_set_length().
In that case, the value of the linked length field \em must match what
you pass to bt_field_array_dynamic_set_length().

The purpose of this feature is to eventually save space when a
\bt_sink_comp writes trace files: if, for example, the trace format can
convey that an anterior, existing field represents the length of a
dynamic array field, then the sink component doesn't need to write the
length of the dynamic array field twice. This is the case of the
<a href="https://diamon.org/ctf/">Common Trace Format</a>, for example.

@image html darray-link.png "A dynamic array field linked to an unsigned integer length field."

How exactly you link a dependent field to another field at the field
class level depends on the effective \bt_mip (MIP) version of the trace
processing \bt_graph:

<dl>
  <dt>MIP&nbsp;0: \bt_c_field_path API
  <dd>
    Pass the class of the linked field when you create the class of the
    dependent field.

    For example, pass the unsigned integer length field class to
    bt_field_class_array_dynamic_create() to create a class of
    \bt_darray_field with a linked length field.

    Then, when you call
    bt_stream_class_set_packet_context_field_class(),
    bt_stream_class_set_event_common_context_field_class(),
    bt_event_class_set_specific_context_field_class(), or
    bt_event_class_set_payload_field_class() with a field class
    containing a dependent field class with the class a linked field,
    the path to the linked field becomes available in the dependent
    field class. The functions to borrow said field path are:

    - bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const()
    - bt_field_class_option_with_selector_field_borrow_selector_field_path_const()
    - bt_field_class_variant_with_selector_field_borrow_selector_field_path_const()
  </dd>

  <dt>MIP&nbsp;1: \bt_c_field_loc API (\bt_avail_since{1})
  <dd>
    Pass the field location when you create the class of the
    dependent field.

    For example, pass the length field location to
    bt_field_class_array_dynamic_with_length_field_location_create() to
    create a class of \bt_darray_field with a linked length field.

    The functions to borrow the field location of a field class are:

    - bt_field_class_blob_dynamic_with_length_field_borrow_length_field_location_const()
    - bt_field_class_array_dynamic_with_length_field_borrow_length_field_location_const()
    - bt_field_class_option_with_selector_field_borrow_selector_field_location_const()
    - bt_field_class_variant_with_selector_field_borrow_selector_field_location_const()
  </dd>
</dl>

Both \bt_p_field_path (MIP&nbsp;0 API) and \bt_p_field_loc (MIP&nbsp;1
API; \bt_avail_since{1}) indicate how to reach the
linked field of a dependent field from some specific root <em>scope</em>
(the starting point). The available scopes are:

<dl>
  <dt>#BT_FIELD_PATH_SCOPE_PACKET_CONTEXT</dt>
  <dt>#BT_FIELD_LOCATION_SCOPE_PACKET_CONTEXT (\bt_avail_since{1})</dt>
  <dd>
    Context field of the current \bt_pkt.

    See bt_packet_borrow_context_field_const().
  </dd>

  <dt>#BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT</dt>
  <dt>#BT_FIELD_LOCATION_SCOPE_EVENT_COMMON_CONTEXT (\bt_avail_since{1})</dt>
  <dd>
    Common context field of the current \bt_ev.

    See bt_event_borrow_common_context_field_const().
  </dd>

  <dt>#BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT</dt>
  <dt>#BT_FIELD_LOCATION_SCOPE_EVENT_SPECIFIC_CONTEXT (\bt_avail_since{1})</dt>
  <dd>
    Specific context field of the current event.

    See bt_event_borrow_specific_context_field_const().
  </dd>

  <dt>#BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD</dt>
  <dt>#BT_FIELD_LOCATION_SCOPE_EVENT_PAYLOAD (\bt_avail_since{1})</dt>
  <dd>
    Payload field of the current event.

    See bt_event_borrow_payload_field_const().
  </dd>
</dl>

The difference between a field path and a field location is that a field
path works with \bt_struct_field member \em indexes while a field
location works with structure field member <em>names</em>. The latter is
more versatile because many fields may be candidates for the
length/selector field of a dependent field (when they're all part of the
same \bt_var_field).

The rules regarding the dependent field&nbsp;\bt_var{A} vs. the linked
field&nbsp;\bt_var{B} are:

- If \bt_var{A} is within some packet context field, then \bt_var{B}
  must also be in the same packet context field.

  See bt_stream_class_set_packet_context_field_class().

- If \bt_var{A} is within some common event context field, then
  \bt_var{B} must be in one of:

  - The same common event context field.
  - The context field of the same packet.

  See bt_stream_class_set_event_common_context_field_class().

- If \bt_var{A} is within some specific event context field, then
  \bt_var{B} must be in one of:

  - The same specific event context field.
  - The common context field of the same event.
  - The context field of the same packet.

  See bt_event_class_set_specific_context_field_class().

- If \bt_var{A} is within some event payload field, then \bt_var{B} must
  be in one of:

  - The same event payload field.
  - The specific context field of the same event.
  - The common context field of the same event.
  - The context field of the same packet.

  See bt_event_class_set_payload_field_class().

- If both \bt_var{A} and \bt_var{B} are in the same scope, then:

  - The lowest common ancestor field of \bt_var{A} and \bt_var{B} must
    be a structure field.

  - \bt_var{B} must precede \bt_var{A}.

    Considering that the members of a structure field are ordered,
    then \bt_var{B} must be part of a member that's before the member
    which contains \bt_var{A} in their lowest common ancestor
    structure field.

  - <strong>Under MIP&nbsp;0</strong>, the path from the lowest common
    ancestor structure field of \bt_var{A} and \bt_var{B} to \bt_var{A}
    and to \bt_var{B} must only contain structure fields.

  - <strong>Under MIP&nbsp;1</strong>, the path from the lowest common
    ancestor structure field of \bt_var{A} and \bt_var{B} to \bt_var{A}
    and to \bt_var{B} must only contain structure and variant fields.

- If \bt_var{A} is in a different scope than \bt_var{B}, then:

  - <strong>Under MIP&nbsp;0</strong>, the path from the root scope of
    \bt_var{B} to \bt_var{B} must only contain structure fields.

  - <strong>Under MIP&nbsp;1</strong>, the path from the root scope of
    \bt_var{B} to \bt_var{B} must only contain structure and variant
    fields.
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
	    \bt_c_blob_fc.

	No field class has this type: use it with
	bt_field_class_type_is().

	@bt_since{1}
	*/
	BT_FIELD_CLASS_TYPE_BLOB						= (1ULL << 29),

	/*!
	@brief
	    \bt_c_sblob_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_BLOB.

	@bt_since{1}
	*/
	BT_FIELD_CLASS_TYPE_STATIC_BLOB						= (1ULL << 30) | BT_FIELD_CLASS_TYPE_BLOB,

	/*!
	@brief
	    \bt_c_dblob_fc.

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_BLOB.

	No field class has this type: use it with
	bt_field_class_type_is().

	@bt_since{1}
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB					= (1ULL << 31) | BT_FIELD_CLASS_TYPE_BLOB,

	/*!
	@brief
	    \bt_c_dblob_fc (instances without a linked length field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB.

	@bt_since{1}
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD			= (1ULL << 32) | BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB,

	/*!
	@brief
	    \bt_c_dblob_fc (instances with a linked length field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB.

	@bt_since{1}
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD			= (1ULL << 33) | BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB,

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
	    \bt_c_darray_fc (instances without a linked length field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY.
	*/
	BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD			= (1ULL << 14) | BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,

	/*!
	@brief
	    \bt_c_darray_fc (instances with a linked length field).

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
	    \bt_c_opt_fc (instances without a linked selector field).
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD			= (1ULL << 17) | BT_FIELD_CLASS_TYPE_OPTION,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked selector field).

	This type conceptually inherits #BT_FIELD_CLASS_TYPE_OPTION.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD				= (1ULL << 18) | BT_FIELD_CLASS_TYPE_OPTION,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked boolean selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD			= (1ULL << 19) | BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD			= (1ULL << 20) | BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked unsigned integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 21) | BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked signed integer selector field).

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
	    \bt_c_var_fc (instances without a linked selector field).
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD			= (1ULL << 24) | BT_FIELD_CLASS_TYPE_VARIANT,

	/*!
	@brief
	    \bt_c_var_fc (instances with a linked selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD				= (1ULL << 25) | BT_FIELD_CLASS_TYPE_VARIANT,

	/*!
	@brief
	    \bt_c_var_fc (instances with a linked integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD.

	No field class has this type: use it with
	bt_field_class_type_is().
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD			= (1ULL << 26) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked unsigned integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD	= (1ULL << 27) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD,

	/*!
	@brief
	    \bt_c_opt_fc (instances with a linked signed integer selector field).

	This type conceptually inherits
	#BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD.
	*/
	BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD		= (1ULL << 28) | BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD,

	/*
	 * Make sure the enumeration type is a 64-bit integer in case
	 * the project needs field class types in the future.
	 *
	 * This isn't part of the API.
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
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
		const bt_field_class_type other_type) __BT_NOEXCEPT
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
    <code>bt_field_class_*_create()</code> functions, the initial user
    attributes of the field class is an empty \bt_map_val. Therefore you
    can borrow it with bt_field_class_borrow_user_attributes() and fill
    it directly instead of setting a new one with this function.

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
		bt_field_class *field_class,
		const bt_value *user_attributes) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the field class \bt_p{field_class}.

See the \ref api-tir-fc-prop-user-attrs "user attributes" property.

@note
    When you create a field class with one of the
    <code>bt_field_class_*_create()</code> functions, the
    initial user attributes of the field class is an empty \bt_map_val.

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
		bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the field class \bt_p{field_class}
    (\c const version).

See bt_field_class_borrow_user_attributes().

@param[in] field_class
    See bt_field_class_borrow_user_attributes().

@returns
    See bt_field_class_borrow_user_attributes().
*/
extern const bt_value *bt_field_class_borrow_user_attributes_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Effective Message Interchange Protocol (MIP) version access
@{
*/

/*!
@brief
    Returns the effective \bt_mip (MIP) version of the trace processing
    \bt_graph containing the \bt_comp from which \bt_p{field_class} was
    created.

@param[in] field_class
    Field class of which to get the effective MIP version.

@returns
    Effective MIP version of \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}

@sa bt_self_component_get_graph_mip_version() &mdash;
    Returns the effective MIP version of the trace processing
    graph which contains a given component.
*/
extern uint64_t bt_field_class_get_graph_mip_version(
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

/*!
@}
*/

/*!
@name Bit array field class
@{
*/

/*!
@typedef struct bt_field_class_bit_array_flag bt_field_class_bit_array_flag;

@brief
    Bit array field class flag.

@bt_since{1}
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
    <td>
      \bt_mip version&nbsp;1:
      \ref api-tir-fc-ba-prop-flags "flags"
      (\bt_avail_since{1})
    <td>\em None
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
		bt_trace_class *trace_class, uint64_t length) __BT_NOEXCEPT;

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_field_class_bit_array_add_flag().

@bt_since{1}
*/
typedef enum bt_field_class_bit_array_add_flag_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

} bt_field_class_bit_array_add_flag_status;

/*!
@brief
    Adds a flag to the \bt_ba_fc \bt_p{field_class} having the
    label \bt_p{label} and the bit index ranges \bt_p{index_ranges}.

See the \ref api-tir-fc-ba-prop-flags "flags" property.

@param[in] field_class
    Bit array field class to which to add a flag having
    the label \bt_p{label} and the bit index ranges \bt_p{index_ranges}.
@param[in] label
    Label of the flag to add to \bt_p{field_class} (copied).
@param[in] index_ranges
    Bit index ranges of the flag to add to \bt_p{field_class}.

@retval #BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_ba_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
@bt_pre_not_null{label}
@pre
    \bt_p{field_class} has no flag with the label \bt_p{label}.
@bt_pre_not_null{ranges}
@pre
    \bt_p{index_ranges} contains one or more unsigned integer ranges.
@pre
    No \link bt_integer_range_unsigned_get_upper() upper value\endlink
    of any range in \bt_p{index_ranges} is greater than or
    equal to the
    \link bt_field_class_bit_array_get_length() length\endlink of
    \bt_p{field_class}.
*/
extern bt_field_class_bit_array_add_flag_status
bt_field_class_bit_array_add_flag(
	bt_field_class *field_class, const char *label,
	const bt_integer_range_set_unsigned *index_ranges) __BT_NOEXCEPT;

/*!
@brief
    Returns the number of flags contained in the \bt_ba_fc
    \bt_p{field_class}.

See the \ref api-tir-fc-ba-prop-flags "flags" property.

@param[in] field_class
    Bit array field class of which to get the number of contained
    flags.

@returns
    Number of contained flags in \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_ba_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
*/
extern uint64_t bt_field_class_bit_array_get_flag_count(
	const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the flag at index \bt_p{index} from the
    \bt_ba_fc \bt_p{field_class}.

See the \ref api-tir-fc-ba-prop-flags "flags" property.

@param[in] field_class
    Bit array field class from which to borrow the flag at
    index \bt_p{index}.
@param[in] index
    Index of the flag to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the flag of
    \bt_p{field_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{field_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_ba_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
@pre
    \bt_p{index} is less than the
    \link bt_field_class_bit_array_get_flag_count() number of flags\endlink
    in \bt_p{field_class}.

@sa bt_field_class_bit_array_get_flag_count() &mdash;
    Returns the number of flags contained in a
    bit array field class.
*/
extern const bt_field_class_bit_array_flag *
bt_field_class_bit_array_borrow_flag_by_index_const(
	const bt_field_class *field_class, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the flag having the label \bt_p{label} from the
    \bt_ba_fc \bt_p{field_class}.

See the \ref api-tir-fc-ba-prop-flags "flags" property.

If there's no flag having the label \bt_p{label} in
\bt_p{field_class}, then this function returns \c NULL.

@param[in] field_class
    Bit array field class from which to borrow the flag
    having the label \bt_p{label}.
@param[in] label
    Label of the flag to borrow from \bt_p{field_class}.

@returns
    @parblock
    \em Borrowed reference of the flag of
    \bt_p{field_class} having the label \bt_p{label}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{field_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_ba_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
@bt_pre_not_null{label}
*/
extern const bt_field_class_bit_array_flag *
bt_field_class_bit_array_borrow_flag_by_label_const(
	const bt_field_class *field_class, const char *label) __BT_NOEXCEPT;

/*!
@brief
    Array of \c const \bt_ba_fc flag labels.

Returned by bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer().

@bt_since{1}
*/
typedef char const * const *bt_field_class_bit_array_flag_label_array;

/*!
@brief
    Status codes for
    bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer().

@bt_since{1}
*/
typedef enum bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_BIT_ARRAY_GET_ACTIVE_FLAG_LABELS_FOR_VALUE_AS_INTEGER_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_BIT_ARRAY_GET_ACTIVE_FLAG_LABELS_FOR_VALUE_AS_INTEGER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer_status;

/*!
@brief
    Returns the labels of all the active flags of the
    \bt_ba_fc \bt_p{field_class} for the set bits of
    \bt_p{value_as_integer} (a bit array field value as an integer).

See the \ref api-tir-fc-ba-prop-flags "flags" property.

This function sets \bt_p{*labels} to the resulting array and
\bt_p{*count} to the number of labels in \bt_p{*labels}.

On success, if there's no active flags for \bt_p{value_as_integer},
\bt_p{*count} is 0.

@param[in] field_class
    Bit array field class from which to get the labels of the
    active flags for \bt_p{value_as_integer}.
@param[in] value_as_integer
    @parblock
    Bits, as an integer, for which to get the labels of the active flags
    of \bt_p{field_class}.

    In this integral value, the index of the least significant bit
    is&nbsp;0.
    @endparblock
@param[out] labels
    @parblock
    <strong>On success</strong>, \bt_p{*labels}
    is an array of labels of the active flags of \bt_p{field_class}
    for \bt_p{value}.

    The number of labels in \bt_p{*labels} is \bt_p{*count}.

    The array is owned by \bt_p{field_class} and remains valid as long
    as:

    - \bt_p{field_class} isn't modified.
    - You don't call this function again with \bt_p{field_class}.
    @endparblock
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of labels
    in \bt_p{*labels} (can be&nbsp;0).

@retval #BT_FIELD_CLASS_BIT_ARRAY_GET_ACTIVE_FLAG_LABELS_FOR_VALUE_AS_INTEGER_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_BIT_ARRAY_GET_ACTIVE_FLAG_LABELS_FOR_VALUE_AS_INTEGER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_ba_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
@bt_pre_not_null{labels}
@bt_pre_not_null{count}
*/
extern bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer_status
bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer(
	const bt_field_class *field_class, uint64_t value_as_integer,
	bt_field_class_bit_array_flag_label_array *labels,
	uint64_t *count) __BT_NOEXCEPT;

/*!
@brief
    Returns the label of the \bt_ba_fc flag \bt_p{flag}.

See the \ref api-tir-fc-ba-prop-flags "flags" property.

@param[in] flag
    Bit array field class flag of which to get the label.

@returns
    @parblock
    Label of \bt_p{flag}.

    The returned pointer remains valid as long as \bt_p{flag} exists.
    @endparblock

@bt_since{1}

@bt_pre_not_null{flag}
*/
extern const char *bt_field_class_bit_array_flag_get_label(
	const bt_field_class_bit_array_flag *flag) __BT_NOEXCEPT;

/*!
@brief
    Borrows the bit index ranges from the \bt_ba_fc flag \bt_p{flag}.

See the \ref api-tir-fc-ba-prop-flags "flags" property.

@param[in] flag
    Bit array field class flag from which to borrow the
    bit index ranges.

@returns
    Bit index ranges of \bt_p{flag}.

@bt_since{1}

@bt_pre_not_null{flag}
*/
extern const bt_integer_range_set_unsigned *
bt_field_class_bit_array_flag_borrow_index_ranges_const(
	const bt_field_class_bit_array_flag *flag) __BT_NOEXCEPT;

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
      <dd>[0,&nbsp;2<sup><em>N</em></sup>&nbsp;−&nbsp;1]</dd>

      <dt>Signed integer field class</dt>
      <dd>[−2<sup><em>N</em>&nbsp;−&nbsp;1</sup>,&nbsp;2<sup><em>N</em>&nbsp;−&nbsp;1</sup>&nbsp;−&nbsp;1]</dd>
    </dl>
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_int_fc{field_class}
@pre
    1 ≤ \bt_p{n} ≤ 64.

@sa bt_field_class_integer_get_field_value_range() &mdash;
    Returns the field value range of an integer field class.
*/
extern void bt_field_class_integer_set_field_value_range(
		bt_field_class *field_class, uint64_t n) __BT_NOEXCEPT;

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
      <dd>[0,&nbsp;2<sup><em>N</em></sup>&nbsp;−&nbsp;1]</dd>

      <dt>Signed integer field class</dt>
      <dd>[−2<sup><em>N</em></sup>,&nbsp;2<sup><em>N</em></sup>&nbsp;−&nbsp;1]</dd>
    </dl>
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_int_fc{field_class}

@sa bt_field_class_integer_set_field_value_range() &mdash;
    Sets the field value range of an integer field class.
*/
extern uint64_t bt_field_class_integer_get_field_value_range(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Integer field class field value hint.

@bt_since{1}
*/
typedef enum bt_field_class_integer_field_value_hint {
	/*!
	@brief
	    Expect small integer field values.
	*/
	BT_FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL = 1ULL << 0,
} bt_field_class_integer_field_value_hint;

/*!
@brief
    Sets the field value hints of the \bt_int_fc \bt_p{field_class}
    to a bitwise disjunction (<em>OR</em>)
    of #bt_field_class_integer_field_value_hint enumerators.

See the \ref api-tir-fc-int-prop-hints "field value hints" property.

@param[in] field_class
    Integer field class of which to set the field value hints to
    \bt_p{hints}.
@param[in] hints
    Bitwise disjunction (<em>OR</em>) of
    of #bt_field_class_integer_field_value_hint enumerators.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_int_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
@pre
    \bt_p{hints} is 0 or #BT_FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL.

@sa bt_field_class_integer_get_field_value_hints() &mdash;
    Returns the field value hints of an integer field class.
@sa bt_field_class_integer_has_field_value_hint() &mdash;
    Returns whether or not an integer field class has a given
    field value hint.
*/
void bt_field_class_integer_set_field_value_hints(bt_field_class *field_class,
		uint64_t hints) __BT_NOEXCEPT;

/*!
@brief
    Returns the field value hints of the \bt_int_fc \bt_p{field_class}.

See the \ref api-tir-fc-int-prop-hints "field value hints" property.

@param[in] field_class
    Integer field class of which to get the field value hints.

@returns
    Field value hints of \bt_p{field_class} as a bitwise disjunction
    (<em>OR</em>) of #bt_field_class_integer_field_value_hint
    enumerators.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_int_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}

@sa bt_field_class_integer_set_field_value_hints() &mdash;
    Sets the field value hints of an integer field class.
@sa bt_field_class_integer_has_field_value_hint() &mdash;
    Returns whether or not an integer field class has a given
    field value hint.
*/
uint64_t bt_field_class_integer_get_field_value_hints(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Returns whether or not the \bt_int_fc \bt_p{field_class} has
    the field value hint \bt_p{hint}.

See the \ref api-tir-fc-int-prop-hints "field value hints" property.

@param[in] field_class
    Integer field class of which to get whether or not it has the
    field value hint \bt_p{hint}.
@param[in] hint
    Field value hint to check.

@retval #BT_TRUE
    \bt_p{field_class} has the field value hint \bt_p{hint}.
@retval #BT_FALSE
    \bt_p{field_class} doesn't have the field value hint \bt_p{hint}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_int_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
@pre
    \bt_p{hint} is 0 or #BT_FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL.

@sa bt_field_class_integer_set_field_value_hints() &mdash;
    Sets the field value hints of an integer field class.
@sa bt_field_class_integer_get_field_value_hints() &mdash;
    Returns the field value hints of an integer field class.
*/
static inline
bt_bool bt_field_class_integer_has_field_value_hint(
		const bt_field_class * const field_class,
		const bt_field_class_integer_field_value_hint hint)
		__BT_NOEXCEPT
{
	return (bt_field_class_integer_get_field_value_hints(field_class) & (uint64_t) hint) ?
		BT_TRUE : BT_FALSE;
}

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
		bt_field_class_integer_preferred_display_base preferred_display_base)
		__BT_NOEXCEPT;

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
    <td>[0,&nbsp;2<sup>64</sup>&nbsp;−&nbsp;1]
  <tr>
    <td>
      \bt_mip version&nbsp;1:
      \ref api-tir-fc-int-prop-hints "field value hints"
      (\bt_avail_since{1})
    <td>\em None
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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
    <td>[−2<sup>63</sup>,&nbsp;2<sup>63</sup>&nbsp;−&nbsp;1]
  <tr>
    <td>
      \bt_mip version&nbsp;1:
      \ref api-tir-fc-int-prop-hints "field value hints"
      (\bt_avail_since{1})
    <td>\em None
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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Enumeration field class
@{
*/

/*!
@brief
    Array of \c const \bt_enum_fc mapping labels.

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
		const bt_field_class_enumeration_mapping *mapping)
		__BT_NOEXCEPT;

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
    <td>[0,&nbsp;2<sup>64</sup>&nbsp;−&nbsp;1]
  <tr>
    <td>
      \bt_mip version&nbsp;1:
      \ref api-tir-fc-int-prop-hints "field value hints"
      (\bt_avail_since{1})
    <td>\em None
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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
		const bt_integer_range_set_unsigned *ranges) __BT_NOEXCEPT;

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
    isn't modified.
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
		const bt_field_class *field_class, uint64_t index)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the mapping having the label \bt_p{label} from the
    \bt_uenum_fc \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

If there's no mapping having the label \bt_p{label} in
\bt_p{field_class}, then this function returns \c NULL.

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
    isn't modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_uenum_fc{field_class}
@bt_pre_not_null{label}
*/
extern const bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_label_const(
		const bt_field_class *field_class, const char *label)
		__BT_NOEXCEPT;

/*!
@brief
    Returns an array of all the labels of the mappings of the
    \bt_uenum_fc \bt_p{field_class} of which the \bt_p_uint_rg contain
    the integral value \bt_p{value}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

This function sets \bt_p{*labels} to the resulting array and
\bt_p{*count} to the number of labels in \bt_p{*labels}.

On success, if there's no mapping ranges containing the value
\bt_p{value}, \bt_p{*count} is&nbsp;0.

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

    - \bt_p{field_class} isn't modified.
    - You don't call this function again with \bt_p{field_class}.
    @endparblock
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of labels
    in \bt_p{*labels} (can be&nbsp;0).

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
		uint64_t *count) __BT_NOEXCEPT;

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
		const bt_field_class_enumeration_unsigned_mapping *mapping)
		__BT_NOEXCEPT;

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
		__BT_NOEXCEPT
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
    <td>[−2<sup>63</sup>,&nbsp;2<sup>63</sup>&nbsp;−&nbsp;1]
  <tr>
    <td>
      \bt_mip version&nbsp;1:
      \ref api-tir-fc-int-prop-hints "field value hints"
      (\bt_avail_since{1})
    <td>\em None
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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
		const bt_integer_range_set_signed *ranges) __BT_NOEXCEPT;

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
    isn't modified.
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
		const bt_field_class *field_class, uint64_t index)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the mapping having the label \bt_p{label} from the
    \bt_senum_fc \bt_p{field_class}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

If there's no mapping having the label \bt_p{label} in
\bt_p{field_class}, then this function returns \c NULL.

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
    isn't modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_senum_fc{field_class}
@bt_pre_not_null{label}
*/
extern const bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_label_const(
		const bt_field_class *field_class, const char *label)
		__BT_NOEXCEPT;

/*!
@brief
    Returns an array of all the labels of the mappings of the
    \bt_senum_fc \bt_p{field_class} of which the \bt_p_sint_rg contain
    the integral value \bt_p{value}.

See the \ref api-tir-fc-enum-prop-mappings "mappings" property.

This function sets \bt_p{*labels} to the resulting array and
\bt_p{*count} to the number of labels in \bt_p{*labels}.

On success, if there's no mapping ranges containing the value
\bt_p{value}, \bt_p{*count} is&nbsp;0.

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

    - \bt_p{field_class} isn't modified.
    - You don't call this function again with \bt_p{field_class}.
    @endparblock
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of labels
    in \bt_p{*labels} (can be&nbsp;0).

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
		uint64_t *count) __BT_NOEXCEPT;

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
		const bt_field_class_enumeration_signed_mapping *mapping)
		__BT_NOEXCEPT;

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
		__BT_NOEXCEPT
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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name BLOB field class (\bt_avail_since{1})
@{
*/

/*!
@brief
    Creates a \bt_sblob_fc having the length \bt_p{length} from the
    trace class \bt_p{trace_class}.

On success, the returned static BLOB field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-sarray-prop-len "Length"
    <td>\bt_p{length}
  <tr>
    <td>\ref api-tir-fc-blob-prop-media-type "Media type"
    <td><code>application/octet-stream</code>
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a static BLOB field class.
@param[in] length
    Length (number of bytes) of the instances of the static BLOB
    field class to create.

@returns
    New static BLOB field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}

@sa bt_field_class_blob_dynamic_without_length_field_location_create() &mdash;
    Creates a class of dynamic BLOB field without a linked length field.
@sa bt_field_class_blob_dynamic_with_length_field_location_create() &mdash;
    Creates a class of dynamic BLOB field with a linked length field.
*/
extern bt_field_class *bt_field_class_blob_static_create(
		bt_trace_class *trace_class, uint64_t length) __BT_NOEXCEPT;

/*!
@brief
    Creates a \bt_dblob_fc (instances without a linked length field)
    from the trace class \bt_p{trace_class}.

On success, the returned dynamic BLOB field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-dblob-prop-len-fl "Length field location"
    <td>
      \em None
  <tr>
    <td>\ref api-tir-fc-blob-prop-media-type "Media type"
    <td><code>application/octet-stream</code>
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a dynamic BLOB field class.

@returns
    New dynamic BLOB field class reference (without a length field),
    or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}

@sa bt_field_class_blob_dynamic_with_length_field_location_create() &mdash;
    Creates a class of dynamic BLOB field with a linked length field.
*/
extern bt_field_class *
bt_field_class_blob_dynamic_without_length_field_location_create(
		bt_trace_class *trace_class) __BT_NOEXCEPT;

/*!
@brief
    Creates a \bt_dblob_fc (instances with a linked length field)
    having the length \bt_field_loc \bt_p{length_field_location} from
    the trace class \bt_p{trace_class}.

On success, the returned dynamic BLOB field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-dblob-prop-len-fl "Length field location"
    <td>
      \bt_p{length_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
  <tr>
    <td>\ref api-tir-fc-blob-prop-media-type "Media type"
    <td><code>application/octet-stream</code>
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a dynamic BLOB field class.
@param[in] length_field_location
    Length field location of the dynamic BLOB field class to create.

@returns
    New dynamic BLOB field class reference (instances with a linked
    length field), or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{length_field_location}

@sa bt_field_class_blob_dynamic_without_length_field_location_create() &mdash;
    Creates a class of dynamic BLOB field without a linked
    length field.
*/
extern bt_field_class *
bt_field_class_blob_dynamic_with_length_field_location_create(
		bt_trace_class *trace_class,
		const bt_field_location *length_field_location) __BT_NOEXCEPT;

/*!
@brief
    Returns the length of the \bt_sblob_fc \bt_p{field_class}.

See the \ref api-tir-fc-sblob-prop-len "length" property.

@param[in] field_class
    Static BLOB field class of which to get the length (number of bytes
    in instances).

@returns
    Length of \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_sblob_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
*/
extern uint64_t bt_field_class_blob_static_get_length(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the length field location from the \bt_dblob_fc (instances
    with a linked length field) \bt_p{field_class}.

See the \ref api-tir-fc-dblob-prop-len-fl "length field location"
property.

@param[in] field_class
    Dynamic BLOB field class from which to borrow the length
    field location.

@returns
    Length field location of \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_dblob_wl_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
*/
extern const bt_field_location *
bt_field_class_blob_dynamic_with_length_field_borrow_length_field_location_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_field_class_blob_set_media_type().

@bt_since{1}
*/
typedef enum bt_field_class_blob_set_media_type_status {
	/*!
	@brief
	    Success.
	*/
	BT_FIELD_CLASS_BLOB_SET_MEDIA_TYPE_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_FIELD_CLASS_BLOB_SET_MEDIA_TYPE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_field_class_blob_set_media_type_status;

/*!
@brief
    Sets the IANA media type of the \bt_blob_fc \bt_p{field_class}
    to a copy of \bt_p{media_type}.

See the \ref api-tir-fc-blob-prop-media-type "media type" property.

@param[in] field_class
    BLOB field class of which to set the media type to a copy of
    \bt_p{media_type}.
@param[in] media_type
    New media type of \bt_p{field_class} (copied).

@retval #BT_FIELD_CLASS_BLOB_SET_MEDIA_TYPE_STATUS_OK
    Success.
@retval #BT_FIELD_CLASS_BLOB_SET_MEDIA_TYPE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_hot{field_class}
@bt_pre_is_blob_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}

@sa bt_field_class_blob_get_media_type() &mdash;
    Returns the IANA media type of BLOB field class.
*/
extern bt_field_class_blob_set_media_type_status
bt_field_class_blob_set_media_type(bt_field_class *field_class,
		const char *media_type) __BT_NOEXCEPT;

/*!
@brief
    Returns the IANA media type of the \bt_blob_fc \bt_p{field_class}.

See the \ref api-tir-fc-blob-prop-media-type "media type" property.

If \bt_p{field_class} has no media type, then this function
returns \c NULL.

@param[in] field_class
    BLOB field class of which to get the media type.

@returns
    @parblock
    Media type of \bt_p{field_class}.

    The returned pointer remains valid as long as \bt_p{field_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_blob_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}

@sa bt_field_class_blob_set_media_type() &mdash;
    Sets the media type of a BLOB field class.
*/
extern const char *bt_field_class_blob_get_media_type(
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
		bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the element field class from the \bt_array_fc
    \bt_p{field_class} (\c const version).

See bt_field_class_array_borrow_element_field_class().

@param[in] field_class
    See bt_field_class_array_borrow_element_field_class().

@returns
    See bt_field_class_array_borrow_element_field_class().
*/
extern const bt_field_class *
bt_field_class_array_borrow_element_field_class_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
@bt_pre_fc_not_in_tc{element_field_class}
@bt_pre_fc_created_from_tc{element_field_class, trace_class}

@bt_post_success_frozen{element_field_class}
*/
extern bt_field_class *bt_field_class_array_static_create(
		bt_trace_class *trace_class,
		bt_field_class *element_field_class, uint64_t length)
		__BT_NOEXCEPT;

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Dynamic array field class
@{
*/

/*!
@brief
    Creates a \bt_darray_fc having the element field class
    \bt_p{element_field_class} from the trace class \bt_p{trace_class}.

@note
    @parblock
    This function is only available when \bt_p{trace_class} was created
    from a \bt_self_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;0.

    To create a dynamic array field class from a trace class which was
    created from a self component which belongs to a trace processing
    graph with the effective MIP version&nbsp;1 (\bt_avail_since{1}), use
    bt_field_class_array_dynamic_without_length_field_location_create()
    or bt_field_class_array_dynamic_with_length_field_location_create().
    @endparblock

If \bt_p{length_field_class} isn't \c NULL, then an instance of
the created dynamic array field class has a linked length field.
See \ref api-tir-fc-link "Fields with links to other fields"
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
      \em None (if \bt_p{length_field_class} isn't \c NULL, this
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
    Class of a linked length field of an instance of the dynamic array
    field class to create.

    Can be \c NULL.
    @endparblock

@returns
    New dynamic array field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 0}
@bt_pre_not_null{element_field_class}
@bt_pre_fc_not_in_tc{element_field_class}
@bt_pre_fc_created_from_tc{element_field_class, trace_class}
@pre
    <strong>If \bt_p{length_field_class} isn't \c NULL</strong>,
    \bt_p{length_field_class} is an \bt_uint_fc.
@pre
    <strong>If \bt_p{length_field_class} isn't \c NULL</strong>,
    \bt_p{length_field_class} was created from \bt_p{trace_class}.

@bt_post_success_frozen{element_field_class}
@bt_post_success_frozen{length_field_class}

@sa bt_field_class_array_dynamic_without_length_field_location_create() &mdash;
    Creates a class of dynamic array field without a linked length field
    (\bt_mip version&nbsp;1).
@sa bt_field_class_array_dynamic_with_length_field_location_create() &mdash;
    Creates a class of dynamic array field with a linked length field
    (\bt_mip version&nbsp;1).
*/
extern bt_field_class *bt_field_class_array_dynamic_create(
		bt_trace_class *trace_class,
		bt_field_class *element_field_class,
		bt_field_class *length_field_class) __BT_NOEXCEPT;

/*!
@brief
    Creates a \bt_darray_fc (instances without a linked length field)
    having the element field class \bt_p{element_field_class} from the
    trace class \bt_p{trace_class}.

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
    <td>\ref api-tir-fc-darray-prop-len-fl "Length field location"
    <td>
      \em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a dynamic array field class.
@param[in] element_field_class
    Class of the element fields of the instances of the dynamic array
    field class to create.

@returns
    New dynamic array field class reference (without a length field),
    or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{element_field_class}
@bt_pre_fc_not_in_tc{element_field_class}
@bt_pre_fc_created_from_tc{element_field_class, trace_class}

@bt_post_success_frozen{element_field_class}

@sa bt_field_class_array_dynamic_with_length_field_location_create() &mdash;
    Creates a class of dynamic array field with a linked length field.
*/
extern bt_field_class *bt_field_class_array_dynamic_without_length_field_location_create(
		bt_trace_class *trace_class,
		bt_field_class *element_field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Dynamic array field class: instances with linked length field
@{
*/

/*!
@brief
    Creates a \bt_darray_fc (instances with a linked length field)
    having the element field class \bt_p{element_field_class} and the
    length \bt_field_loc \bt_p{length_field_location} from the
    trace class \bt_p{trace_class}.

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
    <td>\ref api-tir-fc-darray-prop-len-fl "Length field location"
    <td>
      \bt_p{length_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a dynamic array field class.
@param[in] element_field_class
    Class of the element fields of the instances of the dynamic array
    field class to create.
@param[in] length_field_location
    Length field location of the dynamic array field class to create.

@returns
    New dynamic array field class reference (instances with a linked
    length field), or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{element_field_class}
@bt_pre_fc_not_in_tc{element_field_class}
@bt_pre_fc_created_from_tc{element_field_class, trace_class}
@bt_pre_not_null{length_field_location}

@bt_post_success_frozen{element_field_class}

@sa bt_field_class_array_dynamic_without_length_field_location_create() &mdash;
    Creates a class of dynamic array field without a linked
    length field.
*/
extern bt_field_class *bt_field_class_array_dynamic_with_length_field_location_create(
		bt_trace_class *trace_class,
		bt_field_class *element_field_class,
		const bt_field_location *length_field_location) __BT_NOEXCEPT;

/*!
@brief
    Borrows the length field path from the \bt_darray_fc (instances with
    a linked length field) \bt_p{field_class}.

See the \ref api-tir-fc-darray-prop-len-fp "length field path" property.

This property is only available when all the following are true:

- \bt_p{field_class} was created with
  bt_field_class_array_dynamic_create() with \bt_p{length_field_class}
  set to an \bt_uint_fc (not \c NULL).

- A \bt_struct_fc containing (recursively) \bt_p{field_class} is passed
  to one of:

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
@bt_pre_fc_with_mip{field_class, 0}
*/
extern const bt_field_path *
bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the length field location from the \bt_darray_fc (instances
    with a linked length field) \bt_p{field_class}.

See the \ref api-tir-fc-darray-prop-len-fl "length field location"
property.

This property is only available when \bt_p{field_class} was created
with bt_field_class_array_dynamic_with_length_field_location_create().

@param[in] field_class
    Dynamic array field class from which to borrow the length
    field location.

@returns
    Length field location of \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_darray_wl_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
*/
extern const bt_field_location *
bt_field_class_array_dynamic_with_length_field_borrow_length_field_location_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
@bt_pre_fc_same_tc{member_field_class, field_class}

@bt_post_success_frozen{member_field_class}
*/
extern bt_field_class_structure_append_member_status
bt_field_class_structure_append_member(
		bt_field_class *field_class,
		const char *name, bt_field_class *member_field_class)
		__BT_NOEXCEPT;

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
    isn't modified.
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
		bt_field_class *field_class, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the member at index \bt_p{index} from the
    \bt_struct_fc \bt_p{field_class} (\c const version).

See bt_field_class_structure_borrow_member_by_index().

@param[in] field_class
    See bt_field_class_structure_borrow_member_by_index().
@param[in] index
    See bt_field_class_structure_borrow_member_by_index().

@returns
    See bt_field_class_structure_borrow_member_by_index().
*/
extern const bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index_const(
		const bt_field_class *field_class, uint64_t index)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the member having the name \bt_p{name} from the
    \bt_struct_fc \bt_p{field_class}.

See the \ref api-tir-fc-struct-prop-members "members" property.

If there's no member having the name \bt_p{name} in
\bt_p{field_class}, then this function returns \c NULL.

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
    isn't modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_structure_borrow_member_by_name_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name(
		bt_field_class *field_class, const char *name) __BT_NOEXCEPT;

/*!
@brief
    Borrows the member having the name \bt_p{name} from the
    \bt_struct_fc \bt_p{field_class} (\c const version).

See bt_field_class_structure_borrow_member_by_name().

@param[in] field_class
    See bt_field_class_structure_borrow_member_by_name().
@param[in] name
    See bt_field_class_structure_borrow_member_by_name().

@returns
    See bt_field_class_structure_borrow_member_by_name().
*/
extern const bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name_const(
		const bt_field_class *field_class, const char *name)
		__BT_NOEXCEPT;

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
		const bt_field_class_structure_member *member) __BT_NOEXCEPT;

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
		bt_field_class_structure_member *member) __BT_NOEXCEPT;

/*!
@brief
    Borrows the field class from the \bt_struct_fc member
    \bt_p{member} (\c const version).

See bt_field_class_structure_member_borrow_field_class().

@param[in] member
    See bt_field_class_structure_member_borrow_field_class().

@returns
    See bt_field_class_structure_member_borrow_field_class().
*/
extern const bt_field_class *
bt_field_class_structure_member_borrow_field_class_const(
		const bt_field_class_structure_member *member) __BT_NOEXCEPT;

/*!
@brief
    Sets the user attributes of the \bt_struct_fc member \bt_p{member}
    to \bt_p{user_attributes}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@note
    When you append a member to a structure field class with
    bt_field_class_structure_append_member(), the
    initial user attributes of the member is an empty \bt_map_val.
    Therefore you can borrow it with
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
		const bt_value *user_attributes) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the \bt_struct_fc member
    \bt_p{member}.

See the \ref api-tir-fc-struct-prop-members "members" property.

@note
    When you append a member to a structure field class with
    bt_field_class_structure_append_member(), the
    initial user attributes of the member is an empty \bt_map_val.

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
		bt_field_class_structure_member *member) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the \bt_struct_fc member
    \bt_p{member} (\c const version).

See bt_field_class_structure_member_borrow_user_attributes().

@param[in] member
    See bt_field_class_structure_member_borrow_user_attributes().

@returns
    See bt_field_class_structure_member_borrow_user_attributes().
*/
extern const bt_value *
bt_field_class_structure_member_borrow_user_attributes_const(
		const bt_field_class_structure_member *member) __BT_NOEXCEPT;

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
		bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the optional field class from the \bt_opt_fc
    \bt_p{field_class} (\c const version).

See bt_field_class_option_borrow_field_class().

@param[in] field_class
    See bt_field_class_option_borrow_field_class().

@returns
    See bt_field_class_option_borrow_field_class().
*/
extern const bt_field_class *
bt_field_class_option_borrow_field_class_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Option field class: instances without a linked selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (instances without a linked selector field)
    having the optional field class \bt_p{optional_field_class} from the
    trace class \bt_p{trace_class}.

@note
    @parblock
    This function is only available when \bt_p{trace_class} was created
    from a \bt_self_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;0.

    To create a similar option field class from a trace class which was
    created from a self component which belongs to a trace processing
    graph with the effective MIP version&nbsp;1 (\bt_avail_since{1}),
    use bt_field_class_option_without_selector_field_location_create().
    @endparblock

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
@bt_pre_tc_with_mip{trace_class, 0}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}

@bt_post_success_frozen{optional_field_class}

@sa bt_field_class_option_without_selector_field_location_create() &mdash;
    Creates a class of option field without a linked selector field
    (\bt_mip version&nbsp;1).
*/
extern bt_field_class *bt_field_class_option_without_selector_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class) __BT_NOEXCEPT;

/*!
@brief
    Creates an \bt_opt_fc (instances without a linked selector
    field) having the optional field class \bt_p{optional_field_class}
    from the trace class \bt_p{trace_class}.

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

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}

@bt_post_success_frozen{optional_field_class}

@sa bt_field_class_option_with_selector_field_location_bool_create() &mdash;
    Creates a class of option field with a linked boolean selector field.
@sa bt_field_class_option_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of option field with a linked unsigned integer
    selector field.
@sa bt_field_class_option_with_selector_field_location_integer_signed_create() &mdash;
    Creates a class of option field with a linked signed integer
    selector field.
*/
extern bt_field_class *bt_field_class_option_without_selector_field_location_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Option field class: instances with a linked selector field
@{
*/

/*!
@brief
    Borrows the selector field path from the \bt_opt_fc (instances
    with a linked selector field) \bt_p{field_class}.

See the \ref api-tir-fc-opt-prop-sel-fp "selector field path" property.

This property is only available when all the following are true:

- \bt_p{field_class} was created with
  bt_field_class_option_with_selector_field_bool_create(),
  bt_field_class_option_with_selector_field_integer_unsigned_create(),
  or bt_field_class_option_with_selector_field_integer_signed_create().

- A \bt_struct_fc containing (recursively) \bt_p{field_class} is passed
  to one of:

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
@bt_pre_fc_with_mip{field_class, 0}
*/
extern const bt_field_path *
bt_field_class_option_with_selector_field_borrow_selector_field_path_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the selector field location from the \bt_opt_fc (instances
    with a linked selector field) \bt_p{field_class}.

See the \ref api-tir-fc-opt-prop-sel-fl "selector field location"
property.

This property is only available when \bt_p{field_class} was created
with bt_field_class_option_with_selector_field_location_bool_create().

@param[in] field_class
    Option field class from which to borrow the selector field location.

@returns
    Selector field location of \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_opt_ws_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
*/
extern const bt_field_location *
bt_field_class_option_with_selector_field_borrow_selector_field_location_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Option field class: instances with a linked boolean selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (instances with a linked boolean selector
    field) having the optional field class \bt_p{optional_field_class}
    from the trace class \bt_p{trace_class}.

@note
    @parblock
    This function is only available when \bt_p{trace_class} was created
    from a \bt_self_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;0.

    To create a similar option field class from a trace class which was
    created from a self component which belongs to a trace processing
    graph with the effective MIP version&nbsp;1 (\bt_avail_since{1}), use
    bt_field_class_option_with_selector_field_location_bool_create().
    @endparblock

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
    Class of a linked boolean selector field of an instance of the
    option field class to create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 0}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}
@bt_pre_not_null{selector_field_class}
@pre
    \bt_p{selector_field_class} is a \bt_bool_fc.
@bt_pre_fc_created_from_tc{selector_field_class, trace_class}

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}

@sa bt_field_class_option_with_selector_field_location_bool_create() &mdash;
    Creates a class of option field with a linked boolean selector field
    (\bt_mip version&nbsp;1).
*/
extern bt_field_class *bt_field_class_option_with_selector_field_bool_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		bt_field_class *selector_field_class) __BT_NOEXCEPT;

/*!
@brief
    Creates an \bt_opt_fc (instances with a linked boolean selector
    field) having the optional field class \bt_p{optional_field_class}
    and the selector \bt_field_loc \bt_p{selector_field_location} from
    the trace class \bt_p{trace_class}.

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
    <td>\ref api-tir-fc-opt-prop-sel-fl "Selector field location"
    <td>
      \bt_p{selector_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
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
@param[in] selector_field_location
    Selector field location of the option field class to create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}
@bt_pre_not_null{selector_field_location}

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}

@sa bt_field_class_option_without_selector_field_location_create() &mdash;
    Creates a class of option field without a linked selector field.
@sa bt_field_class_option_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of option field with a linked unsigned integer
    selector field.
@sa bt_field_class_option_with_selector_field_location_integer_signed_create() &mdash;
    Creates a class of option field with a linked signed integer
    selector field.
*/
extern bt_field_class *bt_field_class_option_with_selector_field_location_bool_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		const bt_field_location *selector_field_location) __BT_NOEXCEPT;

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
		bt_field_class *field_class, bt_bool selector_is_reversed)
		__BT_NOEXCEPT;

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Option field class: instances with a linked unsigned integer selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (instances with a linked unsigned integer
    selector field) having the optional field class
    \bt_p{optional_field_class} from the trace class \bt_p{trace_class}.

@note
    @parblock
    This function is only available when \bt_p{trace_class} was created
    from a \bt_self_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;0.

    To create a similar option field class from a trace class which was
    created from a self component which belongs to a trace processing
    graph with the effective MIP version&nbsp;1 (\bt_avail_since{1}), use
    bt_field_class_option_with_selector_field_location_integer_unsigned_create().
    @endparblock

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
    <td>\ref api-tir-fc-opt-prop-uint-rs "Unsigned integer ranges of selector"
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
    Unsigned integer ranges of selector of the option field class to
    create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 0}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}
@bt_pre_not_null{selector_field_class}
@pre
    \bt_p{selector_field_class} is an \bt_uint_fc.
@bt_pre_fc_created_from_tc{selector_field_class, trace_class}
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more \bt_p_uint_rg.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}
@bt_post_success_frozen{ranges}

@sa bt_field_class_option_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of option field with a linked unsigned integer
    selector field (\bt_mip version&nbsp;1).
*/
extern bt_field_class *
bt_field_class_option_with_selector_field_integer_unsigned_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		bt_field_class *selector_field_class,
		const bt_integer_range_set_unsigned *ranges)
		__BT_NOEXCEPT;

/*!
@brief
    Creates an \bt_opt_fc (instances with a linked unsigned integer
    selector field) having the optional field class
    \bt_p{optional_field_class} and the selector \bt_field_loc
    \bt_p{selector_field_location} from the trace class
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
    <td>\ref api-tir-fc-opt-prop-sel-fl "Selector field location"
    <td>
      \bt_p{selector_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
  <tr>
    <td>\ref api-tir-fc-opt-prop-uint-rs "Unsigned integer ranges of selector"
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
@param[in] selector_field_location
    Selector field location of the option field class to create.
@param[in] ranges
    Unsigned integer ranges of selector of the option field class to
    create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}
@bt_pre_not_null{selector_field_location}
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more \bt_p_uint_rg.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{ranges}

@sa bt_field_class_option_without_selector_field_location_create() &mdash;
    Creates a class of option field without a linked selector field.
@sa bt_field_class_option_with_selector_field_location_bool_create() &mdash;
    Creates a class of option field with a linked boolean selector field.
@sa bt_field_class_option_with_selector_field_location_integer_signed_create() &mdash;
    Creates a class of option field with a linked signed integer
    selector field.
*/
extern bt_field_class *
bt_field_class_option_with_selector_field_location_integer_unsigned_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		const bt_field_location *selector_field_location,
		const bt_integer_range_set_unsigned *ranges) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \bt_p_uint_rg from the \bt_opt_fc (with an unsigned
    integer selector field) \bt_p{field_class}.

See the
\ref api-tir-fc-opt-prop-uint-rs "Unsigned integer ranges of selector"
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
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Option field class: instances with a linked signed integer selector field
@{
*/

/*!
@brief
    Creates an \bt_opt_fc (instances with a linked signed integer
    selector field) having the optional field class
    \bt_p{optional_field_class} from the trace class \bt_p{trace_class}.

@note
    @parblock
    This function is only available when \bt_p{trace_class} was created
    from a \bt_self_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;0.

    To create a similar option field class from a trace class which was
    created from a self component which belongs to a trace processing
    graph with the effective MIP version&nbsp;1 (\bt_avail_since{1}), use
    bt_field_class_option_with_selector_field_location_integer_signed_create().
    @endparblock

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
    <td>\ref api-tir-fc-opt-prop-sint-rs "Signed integer ranges of selector"
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
    Signed integer ranges of selector of the option field class to
    create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 0}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}
@bt_pre_not_null{selector_field_class}
@pre
    \bt_p{selector_field_class} is a \bt_sint_fc.
@bt_pre_fc_created_from_tc{selector_field_class, trace_class}
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more \bt_p_sint_rg.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{selector_field_class}
@bt_post_success_frozen{ranges}

@sa bt_field_class_option_with_selector_field_location_integer_signed_create() &mdash;
    Creates an option field class with a linked signed integer
    selector field (\bt_mip version&nbsp;1).
*/
extern bt_field_class *
bt_field_class_option_with_selector_field_integer_signed_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		bt_field_class *selector_field_class,
		const bt_integer_range_set_signed *ranges) __BT_NOEXCEPT;

/*!
@brief
    Creates an \bt_opt_fc (instances with a linked signed integer
    selector field) having the optional field class
    \bt_p{optional_field_class} and the selector \bt_field_loc
    \bt_p{selector_field_location} from the trace class
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
    <td>\ref api-tir-fc-opt-prop-sel-fl "Selector field location"
    <td>
      \bt_p{selector_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
  <tr>
    <td>\ref api-tir-fc-opt-prop-uint-rs "Signed integer ranges of selector"
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
@param[in] selector_field_location
    Selector field location of the option field class to create.
@param[in] ranges
    Signed integer ranges of selector of the option field class to
    create.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{optional_field_class}
@bt_pre_fc_not_in_tc{optional_field_class}
@bt_pre_fc_created_from_tc{optional_field_class, trace_class}
@bt_pre_not_null{selector_field_location}
@bt_pre_not_null{ranges}
@pre
    \bt_p{ranges} contains one or more \bt_p_uint_rg.

@bt_post_success_frozen{optional_field_class}
@bt_post_success_frozen{ranges}

@sa bt_field_class_variant_without_selector_field_location_create() &mdash;
    Creates a class of option field without a linked selector field.
@sa bt_field_class_option_with_selector_field_location_bool_create() &mdash;
    Creates a class of option field with a linked boolean selector field.
@sa bt_field_class_option_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of option field with a linked unsigned integer
    selector field.
*/
extern bt_field_class *
bt_field_class_option_with_selector_field_location_integer_signed_create(
		bt_trace_class *trace_class,
		bt_field_class *optional_field_class,
		const bt_field_location *selector_field_location,
		const bt_integer_range_set_signed *ranges) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \bt_p_sint_rg from the \bt_opt_fc (with a signed
    integer selector field) \bt_p{field_class}.

See the
\ref api-tir-fc-opt-prop-sint-rs "Signed integer ranges of selector"
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
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Variant field class
@{
*/

/*!
@brief
    Creates an empty \bt_var_fc from the trace class \bt_p{trace_class}.

@note
    @parblock
    This function is only available when \bt_p{trace_class} was created
    from a \bt_self_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;0.

    To create a variant field class from a trace class which was
    created from a self component which belongs to a trace processing
    graph with the effective MIP version&nbsp;1 (\bt_avail_since{1}), use
    bt_field_class_variant_without_selector_field_location_create(),
    bt_field_class_variant_with_selector_field_location_integer_unsigned_create(),
    or bt_field_class_variant_with_selector_field_location_integer_signed_create().
    @endparblock

If \bt_p{selector_field_class} isn't \c NULL, then an instance of
the created variant field class has a linked selector field.
See \ref api-tir-fc-link "Fields with links to other fields"
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
      \em None (if \bt_p{selector_field_class} isn't \c NULL, this
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
    Class of a linked selector field of an instance of the variant field
    class to create.

    Can be \c NULL.
    @endparblock

@returns
    New variant field class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 0}
@pre
    <strong>If \bt_p{selector_field_class} isn't \c NULL</strong>,
    then \bt_p{selector_field_class} is an \bt_int_fc.
@pre
    <strong>If \bt_p{selector_field_class} isn't \c NULL</strong>,
    \bt_p{selector_field_class} was created from \bt_p{trace_class}.

@bt_post_success_frozen{element_field_class}
@bt_post_success_frozen{selector_field_class}

@sa bt_field_class_variant_without_selector_field_location_create() &mdash;
    Creates a class of variant field without a linked selector field
    (\bt_mip version&nbsp;1).
@sa bt_field_class_variant_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of variant field with a linked unsigned integer
    selector field (\bt_mip version&nbsp;1).
@sa bt_field_class_variant_with_selector_field_location_integer_signed_create() &mdash;
    Creates a class of variant field with a linked signed integer
    selector field (\bt_mip version&nbsp;1).
*/
extern bt_field_class *bt_field_class_variant_create(
		bt_trace_class *trace_class,
		bt_field_class *selector_field_class) __BT_NOEXCEPT;

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
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
    isn't modified.
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
		bt_field_class *field_class, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc \bt_p{field_class} (\c const version).

See bt_field_class_variant_borrow_option_by_index().

@param[in] field_class
    See bt_field_class_variant_borrow_option_by_index().
@param[in] index
    See bt_field_class_variant_borrow_option_by_index().

@returns
    See bt_field_class_variant_borrow_option_by_index().
*/
extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index_const(
		const bt_field_class *field_class, uint64_t index)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

If there's no option having the name \bt_p{name} in
\bt_p{field_class}, then this function returns \c NULL.

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
    isn't modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_variant_borrow_option_by_name_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name(
		bt_field_class *field_class, const char *name) __BT_NOEXCEPT;

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc \bt_p{field_class} (\c const version).

See bt_field_class_variant_borrow_option_by_name().

@param[in] field_class
    See bt_field_class_variant_borrow_option_by_name().
@param[in] name
    See bt_field_class_variant_borrow_option_by_name().

@returns
    See bt_field_class_variant_borrow_option_by_name().
*/
extern const bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name)
		__BT_NOEXCEPT;

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

This function may return \c NULL when the following are true:

- The variant field class containing \bt_p{option} was created from a
  \bt_trace_cls which was created from a \bt_comp which belongs
  to a trace processing \bt_graph with the effective
  \bt_mip version&nbsp;1 (\bt_avail_since{1}).

- \bt_p{option} has no name.

@param[in] option
    Variant field class option of which to get the name.

@returns
    @parblock
    Name of \bt_p{option}, or \c NULL if none
    (possible under MIP&nbsp;1; \bt_avail_since{1}).

    The returned pointer remains valid as long as \bt_p{option} exists.
    @endparblock

@bt_pre_not_null{option}
*/
extern const char *bt_field_class_variant_option_get_name(
		const bt_field_class_variant_option *option) __BT_NOEXCEPT;

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
		bt_field_class_variant_option *option) __BT_NOEXCEPT;

/*!
@brief
    Borrows the field class from the \bt_var_fc option
    \bt_p{option} (\c const version).

See bt_field_class_variant_option_borrow_field_class().

@param[in] option
    See bt_field_class_variant_option_borrow_field_class().

@returns
    See bt_field_class_variant_option_borrow_field_class().
*/
extern const bt_field_class *
bt_field_class_variant_option_borrow_field_class_const(
		const bt_field_class_variant_option *option) __BT_NOEXCEPT;

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
    the initial user attributes of the option is an empty \bt_map_val.
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
		const bt_value *user_attributes) __BT_NOEXCEPT;

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
    the initial user attributes of the option is an empty \bt_map_val.

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
		bt_field_class_variant_option *option) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the \bt_var_fc option \bt_p{option}
    (\c const version).

See bt_field_class_variant_option_borrow_user_attributes().

@param[in] option
    See bt_field_class_variant_option_borrow_user_attributes().

@returns
    See bt_field_class_variant_option_borrow_user_attributes().
*/
extern const bt_value *bt_field_class_variant_option_borrow_user_attributes_const(
		const bt_field_class_variant_option *option) __BT_NOEXCEPT;

/*! @} */

/*!
@name Variant field class: instances without a linked selector field
@{
*/

/*!
@brief
    Creates an empty \bt_var_fc (instances without a linked
    selector field) from the trace class \bt_p{trace_class}.

On success, the returned variant field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-var-prop-sel-fp "Selector field location"
    <td>
      \em None
  <tr>
    <td>\ref api-tir-fc-var-prop-opts "Options"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a variant field class.

@returns
    New option field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}

@sa bt_field_class_variant_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of variant field with a linked unsigned integer
    selector field.
@sa bt_field_class_variant_with_selector_field_location_integer_signed_create() &mdash;
    Creates a class of variant field with a linked signed integer
    selector field.
*/
extern bt_field_class *bt_field_class_variant_without_selector_field_location_create(
		bt_trace_class *trace_class) __BT_NOEXCEPT;

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
    Appends an option to the \bt_var_fc (instances without a linked
    selector field) \bt_p{field_class} having the name \bt_p{name} and
    the field class \bt_p{option_field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class to which to append an option having
    the name \bt_p{name} and the field class \bt_p{option_field_class}.
@param[in] name
    @parblock
    Name of the option to append to \bt_p{field_class} (copied).

    Can be \c NULL when the effective \bt_mip version of the trace
    processing \bt_graph is&nbsp;1 (\bt_avail_since{1}).
    @endparblock
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
    <strong>If \bt_p{name} isn't \c NULL</strong>, then
    \bt_p{field_class} has no option with the name \bt_p{name}.
@pre
    <strong>If \bt_p{field_class} was created from a
    \bt_trace_cls which was created from a \bt_comp which belongs
    to a trace processing \bt_graph with the effective \bt_mip
    version&nbsp;0</strong>, then \bt_p{name} is \em not \c NULL.
@bt_pre_not_null{option_field_class}
@bt_pre_fc_not_in_tc{option_field_class}
@bt_pre_fc_same_tc{option_field_class, field_class}

@bt_post_success_frozen{option_field_class}
*/
extern bt_field_class_variant_without_selector_append_option_status
bt_field_class_variant_without_selector_append_option(
		bt_field_class *field_class, const char *name,
		bt_field_class *option_field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Variant field class: instances with a linked selector field
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
    Borrows the selector field path from the \bt_var_fc (instances with
    a linked selector field) \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-sel-fp "selector field path" property.

This property is only available when all the following are true:

- \bt_p{field_class} was created with
  bt_field_class_variant_create() with \bt_p{selector_field_class}
  set to an \bt_int_fc (not \c NULL).

- A \bt_struct_fc containing (recursively) \bt_p{field_class} is passed
  to one of:

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
@bt_pre_fc_with_mip{field_class, 0}
*/
extern const bt_field_path *
bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the selector field location from the \bt_var_fc (instances
    with a linked selector field) \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-sel-fl "selector field location"
property.

This property is only available when \bt_p{field_class} was created with
bt_field_class_variant_with_selector_field_location_integer_unsigned_create()
or
bt_field_class_variant_with_selector_field_location_integer_signed_create().

@param[in] field_class
    Option field class from which to borrow the selector field location.

@returns
    Selector field location of \bt_p{field_class}.

@bt_since{1}

@bt_pre_not_null{field_class}
@bt_pre_is_var_ws_fc{field_class}
@bt_pre_fc_with_mip{field_class, 1}
*/
extern const bt_field_location *
bt_field_class_variant_with_selector_field_borrow_selector_field_location_const(
		const bt_field_class *field_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Variant field class: instances with a linked unsigned integer selector field
@{
*/

/*!
@brief
    Creates an empty \bt_var_fc (instances with a linked
    unsigned integer selector field) having the
    selector \bt_field_loc \bt_p{selector_field_location}
    from the trace class \bt_p{trace_class}.

On success, the returned variant field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-var-prop-sel-fp "Selector field location"
    <td>
      \bt_p{selector_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
  <tr>
    <td>\ref api-tir-fc-var-prop-opts "Options"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a variant field class.
@param[in] selector_field_location
    Selector field location of the variant field class to create.

@returns
    New variant field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{selector_field_location}

@sa bt_field_class_variant_without_selector_field_location_create() &mdash;
    Creates a class of variant field without a linked selector field.
@sa bt_field_class_option_with_selector_field_location_integer_signed_create() &mdash;
    Creates a class of variant field with a linked signed integer
    selector field.
*/
extern bt_field_class *bt_field_class_variant_with_selector_field_location_integer_unsigned_create(
		bt_trace_class *trace_class,
		const bt_field_location *selector_field_location) __BT_NOEXCEPT;

/*!
@brief
    Appends an option to the \bt_var_fc (instances with a linked
    unsigned integer selector field) \bt_p{field_class} having the name
    \bt_p{name}, the field class \bt_p{option_field_class}, and the
    \bt_p_uint_rg \bt_p{ranges}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class to which to append an option having
    the name \bt_p{name}, the field class \bt_p{option_field_class},
    and the unsigned integer ranges \bt_p{ranges}.
@param[in] name
    @parblock
    Name of the option to append to \bt_p{field_class} (copied).

    Can be \c NULL when the effective \bt_mip version of the trace
    processing \bt_graph is&nbsp;1 (\bt_avail_since{1}).
    @endparblock
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
    <strong>If \bt_p{name} isn't \c NULL</strong>, then
    \bt_p{field_class} has no option with the name \bt_p{name}.
@pre
    <strong>If \bt_p{field_class} was created from a
    \bt_trace_cls which was created from a \bt_comp which belongs
    to a trace processing \bt_graph with the effective \bt_mip
    version&nbsp;0</strong>, then \bt_p{name} is \em not \c NULL.
@bt_pre_not_null{option_field_class}
@bt_pre_fc_not_in_tc{option_field_class}
@bt_pre_fc_same_tc{option_field_class, field_class}
@bt_pre_not_null{ŗanges}
@pre
    \bt_p{ranges} contains one or more unsigned integer ranges.
@pre
    The unsigned integer ranges in \bt_p{ranges} don't overlap
    any unsigned integer range of any existing option in
    \bt_p{field_class}.

@bt_post_success_frozen{option_field_class}
*/
extern bt_field_class_variant_with_selector_field_integer_append_option_status
bt_field_class_variant_with_selector_field_integer_unsigned_append_option(
		bt_field_class *field_class, const char *name,
		bt_field_class *option_field_class,
		const bt_integer_range_set_unsigned *ranges) __BT_NOEXCEPT;

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc (instances with a linked unsigned integer selector field)
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
    isn't modified.
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
		const bt_field_class *field_class, uint64_t index)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc (instances with a linked unsigned integer selector field)
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

If there's no option having the name \bt_p{name} in
\bt_p{field_class}, then this function returns \c NULL.

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
    isn't modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_wuis_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_variant_borrow_option_by_name_const() &mdash;
    Borrows an option by name from a variant field class.
*/
extern const bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Variant field class option: instances with a linked unsigned integer selector field
@{
*/

/*!
@typedef struct bt_field_class_variant_with_selector_field_integer_unsigned_option bt_field_class_variant_with_selector_field_integer_unsigned_option;

@brief
    Variant field class option (instances with a linked unsigned integer selector field).
*/

/*!
@brief
    Borrows the \bt_p_uint_rg from the \bt_var_fc option
    (instances with a linked unsigned integer selector field)
    \bt_p{option}.

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
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *option)
		__BT_NOEXCEPT;

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_var_fc option (instances
    with a linked unsigned integer selector field) \bt_p{option} to the
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
		__BT_NOEXCEPT
{
	return __BT_UPCAST_CONST(bt_field_class_variant_option, option);
}

/*! @} */

/*!
@name Variant field class: instances with a linked signed integer selector field
@{
*/

/*!
@brief
    Creates an empty \bt_var_fc (instances with a linked
    signed integer selector field) having the
    selector \bt_field_loc \bt_p{selector_field_location}
    from the trace class \bt_p{trace_class}.

On success, the returned variant field class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-fc-var-prop-sel-fp "Selector field location"
    <td>
      \bt_p{selector_field_location}.

      See \ref api-tir-fc-link "Fields with links to other fields"
      to learn more.
  <tr>
    <td>\ref api-tir-fc-var-prop-opts "Options"
    <td>\em None
  <tr>
    <td>\ref api-tir-fc-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create a variant field class.
@param[in] selector_field_location
    Selector field location of the variant field class to create.

@returns
    New variant field class reference, or \c NULL on memory error.

@bt_since{1}

@bt_pre_not_null{trace_class}
@bt_pre_tc_with_mip{trace_class, 1}
@bt_pre_not_null{selector_field_location}

@sa bt_field_class_variant_without_selector_field_location_create() &mdash;
    Creates a class of variant field without a linked selector field.
@sa bt_field_class_option_with_selector_field_location_integer_unsigned_create() &mdash;
    Creates a class of variant field with a linked unsigned integer
    selector field.
*/
extern bt_field_class *bt_field_class_variant_with_selector_field_location_integer_signed_create(
		bt_trace_class *trace_class,
		const bt_field_location *selector_field_location) __BT_NOEXCEPT;

/*!
@brief
    Appends an option to the \bt_var_fc (instances with a linked signed
    integer selector field) \bt_p{field_class} having the name
    \bt_p{name}, the field class \bt_p{option_field_class}, and the
    \bt_p_sint_rg \bt_p{ranges}.

See the \ref api-tir-fc-var-prop-opts "options" property.

@param[in] field_class
    Variant field class to which to append an option having
    the name \bt_p{name} and the field class \bt_p{option_field_class},
    and the signed integer ranges \bt_p{ranges}.
@param[in] name
    @parblock
    Name of the option to append to \bt_p{field_class} (copied).

    Can be \c NULL when the effective \bt_mip version of the trace
    processing \bt_graph is&nbsp;1 (\bt_avail_since{1}).
    @endparblock
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
    <strong>If \bt_p{name} isn't \c NULL</strong>, then
    \bt_p{field_class} has no option with the name \bt_p{name}.
@pre
    <strong>If \bt_p{field_class} was created from a
    \bt_trace_cls which was created from a \bt_comp which belongs
    to a trace processing \bt_graph with the effective \bt_mip
    version&nbsp;0</strong>, then \bt_p{name} is \em not \c NULL.
@bt_pre_not_null{option_field_class}
@bt_pre_fc_not_in_tc{option_field_class}
@bt_pre_fc_same_tc{option_field_class, field_class}
@bt_pre_not_null{ŗanges}
@pre
    \bt_p{ranges} contains one or more signed integer ranges.
@pre
    The signed integer ranges in \bt_p{ranges} don't overlap with
    any signed integer range of any existing option in
    \bt_p{field_class}.

@bt_post_success_frozen{option_field_class}
*/
extern bt_field_class_variant_with_selector_field_integer_append_option_status
bt_field_class_variant_with_selector_field_integer_signed_append_option(
		bt_field_class *field_class, const char *name,
		bt_field_class *option_field_class,
		const bt_integer_range_set_signed *ranges)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the option at index \bt_p{index} from the
    \bt_var_fc (instances with a linked signed integer selector field)
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
    isn't modified.
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
		const bt_field_class *field_class, uint64_t index)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the option having the name \bt_p{name} from the
    \bt_var_fc (instances with a linked signed integer selector field)
    \bt_p{field_class}.

See the \ref api-tir-fc-var-prop-opts "options" property.

If there's no option having the name \bt_p{name} in
\bt_p{field_class}, then this function returns \c NULL.

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
    isn't modified.
    @endparblock

@bt_pre_not_null{field_class}
@bt_pre_is_var_wsis_fc{field_class}
@bt_pre_not_null{name}

@sa bt_field_class_variant_borrow_option_by_name_const() &mdash;
    Borrows an option by name from a variant field class.
*/
extern const bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const(
		const bt_field_class *field_class, const char *name)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Variant field class option: instances with a linked signed integer selector field
@{
*/

/*!
@typedef struct bt_field_class_variant_with_selector_field_integer_signed_option bt_field_class_variant_with_selector_field_integer_signed_option;

@brief
    Variant field class option (instances with a linked signed integer selector field).
*/

/*!
@brief
    Borrows the \bt_p_sint_rg from the \bt_var_fc option
    (instances with a linked signed integer selector field)
    \bt_p{option}.

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
		const bt_field_class_variant_with_selector_field_integer_signed_option *option)
		__BT_NOEXCEPT;

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_var_fc option
    (instances with a linked signed integer selector field)
    \bt_p{option} to the common #bt_field_class_variant_option type.

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
		__BT_NOEXCEPT
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
extern void bt_field_class_get_ref(
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
extern void bt_field_class_put_ref(
		const bt_field_class *field_class) __BT_NOEXCEPT;

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
