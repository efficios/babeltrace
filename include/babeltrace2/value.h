#ifndef BABELTRACE2_VALUE_H
#define BABELTRACE2_VALUE_H

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
@defgroup api-val Values

@brief
    Generic, JSON-like basic data containers.

<strong><em>Values</em></strong> are generic data containers. Except for
the fact that integer values are explicitly unsigned or signed because
of typing limitations,
\bt_name values are very similar to <a href="https://json.org/">JSON</a>
values.

The value API is completely independent from the rest of the
\bt_api.

\bt_c_comp initialization parameters, \ref bt_query_executor_create()
"query" parameters, as well as trace IR user attributes (for example,
bt_event_class_set_user_attributes()) use values.

The available value types are:

<dl>
  <dt>Scalar values</dt>
  <dd>
    - Null
    - Boolean
    - Unsigned integer (64-bit range)
    - Signed integer (64-bit range)
    - Real (\c double range)
    - String
  </dd>

  <dt>Container values</dt>
  <dd>
     - Array
     - Map (string to value)
  </dd>
</dl>

Values are \ref api-fund-shared-object "shared objects": get a new
reference with bt_value_get_ref() and put an existing reference with
bt_value_put_ref().

Some library functions \ref api-fund-freezing "freeze" values on
success. The documentation of those functions indicate this
postcondition.

All the value types share the same C type, #bt_value.

Get the type enumerator of a value with bt_value_get_type(). Get whether
or not a value type conceptually \em is a given type with the inline
bt_value_type_is() function. Get whether or not a value has a specific
type with one of the <code>bt_value_is_*()</code> inline helpers.

The \em null value is special in that it's a singleton variable,
#bt_value_null. You can directly compare any value pointer to
#bt_value_null to check if it's a null value. Like other types of
values, the null value is a shared object: if you get a new null value
reference, you must eventually put it.

Create a value with one of the <code>bt_value_*_create()</code> or
<code>bt_value_*_create_init()</code> functions.

This documentation names the actual data that a scalar value wraps the
<em>raw value</em>.

Set and get the raw values of scalar values with functions that are
named <code>bt_value_*_set()</code> and <code>bt_value_*_get()</code>.

Check that two values are recursively equal with bt_value_is_equal().

Deep-copy a value with bt_value_copy().

Extend a map value with bt_value_map_extend().

The following table shows the available functions and types for each
type of value:

<table>
  <tr>
    <th>Name
    <th>Type enumerator
    <th>Type query function
    <th>Creation functions
    <th>Writing functions
    <th>Reading functions
  <tr>
    <th>\em Null
    <td>#BT_VALUE_TYPE_NULL
    <td>bt_value_is_null()
    <td>\em N/A (use the #bt_value_null variable directly)
    <td>\em N/A
    <td>\em N/A
  <tr>
    <th>\em Boolean
    <td>#BT_VALUE_TYPE_BOOL
    <td>bt_value_is_bool()
    <td>
      bt_value_bool_create()<br>
      bt_value_bool_create_init()
    <td>bt_value_bool_set()
    <td>bt_value_bool_get()
  <tr>
    <th><em>Unsigned integer</em>
    <td>#BT_VALUE_TYPE_UNSIGNED_INTEGER
    <td>bt_value_is_unsigned_integer()
    <td>
      bt_value_integer_unsigned_create()<br>
      bt_value_integer_unsigned_create_init()
    <td>bt_value_integer_unsigned_set()
    <td>bt_value_integer_unsigned_get()
  <tr>
    <th><em>Signed integer</em>
    <td>#BT_VALUE_TYPE_SIGNED_INTEGER
    <td>bt_value_is_signed_integer()
    <td>
      bt_value_integer_signed_create()<br>
      bt_value_integer_signed_create_init()
    <td>bt_value_integer_signed_set()
    <td>bt_value_integer_signed_get()
  <tr>
    <th>\em Real
    <td>#BT_VALUE_TYPE_REAL
    <td>bt_value_is_real()
    <td>
      bt_value_real_create()<br>
      bt_value_real_create_init()
    <td>bt_value_real_set()
    <td>bt_value_real_get()
  <tr>
    <th>\em String
    <td>#BT_VALUE_TYPE_STRING
    <td>bt_value_is_string()
    <td>
      bt_value_string_create()<br>
      bt_value_string_create_init()
    <td>bt_value_string_set()
    <td>bt_value_string_get()
  <tr>
    <th>\em Array
    <td>#BT_VALUE_TYPE_ARRAY
    <td>bt_value_is_array()
    <td>
      bt_value_array_create()
    <td>
      bt_value_array_append_element()<br>
      bt_value_array_append_bool_element()<br>
      bt_value_array_append_unsigned_integer_element()<br>
      bt_value_array_append_signed_integer_element()<br>
      bt_value_array_append_real_element()<br>
      bt_value_array_append_string_element()<br>
      bt_value_array_append_empty_array_element()<br>
      bt_value_array_append_empty_map_element()<br>
      bt_value_array_set_element_by_index()
    <td>
      bt_value_array_get_length()<br>
      bt_value_array_is_empty()<br>
      bt_value_array_borrow_element_by_index()<br>
      bt_value_array_borrow_element_by_index_const()
  <tr>
    <th>\em Map
    <td>#BT_VALUE_TYPE_MAP
    <td>bt_value_is_map()
    <td>
      bt_value_map_create()
    <td>
      bt_value_map_insert_entry()<br>
      bt_value_map_insert_bool_entry()<br>
      bt_value_map_insert_unsigned_integer_entry()<br>
      bt_value_map_insert_signed_integer_entry()<br>
      bt_value_map_insert_real_entry()<br>
      bt_value_map_insert_string_entry()<br>
      bt_value_map_insert_empty_array_entry()<br>
      bt_value_map_insert_empty_map_entry()<br>
      bt_value_map_extend()
    <td>
      bt_value_map_get_size()<br>
      bt_value_map_is_empty()<br>
      bt_value_map_has_entry()<br>
      bt_value_map_borrow_entry_value()<br>
      bt_value_map_borrow_entry_value_const()<br>
      bt_value_map_foreach_entry()<br>
      bt_value_map_foreach_entry_const()
</table>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_value bt_value;

@brief
    Value.

@}
*/

/*!
@name Type query
@{
*/

/*!
@brief
    Value type enumerators.
*/
typedef enum bt_value_type {
	/*!
	@brief
	    Null value.
	*/
	BT_VALUE_TYPE_NULL		= 1 << 0,

	/*!
	@brief
	    Boolean value.
	*/
	BT_VALUE_TYPE_BOOL		= 1 << 1,

	/*!
	@brief
	    Integer value.

	No value has this type: use it with bt_value_type_is().
	*/
	BT_VALUE_TYPE_INTEGER		= 1 << 2,

	/*!
	@brief
	    Unsigned integer value.

	This type conceptually inherits #BT_VALUE_TYPE_INTEGER.
	*/
	BT_VALUE_TYPE_UNSIGNED_INTEGER	= (1 << 3) | BT_VALUE_TYPE_INTEGER,

	/*!
	@brief
	    Signed integer value.

	This type conceptually inherits #BT_VALUE_TYPE_INTEGER.
	*/
	BT_VALUE_TYPE_SIGNED_INTEGER	= (1 << 4) | BT_VALUE_TYPE_INTEGER,

	/*!
	@brief
	    Real value.
	*/
	BT_VALUE_TYPE_REAL		= 1 << 5,

	/*!
	@brief
	    String value.
	*/
	BT_VALUE_TYPE_STRING		= 1 << 6,

	/*!
	@brief
	    Array value.
	*/
	BT_VALUE_TYPE_ARRAY		= 1 << 7,

	/*!
	@brief
	    Map value.
	*/
	BT_VALUE_TYPE_MAP		= 1 << 8,
} bt_value_type;

/*!
@brief
    Returns the type enumerator of the value \bt_p{value}.

@param[in] value
    Value of which to get the type enumerator

@returns
    Type enumerator of \bt_p{value}.

@bt_pre_not_null{value}

@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
@sa bt_value_is_null() &mdash;
    Returns whether or not a value is a null value.
@sa bt_value_is_bool() &mdash;
    Returns whether or not a value is a boolean value.
@sa bt_value_is_unsigned_integer() &mdash;
    Returns whether or not a value is an unsigned integer value.
@sa bt_value_is_signed_integer() &mdash;
    Returns whether or not a value is a signed integer value.
@sa bt_value_is_real() &mdash;
    Returns whether or not a value is a real value.
@sa bt_value_is_string() &mdash;
    Returns whether or not a value is a string value.
@sa bt_value_is_array() &mdash;
    Returns whether or not a value is an array value.
@sa bt_value_is_map() &mdash;
    Returns whether or not a value is a map value.
*/
extern bt_value_type bt_value_get_type(const bt_value *value);

/*!
@brief
    Returns whether or not the value type \bt_p{type} conceptually
    \em is the value type \bt_p{other_type}.

For example, an unsigned integer value conceptually \em is an integer
value, so

@code
bt_value_type_is(BT_VALUE_TYPE_UNSIGNED_INTEGER, BT_VALUE_TYPE_INTEGER)
@endcode

returns #BT_TRUE.

@param[in] type
    Value type to check against \bt_p{other_type}.
@param[in] other_type
    Value type against which to check \bt_p{type}.

@returns
    #BT_TRUE if \bt_p{type} conceptually \em is \bt_p{other_type}.

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_is_null() &mdash;
    Returns whether or not a value is a null value.
@sa bt_value_is_bool() &mdash;
    Returns whether or not a value is a boolean value.
@sa bt_value_is_unsigned_integer() &mdash;
    Returns whether or not a value is an unsigned integer value.
@sa bt_value_is_signed_integer() &mdash;
    Returns whether or not a value is a signed integer value.
@sa bt_value_is_real() &mdash;
    Returns whether or not a value is a real value.
@sa bt_value_is_string() &mdash;
    Returns whether or not a value is a string value.
@sa bt_value_is_array() &mdash;
    Returns whether or not a value is an array value.
@sa bt_value_is_map() &mdash;
    Returns whether or not a value is a map value.
*/
static inline
bt_bool bt_value_type_is(const bt_value_type type,
		const bt_value_type other_type)
{
	return (type & other_type) == other_type;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is a null value.

@note
    Because all null values point to the same null value singleton, you
    can also directly compare \bt_p{value} to the #bt_value_null
    variable.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is a null value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
@sa #bt_value_null &mdash;
    The null value singleton.
*/
static inline
bt_bool bt_value_is_null(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_NULL;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is a boolean value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is a boolean value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_bool(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_BOOL;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is an unsigned integer
    value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is an unsigned integer value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_unsigned_integer(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_UNSIGNED_INTEGER;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is a signed integer
    value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is a signed integer value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_signed_integer(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_SIGNED_INTEGER;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is a real value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is a real value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_real(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_REAL;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is a string value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is a string value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_string(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_STRING;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is an array value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is an array value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_array(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_ARRAY;
}

/*!
@brief
    Returns whether or not the value \bt_p{value} is a map value.

@param[in] value
    Value to check.

@returns
    #BT_TRUE if \bt_p{value} is a map value.

@bt_pre_not_null{value}

@sa bt_value_get_type() &mdash;
    Returns the type enumerator of a value.
@sa bt_value_type_is() &mdash;
    Returns whether or not the type of a value conceptually is a given
    type.
*/
static inline
bt_bool bt_value_is_map(const bt_value *value)
{
	return bt_value_get_type(value) == BT_VALUE_TYPE_MAP;
}

/*! @} */

/*!
@name Null value
@{
*/

/*!
@brief
    The null value singleton.

This is the \em only instance of a null value.

Like any type of value, the null value is a shared object: if you get a
new null value reference with bt_value_get_ref(), you must eventually
put it with bt_value_put_ref(). The null value singleton's reference
count must never reach 0: libbabeltrace2 logs a warning message when
this programming error occurs.

Because all null values point to the same null value singleton, you can
directly compare a value to the \c bt_value_null variable.

@attention
    @parblock
    \c bt_value_null is different from \c NULL: the former is a true
    \bt_name value object while the latter is a C definition which
    usually means "no pointer".

    For example, bt_value_map_borrow_entry_value() can return
    \c bt_value_null if the requested key is mapped to a null value, but
    it can also return \c NULL if the key is not found.
    @endparblock

@sa bt_value_is_null() &mdash;
    Returns whether or not a value is a null value.
*/
extern bt_value *const bt_value_null;

/*! @} */

/*!
@name Boolean value
@{
*/

/*!
@brief
    Creates and returns a boolean value initialized to #BT_FALSE.

The returned value has the type #BT_VALUE_TYPE_BOOL.

@returns
    New boolean value reference, or \c NULL on memory error.

@sa bt_value_bool_create_init() &mdash;
    Creates a boolean value with a given initial raw value.
*/
extern bt_value *bt_value_bool_create(void);

/*!
@brief
    Creates and returns a boolean value initialized to \bt_p{raw_value}.

The returned value has the type #BT_VALUE_TYPE_BOOL.

@param[in] raw_value
    Initial raw value of the boolean value to create.

@returns
    New boolean value reference, or \c NULL on memory error.

@sa bt_value_bool_create() &mdash;
    Creates a boolean value initialized to #BT_FALSE.
*/
extern bt_value *bt_value_bool_create_init(bt_bool raw_value);

/*!
@brief
    Sets the raw value of the boolean value \bt_p{value} to
    \bt_p{raw_value}.

@param[in] value
    Boolean value of which to set the raw value to \bt_p{raw_value}.
@param[in] raw_value
    New raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_bool_val{value}
@bt_pre_hot{value}

@sa bt_value_bool_get() &mdash;
    Returns the raw value of a boolean value.
*/
extern void bt_value_bool_set(bt_value *value, bt_bool raw_value);

/*!
@brief
    Returns the raw value of the boolean value \bt_p{value}.

@param[in] value
    Boolean value of which to get the raw value.

@returns
    Raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_bool_val{value}

@sa bt_value_bool_set() &mdash;
    Sets the raw value of a boolean value.
*/
extern bt_bool bt_value_bool_get(const bt_value *value);

/*! @} */

/*!
@name Unsigned integer value
@{
*/

/*!
@brief
    Creates and returns an unsigned integer value initialized to 0.

The returned value has the type #BT_VALUE_TYPE_UNSIGNED_INTEGER.

@returns
    New unsigned integer value reference, or \c NULL on memory error.

@sa bt_value_integer_unsigned_create_init() &mdash;
    Creates an unsigned integer value with a given initial raw value.
*/
extern bt_value *bt_value_integer_unsigned_create(void);

/*!
@brief
    Creates and returns an unsigned integer value initialized to
    \bt_p{raw_value}.

The returned value has the type #BT_VALUE_TYPE_UNSIGNED_INTEGER.

@param[in] raw_value
    Initial raw value of the unsigned integer value to create.

@returns
    New unsigned integer value reference, or \c NULL on memory error.

@sa bt_value_bool_create() &mdash;
    Creates an unsigned integer value initialized to 0.
*/
extern bt_value *bt_value_integer_unsigned_create_init(uint64_t raw_value);

/*!
@brief
    Sets the raw value of the unsigned integer value \bt_p{value} to
    \bt_p{raw_value}.

@param[in] value
    Unsigned integer value of which to set the raw value to
    \bt_p{raw_value}.
@param[in] raw_value
    New raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_uint_val{value}
@bt_pre_hot{value}

@sa bt_value_integer_unsigned_get() &mdash;
    Returns the raw value of an unsigned integer value.
*/
extern void bt_value_integer_unsigned_set(bt_value *value,
		uint64_t raw_value);

/*!
@brief
    Returns the raw value of the unsigned integer value \bt_p{value}.

@param[in] value
    Unsigned integer value of which to get the raw value.

@returns
    Raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_uint_val{value}

@sa bt_value_integer_unsigned_set() &mdash;
    Sets the raw value of an unsigned integer value.
*/
extern uint64_t bt_value_integer_unsigned_get(const bt_value *value);

/*! @} */

/*!
@name Signed integer value
@{
*/

/*!
@brief
    Creates and returns a signed integer value initialized to 0.

The returned value has the type #BT_VALUE_TYPE_SIGNED_INTEGER.

@returns
    New signed integer value reference, or \c NULL on memory error.

@sa bt_value_integer_signed_create_init() &mdash;
    Creates a signed integer value with a given initial raw value.
*/
extern bt_value *bt_value_integer_signed_create(void);

/*!
@brief
    Creates and returns a signed integer value initialized to
    \bt_p{raw_value}.

The returned value has the type #BT_VALUE_TYPE_SIGNED_INTEGER.

@param[in] raw_value
    Initial raw value of the signed integer value to create.

@returns
    New signed integer value reference, or \c NULL on memory error.

@sa bt_value_bool_create() &mdash;
    Creates a signed integer value initialized to 0.
*/
extern bt_value *bt_value_integer_signed_create_init(int64_t raw_value);

/*!
@brief
    Sets the raw value of the signed integer value \bt_p{value} to
    \bt_p{raw_value}.

@param[in] value
    Signed integer value of which to set the raw value to
    \bt_p{raw_value}.
@param[in] raw_value
    New raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_sint_val{value}
@bt_pre_hot{value}

@sa bt_value_integer_signed_get() &mdash;
    Returns the raw value of a signed integer value.
*/
extern void bt_value_integer_signed_set(bt_value *value, int64_t raw_value);

/*!
@brief
    Returns the raw value of the signed integer value \bt_p{value}.

@param[in] value
    Signed integer value of which to get the raw value.

@returns
    Raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_sint_val{value}

@sa bt_value_integer_signed_set() &mdash;
    Sets the raw value of a signed integer value.
*/
extern int64_t bt_value_integer_signed_get(const bt_value *value);

/*! @} */

/*!
@name Real value
@{
*/

/*!
@brief
    Creates and returns a real value initialized to 0.

The returned value has the type #BT_VALUE_TYPE_REAL.

@returns
    New real value reference, or \c NULL on memory error.

@sa bt_value_real_create_init() &mdash;
    Creates a real value with a given initial raw value.
*/
extern bt_value *bt_value_real_create(void);

/*!
@brief
    Creates and returns a real value initialized to \bt_p{raw_value}.

The returned value has the type #BT_VALUE_TYPE_REAL.

@param[in] raw_value
    Initial raw value of the real value to create.

@returns
    New real value reference, or \c NULL on memory error.

@sa bt_value_real_create() &mdash;
    Creates a real value initialized to 0.
*/
extern bt_value *bt_value_real_create_init(double raw_value);

/*!
@brief
    Sets the raw value of the real value \bt_p{value} to
    \bt_p{raw_value}.

@param[in] value
    Real value of which to set the raw value to \bt_p{raw_value}.
@param[in] raw_value
    New raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_real_val{value}
@bt_pre_hot{value}

@sa bt_value_real_get() &mdash;
    Returns the raw value of a real value.
*/
extern void bt_value_real_set(bt_value *value, double raw_value);

/*!
@brief
    Returns the raw value of the real value \bt_p{value}.

@param[in] value
    Real value of which to get the raw value.

@returns
    Raw value of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_real_val{value}

@sa bt_value_real_set() &mdash;
    Sets the raw value of a real value.
*/
extern double bt_value_real_get(const bt_value *value);

/*! @} */

/*!
@name String value
@{
*/

/*!
@brief
    Creates and returns an empty string value.

The returned value has the type #BT_VALUE_TYPE_STRING.

@returns
    New string value reference, or \c NULL on memory error.

@sa bt_value_string_create_init() &mdash;
    Creates a string value with a given initial raw value.
*/
extern bt_value *bt_value_string_create(void);

/*!
@brief
    Creates and returns a string value initialized to a copy of
    \bt_p{raw_value}.

The returned value has the type #BT_VALUE_TYPE_STRING.

@param[in] raw_value
    Initial raw value of the string value to create (copied).

@returns
    New string value reference, or \c NULL on memory error.

@bt_pre_not_null{raw_value}

@sa bt_value_string_create() &mdash;
    Creates an empty string value.
*/
extern bt_value *bt_value_string_create_init(const char *raw_value);

/*!
@brief
    Status codes for bt_value_string_set().
*/
typedef enum bt_value_string_set_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_STRING_SET_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_STRING_SET_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_value_string_set_status;

/*!
@brief
    Sets the raw value of the string value \bt_p{value} to a copy of
    \bt_p{raw_value}.

@param[in] value
    String value of which to set the raw value to a copy of
    \bt_p{raw_value}.
@param[in] raw_value
    New raw value of \bt_p{value} (copied).

@retval #BT_VALUE_STRING_SET_STATUS_OK
    Success.
@retval #BT_VALUE_STRING_SET_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_string_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{raw_value}

@sa bt_value_string_get() &mdash;
    Returns the raw value of a string value.
*/
extern bt_value_string_set_status bt_value_string_set(bt_value *value,
		const char *raw_value);

/*!
@brief
    Returns the raw value of the string value \bt_p{value}.

@param[in] value
    String value of which to get the raw value.

@returns
    @parblock
    Raw value of \bt_p{value}.

    The returned pointer remains valid until \bt_p{value} is modified.
    @endparblock

@bt_pre_not_null{value}
@bt_pre_is_string_val{value}

@sa bt_value_string_set() &mdash;
    Sets the raw value of a string value.
*/
extern const char *bt_value_string_get(const bt_value *value);

/*! @} */

/*!
@name Array value
@{
*/

/*!
@brief
    Creates and returns an empty array value.

The returned value has the type #BT_VALUE_TYPE_ARRAY.

@returns
    New array value reference, or \c NULL on memory error.
*/
extern bt_value *bt_value_array_create(void);

/*!
@brief
    Status codes for the <code>bt_value_array_append_*()</code>
    functions.
*/
typedef enum bt_value_array_append_element_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_value_array_append_element_status;

/*!
@brief
    Appends the value \bt_p{element_value} to the array value \bt_p{value}.

To append a null value, pass #bt_value_null as \bt_p{element_value}.

@param[in] value
    Array value to which to append \bt_p{element_value}.
@param[in] element_value
    Value to append to \bt_p{value}.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{element_value}
@pre
    \bt_p{element_value} does not contain \bt_p{value}, recursively.

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_bool_element() &mdash;
    Creates and appends a boolean value to an array value.
@sa bt_value_array_append_unsigned_integer_element() &mdash;
    Creates and appends an unsigned integer value to an array value.
@sa bt_value_array_append_signed_integer_element() &mdash;
    Creates and appends a signed integer value to an array value.
@sa bt_value_array_append_real_element() &mdash;
    Creates and appends a real value to an array value.
@sa bt_value_array_append_string_element() &mdash;
    Creates and appends a string value to an array value.
@sa bt_value_array_append_empty_array_element() &mdash;
    Creates and appends an empty array value to an array value.
@sa bt_value_array_append_empty_map_element() &mdash;
    Creates and appends an empty map value to an array value.
*/
extern bt_value_array_append_element_status bt_value_array_append_element(
		bt_value *value, bt_value *element_value);

/*!
@brief
    Creates a boolean value initialized to \bt_p{raw_value} and appends
    it to the array value \bt_p{value}.

@param[in] value
    Array value to which to append the created boolean value.
@param[in] raw_value
    Raw value of the boolean value to create and append to \bt_p{value}.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_bool_element(bt_value *value, bt_bool raw_value);

/*!
@brief
    Creates an unsigned integer value initialized to \bt_p{raw_value}
    and appends it to the array value \bt_p{value}.

@param[in] value
    Array value to which to append the created unsigned integer value.
@param[in] raw_value
    Raw value of the unsigned integer value to create and append to
    \bt_p{value}.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_unsigned_integer_element(bt_value *value,
		uint64_t raw_value);

/*!
@brief
    Creates a signed integer value initialized to \bt_p{raw_value} and
    appends it to the array value \bt_p{value}.

@param[in] value
    Array value to which to append the created signed integer value.
@param[in] raw_value
    Raw value of the signed integer value to create and append to
    \bt_p{value}.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_signed_integer_element(bt_value *value,
		int64_t raw_value);

/*!
@brief
    Creates a real value initialized to \bt_p{raw_value} and appends
    it to the array value \bt_p{value}.

@param[in] value
    Array value to which to append the created real value.
@param[in] raw_value
    Raw value of the real value to create and append to \bt_p{value}.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_real_element(bt_value *value, double raw_value);

/*!
@brief
    Creates a string value initialized to a copy of \bt_p{raw_value} and
    appends it to the array value \bt_p{value}.

@param[in] value
    Array value to which to append the created string value.
@param[in] raw_value
    Raw value of the string value to create and append to \bt_p{value}
    (copied).

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_string_element(bt_value *value, const char *raw_value);

/*!
@brief
    Creates an empty array value and appends it to the array
    value \bt_p{value}.

On success, if \bt_p{element_value} is not \c NULL, this function sets
\bt_p{*element_value} to a \em borrowed reference of the created empty
array value.

@param[in] value
    Array value to which to append the created empty array value.
@param[out] element_value
    <strong>On success, if not \c NULL</strong>, \bt_p{*element_value}
    is a \em borrowed reference of the created empty array value.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_empty_array_element(bt_value *value,
		bt_value **element_value);

/*!
@brief
    Creates an empty map value and appends it to the array
    value \bt_p{value}.

On success, if \bt_p{element_value} is not \c NULL, this function sets
\bt_p{*element_value} to a \em borrowed reference of the created empty
map value.

@param[in] value
    Array value to which to append the created empty array value.
@param[out] element_value
    <strong>On success, if not \c NULL</strong>, \bt_p{*element_value}
    is a \em borrowed reference of the created empty map value.

@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends an existing value to an array value.
*/
extern bt_value_array_append_element_status
bt_value_array_append_empty_map_element(bt_value *value,
		bt_value **element_value);

/*!
@brief
    Status codes for bt_value_array_set_element_by_index().
*/
typedef enum bt_value_array_set_element_by_index_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_ARRAY_SET_ELEMENT_BY_INDEX_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_ARRAY_SET_ELEMENT_BY_INDEX_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_value_array_set_element_by_index_status;

/*!
@brief
    Sets the element of the array value \bt_p{value} at index
    \bt_p{index} to the value \bt_p{element_value}.

On success, this function replaces the existing element of \bt_p{value}
at index \bt_p{index}.

@param[in] value
    Array value of which to set the element at index \bt_p{index}.
@param[in] index
    Index of the element to set in \bt_p{value}.
@param[in] element_value
    Value to set as the element of \bt_p{value} at index \bt_p{index}.

@retval #BT_VALUE_ARRAY_SET_ELEMENT_BY_INDEX_STATUS_OK
    Success.
@retval #BT_VALUE_ARRAY_SET_ELEMENT_BY_INDEX_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_hot{value}
@pre
    \bt_p{index} is less than the length of \bt_p{value} (as returned by
    bt_value_array_get_length()).
@bt_pre_not_null{element_value}
@pre
    \bt_p{element_value} does not contain \bt_p{value}, recursively.

@post
    <strong>On success</strong>, the length of \bt_p{value} is
    incremented.

@sa bt_value_array_append_element() &mdash;
    Appends a value to an array value.
*/
extern bt_value_array_set_element_by_index_status
bt_value_array_set_element_by_index(bt_value *value, uint64_t index,
		bt_value *element_value);

/*!
@brief
    Borrows the element at index \bt_p{index} from the array value
    \bt_p{value}.

@param[in] value
    Array value from which to borrow the element at index \bt_p{index}.
@param[in] index
    Index of the element to borrow from \bt_p{value}.

@returns
    @parblock
    \em Borrowed reference of the element of \bt_p{value} at index
    \bt_p{index}.

    The returned pointer remains valid until \bt_p{value} is modified.
    @endparblock

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@pre
    \bt_p{index} is less than the length of \bt_p{value} (as returned by
    bt_value_array_get_length()).

@sa bt_value_array_borrow_element_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_value_array_borrow_element_by_index(bt_value *value,
		uint64_t index);

/*!
@brief
    Borrows the element at index \bt_p{index} from the array value
    \bt_p{value} (\c const version).

See bt_value_array_borrow_element_by_index().
*/
extern const bt_value *bt_value_array_borrow_element_by_index_const(
		const bt_value *value, uint64_t index);

/*!
@brief
    Returns the length of the array value \bt_p{value}.

@param[in] value
    Array value of which to get the length.

@returns
    Length (number of contained elements) of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}

@sa bt_value_array_is_empty() &mdash;
    Returns whether or not an array value is empty.
*/
extern uint64_t bt_value_array_get_length(const bt_value *value);

/*!
@brief
    Returns whether or not the array value \bt_p{value} is empty.

@param[in] value
    Array value to check.

@returns
    #BT_TRUE if \bt_p{value} is empty (has the length&nbsp;0).

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}

@sa bt_value_array_get_length() &mdash;
    Returns the length of an array value.
*/
static inline
bt_bool bt_value_array_is_empty(const bt_value *value)
{
	return bt_value_array_get_length(value) == 0;
}

/*! @} */

/*!
@name Map value
@{
*/

/*!
@brief
    Creates and returns an empty map value.

The returned value has the type #BT_VALUE_TYPE_MAP.

@returns
    New map value reference, or \c NULL on memory error.
*/
extern bt_value *bt_value_map_create(void);

/*!
@brief
    Status codes for the <code>bt_value_map_insert_*()</code> functions.
*/
typedef enum bt_value_map_insert_entry_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_value_map_insert_entry_status;

/*!
@brief
    Inserts or replaces an entry with the key \bt_p{key} and the value
    \bt_p{entry_value} in the map value \bt_p{value}.

To insert an entry having a null value, pass #bt_value_null as
\bt_p{entry_value}.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with
\bt_p{entry_value}.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and value \bt_p{entry_value}.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[in] entry_value
    Value of the entry to insert or replace in \bt_p{value}.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}
@bt_pre_not_null{entry_value}
@pre
    \bt_p{entry_value} does not contain \bt_p{value}, recursively.

@sa bt_value_map_insert_bool_entry() &mdash;
    Creates a boolean value and uses it to insert an entry in a map
    value.
@sa bt_value_map_insert_unsigned_integer_entry() &mdash;
    Creates an unsigned integer value and uses it to insert an entry in
    a map value.
@sa bt_value_map_insert_signed_integer_entry() &mdash;
    Creates a signed value and uses it to insert an entry in a map
    value.
@sa bt_value_map_insert_real_entry() &mdash;
    Creates a real value and uses it to insert an entry in a map value.
@sa bt_value_map_insert_string_entry() &mdash;
    Creates a string value and uses it to insert an entry in a map
    value.
@sa bt_value_map_insert_empty_array_entry() &mdash;
    Creates an empty array value and uses it to insert an entry in a map
    value.
@sa bt_value_map_insert_empty_map_entry() &mdash;
    Creates a map value and uses it to insert an entry in a map value.
*/
extern bt_value_map_insert_entry_status bt_value_map_insert_entry(
		bt_value *value, const char *key, bt_value *entry_value);

/*!
@brief
    Creates a boolean value initialized to \bt_p{raw_value} and
    inserts or replaces an entry with the key \bt_p{key} and this value
    in the map value \bt_p{value}.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created boolean value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created boolean value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[in] raw_value
    Initial raw value of the boolean value to create.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status bt_value_map_insert_bool_entry(
		bt_value *value, const char *key, bt_bool raw_value);

/*!
@brief
    Creates an unsigned integer value initialized to \bt_p{raw_value}
    and inserts or replaces an entry with the key \bt_p{key} and this
    value in the map value \bt_p{value}.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created unsigned integer value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created unsigned integer value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[in] raw_value
    Initial raw value of the unsigned integer value to create.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status
bt_value_map_insert_unsigned_integer_entry(bt_value *value, const char *key,
		uint64_t raw_value);

/*!
@brief
    Creates a signed integer value initialized to \bt_p{raw_value} and
    inserts or replaces an entry with the key \bt_p{key} and this value
    in the map value \bt_p{value}.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created signed integer value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created signed integer value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[in] raw_value
    Initial raw value of the signed integer value to create.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status
bt_value_map_insert_signed_integer_entry(bt_value *value, const char *key,
		int64_t raw_value);

/*!
@brief
    Creates a real value initialized to \bt_p{raw_value} and inserts or
    replaces an entry with the key \bt_p{key} and this value in the map
    value \bt_p{value}.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created real value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created real value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[in] raw_value
    Initial raw value of the real value to create.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status bt_value_map_insert_real_entry(
		bt_value *value, const char *key, double raw_value);

/*!
@brief
    Creates a string value initialized to a copy of \bt_p{raw_value} and
    inserts or replaces an entry with the key \bt_p{key} and this value
    in the map value \bt_p{value}.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created string value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created string value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[in] raw_value
    Initial raw value of the string value to create (copied).

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status
bt_value_map_insert_string_entry(bt_value *value, const char *key,
		const char *raw_value);

/*!
@brief
    Creates an empty array value and inserts or replaces an entry with
    the key \bt_p{key} and this value in the map value \bt_p{value}.

On success, if \bt_p{entry_value} is not \c NULL, this function sets
\bt_p{*entry_value} to a \em borrowed reference of the created empty
array value.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created empty array value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created empty array value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[out] entry_value
    <strong>On success, if not \c NULL</strong>, \bt_p{*entry_value} is
    a \em borrowed reference of the created empty array value.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status
bt_value_map_insert_empty_array_entry(bt_value *value, const char *key,
		bt_value **entry_value);

/*!
@brief
    Creates an empty map value and inserts or replaces an entry with
    the key \bt_p{key} and this value in the map value \bt_p{value}.

On success, if \bt_p{entry_value} is not \c NULL, this function sets
\bt_p{*entry_value} to a \em borrowed reference of the created empty map
value.

On success, if \bt_p{value} already contains an entry with key
\bt_p{key}, this function replaces the existing entry's value with the
created empty map value.

@param[in] value
    Map value in which to insert or replace an entry with key \bt_p{key}
    and the created empty map value.
@param[in] key
    Key of the entry to insert or replace in \bt_p{value} (copied).
@param[out] entry_value
    <strong>On success, if not \c NULL</strong>, \bt_p{*entry_value} is
    a \em borrowed reference of the created empty map value.

@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_hot{value}
@bt_pre_not_null{key}

@sa bt_value_map_insert_entry() &mdash;
    Inserts an entry with an existing value in a map value.
*/
extern bt_value_map_insert_entry_status
bt_value_map_insert_empty_map_entry(bt_value *value, const char *key,
		bt_value **entry_value);

/*!
@brief
    Borrows the value of the entry with the key \bt_p{key} in the map
    value \bt_p{value}.

If no entry with key \bt_p{key} exists in \bt_p{value}, this function
returns \c NULL.

@param[in] value
    Map value from which to borrow the value of the entry with the
    key \bt_p{key}.
@param[in] key
    Key of the entry from which to borrow the value in \bt_p{value}.

@returns
    @parblock
    \em Borrowed reference of the value of the entry with key \bt_p{key}
    in \bt_p{value}, or \c NULL if none.

    The returned pointer remains valid until \bt_p{value} is modified.
    @endparblock

@bt_pre_not_null{value}
@bt_pre_is_array_val{value}
@bt_pre_not_null{key}

@sa bt_value_map_borrow_entry_value_const() &mdash;
    \c const version of this function.
@sa bt_value_map_has_entry() &mdash;
    Returns whether or not a map value has an entry with a given key.
*/
extern bt_value *bt_value_map_borrow_entry_value(
		bt_value *value, const char *key);

/*!
@brief
    Borrows the value of the entry with the key \bt_p{key} in the map
    value \bt_p{value} (\c const version).

See bt_value_map_borrow_entry_value().
*/
extern const bt_value *bt_value_map_borrow_entry_value_const(
		const bt_value *value, const char *key);

/*!
@brief
    Status codes for #bt_value_map_foreach_entry_func.
*/
typedef enum bt_value_map_foreach_entry_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Interrupt the iteration process.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_INTERRUPT	= __BT_FUNC_STATUS_INTERRUPTED,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_value_map_foreach_entry_func_status;

/*!
@brief
    User function for bt_value_map_foreach_entry().

This is the type of the user function that bt_value_map_foreach_entry()
calls for each entry of the map value.

@param[in] key
    Key of the map value entry.
@param[in] value
    Value of the map value entry.
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_value_map_foreach_entry().

@retval #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_INTERRUPT
    Interrupt the iteration process.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_ERROR
    User error.

@bt_pre_not_null{key}
@bt_pre_not_null{value}

@sa bt_value_map_foreach_entry() &mdash;
    Iterates the entries of a map value.
*/
typedef bt_value_map_foreach_entry_func_status
		(* bt_value_map_foreach_entry_func)(const char *key,
			bt_value *value, void *user_data);

/*!
@brief
    Status codes for bt_value_map_foreach_entry().
*/
typedef enum bt_value_map_foreach_entry_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    User function interrupted the iteration process.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_STATUS_INTERRUPTED	= __BT_FUNC_STATUS_INTERRUPTED,

	/*!
	@brief
	    User function error.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_STATUS_USER_ERROR	= __BT_FUNC_STATUS_USER_ERROR,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_value_map_foreach_entry_status;

/*!
@brief
    Iterates the entries of the map value \bt_p{value}, calling
    \bt_p{user_func} for each entry.

This function iterates the entries of \bt_p{value} in no particular
order.

@attention
    You must \em not modify \bt_p{value} during the iteration process.

\bt_p{user_func} receives \bt_p{user_data} as its last parameter.

The iteration process stops when one of:

- \bt_p{user_func} was called for each entry.
- \bt_p{user_func} does not return
  #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_OK.

@param[in] value
    Map value of which to iterate the entries.
@param[in] user_func
    User function to call for each entry of \bt_p{value}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of each call to
    \bt_p{user_func}.

@retval #BT_VALUE_MAP_FOREACH_ENTRY_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_STATUS_INTERRUPTED
    \bt_p{user_func} returned
    #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_INTERRUPT to interrupt the
    iteration process.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_STATUS_USER_ERROR
    \bt_p{user_func} returned
    #BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_ERROR.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_STATUS_ERROR
    Other error caused the <code>bt_value_map_foreach_entry()</code>
    function itself.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_not_null{user_func}

@sa bt_value_map_foreach_entry_const() &mdash;
    \c const version of this function.
@sa bt_value_map_borrow_entry_value() &mdash;
    Borrows the value of a specific map value entry.
*/
extern bt_value_map_foreach_entry_status bt_value_map_foreach_entry(
		bt_value *value, bt_value_map_foreach_entry_func user_func,
		void *user_data);

/*!
@brief
    Status codes for #bt_value_map_foreach_entry_const_func.
*/
typedef enum bt_value_map_foreach_entry_const_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Interrupt the iteration process.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_INTERRUPT		= __BT_FUNC_STATUS_INTERRUPTED,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_value_map_foreach_entry_const_func_status;

/*!
@brief
    User function for bt_value_map_foreach_entry_const_func().

This is the type of the user function that
bt_value_map_foreach_entry_const_func() calls for each entry of the map
value.

@param[in] key
    Key of the map value entry.
@param[in] value
    Value of the map value entry.
@param[in] user_data
    User data.

@retval #BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_INTERRUPT
    Interrupt the iteration process.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_ERROR
    User error.

@bt_pre_not_null{key}
@bt_pre_not_null{value}

@sa bt_value_map_foreach_entry_const() &mdash;
    Iterates the entries of a \c const map value.
*/
typedef bt_value_map_foreach_entry_const_func_status
		(* bt_value_map_foreach_entry_const_func)(const char *key,
			const bt_value *value, void *user_data);

/*!
@brief
    Status codes for bt_value_map_foreach_entry_const().
*/
typedef enum bt_value_map_foreach_entry_const_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    User function interrupted the iteration process.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_INTERRUPTED	= __BT_FUNC_STATUS_INTERRUPTED,

	/*!
	@brief
	    User function error.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_USER_ERROR	= __BT_FUNC_STATUS_USER_ERROR,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_value_map_foreach_entry_const_status;

/*!
@brief
    Iterates the entries of the map value \bt_p{value}, calling
    \bt_p{user_func} for each entry (\c const version).

See bt_value_map_foreach_entry().

@sa bt_value_map_borrow_entry_value_const() &mdash;
    Borrows the value of a specific map value entry.
*/
extern bt_value_map_foreach_entry_const_status bt_value_map_foreach_entry_const(
		const bt_value *value,
		bt_value_map_foreach_entry_const_func user_func,
		void *user_data);

/*!
@brief
    Returns the size of the map value \bt_p{value}.

@param[in] value
    Map value of which to get the size.

@returns
    Size (number of contained entries) of \bt_p{value}.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}

@sa bt_value_map_is_empty() &mdash;
    Returns whether or not a map value is empty.
*/
extern uint64_t bt_value_map_get_size(const bt_value *value);

/*!
@brief
    Returns whether or not the map value \bt_p{value} is empty.

@param[in] value
    Map value to check.

@returns
    #BT_TRUE if \bt_p{value} is empty (has the size&nbsp;0).

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}

@sa bt_value_map_get_size() &mdash;
    Returns the size of a map value.
*/
static inline
bt_bool bt_value_map_is_empty(const bt_value *value)
{
	return bt_value_map_get_size(value) == 0;
}

/*!
@brief
    Returns whether or not the map value \bt_p{value} has an entry with
    the key \bt_p{key}.

@param[in] value
    Map value to check.
@param[in] key
    Key to check.

@returns
    #BT_TRUE if \bt_p{value} has an entry with the key \bt_p{key}.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_not_null{key}

@sa bt_value_map_borrow_entry_value_const() &mdash;
    Borrows the value of a specific map value entry.
*/
extern bt_bool bt_value_map_has_entry(const bt_value *value,
		const char *key);

/*!
@brief
    Status codes for bt_value_map_extend().
*/
typedef enum bt_value_map_extend_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_MAP_EXTEND_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_MAP_EXTEND_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_value_map_extend_status;

/*!
@brief
    Extends the map value \bt_p{value} with the map value
    \bt_p{extension_value}.

For each entry in \bt_p{extension_value}, this function calls
bt_value_map_insert_entry() to insert or replace it in the map value
\bt_p{value}.

For example, with:

<dl>
  <dt>
    \bt_p{value}
  </dt>
  <dd>
    @code{.json}
    {
        "man": "giant",
        "strange": 23
    }
    @endcode
  </dd>

  <dt>
    \bt_p{extension_value}
  </dt>
  <dd>
    @code{.json}
    {
        "balance": -17
        "strange": false
    }
    @endcode
  </dd>
</dl>

The map value \bt_p{value} becomes:

@code{.json}
{
    "man": "giant",
    "strange": false,
    "balance": -17
}
@endcode

@param[in] value
    Map value to extend.
@param[in] extension_value
    Extension map value.

@retval #BT_VALUE_MAP_EXTEND_STATUS_OK
    Success.
@retval #BT_VALUE_MAP_EXTEND_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_is_map_val{value}
@bt_pre_not_null{extension_value}
@bt_pre_is_map_val{extension_value}

@sa bt_value_copy() &mdash;
    Deep-copies a value.
*/
extern bt_value_map_extend_status bt_value_map_extend(
		bt_value *value, const bt_value *extension_value);

/*! @} */

/*!
@name General
@{
*/

/*!
@brief
    Status codes for bt_value_copy().
*/
typedef enum bt_value_copy_status {
	/*!
	@brief
	    Success.
	*/
	BT_VALUE_COPY_STATUS_OK     = __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_VALUE_COPY_STATUS_MEMORY_ERROR = __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_value_copy_status;

/*!
@brief
    Deep-copies a value object.

This function deep-copies \bt_p{value} and sets \bt_p{*copy} to the
result.

Because \bt_p{*copy} is a deep copy, it does not contain, recursively,
any reference that \bt_p{value} contains, but the raw values are
identical.

@param[in] value
    Value to deep-copy.
@param[in] copy_value
    <strong>On success</strong>, \bt_p{*copy_value} is a deep copy of
    \bt_p{value}.

@retval #BT_VALUE_COPY_STATUS_OK
    Success.
@retval #BT_VALUE_COPY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{value}
@bt_pre_not_null{copy_value}
*/
extern bt_value_copy_status bt_value_copy(const bt_value *value,
		bt_value **copy_value);

/*!
@brief
    Returns whether or not the value \bt_p{a_value} is equal,
    recursively, to \bt_p{b_value}.

@note
    If you want to know whether or not a value is a null value, you can
    also directly compare its pointer to the #bt_value_null variable.

@param[in] a_value
    Value A.
@param[in] b_value
    Value B.

@returns
    #BT_TRUE if \bt_p{a_value} is equal, recursively, to \bt_p{b_value}.

@bt_pre_not_null{a_value}
@bt_pre_not_null{b_value}
*/
extern bt_bool bt_value_is_equal(const bt_value *a_value,
		const bt_value *b_value);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the value \bt_p{value}.

@param[in] value
    @parblock
    Value of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_value_put_ref() &mdash;
    Decrements the reference count of a value.
*/
extern void bt_value_get_ref(const bt_value *value);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the value \bt_p{value}.

@param[in] value
    @parblock
    Value of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_value_get_ref() &mdash;
    Increments the reference count of a value.
*/
extern void bt_value_put_ref(const bt_value *value);

/*!
@brief
    Decrements the reference count of the value \bt_p{_value}, and then
    sets \bt_p{_value} to \c NULL.

@param _value
    @parblock
    Value of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_value}
*/
#define BT_VALUE_PUT_REF_AND_RESET(_value)	\
	do {					\
		bt_value_put_ref(_value);	\
		(_value) = NULL;		\
	} while (0)

/*!
@brief
    Decrements the reference count of the value \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a value reference from the expression
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
#define BT_VALUE_MOVE_REF(_dst, _src)	\
	do {				\
		bt_value_put_ref(_dst);	\
		(_dst) = (_src);	\
		(_src) = NULL;		\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_VALUE_H */
