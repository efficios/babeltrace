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

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_bool */
#include <babeltrace/types.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctfirfieldtypes CTF IR field types
@ingroup ctfir
@brief CTF IR field types.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>field type</em></strong> is a field type that you
can use to create concrete @fields.

You can create a @field object from a CTF IR field type object
with bt_field_create().

In the CTF IR hierarchy, you can set the root field types of three
objects:

- \ref ctfirtraceclass
  - Trace packet header field type: bt_trace_set_packet_header_field_type().
- \ref ctfirstreamclass
  - Stream packet context field type:
    bt_stream_class_set_packet_context_field_type().
  - Stream event header field type:
    bt_stream_class_set_event_header_field_type().
  - Stream event context field type:
    bt_stream_class_set_event_context_field_type().
- \ref ctfireventclass
  - Event context field type: bt_event_class_set_context_field_type().
  - Event payload field type: bt_event_class_set_payload_field_type().

As of Babeltrace \btversion, those six previous "root" field types
\em must be @structft objects.

If, at any level within a given root field type, you add a @seqft or a
@varft, you do not need to specify its associated length
or tag field type: the length or tag string is enough for the Babeltrace
system to resolve the needed field type depending on where this
dynamic field type is located within the whole hierarchy. It is
guaranteed that this automatic resolving is performed for all the field
types contained in a given
\link ctfirstreamclass CTF IR stream class\endlink (and in its
children \link ctfireventclass CTF IR event classes\endlink) once you
add it to a \link ctfirtraceclass CTF IR trace class\endlink with
bt_trace_add_stream_class(). Once a stream class is the child of
a trace class, this automatic resolving is performed for the field
types of an event class when you add it with
bt_stream_class_add_event_class(). If the system cannot find a path
to a field in the hierarchy for a dynamic field type, the adding
function fails.

The standard CTF field types are:

<table>
  <tr>
    <th>Type ID
    <th>CTF IR field type
    <th>CTF IR field which you can create from this field type
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_INTEGER
    <td>\ref ctfirintfieldtype
    <td>\ref ctfirintfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_FLOAT
    <td>\ref ctfirfloatfieldtype
    <td>\ref ctfirfloatfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_ENUM
    <td>\ref ctfirenumfieldtype
    <td>\ref ctfirenumfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_STRING
    <td>\ref ctfirstringfieldtype
    <td>\ref ctfirstringfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_STRUCT
    <td>\ref ctfirstructfieldtype
    <td>\ref ctfirstructfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_ARRAY
    <td>\ref ctfirarrayfieldtype
    <td>\ref ctfirarrayfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_SEQUENCE
    <td>\ref ctfirseqfieldtype
    <td>\ref ctfirseqfield
  </tr>
  <tr>
    <td>#BT_FIELD_TYPE_ID_VARIANT
    <td>\ref ctfirvarfieldtype
    <td>\ref ctfirvarfield
  </tr>
</table>

Each field type has its own <strong>type ID</strong> (see
#bt_field_type_id). You get the type ID of a field type object
with bt_field_type_get_type_id().

You can get a deep copy of a field type with bt_field_type_copy().
This function resets, in the field type copy, the resolved field type
of the dynamic field types. The automatic resolving can be done again
when you eventually call bt_event_create(),
bt_stream_class_add_event_class(), or
bt_trace_add_stream_class().

You \em must always use bt_field_type_compare() to compare two
field types. Since some parts of the Babeltrace system can copy field
types behind the scenes, you \em cannot rely on a simple field type
pointer comparison.

As with any Babeltrace object, CTF IR field type objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

The following functions can \em freeze field type objects:

- bt_field_create() freezes its field type parameter.
- bt_stream_class_add_event_class(), if its
  \link ctfirstreamclass CTF IR stream class\endlink parameter has a
  \link ctfirtraceclass CTF IR trace class\endlink parent, freezes
  the root field types of its
  \link ctfireventclass CTF IR event class\endlink parameter.
- bt_trace_add_stream_class() freezes the root field types of the
  whole trace class hierarchy (trace class, children stream classes,
  and their children event classes).
- bt_writer_create_stream() freezes the root field types of the
  whole CTF writer's trace class hierarchy.
- bt_event_create() freezes the root field types of its event class
  parameter and of ther parent stream class of this event class.

You cannot modify a frozen field type object: it is considered
immutable, except for \link refs reference counting\endlink.

@sa ctfirfields
@sa \ref ctfirfieldtypesexamples "Examples"

@file
@brief CTF IR field types type and functions.
@sa ctfirfieldtypes

@addtogroup ctfirfieldtypes
@{
*/

/**
@struct bt_field_type
@brief A CTF IR field type.
@sa ctfirfieldtypes
*/
struct bt_field_type;
struct bt_event_class;
struct bt_event;
struct bt_field;
struct bt_field_path;
struct bt_field_type_enumeration_mapping_iterator;

/**
@brief	CTF scope.
*/
enum bt_scope {
	/// Unknown, used for errors.
	BT_SCOPE_UNKNOWN		= -1,

	/// Trace packet header.
	BT_SCOPE_TRACE_PACKET_HEADER	= 1,

	/// Stream packet context.
	BT_SCOPE_STREAM_PACKET_CONTEXT	= 2,

	/// Stream event header.
	BT_SCOPE_STREAM_EVENT_HEADER	= 3,

	/// Stream event context.
	BT_SCOPE_STREAM_EVENT_CONTEXT	= 4,

	/// Event context.
	BT_SCOPE_EVENT_CONTEXT		= 5,

	/// Event payload.
	BT_SCOPE_EVENT_PAYLOAD		= 6,

	/// @cond DOCUMENT
	BT_SCOPE_ENV			= 0,
	BT_SCOPE_EVENT_FIELDS		= 6,
	/// @endcond
};

/**
@name Type information
@{
*/

/**
@brief	Type ID of a @ft.
*/
enum bt_field_type_id {
	/// Unknown, used for errors.
	BT_FIELD_TYPE_ID_UNKNOWN	= -1,

	/// \ref ctfirintfieldtype
	BT_FIELD_TYPE_ID_INTEGER	= 0,

	/// \ref ctfirfloatfieldtype
	BT_FIELD_TYPE_ID_FLOAT		= 1,

	/// \ref ctfirenumfieldtype
	BT_FIELD_TYPE_ID_ENUM		= 2,

	/// \ref ctfirstringfieldtype
	BT_FIELD_TYPE_ID_STRING		= 3,

	/// \ref ctfirstructfieldtype
	BT_FIELD_TYPE_ID_STRUCT		= 4,

	/// \ref ctfirarrayfieldtype
	BT_FIELD_TYPE_ID_ARRAY		= 6,

	/// \ref ctfirseqfieldtype
	BT_FIELD_TYPE_ID_SEQUENCE	= 7,

	/// \ref ctfirvarfieldtype
	BT_FIELD_TYPE_ID_VARIANT	= 5,

	/// Number of enumeration entries.
	BT_FIELD_TYPE_ID_NR		= 8,
};

/**
@brief	Returns the type ID of the @ft \p field_type.

@param[in] field_type	Field type of which to get the type ID.
@returns		Type ID of \p field_type,
			or #BT_FIELD_TYPE_ID_UNKNOWN on error.

@prenotnull{field_type}
@postrefcountsame{field_type}

@sa #bt_field_type_id: CTF IR field type ID.
@sa bt_field_type_is_integer(): Returns whether or not a given
	field type is a @intft.
@sa bt_field_type_is_floating_point(): Returns whether or not a
	given field type is a @floatft.
@sa bt_field_type_is_enumeration(): Returns whether or not a given
	field type is a @enumft.
@sa bt_field_type_is_string(): Returns whether or not a given
	field type is a @stringft.
@sa bt_field_type_is_structure(): Returns whether or not a given
	field type is a @structft.
@sa bt_field_type_is_array(): Returns whether or not a given
	field type is a @arrayft.
@sa bt_field_type_is_sequence(): Returns whether or not a given
	field type is a @seqft.
@sa bt_field_type_is_variant(): Returns whether or not a given
	field type is a @varft.
*/
extern enum bt_field_type_id bt_field_type_get_type_id(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @intft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is an integer field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@prenotnull{field_type}
@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_integer(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @floatft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is a floating point
			#BT_FALSE field type,
			or 0 otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_floating_point(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @enumft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is an enumeration field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_enumeration(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @stringft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is a string field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_string(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @structft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is a structure field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_structure(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @arrayft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is an array field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_array(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @seqft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is a sequence field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_sequence(
		struct bt_field_type *field_type);

/**
@brief	Returns whether or not the @ft \p field_type is a @varft.

@param[in] field_type	Field type to check (can be \c NULL).
@returns		#BT_TRUE if \p field_type is a variant field type,
			or #BT_FALSE otherwise (including if \p field_type is
			\c NULL).

@postrefcountsame{field_type}

@sa bt_field_type_get_type_id(): Returns the type ID of a given
	field type.
*/
extern bt_bool bt_field_type_is_variant(
		struct bt_field_type *field_type);

/** @} */

/**
@name Common properties types and functions
@{
*/

/**
@brief	<a href="https://en.wikipedia.org/wiki/Endianness">Byte order</a>
	of a @ft.
*/
enum bt_byte_order {
	/// Unknown, used for errors.
	BT_BYTE_ORDER_UNKNOWN	= -1,

	/*
	 * Note that native, in the context of the CTF specification, is defined
	 * as "the byte order described in the trace" and does not mean that the
	 * host's endianness will be used.
	 */
	/// Native (default) byte order.
	BT_BYTE_ORDER_NATIVE	= 0,

	/**
	Unspecified byte order; the initial native byte order of a
	\link ctfirtraceclass CTF IR trace class\endlink.
	*/
	BT_BYTE_ORDER_UNSPECIFIED,

	/// Little-endian.
	BT_BYTE_ORDER_LITTLE_ENDIAN,

	/// Big-endian.
	BT_BYTE_ORDER_BIG_ENDIAN,

	/// Network byte order (big-endian).
	BT_BYTE_ORDER_NETWORK,
};

/**
@brief	String encoding of a @ft.
*/
enum bt_string_encoding {
	/// Unknown, used for errors.
	BT_STRING_ENCODING_UNKNOWN	= -1,

	/// No encoding.
	BT_STRING_ENCODING_NONE,

	/// <a href="https://en.wikipedia.org/wiki/UTF-8">UTF-8</a>.
	BT_STRING_ENCODING_UTF8,

	/// <a href="https://en.wikipedia.org/wiki/ASCII">ASCII</a>.
	BT_STRING_ENCODING_ASCII,
};

/**
@brief  Returns the alignment of the @fields described by
	the @ft \p field_type.

@param[in] field_type	Field type which describes the
			fields of which to get the alignment.
@returns		Alignment of the fields described by
			\p field_type, or a negative value on error.

@prenotnull{field_type}
@postrefcountsame{field_type}

@sa bt_field_type_set_alignment(): Sets the alignment
	of the fields described by a given field type.
*/
extern int bt_field_type_get_alignment(
		struct bt_field_type *field_type);

/**
@brief  Sets the alignment of the @fields described by the
	@ft \p field_type to \p alignment.

\p alignment \em must be greater than 0 and a power of two.

@param[in] field_type	Field type which describes the fields of
			which to set the alignment.
@param[in] alignment	Alignment of the fields described by
			\p field_type.
@returns		0 on success, or a negative value on error.

@prenotnull{field_type}
@prehot{field_type}
@pre \p alignment is greater than 0 and a power of two.
@postrefcountsame{field_type}

@sa bt_field_type_get_alignment(): Returns the alignment of the
	fields described by a given field type.
*/
extern int bt_field_type_set_alignment(struct bt_field_type *field_type,
		unsigned int alignment);

/**
@brief  Returns the byte order of the @fields described by
	the @ft \p field_type.

You can only call this function if \p field_type is a @intft, a
@floatft, or a @enumft.

@param[in] field_type	Field type which describes the
			fields of which to get the byte order.
@returns		Byte order of the fields described by
			\p field_type, or #BT_BYTE_ORDER_UNKNOWN on
			error.

@prenotnull{field_type}
@pre \p field_type is a @intft, a @floatft, or a @enumft.
@postrefcountsame{field_type}

@sa bt_field_type_set_byte_order(): Sets the byte order
	of the fields described by a given field type.
*/
extern enum bt_byte_order bt_field_type_get_byte_order(
		struct bt_field_type *field_type);

/**
@brief  Sets the byte order of the @fields described by the
	@ft \p field_type to \p byte_order.

If \p field_type is a compound field type, this function also
recursively sets the byte order of its children to \p byte_order.

@param[in] field_type	Field type which describes the fields of
			which to set the byte order.
@param[in] byte_order	Alignment of the fields described by
			\p field_type.
@returns		0 on success, or a negative value on error.

@prenotnull{field_type}
@prehot{field_type}
@pre \p byte_order is #BT_BYTE_ORDER_NATIVE,
	#BT_BYTE_ORDER_LITTLE_ENDIAN, #BT_BYTE_ORDER_BIG_ENDIAN,
	or #BT_BYTE_ORDER_NETWORK.
@postrefcountsame{field_type}

@sa bt_field_type_get_byte_order(): Returns the byte order of the
	fields described by a given field type.
*/
extern int bt_field_type_set_byte_order(
		struct bt_field_type *field_type,
		enum bt_byte_order byte_order);

/** @} */

/**
@name Utility functions
@{
*/

/**
@brief	Returns whether or not the @ft \p field_type_a
	is equivalent to the field type \p field_type_b.

You \em must use this function to compare two field types: it is not
safe to compare two pointer values directly, because, for internal
reasons, some parts of the Babeltrace system can copy user field types
and discard the original ones.

@param[in] field_type_a	Field type to compare to \p field_type_b.
@param[in] field_type_b Field type to compare to \p field_type_a.
@returns		0 if \p field_type_a is equivalent to
			\p field_type_b, 1 if they are not equivalent,
			or a negative value on error.

@prenotnull{field_type_a}
@prenotnull{field_type_b}
@postrefcountsame{field_type_a}
@postrefcountsame{field_type_b}
*/
extern int bt_field_type_compare(struct bt_field_type *field_type_a,
		struct bt_field_type *field_type_b);

/**
@brief	Creates a \em deep copy of the @ft \p field_type.

You can copy a frozen field type: the resulting copy is
<em>not frozen</em>.

This function resets the tag field type of a copied @varft. The
automatic field resolving which some functions of the API perform
can set it again when the returned field type is used (learn more
in the detailed description of this module).

@param[in] field_type	Field type to copy.
@returns		Deep copy of \p field_type on success,
			or \c NULL on error.

@prenotnull{field_type}
@postrefcountsame{field_type}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned field type is not frozen.
*/
extern struct bt_field_type *bt_field_type_copy(
		struct bt_field_type *field_type);

/** @} */

/** @} */

/**
@defgroup ctfirintfieldtype CTF IR integer field type
@ingroup ctfirfieldtypes
@brief CTF IR integer field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>integer field type</em></strong> is a field type that
you can use to create concrete @intfield objects.

You can create an integer field type
with bt_field_type_integer_create().

An integer field type has the following properties:

<table>
  <tr>
    <th>Property
    <th>Value at creation
    <th>Getter
    <th>Setter
  </tr>
  <tr>
    <td>\b Alignment (bits) of the described integer fields
    <td>1
    <td>bt_field_type_get_alignment()
    <td>bt_field_type_set_alignment()
  </tr>
  <tr>
    <td><strong>Byte order</strong> of the described integer fields
    <td>#BT_BYTE_ORDER_NATIVE
    <td>bt_field_type_get_byte_order()
    <td>bt_field_type_set_byte_order()
  </tr>
  <tr>
    <td><strong>Storage size</strong> (bits) of the described
        integer fields
    <td>Specified at creation
    <td>bt_field_type_integer_get_size()
    <td>bt_field_type_integer_set_size()
  </tr>
  <tr>
    <td><strong>Signedness</strong> of the described integer fields
    <td>Unsigned
    <td>bt_field_type_integer_is_signed()
    <td>bt_field_type_integer_set_is_signed()
  </tr>
  <tr>
    <td><strong>Preferred display base</strong> of the described
        integer fields
    <td>#BT_INTEGER_BASE_DECIMAL
    <td>bt_field_type_integer_get_base()
    <td>bt_field_type_integer_set_base()
  </tr>
  <tr>
    <td>\b Encoding of the described integer fields
    <td>#BT_STRING_ENCODING_NONE
    <td>bt_field_type_integer_get_encoding()
    <td>bt_field_type_integer_set_encoding()
  </tr>
  <tr>
    <td><strong>Mapped
        \link ctfirclockclass CTF IR clock class\endlink</strong>
    <td>None
    <td>bt_field_type_integer_get_mapped_clock_class()
    <td>bt_field_type_integer_set_mapped_clock_class()
  </tr>
</table>

@sa ctfirintfield
@sa ctfirfieldtypes
@sa \ref ctfirfieldtypesexamples_intfieldtype "Examples"

@addtogroup ctfirintfieldtype
@{
*/

/**
@brief	Preferred display base (radix) of a @intft.
*/
enum bt_integer_base {
	/// Unknown, used for errors.
	BT_INTEGER_BASE_UNKNOWN		= -1,

	/// Unspecified by the tracer.
	BT_INTEGER_BASE_UNSPECIFIED	= 0,

	/// Binary.
	BT_INTEGER_BASE_BINARY		= 2,

	/// Octal.
	BT_INTEGER_BASE_OCTAL		= 8,

	/// Decimal.
	BT_INTEGER_BASE_DECIMAL		= 10,

	/// Hexadecimal.
	BT_INTEGER_BASE_HEXADECIMAL	= 16,
};

/**
@brief  Creates a default @intft with \p size bits as the storage size
	of the @intfields it describes.

You can change the storage size of the integer fields described by
the created integer field type later with
bt_field_type_integer_set_size().

@param[in] size	Storage size (bits) of the described integer fields.
@returns	Created integer field type, or \c NULL on error.

@pre \p size is greater than 0 and lesser than or equal to 64.
@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_integer_create(
		unsigned int size);

/**
@brief	Returns the storage size, in bits, of the @intfields
	described by the @intft \p int_field_type.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to get the
				storage size.
@returns			Storage size (bits) of the integer
				fields described by \p int_field_type,
				or a negative value on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_set_size(): Sets the storage size of the
	integer fields described by a given integer field type.
*/
extern int bt_field_type_integer_get_size(
		struct bt_field_type *int_field_type);

/**
@brief	Sets the storage size, in bits, of the @intfields described by
	the @intft \p int_field_type.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to set the
				storage size.
@param[in] size			Storage size (bits) of the integer fields
				described by \p int_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@prehot{int_field_type}
@pre \p size is greater than 0 and lesser than or equal to 64.
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_get_size(): Returns the storage size of
	the integer fields described by a given integer field type.
*/
extern int bt_field_type_integer_set_size(
		struct bt_field_type *int_field_type, unsigned int size);

/**
@brief  Returns whether or not the @intfields described by the @intft
	\p int_field_type are signed.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to get the
				signedness.
@returns			#BT_TRUE if the integer fields described by
				\p int_field_type are signed, #BT_FALSE if they
				are unsigned.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_set_is_signed(): Sets the signedness of the
	integer fields described by a given integer field type.
*/
extern bt_bool bt_field_type_integer_is_signed(
		struct bt_field_type *int_field_type);

/**
@brief	Sets whether or not the @intfields described by
	the @intft \p int_field_type are signed.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to set the
				signedness.
@param[in] is_signed		Signedness of the integer fields
				described by \p int_field_type; #BT_FALSE means
				\em unsigned, #BT_TRUE means \em signed.
@returns			0 on success, or a negative value on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@prehot{int_field_type}
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_is_signed(): Returns the signedness of
	the integer fields described by a given integer field type.
*/
extern int bt_field_type_integer_set_is_signed(
		struct bt_field_type *int_field_type, bt_bool is_signed);

/**
@brief  Returns the preferred display base (radix) of the @intfields
	described by the @intft \p int_field_type.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to get the
				preferred display base.
@returns			Preferred display base of the integer
				fields described by \p int_field_type,
				#BT_INTEGER_BASE_UNSPECIFIED if
				not specified, or
				#BT_INTEGER_BASE_UNKNOWN on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_set_base(): Sets the preferred display
	base of the integer fields described by a given integer field
	type.
*/
extern enum bt_integer_base bt_field_type_integer_get_base(
		struct bt_field_type *int_field_type);

/**
@brief  Sets the preferred display base (radix) of the @intfields
	described by the @intft \p int_field_type to \p base.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to set the
				preferred display base.
@param[in] base			Preferred display base of the integer
				fields described by \p int_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@prehot{int_field_type}
@pre \p base is #BT_INTEGER_BASE_UNSPECIFIED,
	#BT_INTEGER_BASE_BINARY, #BT_INTEGER_BASE_OCTAL,
	#BT_INTEGER_BASE_DECIMAL, or #BT_INTEGER_BASE_HEXADECIMAL.
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_get_base(): Returns the preferred display
	base of the integer fields described by a given
	integer field type.
*/
extern int bt_field_type_integer_set_base(
		struct bt_field_type *int_field_type,
		enum bt_integer_base base);

/**
@brief  Returns the encoding of the @intfields described by
	the @intft \p int_field_type.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to get the
				encoding.
@returns			Encoding of the integer
				fields described by \p int_field_type,
				or #BT_STRING_ENCODING_UNKNOWN on
				error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_set_encoding(): Sets the encoding
	of the integer fields described by a given integer field type.
*/
extern enum bt_string_encoding bt_field_type_integer_get_encoding(
		struct bt_field_type *int_field_type);

/**
@brief  Sets the encoding of the @intfields described by the @intft
	\p int_field_type to \p encoding.

You can use this property, in CTF IR, to create "text" @arrayfts or
@seqfts. A text array field type is array field type with an unsigned,
8-bit integer field type having an encoding as its element field type.

@param[in] int_field_type	Integer field type which describes the
				integer fields of which to set the
				encoding.
@param[in] encoding		Encoding of the integer
				fields described by \p int_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@prehot{int_field_type}
@pre \p encoding is #BT_STRING_ENCODING_NONE,
	#BT_STRING_ENCODING_ASCII, or
	#BT_STRING_ENCODING_UTF8.
@postrefcountsame{int_field_type}

@sa bt_field_type_integer_get_encoding(): Returns the encoding of
	the integer fields described by a given integer field type.
*/
extern int bt_field_type_integer_set_encoding(
		struct bt_field_type *int_field_type,
		enum bt_string_encoding encoding);

extern struct bt_clock_class *bt_field_type_integer_borrow_mapped_clock_class(
		struct bt_field_type *int_field_type);

/**
@brief  Returns the \link ctfirclockclass CTF IR clock class\endlink
	mapped to the @intft \p int_field_type.

The mapped clock class, if any, indicates the class of the clock which
an @intfield described by \p int_field_type should sample or update.
This mapped clock class is only indicative.

@param[in] int_field_type	Integer field type of which to get the
				mapped clock class.
@returns			Mapped clock class of \p int_field_type,
				or \c NULL if there's no mapped clock
				class or on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@postrefcountsame{int_field_type}
@postsuccessrefcountretinc

@sa bt_field_type_integer_set_mapped_clock_class(): Sets the mapped
	clock class of a given integer field type.
*/
static inline
struct bt_clock_class *bt_field_type_integer_get_mapped_clock_class(
		struct bt_field_type *int_field_type)
{
	return bt_get(bt_field_type_integer_borrow_mapped_clock_class(
		int_field_type));
}

/**
@brief	Sets the \link ctfirclockclass CTF IR clock class\endlink mapped
	to the @intft \p int_field_type to \p clock_class.

The mapped clock class, if any, indicates the class of the clock which
an integer field described by \p int_field_type should sample or update.
This mapped clock class is only indicative.

@param[in] int_field_type	Integer field type of which to set the
				mapped clock class.
@param[in] clock_class		Mapped clock class of \p int_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{int_field_type}
@prenotnull{clock_class}
@preisintft{int_field_type}
@prehot{int_field_type}
@postrefcountsame{int_field_type}
@postsuccessrefcountinc{clock_class}

@sa bt_field_type_integer_get_mapped_clock_class(): Returns the mapped
	clock class of a given integer field type.
*/
extern int bt_field_type_integer_set_mapped_clock_class(
		struct bt_field_type *int_field_type,
		struct bt_clock_class *clock_class);

/** @} */

/**
@defgroup ctfirfloatfieldtype CTF IR floating point number field type
@ingroup ctfirfieldtypes
@brief CTF IR floating point number field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>floating point number field type</em></strong> is
a field type that you can use to create concrete @floatfields.

You can create a floating point number field type
with bt_field_type_floating_point_create().

A floating point number field type has the following properties:

<table>
  <tr>
    <th>Property
    <th>Value at creation
    <th>Getter
    <th>Setter
  </tr>
  <tr>
    <td>\b Alignment (bits) of the described floating point
        number fields
    <td>1
    <td>bt_field_type_get_alignment()
    <td>bt_field_type_set_alignment()
  </tr>
  <tr>
    <td><strong>Byte order</strong> of the described floating point
        number fields
    <td>#BT_BYTE_ORDER_NATIVE
    <td>bt_field_type_get_byte_order()
    <td>bt_field_type_set_byte_order()
  </tr>
  <tr>
    <td><strong>Exponent storage size</strong> (bits) of the described
        floating point number fields
    <td>8
    <td>bt_field_type_floating_point_get_exponent_digits()
    <td>bt_field_type_floating_point_set_exponent_digits()
  </tr>
  <tr>
    <td><strong>Mantissa and sign storage size</strong> (bits) of the
        described floating point number fields
    <td>24 (23-bit mantissa, 1-bit sign)
    <td>bt_field_type_floating_point_get_mantissa_digits()
    <td>bt_field_type_floating_point_set_mantissa_digits()
  </tr>
</table>

@sa ctfirfloatfield
@sa ctfirfieldtypes
@sa \ref ctfirfieldtypesexamples_floatfieldtype "Examples"

@addtogroup ctfirfloatfieldtype
@{
*/

/**
@brief	Creates a default @floatft.

@returns	Created floating point number field type,
		or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_floating_point_create(void);

/**
@brief  Returns the exponent storage size of the @floatfields
	described by the @floatft \p float_field_type.

@param[in] float_field_type	Floating point number field type which
				describes the floating point number
				fields of which to get the exponent
				storage size.
@returns			Exponent storage size of the
				floating point number fields
				described by \p float_field_type,
				or a negative value on error.

@prenotnull{float_field_type}
@preisfloatft{float_field_type}
@postrefcountsame{float_field_type}

@sa bt_field_type_floating_point_set_exponent_digits(): Sets the
	exponent storage size of the floating point number fields
	described by a given floating point number field type.
*/
extern int bt_field_type_floating_point_get_exponent_digits(
		struct bt_field_type *float_field_type);

/**
@brief  Sets the exponent storage size of the @floatfields described by
	the @floatft \p float_field_type to \p exponent_size.

As of Babeltrace \btversion, \p exponent_size can only be 8 or 11.

@param[in] float_field_type	Floating point number field type which
				describes the floating point number
				fields of which to set the exponent
				storage size.
@param[in] exponent_size	Exponent storage size of the floating
				point number fields described by \p
				float_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{float_field_type}
@preisfloatft{float_field_type}
@prehot{float_field_type}
@pre \p exponent_size is 8 or 11.
@postrefcountsame{float_field_type}

@sa bt_field_type_floating_point_get_exponent_digits(): Returns the
	exponent storage size of the floating point number fields
	described by a given floating point number field type.
*/
extern int bt_field_type_floating_point_set_exponent_digits(
		struct bt_field_type *float_field_type,
		unsigned int exponent_size);

/**
@brief  Returns the mantissa and sign storage size of the @floatfields
	described by the @floatft \p float_field_type.

On success, the returned value is the sum of the mantissa \em and
sign storage sizes.

@param[in] float_field_type	Floating point number field type which
				describes the floating point number
				fields of which to get the mantissa and
				sign storage size.
@returns			Mantissa and sign storage size of the
				floating point number fields
				described by \p float_field_type,
				or a negative value on error.

@prenotnull{float_field_type}
@preisfloatft{float_field_type}
@postrefcountsame{float_field_type}

@sa bt_field_type_floating_point_set_mantissa_digits(): Sets the
	mantissa and size storage size of the floating point number
	fields described by a given floating point number field type.
*/
extern int bt_field_type_floating_point_get_mantissa_digits(
		struct bt_field_type *float_field_type);

/**
@brief  Sets the mantissa and sign storage size of the @floatfields
	described by the @floatft \p float_field_type to \p
	mantissa_sign_size.

As of Babeltrace \btversion, \p mantissa_sign_size can only be 24 or 53.

@param[in] float_field_type	Floating point number field type which
				describes the floating point number
				fields of which to set the mantissa and
				sign storage size.
@param[in] mantissa_sign_size	Mantissa and sign storage size of the
				floating point number fields described
				by \p float_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{float_field_type}
@preisfloatft{float_field_type}
@prehot{float_field_type}
@pre \p mantissa_sign_size is 24 or 53.
@postrefcountsame{float_field_type}

@sa bt_field_type_floating_point_get_mantissa_digits(): Returns the
	mantissa and sign storage size of the floating point number
	fields described by a given floating point number field type.
*/
extern int bt_field_type_floating_point_set_mantissa_digits(
		struct bt_field_type *float_field_type,
		unsigned int mantissa_sign_size);

/** @} */

/**
@defgroup ctfirenumfieldtype CTF IR enumeration field type
@ingroup ctfirfieldtypes
@brief CTF IR enumeration field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>enumeration field type</em></strong> is
a field type that you can use to create concrete @enumfields.

You can create an enumeration field type with
bt_field_type_enumeration_create(). This function needs a @intft
which represents the storage field type of the created enumeration field
type. In other words, an enumeration field type wraps an integer field
type and adds label-value mappings to it.

An enumeration mapping has:

- A <strong>name</strong>.
- A <strong>range of values</strong> given by a beginning and an ending
  value, both included in the range.

You can add a mapping to an enumeration field type with
bt_field_type_enumeration_signed_add_mapping() or
bt_field_type_enumeration_unsigned_add_mapping(), depending on the
signedness of the wrapped @intft.

You can find mappings by name or by value with the following find
operations:

- bt_field_type_enumeration_find_mappings_by_name(): Finds the
  mappings with a given name.
- bt_field_type_enumeration_unsigned_find_mappings_by_value():
  Finds the mappings which contain a given unsigned value in their
  range.
- bt_field_type_enumeration_signed_find_mappings_by_value():
  Finds the mappings which contain a given signed value in their range.

Those functions return a @enumftiter on the result set of the find
operation.

Many mappings can share the same name, and the ranges of a given
enumeration field type are allowed to overlap. For example,
this is a valid set of mappings:

@verbatim
APPLE  -> [  3, 19]
BANANA -> [-15,  1]
CHERRY -> [ 25, 34]
APPLE  -> [ 55, 55]
@endverbatim

The following set of mappings is also valid:

@verbatim
APPLE  -> [  3, 19]
BANANA -> [-15,  1]
CHERRY -> [ 25, 34]
APPLE  -> [ 30, 55]
@endverbatim

Here, the range of the second \c APPLE mapping overlaps the range of
the \c CHERRY mapping.

@sa ctfirenumftmappingiter
@sa ctfirenumfield
@sa ctfirfieldtypes

@addtogroup ctfirenumfieldtype
@{
*/

/**
@brief	Creates a default @enumft wrapping the @intft \p int_field_type.

@param[in] int_field_type	Integer field type wrapped by the
				created enumeration field type.
@returns			Created enumeration field type,
				or \c NULL on error.

@prenotnull{int_field_type}
@preisintft{int_field_type}
@postsuccessrefcountinc{int_field_type}
@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_enumeration_create(
		struct bt_field_type *int_field_type);

extern
struct bt_field_type *bt_field_type_enumeration_borrow_container_field_type(
		struct bt_field_type *enum_field_type);

/**
@brief  Returns the @intft wrapped by the @enumft \p enum_field_type.

@param[in] enum_field_type	Enumeration field type of which to get
				the wrapped integer field type.
@returns			Integer field type wrapped by
				\p enum_field_type, or \c NULL on
				error.

@prenotnull{enum_field_type}
@preisenumft{enum_field_type}
@postrefcountsame{enum_field_type}
@postsuccessrefcountretinc
*/
static inline
struct bt_field_type *bt_field_type_enumeration_get_container_field_type(
		struct bt_field_type *enum_field_type)
{
	return bt_get(bt_field_type_enumeration_borrow_container_field_type(
		enum_field_type));
}

/**
@brief  Returns the number of mappings contained in the
	@enumft \p enum_field_type.

@param[in] enum_field_type	Enumeration field type of which to get
				the number of contained mappings.
@returns			Number of mappings contained in
				\p enum_field_type, or a negative
				value on error.

@prenotnull{enum_field_type}
@preisenumft{enum_field_type}
@postrefcountsame{enum_field_type}
*/
extern int64_t bt_field_type_enumeration_get_mapping_count(
		struct bt_field_type *enum_field_type);

/**
@brief	Returns the signed mapping of the @enumft
	\p enum_field_type at index \p index.

The @intft wrapped by \p enum_field_type, as returned by
bt_field_type_enumeration_get_container_field_type(), must be \b signed
to use this function.

On success, \p enum_field_type remains the sole owner of \p *name.

@param[in] enum_field_type	Enumeration field type of which to get
				the mapping at index \p index.
@param[in] index		Index of the mapping to get from
				\p enum_field_type.
@param[out] name		Returned name of the mapping at index
				\p index.
@param[out] range_begin		Returned beginning of the range
				(included) of the mapping at index \p
				index.
@param[out] range_end		Returned end of the range (included) of
				the mapping at index \p index.
@returns			0 on success, or a negative value on error.

@prenotnull{enum_field_type}
@prenotnull{name}
@prenotnull{range_begin}
@prenotnull{range_end}
@preisenumft{enum_field_type}
@pre The wrapped @intft of \p enum_field_type is signed.
@pre \p index is lesser than the number of mappings contained in the
	enumeration field type \p enum_field_type (see
	bt_field_type_enumeration_get_mapping_count()).
@postrefcountsame{enum_field_type}

@sa bt_field_type_enumeration_unsigned_get_mapping_by_index(): Returns the
	unsigned mapping contained by a given enumeration field type
	at a given index.
*/
extern int bt_field_type_enumeration_signed_get_mapping_by_index(
		struct bt_field_type *enum_field_type, uint64_t index,
		const char **name, int64_t *range_begin, int64_t *range_end);

/**
@brief  Returns the unsigned mapping of the @enumft
	\p enum_field_type at index \p index.

The @intft wrapped by \p enum_field_type, as returned by
bt_field_type_enumeration_get_container_field_type(), must be
\b unsigned to use this function.

On success, \p enum_field_type remains the sole owner of \p *name.

@param[in] enum_field_type	Enumeration field type of which to get
				the mapping at index \p index.
@param[in] index		Index of the mapping to get from
				\p enum_field_type.
@param[out] name		Returned name of the mapping at index
				\p index.
@param[out] range_begin		Returned beginning of the range
				(included) of the mapping at index \p
				index.
@param[out] range_end		Returned end of the range (included) of
				the mapping at index \p index.
@returns			0 on success, or a negative value on error.

@prenotnull{enum_field_type}
@prenotnull{name}
@prenotnull{range_begin}
@prenotnull{range_end}
@preisenumft{enum_field_type}
@pre The wrapped @intft of \p enum_field_type is unsigned.
@pre \p index is lesser than the number of mappings contained in the
	enumeration field type \p enum_field_type (see
	bt_field_type_enumeration_get_mapping_count()).
@postrefcountsame{enum_field_type}

@sa bt_field_type_enumeration_signed_get_mapping_by_index(): Returns the
	signed mapping contained by a given enumeration field type
	at a given index.
*/
extern int bt_field_type_enumeration_unsigned_get_mapping_by_index(
		struct bt_field_type *enum_field_type, uint64_t index,
		const char **name, uint64_t *range_begin,
		uint64_t *range_end);

/**
@brief  Finds the mappings of the @enumft \p enum_field_type which
	are named \p name.

This function returns an iterator on the result set of this find
operation. See \ref ctfirenumftmappingiter for more details.

@param[in] enum_field_type	Enumeration field type of which to find
				the mappings named \p name.
@param[in] name			Name of the mappings to find in
				\p enum_field_type.
@returns			@enumftiter on the set of mappings named
				\p name in \p enum_field_type, or
				\c NULL if no mappings were found or
				on error.

@prenotnull{enum_field_type}
@prenotnull{name}
@preisenumft{enum_field_type}
@postrefcountsame{enum_field_type}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned @enumftiter can iterate
	on at least one mapping.

@sa bt_field_type_enumeration_signed_find_mappings_by_value(): Finds
	the mappings of a given enumeration field type which contain
	a given signed value in their range.
@sa bt_field_type_enumeration_unsigned_find_mappings_by_value(): Finds
	the mappings of a given enumeration field type which contain
	a given unsigned value in their range.
*/
extern struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_name(
		struct bt_field_type *enum_field_type,
		const char *name);

/**
@brief  Finds the mappings of the @enumft \p enum_field_type which
	contain the signed value \p value in their range.

This function returns an iterator on the result set of this find
operation. See \ref ctfirenumftmappingiter for more details.

@param[in] enum_field_type	Enumeration field type of which to find
				the mappings which contain \p value.
@param[in] value		Value to find in the ranges of the
				mappings of \p enum_field_type.
@returns			@enumftiter on the set of mappings of
				\p enum_field_type which contain
				\p value in their range, or \c NULL if
				no mappings were found or on error.

@prenotnull{enum_field_type}
@preisenumft{enum_field_type}
@postrefcountsame{enum_field_type}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned @enumftiter can iterate
	on at least one mapping.

@sa bt_field_type_enumeration_find_mappings_by_name(): Finds the
	mappings of a given enumeration field type which have a given
	name.
@sa bt_field_type_enumeration_unsigned_find_mappings_by_value(): Finds
	the mappings of a given enumeration field type which contain
	a given unsigned value in their range.
*/
extern struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_signed_find_mappings_by_value(
		struct bt_field_type *enum_field_type,
		int64_t value);

/**
@brief  Finds the mappings of the @enumft \p enum_field_type which
	contain the unsigned value \p value in their range.

This function returns an iterator on the result set of this find
operation. See \ref ctfirenumftmappingiter for more details.

@param[in] enum_field_type	Enumeration field type of which to find
				the mappings which contain \p value.
@param[in] value		Value to find in the ranges of the
				mappings of \p enum_field_type.
@returns			@enumftiter on the set of mappings of
				\p enum_field_type which contain
				\p value in their range, or \c NULL
				if no mappings were found or
				on error.

@prenotnull{enum_field_type}
@preisenumft{enum_field_type}
@postrefcountsame{enum_field_type}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned @enumftiter can iterate
	on at least one mapping.

@sa bt_field_type_enumeration_find_mappings_by_name(): Finds the
	mappings of a given enumeration field type which have a given
	name.
@sa bt_field_type_enumeration_signed_find_mappings_by_value(): Finds
	the mappings of a given enumeration field type which contain
	a given unsigned value in their range.
*/
extern struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_unsigned_find_mappings_by_value(
		struct bt_field_type *enum_field_type,
		uint64_t value);

/**
@brief  Adds a mapping to the @enumft \p enum_field_type which maps the
	name \p name to the signed range \p range_begin (included) to
	\p range_end (included).

Make \p range_begin and \p range_end the same value to add a mapping
to a single value.

The @intft wrapped by \p enum_field_type, as returned by
bt_field_type_enumeration_get_container_field_type(), must be
\b signed to use this function.

A mapping in \p enum_field_type can exist with the name \p name.

@param[in] enum_field_type	Enumeration field type to which to add
				a mapping.
@param[in] name			Name of the mapping to add (copied
				on success).
@param[in] range_begin		Beginning of the range of the mapping
				(included).
@param[in] range_end		End of the range of the mapping
				(included).
@returns			0 on success, or a negative value on error.

@prenotnull{enum_field_type}
@prenotnull{name}
@prehot{enum_field_type}
@preisenumft{enum_field_type}
@pre The wrapped @intft of \p enum_field_type is signed.
@pre \p range_end is greater than or equal to \p range_begin.
@postrefcountsame{enum_field_type}

@sa bt_field_type_enumeration_unsigned_add_mapping(): Adds an
	unsigned mapping to a given enumeration field type.
*/
extern int bt_field_type_enumeration_signed_add_mapping(
		struct bt_field_type *enum_field_type, const char *name,
		int64_t range_begin, int64_t range_end);

/**
@brief	Adds a mapping to the @enumft \p enum_field_type which maps
	the name \p name to the unsigned
	range \p range_begin (included) to \p range_end (included).

Make \p range_begin and \p range_end the same value to add a mapping
to a single value.

The @intft wrapped by \p enum_field_type, as returned by
bt_field_type_enumeration_get_container_field_type(), must be
\b unsigned to use this function.

A mapping in \p enum_field_type can exist with the name \p name.

@param[in] enum_field_type	Enumeration field type to which to add
				a mapping.
@param[in] name			Name of the mapping to add (copied
				on success).
@param[in] range_begin		Beginning of the range of the mapping
				(included).
@param[in] range_end		End of the range of the mapping
				(included).
@returns			0 on success, or a negative value on error.

@prenotnull{enum_field_type}
@prenotnull{name}
@prehot{enum_field_type}
@preisenumft{enum_field_type}
@pre The wrapped @intft of \p enum_field_type is unsigned.
@pre \p range_end is greater than or equal to \p range_begin.
@postrefcountsame{enum_field_type}

@sa bt_field_type_enumeration_signed_add_mapping(): Adds a signed
	mapping to a given enumeration field type.
*/
extern int bt_field_type_enumeration_unsigned_add_mapping(
		struct bt_field_type *enum_field_type, const char *name,
		uint64_t range_begin, uint64_t range_end);

/** @} */

/**
@defgroup ctfirenumftmappingiter CTF IR enumeration field type mapping iterator
@ingroup ctfirenumfieldtype
@brief CTF IR enumeration field type mapping iterator.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>enumeration field type mapping
iterator</em></strong> is an iterator on @enumft mappings.

You can get an enumeration mapping iterator from one of the following
functions:

- Find operations of an @enumft object:
  - bt_field_type_enumeration_find_mappings_by_name(): Finds the
    mappings with a given name.
  - bt_field_type_enumeration_unsigned_find_mappings_by_value():
    Finds the mappings which contain a given unsigned value in their
    range.
  - bt_field_type_enumeration_signed_find_mappings_by_value():
    Finds the mappings which contain a given signed value in their range.
- bt_field_enumeration_get_mappings(): Finds the mappings in the
  @enumft of an @enumfield containing its current integral value in
  their range.

Those functions guarantee that the returned iterator can iterate on
at least one mapping. Otherwise, they return \c NULL.

You can get the name and the range of a mapping iterator's current
mapping with
bt_field_type_enumeration_mapping_iterator_signed_get()
or
bt_field_type_enumeration_mapping_iterator_unsigned_get(),
depending on the signedness of the @intft wrapped by the
@enumft. If you only need the name of the current mapping, you can
use any of the two functions and set the \p range_begin and \p range_end
parameters to \c NULL.

You can advance an enumeration field type mapping iterator to the next
mapping with
bt_field_type_enumeration_mapping_iterator_next(). This
function returns a negative value when you reach the end of the
result set.

As with any Babeltrace object, CTF IR enumeration field type mapping
iterator objects have <a
href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

@sa ctfirenumfieldtype

@addtogroup ctfirenumftmappingiter
@{
*/

/**
@struct bt_field_type_enumeration_mapping_iterator
@brief A CTF IR enumeration field type mapping iterator.
@sa ctfirenumftmappingiter
*/

/**
@brief  Returns the name and the range of the current (signed) mapping
	of the @enumftiter \p iter.

If one of \p range_begin or \p range_end is not \c NULL, the @intft
wrapped by the @enumft from which \p iter was obtained, as returned by
bt_field_type_enumeration_get_container_field_type(), must be
\b signed to use this function. Otherwise, if you only need to get the
name of the current mapping, set \p range_begin and \p range_end to
\c NULL.

On success, if \p name is not \c NULL, \p *name remains valid as long
as \p iter exists and
bt_field_type_enumeration_mapping_iterator_next() is
\em not called on \p iter.

@param[in] iter			Enumeration field type mapping iterator
				of which to get the range of the current
				mapping.
@param[out] name		Returned name of the current mapping of
				\p iter (can be \c NULL to ignore).
@param[out] range_begin		Returned beginning of the range
				(included) of the current mapping of
				\p iter (can be \c NULL to ignore).
@param[out] range_end		Returned end of the range
				(included) of the current mapping of
				\p iter (can be \c NULL to ignore).
@returns			0 on success, or a negative value on error.

@prenotnull{iter}
@postrefcountsame{iter}

@sa bt_field_type_enumeration_mapping_iterator_unsigned_get():
	Returns the name and the unsigned range of the current mapping
	of a given enumeration field type mapping iterator.
*/
extern int bt_field_type_enumeration_mapping_iterator_signed_get(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **name, int64_t *range_begin, int64_t *range_end);

/**
@brief  Returns the name and the range of the current (unsigned) mapping
	of the @enumftiter \p iter.

If one of \p range_begin or \p range_end is not \c NULL, the @intft
wrapped by the @enumft from which \p iter was obtained, as returned by
bt_field_type_enumeration_get_container_field_type(), must be
\b unsigned to use this function. Otherwise, if you only need to get the
name of the current mapping, set \p range_begin and \p range_end to
\c NULL.

On success, if \p name is not \c NULL, \p *name remains valid as long
as \p iter exists and
bt_field_type_enumeration_mapping_iterator_next() is
\em not called on \p iter.

@param[in] iter			Enumeration field type mapping iterator
				of which to get the range of the current
				mapping.
@param[out] name		Returned name of the current mapping of
				\p iter (can be \c NULL to ignore).
@param[out] range_begin		Returned beginning of the range
				(included) of the current mapping of
				\p iter (can be \c NULL to ignore).
@param[out] range_end		Returned end of the range
				(included) of the current mapping of
				\p iter (can be \c NULL to ignore).
@returns			0 on success, or a negative value on error.

@prenotnull{iter}
@postrefcountsame{iter}

@sa
	bt_field_type_enumeration_mapping_iterator_signed_get():
	Returns the name and the signed range of the current mapping of
	a given enumeration field type mapping iterator.
*/
extern int bt_field_type_enumeration_mapping_iterator_unsigned_get(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **name, uint64_t *range_begin, uint64_t *range_end);

/**
@brief	Advances the @enumftiter \p iter to the next mapping.

@param[in] iter		Enumeration field type mapping iterator to
			advance.
@returns		0 on success, or a negative value on error or
			when you reach the end of the set.

@prenotnull{iter}
@postrefcountsame{iter}
*/
extern int bt_field_type_enumeration_mapping_iterator_next(
		struct bt_field_type_enumeration_mapping_iterator *iter);

/** @} */

/**
@defgroup ctfirstringfieldtype CTF IR string field type
@ingroup ctfirfieldtypes
@brief CTF IR string field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>string field type</em></strong> is a field type that
you can use to create concrete @stringfields.

You can create a string field type
with bt_field_type_string_create().

A string field type has only one property: the \b encoding of its
described @stringfields. By default, the encoding of the string fields
described by a string field type is #BT_STRING_ENCODING_UTF8. You
can set the encoding of the string fields described by a string field
type with bt_field_type_string_set_encoding().

@sa ctfirstringfield
@sa ctfirfieldtypes

@addtogroup ctfirstringfieldtype
@{
*/

/**
@brief	Creates a default @stringft.

@returns	Created string field type, or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_string_create(void);

/**
@brief  Returns the encoding of the @stringfields described by
	the @stringft \p string_field_type.

@param[in] string_field_type	String field type which describes the
				string fields of which to get the
				encoding.
@returns			Encoding of the string
				fields described by \p string_field_type,
				or #BT_STRING_ENCODING_UNKNOWN on
				error.

@prenotnull{string_field_type}
@preisstringft{string_field_type}
@postrefcountsame{string_field_type}

@sa bt_field_type_string_set_encoding(): Sets the encoding
	of the string fields described by a given string field type.
*/
extern enum bt_string_encoding bt_field_type_string_get_encoding(
		struct bt_field_type *string_field_type);

/**
@brief  Sets the encoding of the @stringfields described by the
	@stringft \p string_field_type to \p encoding.

@param[in] string_field_type	String field type which describes the
				string fields of which to set the
				encoding.
@param[in] encoding		Encoding of the string fields described
				by \p string_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{string_field_type}
@preisstringft{string_field_type}
@prehot{string_field_type}
@pre \p encoding is #BT_STRING_ENCODING_ASCII or
	#BT_STRING_ENCODING_UTF8.
@postrefcountsame{string_field_type}

@sa bt_field_type_string_get_encoding(): Returns the encoding of
	the string fields described by a given string field type.
*/
extern int bt_field_type_string_set_encoding(
		struct bt_field_type *string_field_type,
		enum bt_string_encoding encoding);

/** @} */

/**
@defgroup ctfirstructfieldtype CTF IR structure field type
@ingroup ctfirfieldtypes
@brief CTF IR structure field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>structure field type</em></strong> is
a field type that you can use to create concrete @structfields.

You can create a structure field type
with bt_field_type_structure_create(). This function creates
an empty structure field type, with no fields.

You can add a field to a structure field type with
bt_field_type_structure_add_field(). Two fields in a structure
field type cannot have the same name.

You can set the \em minimum alignment of the structure fields described
by a structure field type with the common
bt_field_type_set_alignment() function. The \em effective alignment
of the structure fields described by a structure field type, as per
<a href="http://diamon.org/ctf/">CTF</a>, is the \em maximum value amongst
the effective alignments of all its fields. Note that the effective
alignment of @varfields is always 1.

You can set the byte order of <em>all the contained fields</em>,
recursively, of a structure field type with the common
bt_field_type_set_byte_order() function.

@sa ctfirstructfield
@sa ctfirfieldtypes

@addtogroup ctfirstructfieldtype
@{
*/

/**
@brief	Creates a default, empty @structft.

@returns			Created structure field type,
				or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_structure_create(void);

/**
@brief	Returns the number of fields contained in the
	@structft \p struct_field_type.

@param[in] struct_field_type	Structure field type of which to get
				the number of contained fields.
@returns			Number of fields contained in
				\p struct_field_type, or a negative
				value on error.

@prenotnull{struct_field_type}
@preisstructft{struct_field_type}
@postrefcountsame{struct_field_type}
*/
extern int64_t bt_field_type_structure_get_field_count(
		struct bt_field_type *struct_field_type);

extern int bt_field_type_structure_borrow_field_by_index(
		struct bt_field_type *struct_field_type,
		const char **field_name, struct bt_field_type **field_type,
		uint64_t index);

/**
@brief	Returns the field of the @structft \p struct_field_type
	at index \p index.

On success, the field's type is placed in \p *field_type if
\p field_type is not \c NULL. The field's name is placed in
\p *field_name if \p field_name is not \c NULL.
\p struct_field_type remains the sole owner of \p *field_name.

@param[in] struct_field_type	Structure field type of which to get
				the field at index \p index.
@param[out] field_name		Returned name of the field at index
				\p index (can be \c NULL).
@param[out] field_type		Returned field type of the field
				at index \p index (can be \c NULL).
­@param[in] index		Index of the field to get from
				\p struct_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{struct_field_type}
@preisstructft{struct_field_type}
@pre \p index is lesser than the number of fields contained in the
	structure field type \p struct_field_type (see
	bt_field_type_structure_get_field_count()).
@postrefcountsame{struct_field_type}
@post <strong>On success</strong>, the returned field's type is placed
	in \p *field_type and its reference count is incremented.

@sa bt_field_type_structure_get_field_type_by_name(): Finds a
	structure field type's field by name.
*/
static inline
int bt_field_type_structure_get_field_by_index(
		struct bt_field_type *struct_field_type,
		const char **field_name, struct bt_field_type **field_type,
		uint64_t index)
{
	int ret = bt_field_type_structure_borrow_field_by_index(
		struct_field_type, field_name, field_type, index);

	if (ret == 0 && field_type) {
		bt_get(*field_type);
	}

	return ret;
}

extern
struct bt_field_type *bt_field_type_structure_borrow_field_type_by_name(
		struct bt_field_type *struct_field_type,
		const char *field_name);

/**
@brief  Returns the type of the field named \p field_name found in
	the @structft \p struct_field_type.

@param[in] struct_field_type	Structure field type of which to get
				a field's type.
@param[in] field_name		Name of the field to find.
@returns			Type of the field named \p field_name in
				\p struct_field_type, or
				\c NULL on error.

@prenotnull{struct_field_type}
@prenotnull{field_name}
@preisstructft{struct_field_type}
@postrefcountsame{struct_field_type}
@postsuccessrefcountretinc

@sa bt_field_type_structure_get_field_by_index(): Finds a
	structure field type's field by index.
*/
static inline
struct bt_field_type *bt_field_type_structure_get_field_type_by_name(
		struct bt_field_type *struct_field_type,
		const char *field_name)
{
	return bt_get(bt_field_type_structure_borrow_field_type_by_name(
		struct_field_type, field_name));
}

/**
@brief	Adds a field named \p field_name with the @ft
	\p field_type to the @structft \p struct_field_type.

On success, \p field_type becomes the child of \p struct_field_type.

This function adds the new field after the current last field of
\p struct_field_type (append mode).

You \em cannot add a field named \p field_name if there's already a
field named \p field_name in \p struct_field_type.

@param[in] struct_field_type	Structure field type to which to add
				a new field.
@param[in] field_type		Field type of the field to add to
				\p struct_field_type.
@param[in] field_name		Name of the field to add to
				\p struct_field_type
				(copied on success).
@returns			0 on success, or a negative value on error.

@prenotnull{struct_field_type}
@prenotnull{field_type}
@prenotnull{field_name}
@preisstructft{struct_field_type}
@pre \p field_type is not and does not contain \p struct_field_type,
	recursively, as a field's type.
@prehot{struct_field_type}
@postrefcountsame{struct_field_type}
@postsuccessrefcountinc{field_type}
*/
extern int bt_field_type_structure_add_field(
		struct bt_field_type *struct_field_type,
		struct bt_field_type *field_type,
		const char *field_name);

/** @} */

/**
@defgroup ctfirarrayfieldtype CTF IR array field type
@ingroup ctfirfieldtypes
@brief CTF IR array field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>array field type</em></strong> is a field type that
you can use to create concrete @arrayfields.

You can create an array field type
with bt_field_type_array_create(). This function needs
the @ft of the fields contained by the array fields described by the
array field type to create.

@sa ctfirarrayfield
@sa ctfirfieldtypes

@addtogroup ctfirarrayfieldtype
@{
*/

/**
@brief	Creates a default @arrayft with
	\p element_field_type as the field type of the fields contained
	in its described @arrayfields of length \p length.

@param[in] element_field_type	Field type of the fields contained in
				the array fields described by the
				created array field type.
@param[in] length		Length of the array fields described by
				the created array field type.
@returns			Created array field type, or
				\c NULL on error.

@prenotnull{element_field_type}
@postsuccessrefcountinc{element_field_type}
@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_array_create(
		struct bt_field_type *element_field_type,
		unsigned int length);

extern struct bt_field_type *bt_field_type_array_borrow_element_field_type(
		struct bt_field_type *array_field_type);

/**
@brief	Returns the @ft of the @fields contained in
	the @arrayfields described by the @arrayft \p array_field_type.

@param[in] array_field_type	Array field type of which to get
				the type of the fields contained in its
				described array fields.
@returns			Type of the fields contained in the
				array fields described by
				\p array_field_type, or \c NULL
				on error.

@prenotnull{array_field_type}
@preisarrayft{array_field_type}
@postrefcountsame{array_field_type}
@postsuccessrefcountretinc
*/
static inline
struct bt_field_type *bt_field_type_array_get_element_field_type(
		struct bt_field_type *array_field_type)
{
	return bt_get(bt_field_type_array_borrow_element_field_type(
		array_field_type));
}

/**
@brief	Returns the number of @fields contained in the
	@arrayfields described by the @arrayft \p array_field_type.

@param[in] array_field_type	Array field type of which to get
				the number of fields contained in its
				described array fields.
@returns			Number of fields contained in the
				array fields described by
				\p array_field_type, or a negative value
				on error.

@prenotnull{array_field_type}
@preisarrayft{array_field_type}
@postrefcountsame{array_field_type}
*/
extern int64_t bt_field_type_array_get_length(
		struct bt_field_type *array_field_type);

/** @} */

/**
@defgroup ctfirseqfieldtype CTF IR sequence field type
@ingroup ctfirfieldtypes
@brief CTF IR sequence field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>sequence field type</em></strong> is
a field type that you can use to create concrete @seqfields.

You can create a sequence field type with
bt_field_type_sequence_create(). This function needs the @ft
of the fields contained by the sequence fields described by the created
sequence field type. This function also needs the length name of the
sequence field type to create. The length name is used to automatically
resolve the length's field type. See \ref ctfirfieldtypes to learn more
about the automatic resolving.

@sa ctfirseqfield
@sa ctfirfieldtypes

@addtogroup ctfirseqfieldtype
@{
*/

/**
@brief	Creates a default @seqft with \p element_field_type as the
	@ft of the @fields contained in its described @seqfields
	with the length name \p length_name.

\p length_name can be an absolute or relative reference. See
<a href="http://diamon.org/ctf/">CTF</a> for more details.

@param[in] element_field_type	Field type of the fields contained in
				the sequence fields described by the
				created sequence field type.
@param[in] length_name		Length name (copied on success).
@returns			Created array field type, or
				\c NULL on error.

@prenotnull{element_field_type}
@prenotnull{length_name}
@postsuccessrefcountinc{element_field_type}
@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_sequence_create(
		struct bt_field_type *element_field_type,
		const char *length_name);

extern struct bt_field_type *bt_field_type_sequence_borrow_element_field_type(
		struct bt_field_type *sequence_field_type);

/**
@brief	Returns the @ft of the @fields contained in the @seqft
	described by the @seqft \p sequence_field_type.

@param[in] sequence_field_type	Sequence field type of which to get
				the type of the fields contained in its
				described sequence fields.
@returns			Type of the fields contained in the
				sequence fields described by
				\p sequence_field_type, or \c NULL
				on error.

@prenotnull{sequence_field_type}
@preisseqft{sequence_field_type}
@postrefcountsame{sequence_field_type}
@postsuccessrefcountretinc
*/
static inline
struct bt_field_type *bt_field_type_sequence_get_element_field_type(
		struct bt_field_type *sequence_field_type)
{
	return bt_get(bt_field_type_sequence_borrow_element_field_type(
		sequence_field_type));
}

/**
@brief  Returns the length name of the @seqft \p sequence_field_type.

On success, \p sequence_field_type remains the sole owner of
the returned string.

@param[in] sequence_field_type	Sequence field type of which to get the
				length name.
@returns			Length name of \p sequence_field_type,
				or \c NULL on error.

@prenotnull{sequence_field_type}
@preisseqft{sequence_field_type}

@sa bt_field_type_sequence_get_length_field_path(): Returns the
	length's CTF IR field path of a given sequence field type.
*/
extern const char *bt_field_type_sequence_get_length_field_name(
		struct bt_field_type *sequence_field_type);

extern struct bt_field_path *bt_field_type_sequence_borrow_length_field_path(
		struct bt_field_type *sequence_field_type);

/**
@brief  Returns the length's CTF IR field path of the @seqft
	\p sequence_field_type.

The length's field path of a sequence field type is set when automatic
resolving is performed (see \ref ctfirfieldtypes).

@param[in] sequence_field_type	Sequence field type of which to get the
				length's field path.
@returns			Length's field path of
				\p sequence_field_type, or
				\c NULL if the length's field path is
				not set yet is not set or on error.

@prenotnull{sequence_field_type}
@preisseqft{sequence_field_type}
@postsuccessrefcountretinc

@sa bt_field_type_sequence_get_length_field_name(): Returns the
	length's name of a given sequence field type.
*/
static inline
struct bt_field_path *bt_field_type_sequence_get_length_field_path(
		struct bt_field_type *sequence_field_type)
{
	return bt_get(bt_field_type_sequence_borrow_length_field_path(
		sequence_field_type));
}

/** @} */

/**
@defgroup ctfirvarfieldtype CTF IR variant field type
@ingroup ctfirfieldtypes
@brief CTF IR variant field type.

@code
#include <babeltrace/ctf-ir/field-types.h>
@endcode

A CTF IR <strong><em>variant field type</em></strong> is
a field type that you can use to create concrete @varfields.

You can create a variant field type with
bt_field_type_variant_create(). This function expects you to pass
both the tag's @enumft and the tag name of the variant field type to
create. The tag's field type is optional, as the Babeltrace system can
automatically resolve it using the tag name. You can leave the tag name
to \c NULL initially, and set it later with
bt_field_type_variant_set_tag_name(). The tag name must be set when
the variant field type is frozen. See \ref ctfirfieldtypes to learn more
about the automatic resolving and the conditions under which a field
type can be frozen.

You can add a field to a variant field type with
bt_field_type_variant_add_field(). All the field names of a
variant field type \em must exist as mapping names in its tag's @enumft.

The effective alignment of the @varfields described by a
variant field type is always 1, but the individual fields of a
@varfield can have custom alignments.

You can set the byte order of <em>all the contained fields</em>,
recursively, of a variant field type with the common
bt_field_type_set_byte_order() function.

@sa ctfirvarfield
@sa ctfirfieldtypes

@addtogroup ctfirvarfieldtype
@{
*/

/**
@brief  Creates a default, empty @varft with the tag's @enumft
	\p tag_field_type and the tag name \p tag_name.

\p tag_field_type can be \c NULL; the tag's field type can be
automatically resolved from the variant field type's tag name (see
\ref ctfirfieldtypes). If \p tag_name is \c NULL, it \em must be set
with bt_field_type_variant_set_tag_name() \em before the variant
field type is frozen.

\p tag_name can be an absolute or relative reference. See
<a href="http://diamon.org/ctf/">CTF</a> for more details.

@param[in] tag_field_type	Tag's enumeration field type
				(can be \c NULL).
@param[in] tag_name		Tag name (copied on success,
				can be \c NULL).
@returns			Created variant field type, or
				\c NULL on error.

@pre \p tag_field_type is an enumeration field type or \c NULL.
@post <strong>On success, if \p tag_field_type is not \c NULL</strong>,
	its reference count is incremented.
@postsuccessrefcountret1
*/
extern struct bt_field_type *bt_field_type_variant_create(
		struct bt_field_type *tag_field_type,
		const char *tag_name);

extern struct bt_field_type *bt_field_type_variant_borrow_tag_field_type(
		struct bt_field_type *variant_field_type);

/**
@brief	Returns the tag's @enumft of the @varft \p variant_field_type.

@param[in] variant_field_type	Variant field type of which to get
				the tag's enumeration field type.
@returns			Tag's enumeration field type of
				\p variant_field_type, or \c NULL if the
				tag's field type is not set or on
				error.

@prenotnull{variant_field_type}
@preisvarft{variant_field_type}
@postrefcountsame{variant_field_type}
@postsuccessrefcountretinc
*/
static inline
struct bt_field_type *bt_field_type_variant_get_tag_field_type(
		struct bt_field_type *variant_field_type)
{
	return bt_get(bt_field_type_variant_borrow_tag_field_type(
		variant_field_type));
}

/**
@brief  Returns the tag name of the @varft \p variant_field_type.

On success, \p variant_field_type remains the sole owner of
the returned string.

@param[in] variant_field_type	Variant field type of which to get the
				tag name.
@returns			Tag name of \p variant_field_type, or
				\c NULL if the tag name is not set or
				on error.

@prenotnull{variant_field_type}
@preisvarft{variant_field_type}

@sa bt_field_type_variant_set_tag_name(): Sets the tag name of
	a given variant field type.
@sa bt_field_type_variant_get_tag_field_path(): Returns the tag's
	CTF IR field path of a given variant field type.
*/
extern const char *bt_field_type_variant_get_tag_name(
		struct bt_field_type *variant_field_type);

/**
@brief	Sets the tag name of the @varft \p variant_field_type.

\p tag_name can be an absolute or relative reference. See
<a href="http://diamon.org/ctf/">CTF</a> for more details.

@param[in] variant_field_type	Variant field type of which to set
				the tag name.
@param[in] tag_name		Tag name of \p variant_field_type
				(copied on success).
@returns			0 on success, or a negative value on error.

@prenotnull{variant_field_type}
@prenotnull{name}
@prehot{variant_field_type}
@postrefcountsame{variant_field_type}

@sa bt_field_type_variant_get_tag_name(): Returns the tag name of
	a given variant field type.
*/
extern int bt_field_type_variant_set_tag_name(
		struct bt_field_type *variant_field_type,
		const char *tag_name);

extern struct bt_field_path *bt_field_type_variant_borrow_tag_field_path(
		struct bt_field_type *variant_field_type);

/**
@brief  Returns the tag's CTF IR field path of the @varft
	\p variant_field_type.

The tag's field path of a variant field type is set when automatic
resolving is performed (see \ref ctfirfieldtypes).

@param[in] variant_field_type	Variant field type of which to get the
				tag's field path.
@returns			Tag's field path of
				\p variant_field_type, or
				\c NULL if the tag's field path is not
				set yet is not set or on error.

@prenotnull{variant_field_type}
@preisvarft{variant_field_type}
@postsuccessrefcountretinc

@sa bt_field_type_variant_get_tag_name(): Returns the tag's
	name of a given variant field type.
*/
static inline
struct bt_field_path *bt_field_type_variant_get_tag_field_path(
		struct bt_field_type *variant_field_type)
{
	return bt_get(bt_field_type_variant_borrow_tag_field_path(
		variant_field_type));
}

/**
@brief	Returns the number of fields (choices) contained in the @varft
	\p variant_field_type.

@param[in] variant_field_type	Variant field type of which to get
				the number of contained fields.
@returns			Number of fields contained in
				\p variant_field_type, or a negative
				value on error.

@prenotnull{variant_field_type}
@preisvarft{variant_field_type}
@postrefcountsame{variant_field_type}
*/
extern int64_t bt_field_type_variant_get_field_count(
		struct bt_field_type *variant_field_type);

extern int bt_field_type_variant_borrow_field_by_index(
		struct bt_field_type *variant_field_type,
		const char **field_name,
		struct bt_field_type **field_type, uint64_t index);

/**
@brief	Returns the field (choice) of the @varft \p variant_field_type
	at index \p index.

On success, the field's type is placed in \p *field_type if
\p field_type is not \c NULL. The field's name is placed in
\p *field_name if \p field_name is not \c NULL.
\p variant_field_type remains the sole owner of \p *field_name.

@param[in] variant_field_type	Variant field type of which to get
				the field at index \p index.
@param[out] field_name		Returned name of the field at index
				\p index (can be \c NULL).
@param[out] field_type		Returned field type of the field
				at index \p index (can be \c NULL).
­@param[in] index		Index of the field to get from
				\p variant_field_type.
@returns			0 on success, or a negative value on error.

@prenotnull{variant_field_type}
@preisvarft{variant_field_type}
@pre \p index is lesser than the number of fields contained in the
	variant field type \p variant_field_type (see
	bt_field_type_variant_get_field_count()).
@postrefcountsame{variant_field_type}
@post <strong>On success</strong>, the returned field's type is placed
	in \p *field_type and its reference count is incremented.

@sa bt_field_type_variant_get_field_type_by_name(): Finds a variant
	field type's field by name.
@sa bt_field_type_variant_get_field_type_from_tag(): Finds a variant
	field type's field by current tag value.
*/
static inline
int bt_field_type_variant_get_field_by_index(
		struct bt_field_type *variant_field_type,
		const char **field_name,
		struct bt_field_type **field_type, uint64_t index)
{
	int ret = bt_field_type_variant_borrow_field_by_index(
		variant_field_type, field_name, field_type, index);

	if (ret == 0 && field_type) {
		bt_get(*field_type);
	}

	return ret;
}

extern
struct bt_field_type *bt_field_type_variant_borrow_field_type_by_name(
		struct bt_field_type *variant_field_type,
		const char *field_name);

/**
@brief  Returns the type of the field (choice) named \p field_name
	found in the @varft \p variant_field_type.

@param[in] variant_field_type	Variant field type of which to get
				a field's type.
@param[in] field_name		Name of the field to find.
@returns			Type of the field named \p field_name in
				\p variant_field_type, or
				\c NULL on error.

@prenotnull{variant_field_type}
@prenotnull{field_name}
@preisvarft{variant_field_type}
@postrefcountsame{variant_field_type}
@postsuccessrefcountretinc

@sa bt_field_type_variant_get_field_by_index(): Finds a variant field type's
	field by index.
@sa bt_field_type_variant_get_field_type_from_tag(): Finds a variant
	field type's field by current tag value.
*/
static inline
struct bt_field_type *bt_field_type_variant_get_field_type_by_name(
		struct bt_field_type *variant_field_type,
		const char *field_name)
{
	return bt_get(bt_field_type_variant_borrow_field_type_by_name(
		variant_field_type, field_name));
}

extern
struct bt_field_type *bt_field_type_variant_borrow_field_type_from_tag(
		struct bt_field_type *variant_field_type,
		struct bt_field *tag_field);

/**
@brief  Returns the type of the field (choice) selected by the value of
	the @enumfield \p tag_field in the @varft \p variant_field_type.

\p tag_field is the current tag value.

The field type of \p tag_field, as returned by bt_field_get_type(),
\em must be equivalent to the field type returned by
bt_field_type_variant_get_tag_field_type() for \p variant_field_type.

@param[in] variant_field_type	Variant field type of which to get
				a field's type.
@param[in] tag_field		Current tag value (variant field type's
				selector).
@returns			Type of the field selected by
				\p tag_field in \p variant_field_type,
				or \c NULL on error.

@prenotnull{variant_field_type}
@prenotnull{tag_field}
@preisvarft{variant_field_type}
@preisenumfield{tag_field}
@postrefcountsame{variant_field_type}
@postrefcountsame{tag_field}
@postsuccessrefcountretinc

@sa bt_field_type_variant_get_field_by_index(): Finds a variant field type's
	field by index.
@sa bt_field_type_variant_get_field_type_by_name(): Finds a variant
	field type's field by name.
*/
static inline
struct bt_field_type *bt_field_type_variant_get_field_type_from_tag(
		struct bt_field_type *variant_field_type,
		struct bt_field *tag_field)
{
	return bt_get(bt_field_type_variant_borrow_field_type_from_tag(
		variant_field_type, tag_field));
}

/**
@brief	Adds a field (a choice) named \p field_name with the @ft
	\p field_type to the @varft \p variant_field_type.

On success, \p field_type becomes the child of \p variant_field_type.

You \em cannot add a field named \p field_name if there's already a
field named \p field_name in \p variant_field_type.

\p field_name \em must name an existing mapping in the tag's
enumeration field type of \p variant_field_type.

@param[in] variant_field_type	Variant field type to which to add
				a new field.
@param[in] field_type		Field type of the field to add to
				\p variant_field_type.
@param[in] field_name		Name of the field to add to
				\p variant_field_type
				(copied on success).
@returns			0 on success, or a negative value on error.

@prenotnull{variant_field_type}
@prenotnull{field_type}
@prenotnull{field_name}
@preisvarft{variant_field_type}
@pre \p field_type is not and does not contain \p variant_field_type,
	recursively, as a field's type.
@prehot{variant_field_type}
@postrefcountsame{variant_field_type}
@postsuccessrefcountinc{field_type}
*/
extern int bt_field_type_variant_add_field(
		struct bt_field_type *variant_field_type,
		struct bt_field_type *field_type,
		const char *field_name);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_H */
