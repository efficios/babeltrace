#ifndef BABELTRACE_VALUES_H
#define BABELTRACE_VALUES_H

/*
 * Babeltrace - Value objects
 *
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2016 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <stddef.h>

/* For bt_bool */
#include <babeltrace/types.h>

/* For bt_get() */
#include <babeltrace/ref.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup values Value objects
@ingroup apiref
@brief Value objects.

@code
#include <babeltrace/values.h>
@endcode

This is a set of <strong><em>value objects</em></strong>. With the
functions documented here, you can create and modify:

- \link bt_value_bool_create() Boolean value objects\endlink.
- \link bt_value_integer_create() Integer value objects\endlink.
- \link bt_value_float_create() Floating point number
  value objects\endlink.
- \link bt_value_string_create() String value objects\endlink.
- \link bt_value_array_create() Array value objects\endlink,
  containing zero or more value objects.
- \link bt_value_map_create() Map value objects\endlink, mapping
  string keys to value objects.

As with any Babeltrace object, value objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. When you \link bt_value_array_append_element() append a value object
to an array value object\endlink, or when you \link bt_value_map_insert_entry()
insert a value object into a map value object\endlink, its reference
count is incremented, as well as when you get a value object back from
those objects. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

Most functions of this API return a <em>status code</em>, one of the
values of #bt_value_status.

You can create a deep copy of any value object with bt_value_copy(). You
can compare two value objects with bt_value_compare().

The following matrix shows some categorized value object functions
to use for each value object type:

<table>
  <tr>
    <th>Function role &rarr;<br>
        Value object type &darr;
    <th>Create
    <th>Check type
    <th>Get value
    <th>Set value
  </tr>
  <tr>
    <th>Null
    <td>Use the \ref bt_value_null variable
    <td>bt_value_is_null()
    <td>N/A
    <td>N/A
  </tr>
  <tr>
    <th>Boolean
    <td>bt_value_bool_create()<br>
        bt_value_bool_create_init()
    <td>bt_value_is_bool()
    <td>bt_value_bool_get()
    <td>bt_value_bool_set()
  </tr>
  <tr>
    <th>Integer
    <td>bt_value_integer_create()<br>
        bt_value_integer_create_init()
    <td>bt_value_is_integer()
    <td>bt_value_integer_get()
    <td>bt_value_integer_set()
  </tr>
  <tr>
    <th>Floating point<br>number
    <td>bt_value_float_create()<br>
        bt_value_float_create_init()
    <td>bt_value_is_float()
    <td>bt_value_float_get()
    <td>bt_value_float_set()
  </tr>
  <tr>
    <th>String
    <td>bt_value_string_create()<br>
        bt_value_string_create_init()
    <td>bt_value_is_string()
    <td>bt_value_string_get()
    <td>bt_value_string_set()
  </tr>
  <tr>
    <th>Array
    <td>bt_value_array_create()
    <td>bt_value_is_array()
    <td>bt_value_array_get()
    <td>bt_value_array_append_element()<br>
        bt_value_array_append_bool_element()<br>
        bt_value_array_append_integer_element()<br>
        bt_value_array_append_element_float()<br>
        bt_value_array_append_string_element()<br>
        bt_value_array_append_empty_array_element()<br>
        bt_value_array_append_empty_map_element()<br>
        bt_value_array_set_element_by_index()
  </tr>
  <tr>
    <th>Map
    <td>bt_value_map_create()<br>
        bt_value_map_extend()
    <td>bt_value_is_map()
    <td>bt_value_map_get()<br>
        bt_value_map_foreach_entry()
    <td>bt_value_map_insert_entry()<br>
        bt_value_map_insert_bool_entry()<br>
        bt_value_map_insert_integer_entry()<br>
        bt_value_map_insert_float_entry()<br>
        bt_value_map_insert_string_entry()<br>
        bt_value_map_insert_empty_array_entry()<br>
        bt_value_map_insert_empty_map_entry()
  </tr>
</table>

@file
@brief Value object types and functions.
@sa values

@addtogroup values
@{
*/

/**
@brief Status codes.
*/
enum bt_value_status {
	/// Operation canceled.
	BT_VALUE_STATUS_CANCELED =	-3,

	/* -22 for compatibility with -EINVAL */
	/// Invalid argument.
	BT_VALUE_STATUS_INVAL =		-22,

	/// General error.
	BT_VALUE_STATUS_ERROR =		-1,

	/// Okay, no error.
	BT_VALUE_STATUS_OK =		0,
};

/**
@struct bt_value
@brief A value object.
@sa values
*/
struct bt_value;

/**
@brief	The null value object singleton.

You \em must use this variable when you need the null value object.

The null value object singleton has no reference count: there is only
one. You can compare any value object address to the null value object
singleton to check if it's the null value object, or otherwise with
bt_value_is_null().

You can pass \ref bt_value_null to bt_get() or bt_put(): it has
<em>no effect</em>.

The null value object singleton is <em>always frozen</em> (see
bt_value_is_frozen()).

The functions of this API return this variable when the value object
is actually the null value object (of type #BT_VALUE_TYPE_NULL),
whereas \c NULL means an error of some sort.
*/
extern struct bt_value *bt_value_null;

/**
@name Type information
@{
*/

/**
@brief Value object type.
*/
enum bt_value_type {
	/// Null value object.
	BT_VALUE_TYPE_NULL =		0,

	/// Boolean value object (holds #BT_TRUE or #BT_FALSE).
	BT_VALUE_TYPE_BOOL =		1,

	/// Integer value object (holds a signed 64-bit integer raw value).
	BT_VALUE_TYPE_INTEGER =		2,

	/// Floating point number value object (holds a \c double raw value).
	BT_VALUE_TYPE_REAL =		3,

	/// String value object.
	BT_VALUE_TYPE_STRING =		4,

	/// Array value object.
	BT_VALUE_TYPE_ARRAY =		5,

	/// Map value object.
	BT_VALUE_TYPE_MAP =		6,
};

/**
@brief	Returns the type of the value object \p object.

@param[in] object	Value object of which to get the type.
@returns		Type of value object \p object,
			or #BT_VALUE_TYPE_UNKNOWN on error.

@prenotnull{object}
@postrefcountsame{object}

@sa #bt_value_type: Value object types.
@sa bt_value_is_null(): Returns whether or not a given value object
	is the null value object.
@sa bt_value_is_bool(): Returns whether or not a given value object
	is a boolean value object.
@sa bt_value_is_integer(): Returns whether or not a given value
	object is an integer value object.
@sa bt_value_is_float(): Returns whether or not a given value object
	is a floating point number value object.
@sa bt_value_is_string(): Returns whether or not a given value object
	is a string value object.
@sa bt_value_is_array(): Returns whether or not a given value object
	is an array value object.
@sa bt_value_is_map(): Returns whether or not a given value object
	is a map value object.
*/
extern enum bt_value_type bt_value_get_type(const struct bt_value *object);

/**
@brief	Returns whether or not the value object \p object is the null
	value object.

The only valid null value object is \ref bt_value_null.

An alternative to calling this function is to directly compare the value
object pointer to the \ref bt_value_null variable.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is the null value object.

@prenotnull{object}
@postrefcountsame{object}

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_null(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_NULL;
}

/**
@brief	Returns whether or not the value object \p object is a boolean
	value object.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is a boolean value object.

@prenotnull{object}
@postrefcountsame{object}

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_bool(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_BOOL;
}

/**
@brief	Returns whether or not the value object \p object is an integer
	value object.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is an integer value object.

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_integer(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_INTEGER;
}

/**
@brief	Returns whether or not the value object \p object is a floating
	point number value object.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is a floating point
			number value object.

@prenotnull{object}
@postrefcountsame{object}

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_real(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_REAL;
}

/**
@brief	Returns whether or not the value object \p object is a string
	value object.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is a string value object.

@prenotnull{object}
@postrefcountsame{object}

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_string(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_STRING;
}

/**
@brief	Returns whether or not the value object \p object is an array
	value object.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is an array value object.

@prenotnull{object}
@postrefcountsame{object}

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_array(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_ARRAY;
}

/**
@brief	Returns whether or not the value object \p object is a map value
	object.

@param[in] object	Value object to check.
@returns		#BT_TRUE if \p object is a map value object.

@prenotnull{object}
@postrefcountsame{object}

@sa bt_value_get_type(): Returns the type of a given value object.
*/
static inline
bt_bool bt_value_is_map(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_MAP;
}

/** @} */

/**
@name Common value object functions
@{
*/

/**
@brief	Creates a \em deep copy of the value object \p object.

You can copy a frozen value object: the resulting copy is
<em>not frozen</em>.

@param[in] object	Value object to copy.
@returns		Deep copy of \p object on success, or \c NULL
			on error.

@prenotnull{object}
@post <strong>On success, if the returned value object is not
	\ref bt_value_null</strong>, its reference count is 1.
@postrefcountsame{object}
*/
extern struct bt_value *bt_value_copy(const struct bt_value *object);

/**
@brief	Recursively compares the value objects \p object_a and
	\p object_b and returns #BT_TRUE if they have the same
	\em content (raw values).

@param[in] object_a	Value object A to compare to \p object_b.
@param[in] object_b	Value object B to compare to \p object_a.
@returns		#BT_TRUE if \p object_a and \p object_b have the
			same \em content, or #BT_FALSE if they differ
			or on error.

@postrefcountsame{object_a}
@postrefcountsame{object_b}
*/
extern bt_bool bt_value_compare(const struct bt_value *object_a,
		const struct bt_value *object_b);

/** @} */

/**
@name Boolean value object functions
@{
*/

/**
@brief	Creates a default boolean value object.

The created boolean value object's initial raw value is #BT_FALSE.

@returns	Created boolean value object on success, or \c NULL
		on error.

@postsuccessrefcountret1

@sa bt_value_bool_create_init(): Creates an initialized boolean
	value object.
*/
extern struct bt_value *bt_value_bool_create(void);

/**
@brief	Creates a boolean value object with its initial raw value set to
	\p val.

@param[in] val	Initial raw value.
@returns	Created boolean value object on success, or
		\c NULL on error.

@postsuccessrefcountret1

@sa bt_value_bool_create(): Creates a default boolean value object.
*/
extern struct bt_value *bt_value_bool_create_init(bt_bool val);

/**
@brief	Returns the boolean raw value of the boolean value object
	\p bool_obj.

@param[in] bool_obj	Boolean value object of which to get the
			raw value.
@param[out] val		Returned boolean raw value.
@returns		Status code.

@prenotnull{bool_obj}
@prenotnull{val}
@pre \p bool_obj is a boolean value object.
@postrefcountsame{bool_obj}

@sa bt_value_bool_set(): Sets the raw value of a boolean value object.
*/
extern enum bt_value_status bt_value_bool_get(
		const struct bt_value *bool_obj, bt_bool *val);

/**
@brief	Sets the boolean raw value of the boolean value object
	\p bool_obj to \p val.

@param[in] bool_obj	Boolean value object of which to set
			the raw value.
@param[in] val		New boolean raw value.
@returns		Status code.

@prenotnull{bool_obj}
@pre \p bool_obj is a boolean value object.
@prehot{bool_obj}
@postrefcountsame{bool_obj}

@sa bt_value_bool_get(): Returns the raw value of a given boolean
	value object.
*/
extern enum bt_value_status bt_value_bool_set(struct bt_value *bool_obj,
		bt_bool val);

/** @} */

/**
@name Integer value object functions
@{
*/

/**
@brief	Creates a default integer value object.

The created integer value object's initial raw value is 0.

@returns	Created integer value object on success, or \c NULL
		on error.

@postsuccessrefcountret1

@sa bt_value_integer_create_init(): Creates an initialized integer
	value object.
*/
extern struct bt_value *bt_value_integer_create(void);

/**
@brief	Creates an integer value object with its initial raw value set to
	\p val.

@param[in] val	Initial raw value.
@returns	Created integer value object on success, or
		\c NULL on error.

@postsuccessrefcountret1

@sa bt_value_integer_create(): Creates a default integer
	value object.
*/
extern struct bt_value *bt_value_integer_create_init(int64_t val);

/**
@brief	Returns the integer raw value of the integer value object
	\p integer_obj.

@param[in] integer_obj	Integer value object of which to get the
			raw value.
@param[out] val		Returned integer raw value.
@returns		Status code.

@prenotnull{integer_obj}
@prenotnull{val}
@pre \p integer_obj is an integer value object.
@postrefcountsame{integer_obj}

@sa bt_value_integer_set(): Sets the raw value of an integer value
	object.
*/
extern enum bt_value_status bt_value_integer_get(
		const struct bt_value *integer_obj, int64_t *val);

/**
@brief	Sets the integer raw value of the integer value object
	\p integer_obj to \p val.

@param[in] integer_obj	Integer value object of which to set
			the raw value.
@param[in] val		New integer raw value.
@returns		Status code.

@prenotnull{integer_obj}
@pre \p integer_obj is an integer value object.
@prehot{integer_obj}
@postrefcountsame{integer_obj}

@sa bt_value_integer_get(): Returns the raw value of a given integer
	value object.
*/
extern enum bt_value_status bt_value_integer_set(
		struct bt_value *integer_obj, int64_t val);

/** @} */

/**
@name Floating point number value object functions
@{
*/

/**
@brief	Creates a default floating point number value object.

The created floating point number value object's initial raw value is 0.

@returns	Created floating point number value object on success,
		or \c NULL on error.

@postsuccessrefcountret1

@sa bt_value_float_create_init(): Creates an initialized floating
	point number value object.
*/
extern struct bt_value *bt_value_real_create(void);

/**
@brief	Creates a floating point number value object with its initial raw
	value set to \p val.

@param[in] val	Initial raw value.
@returns	Created floating point number value object on
		success, or \c NULL on error.

@postsuccessrefcountret1

@sa bt_value_float_create(): Creates a default floating point number
	value object.
*/
extern struct bt_value *bt_value_real_create_init(double val);

/**
@brief	Returns the floating point number raw value of the floating point
	number value object \p float_obj.

@param[in] float_obj	Floating point number value object of which to
			get the raw value.
@param[out] val		Returned floating point number raw value
@returns		Status code.

@prenotnull{float_obj}
@prenotnull{val}
@pre \p float_obj is a floating point number value object.
@postrefcountsame{float_obj}

@sa bt_value_float_set(): Sets the raw value of a given floating
	point number value object.
*/
extern enum bt_value_status bt_value_real_get(
		const struct bt_value *real_obj, double *val);

/**
@brief	Sets the floating point number raw value of the floating point
	number value object \p float_obj to \p val.

@param[in] float_obj	Floating point number value object of which to set
			the raw value.
@param[in] val		New floating point number raw value.
@returns		Status code.

@prenotnull{float_obj}
@pre \p float_obj is a floating point number value object.
@prehot{float_obj}
@postrefcountsame{float_obj}

@sa bt_value_float_get(): Returns the raw value of a floating point
	number value object.
*/
extern enum bt_value_status bt_value_real_set(
		struct bt_value *real_obj, double val);

/** @} */

/**
@name String value object functions
@{
*/

/**
@brief	Creates a default string value object.

The string value object is initially empty.

@returns	Created string value object on success, or \c NULL
		on error.

@postsuccessrefcountret1

@sa bt_value_string_create_init(): Creates an initialized string
	value object.
*/
extern struct bt_value *bt_value_string_create(void);

/**
@brief	Creates a string value object with its initial raw value set to
	\p val.

On success, \p val is copied.

@param[in] val		Initial raw value (copied on success).
@returns		Created string value object on success, or
			\c NULL on error.

@prenotnull{val}
@postsuccessrefcountret1

@sa bt_value_float_create(): Creates a default string value object.
*/
extern struct bt_value *bt_value_string_create_init(const char *val);

/**
@brief	Returns the string raw value of the string value object
	\p string_obj.

The returned string is placed in \p *val. It is valid as long as this
value object exists and is \em not modified. The ownership of the
returned string is \em not transferred to the caller.

@param[in] string_obj	String value object of which to get the
			raw value.
@param[out] val		Returned string raw value.
@returns		Status code.

@prenotnull{string_obj}
@prenotnull{val}
@pre \p string_obj is a string value object.
@postrefcountsame{string_obj}

@sa bt_value_string_set(): Sets the raw value of a string
	value object.
*/
extern enum bt_value_status bt_value_string_get(
		const struct bt_value *string_obj, const char **val);

/**
@brief	Sets the string raw value of the string value object
	\p string_obj to \p val.

On success, \p val is copied.

@param[in] string_obj	String value object of which to set
			the raw value.
@param[in] val		New string raw value (copied on success).
@returns		Status code.

@prenotnull{string_obj}
@prenotnull{val}
@pre \p string_obj is a string value object.
@prehot{string_obj}
@postrefcountsame{string_obj}

@sa bt_value_string_get(): Returns the raw value of a given string
	value object.
*/
extern enum bt_value_status bt_value_string_set(struct bt_value *string_obj,
		const char *val);

/**
 * @}
 */

/**
 * @name Array value object functions
 * @{
 */

/**
@brief	Creates an empty array value object.

@returns	Created array value object on success, or \c NULL
		on error.

@postsuccessrefcountret1
*/
extern struct bt_value *bt_value_array_create(void);

/**
@brief	Returns the size of the array value object \p array_obj, that is,
	the number of value objects contained in \p array_obj.

@param[in] array_obj	Array value object of which to get the size.
@returns		Number of value objects contained in
			\p array_obj if the return value is 0 (empty)
			or a positive value, or one of
			#bt_value_status negative values otherwise.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@postrefcountsame{array_obj}

@sa bt_value_array_is_empty(): Checks whether or not a given array
	value object is empty.
*/
extern int64_t bt_value_array_get_size(const struct bt_value *array_obj);

/**
@brief	Checks whether or not the array value object \p array_obj
	is empty.

@param[in] array_obj	Array value object to check.
@returns		#BT_TRUE if \p array_obj is empty.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@postrefcountsame{array_obj}

@sa bt_value_array_get_size(): Returns the size of a given array value
	object.
*/
extern bt_bool bt_value_array_is_empty(const struct bt_value *array_obj);

extern struct bt_value *bt_value_array_borrow_element_by_index(
		const struct bt_value *array_obj, uint64_t index);

/**
@brief	Appends the value object \p element_obj to the array value
	object \p array_obj.

@param[in] array_obj	Array value object in which to append
			\p element_obj.
@param[in] element_obj	Value object to append.
@returns		Status code.

@prenotnull{array_obj}
@prenotnull{element_obj}
@pre \p array_obj is an array value object.
@prehot{array_obj}
@post <strong>On success, if \p element_obj is not
	\ref bt_value_null</strong>, its reference count is incremented.
@postrefcountsame{array_obj}

@sa bt_value_array_append_bool_element(): Appends a boolean raw value to a
	given array value object.
@sa bt_value_array_append_integer_element(): Appends an integer raw value
	to a given array value object.
@sa bt_value_array_append_element_float(): Appends a floating point number
	raw value to a given array value object.
@sa bt_value_array_append_string_element(): Appends a string raw value to a
	given array value object.
@sa bt_value_array_append_empty_array_element(): Appends an empty array value
	object to a given array value object.
@sa bt_value_array_append_empty_map_element(): Appends an empty map value
	object to a given array value object.
*/
extern enum bt_value_status bt_value_array_append_element(
		struct bt_value *array_obj, struct bt_value *element_obj);

/**
@brief	Appends the boolean raw value \p val to the array value object
	\p array_obj.

This is a convenience function which creates the underlying boolean
value object before appending it.

@param[in] array_obj	Array value object in which to append \p val.
@param[in] val		Boolean raw value to append to \p array_obj.
@returns		Status code.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@prehot{array_obj}
@postrefcountsame{array_obj}

@sa bt_value_array_append_element(): Appends a value object to a given
	array value object.
*/
extern enum bt_value_status bt_value_array_append_bool_element(
		struct bt_value *array_obj, bt_bool val);

/**
@brief	Appends the integer raw value \p val to the array value object
	\p array_obj.

This is a convenience function which creates the underlying integer
value object before appending it.

@param[in] array_obj	Array value object in which to append \p val.
@param[in] val		Integer raw value to append to \p array_obj.
@returns		Status code.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@prehot{array_obj}
@postrefcountsame{array_obj}

@sa bt_value_array_append_element(): Appends a value object to a given
	array value object.
*/
extern enum bt_value_status bt_value_array_append_integer_element(
		struct bt_value *array_obj, int64_t val);

/**
@brief	Appends the floating point number raw value \p val to the array
	value object \p array_obj.

This is a convenience function which creates the underlying floating
point number value object before appending it.

@param[in] array_obj	Array value object in which to append \p val.
@param[in] val		Floating point number raw value to append
			to \p array_obj.
@returns		Status code.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@prehot{array_obj}
@postrefcountsame{array_obj}

@sa bt_value_array_append_element(): Appends a value object to a given
	array value object.
*/
extern enum bt_value_status bt_value_array_append_real_element(
		struct bt_value *array_obj, double val);

/**
@brief	Appends the string raw value \p val to the array value object
	\p array_obj.

This is a convenience function which creates the underlying string value
object before appending it.

On success, \p val is copied.

@param[in] array_obj	Array value object in which to append \p val.
@param[in] val		String raw value to append to \p array_obj
			(copied on success).
@returns		Status code.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@prenotnull{val}
@prehot{array_obj}
@postrefcountsame{array_obj}

@sa bt_value_array_append_element(): Appends a value object to a given
	array value object.
*/
extern enum bt_value_status bt_value_array_append_string_element(
		struct bt_value *array_obj, const char *val);

/**
@brief	Appends an empty array value object to the array value object
	\p array_obj.

This is a convenience function which creates the underlying array value
object before appending it.

@param[in] array_obj	Array value object in which to append an
			empty array value object
@returns		Status code.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@prehot{array_obj}
@postrefcountsame{array_obj}

@sa bt_value_array_append_element(): Appends a value object to a given
	array value object.
*/
extern enum bt_value_status bt_value_array_append_empty_array_element(
		struct bt_value *array_obj);

/**
@brief	Appends an empty map value object to the array value object
	\p array_obj.

This is a convenience function which creates the underlying map value
object before appending it.

@param[in] array_obj	Array value object in which to append an empty
			map value object.
@returns		Status code.

@prenotnull{array_obj}
@pre \p array_obj is an array value object.
@prehot{array_obj}
@postrefcountsame{array_obj}

@sa bt_value_array_append_element(): Appends a value object to a given
	array value object.
*/
extern enum bt_value_status bt_value_array_append_empty_map_element(
		struct bt_value *array_obj);

/**
@brief	Replaces the value object contained in the array value object
	\p array_obj at the index \p index by \p element_obj.

@param[in] array_obj		Array value object in which to replace
				an element.
@param[in] index		Index of value object to replace in
				\p array_obj.
@param[in] element_obj		New value object at position \p index of
				\p array_obj.
@returns			Status code.

@prenotnull{array_obj}
@prenotnull{element_obj}
@pre \p array_obj is an array value object.
@pre \p index is lesser than the size of \p array_obj (see
	bt_value_array_get_size()).
@prehot{array_obj}
@post <strong>On success, if the replaced value object is not
	\ref bt_value_null</strong>, its reference count is decremented.
@post <strong>On success, if \p element_obj is not
	\ref bt_value_null</strong>, its reference count is incremented.
@postrefcountsame{array_obj}
*/
extern enum bt_value_status bt_value_array_set_element_by_index(
		struct bt_value *array_obj, uint64_t index,
		struct bt_value *element_obj);

/** @} */

/**
@name Map value object functions
@{
*/

/**
@brief	Creates an empty map value object.

@returns	Created map value object on success, or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_value *bt_value_map_create(void);

/**
@brief	Returns the size of the map value object \p map_obj, that is, the
	number of entries contained in \p map_obj.

@param[in] map_obj	Map value object of which to get the size.
@returns                Number of entries contained in \p map_obj if the
			return value is 0 (empty) or a positive value,
			or one of #bt_value_status negative values
			otherwise.

@prenotnull{map_obj}
@pre \p map_obj is a map value object.
@postrefcountsame{map_obj}

@sa bt_value_map_is_empty(): Checks whether or not a given map value
	object is empty.
*/
extern int64_t bt_value_map_get_size(const struct bt_value *map_obj);

/**
@brief	Checks whether or not the map value object \p map_obj is empty.

@param[in] map_obj	Map value object to check.
@returns		#BT_TRUE if \p map_obj is empty.

@prenotnull{map_obj}
@pre \p map_obj is a map value object.
@postrefcountsame{map_obj}

@sa bt_value_map_get_size(): Returns the size of a given map value object.
*/
extern bt_bool bt_value_map_is_empty(const struct bt_value *map_obj);

extern struct bt_value *bt_value_map_borrow_entry_value(
		const struct bt_value *map_obj, const char *key);

/**
@brief	User function type to use with bt_value_map_foreach_entry().

\p object is a <em>weak reference</em>: you \em must pass it to bt_get()
if you need to keep a reference after this function returns.

This function \em must return #BT_TRUE to continue the map value object
traversal, or #BT_FALSE to break it.

@param[in] key		Key of map entry.
@param[in] object	Value object of map entry (weak reference).
@param[in] data		User data.
@returns		#BT_TRUE to continue the loop, or #BT_FALSE to break it.

@prenotnull{key}
@prenotnull{object}
@post The reference count of \p object is not lesser than what it is
	when the function is called.
*/
typedef bt_bool (* bt_value_map_foreach_entry_cb)(const char *key,
	struct bt_value *object, void *data);

/**
@brief	Calls a provided user function \p cb for each value object of the
	map value object \p map_obj.

The value object passed to the user function is a <b>weak reference</b>:
you \em must pass it to bt_get() if you need to keep a persistent
reference after the user function returns.

The key passed to the user function is only valid in the scope of
this user function call.

The user function \em must return #BT_TRUE to continue the traversal of
\p map_obj, or #BT_FALSE to break it.

@param[in] map_obj	Map value object on which to iterate.
@param[in] cb		User function to call back.
@param[in] data		User data passed to the user function.
@returns		Status code. More
			specifically, #BT_VALUE_STATUS_CANCELED is
			returned if the loop was canceled by the user
			function.

@prenotnull{map_obj}
@prenotnull{cb}
@pre \p map_obj is a map value object.
@postrefcountsame{map_obj}
*/
extern enum bt_value_status bt_value_map_foreach_entry(
		const struct bt_value *map_obj,
		bt_value_map_foreach_entry_cb cb, void *data);

/**
@brief	Returns whether or not the map value object \p map_obj contains
	an entry mapped to the key \p key.

@param[in] map_obj	Map value object to check.
@param[in] key		Key to check.
@returns		#BT_TRUE if \p map_obj has an entry mapped to the
			key \p key, or #BT_FALSE if it does not or
			on error.

@prenotnull{map_obj}
@prenotnull{key}
@pre \p map_obj is a map value object.
@postrefcountsame{map_obj}
*/
extern bt_bool bt_value_map_has_entry(const struct bt_value *map_obj,
		const char *key);

/**
@brief	Inserts the value object \p element_obj mapped to the key
	\p key into the map value object \p map_obj.

If a value object is already mapped to \p key in \p map_obj, the
associated value object is first put, and then replaced by
\p element_obj.

On success, \p key is copied.

@param[in] map_obj	Map value object in which to insert
			\p element_obj.
@param[in] key		Key (copied on success) to which the
			value object to insert is mapped.
@param[in] element_obj	Value object to insert, mapped to the
			key \p key.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@prenotnull{element_obj}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@post <strong>On success, if \p element_obj is not
	\ref bt_value_null</strong>, its reference count is incremented.
@postrefcountsame{map_obj}

@sa bt_value_map_insert_bool_entry(): Inserts a boolean raw value into a
	given map value object.
@sa bt_value_map_insert_integer_entry(): Inserts an integer raw value into
	a given map value object.
@sa bt_value_map_insert_float_entry(): Inserts a floating point number raw
	value into a given map value object.
@sa bt_value_map_insert_string_entry(): Inserts a string raw value into a
	given map value object.
@sa bt_value_map_insert_empty_array_entry(): Inserts an empty array value
	object into a given map value object.
@sa bt_value_map_insert_empty_map_entry(): Inserts an empty map value
	object into a given map value object.
*/
extern enum bt_value_status bt_value_map_insert_entry(
		struct bt_value *map_obj, const char *key,
		struct bt_value *element_obj);

/**
@brief	Inserts the boolean raw value \p val mapped to the key \p key
	into the map value object \p map_obj.

This is a convenience function which creates the underlying boolean
value object before inserting it.

On success, \p key is copied.

@param[in] map_obj	Map value object in which to insert \p val.
@param[in] key		Key (copied on success) to which the boolean
			value object to insert is mapped.
@param[in] val		Boolean raw value to insert, mapped to
			the key \p key.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@postrefcountsame{map_obj}

@sa bt_value_map_insert_entry(): Inserts a value object into a given map
	value object.
*/
extern enum bt_value_status bt_value_map_insert_bool_entry(
		struct bt_value *map_obj, const char *key, bt_bool val);

/**
@brief	Inserts the integer raw value \p val mapped to the key \p key
	into the map value object \p map_obj.

This is a convenience function which creates the underlying integer
value object before inserting it.

On success, \p key is copied.

@param[in] map_obj	Map value object in which to insert \p val.
@param[in] key		Key (copied on success) to which the integer
			value object to insert is mapped.
@param[in] val		Integer raw value to insert, mapped to
			the key \p key.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@postrefcountsame{map_obj}

@sa bt_value_map_insert_entry(): Inserts a value object into a given map
	value object.
*/
extern enum bt_value_status bt_value_map_insert_integer_entry(
		struct bt_value *map_obj, const char *key, int64_t val);

/**
@brief	Inserts the floating point number raw value \p val mapped to
	the key \p key into the map value object \p map_obj.

This is a convenience function which creates the underlying floating
point number value object before inserting it.

On success, \p key is copied.

@param[in] map_obj	Map value object in which to insert \p val.
@param[in] key		Key (copied on success) to which the floating
			point number value object to insert is mapped.
@param[in] val		Floating point number raw value to insert,
			mapped to the key \p key.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@postrefcountsame{map_obj}

@sa bt_value_map_insert_entry(): Inserts a value object into a given map
	value object.
*/
extern enum bt_value_status bt_value_map_insert_real_entry(
		struct bt_value *map_obj, const char *key, double val);

/**
@brief	Inserts the string raw value \p val mapped to the key \p key
	into the map value object \p map_obj.

This is a convenience function which creates the underlying string value
object before inserting it.

On success, \p val and \p key are copied.

@param[in] map_obj	Map value object in which to insert \p val.
@param[in] key		Key (copied on success) to which the string
			value object to insert is mapped.
@param[in] val		String raw value to insert (copied on success),
			mapped to the key \p key.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@prenotnull{val}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@postrefcountsame{map_obj}

@sa bt_value_map_insert_entry(): Inserts a value object into a given map
	value object.
*/
extern enum bt_value_status bt_value_map_insert_string_entry(
		struct bt_value *map_obj, const char *key, const char *val);

/**
@brief	Inserts an empty array value object mapped to the key \p key
	into the map value object \p map_obj.

This is a convenience function which creates the underlying array value
object before inserting it.

On success, \p key is copied.

@param[in] map_obj	Map value object in which to insert an empty
			array value object.
@param[in] key		Key (copied on success) to which the empty array
			value object to insert is mapped.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@postrefcountsame{map_obj}

@sa bt_value_map_insert_entry(): Inserts a value object into a given map
	value object.
*/
extern enum bt_value_status bt_value_map_insert_empty_array_entry(
		struct bt_value *map_obj, const char *key);

/**
@brief	Inserts an empty map value object mapped to the key \p key into
	the map value object \p map_obj.

This is a convenience function which creates the underlying map value
object before inserting it.

On success, \p key is copied.

@param[in] map_obj	Map value object in which to insert an empty
			map object.
@param[in] key		Key (copied on success) to which the empty map
			value object to insert is mapped.
@returns		Status code.

@prenotnull{map_obj}
@prenotnull{key}
@pre \p map_obj is a map value object.
@prehot{map_obj}
@postrefcountsame{map_obj}

@sa bt_value_map_insert_entry(): Inserts a value object into a given map
	value object.
*/
extern enum bt_value_status bt_value_map_insert_empty_map_entry(
		struct bt_value *map_obj, const char *key);

/**
@brief  Creates a copy of the base map value object \p base_map_obj
	superficially extended with the entries of the extension map
	value object \p extension_map_obj.

This function creates a superficial extension of \p base_map_obj with
\p extension_map_obj by adding new entries to it and replacing the
ones that share the keys in the extension object. The extension is
\em superficial because it does not merge internal array and map
value objects.

For example, consider the following \p base_map_obj (JSON representation):

@verbatim
{
  "hello": 23,
  "code": -17,
  "em": false,
  "return": [5, 6, null]
}
@endverbatim

and the following \p extension_map_obj (JSON representation):

@verbatim
{
  "comma": ",",
  "code": 22,
  "return": 17.88
}
@endverbatim

The extended object is (JSON representation):

@verbatim
{
  "hello": 23,
  "code": 22,
  "em": false,
  "return": 17.88,
  "comma": ","
}
@endverbatim

@param[in] base_map_obj		Base map value object with initial
				entries.
@param[in] extension_map_obj	Extension map value object containing
				the entries to add to or replace in
				\p base_map_obj.
@returns			Created extended map value object, or
				\c NULL on error.

@prenotnull{base_map_obj}
@prenotnull{extension_map_obj}
@pre \p base_map_obj is a map value object.
@pre \p extension_map_obj is a map value object.
@postrefcountsame{base_map_obj}
@postrefcountsame{extension_map_obj}
@postsuccessrefcountret1
*/
extern struct bt_value *bt_value_map_extend(struct bt_value *base_map_obj,
		struct bt_value *extension_map_obj);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_VALUES_H */
