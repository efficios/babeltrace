#ifndef _BABELTRACE_VALUES_H
#define _BABELTRACE_VALUES_H

/*
 * Babeltrace - Value objects
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
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

/**
 * @file values.h
 * @brief Value objects
 *
 * This is a set of value objects. The following functions allow you
 * to create, modify, and destroy:
 *
 *   - \link bt_value_null null value objects\endlink
 *   - \link bt_value_bool_create() boolean value objects\endlink
 *   - \link bt_value_integer_create() integer value objects\endlink
 *   - \link bt_value_float_create() floating point number
 *     value objects\endlink
 *   - \link bt_value_string_create() string value objects\endlink
 *   - \link bt_value_array_create() array value objects\endlink,
 *     containing zero or more value objects
 *   - \link bt_value_map_create() map value objects\endlink, mapping
 *     string keys to value objects
 *
 * All the value object types above, except for null values (which
 * always point to the same \link bt_value_null singleton\endlink), have
 * a reference count property. Once a value object is created, its
 * reference count is set to 1. When \link bt_value_array_append()
 * appending a value to an array value object\endlink, or
 * \link bt_value_map_insert() inserting a value object into a map
 * value object\endlink, its reference count is incremented, as well as
 * when getting a value object back from those structures. The
 * bt_get() and bt_put() functions are to be used to handle reference counting
 * Once you are done with a value object, pass it to bt_put().
 *
 * Most functions of this API return a status code, one of the values in
 * #bt_value_status.
 *
 * You can create a deep copy of any value object using the
 * bt_value_copy() function. You can compare two given value objects
 * using bt_value_compare().
 *
 * Any value object may be frozen using bt_value_freeze(). You may get
 * the raw value of a frozen value object, but you cannot modify it.
 * Reference counting still works on frozen value objects. You may also
 * copy and compare frozen value objects.
 *
 * @author	Philippe Proulx <pproulx@efficios.com>
 * @bug		No known bugs
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <babeltrace/ref.h>
#include <babeltrace/babeltrace-internal.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Value object type.
 */
enum bt_value_type {
	/** Unknown value object, used as an error code. */
	BT_VALUE_TYPE_UNKNOWN =		-1,

	/** Null value object. */
	BT_VALUE_TYPE_NULL =		0,

	/** Boolean value object (holds \c true or \c false). */
	BT_VALUE_TYPE_BOOL =		1,

	/** Integer value object (holds a signed 64-bit integer raw value). */
	BT_VALUE_TYPE_INTEGER =		2,

	/** Floating point number value object (holds a \c double raw value). */
	BT_VALUE_TYPE_FLOAT =		3,

	/** String value object. */
	BT_VALUE_TYPE_STRING =		4,

	/** Array value object. */
	BT_VALUE_TYPE_ARRAY =		5,

	/** Map value object. */
	BT_VALUE_TYPE_MAP =		6,
};

/**
 * Status codes.
 */
enum bt_value_status {
	/** Value object cannot be altered because it's frozen. */
	BT_VALUE_STATUS_FROZEN =	-4,

	/** Operation cancelled. */
	BT_VALUE_STATUS_CANCELLED =	-3,

	/** Invalid arguments. */
	/* -22 for compatibility with -EINVAL */
	BT_VALUE_STATUS_INVAL =	-22,

	/** General error. */
	BT_VALUE_STATUS_ERROR =	-1,

	/** Okay, no error. */
	BT_VALUE_STATUS_OK =		0,
};

/**
 * Value object.
 */
struct bt_value;

/**
 * The null value object singleton.
 *
 * Use this everytime you need a null value object.
 *
 * The null value object singleton has no reference count; there's only
 * one. You may directly compare any value object to the null value
 * object singleton to find out if it's a null value object, or
 * otherwise use bt_value_is_null().
 *
 * The null value object singleton is always frozen (see
 * bt_value_freeze() and bt_value_is_frozen()).
 *
 * Functions of this API return this when the value object is actually a
 * null value object (of type #BT_VALUE_TYPE_NULL), whereas \c NULL
 * means an error of some sort.
 */
BT_HIDDEN
struct bt_value *bt_value_null;

/**
 * User function type for bt_value_map_foreach().
 *
 * \p object is a \em weak reference; you must pass it to
 * bt_get() to get your own reference.
 *
 * Return \c true to continue the loop, or \c false to break it.
 *
 * @param key		Key of map entry
 * @param object	Value object of map entry (weak reference)
 * @param data		User data
 * @returns		\c true to continue the loop
 */
typedef bool (* bt_value_map_foreach_cb)(const char *key,
	struct bt_value *object, void *data);

/**
 * Recursively freezes the value object \p object.
 *
 * A frozen value object cannot be modified; it is considered immutable.
 * Reference counting still works on a frozen value object though: you
 * may pass a frozen value object to bt_get() and bt_put().
 *
 * @param object	Value object to freeze
 * @returns		One of #bt_value_status values; if \p object
 *			is already frozen, though, #BT_VALUE_STATUS_OK
 *			is returned anyway (i.e. this function never
 *			returns #BT_VALUE_STATUS_FROZEN)
 *
 * @see bt_value_is_frozen()
 */
BT_HIDDEN
enum bt_value_status bt_value_freeze(struct bt_value *object);

/**
 * Checks whether \p object is frozen or not.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is frozen
 *
 * @see bt_value_freeze()
 */
BT_HIDDEN
bool bt_value_is_frozen(const struct bt_value *object);

/**
 * Returns the type of \p object.
 *
 * @param object	Value object of which to get the type
 * @returns		Value object's type, or #BT_VALUE_TYPE_UNKNOWN
 *			on error
 *
 * @see #bt_value_type (value object types)
 * @see bt_value_is_null()
 * @see bt_value_is_bool()
 * @see bt_value_is_integer()
 * @see bt_value_is_float()
 * @see bt_value_is_string()
 * @see bt_value_is_array()
 * @see bt_value_is_map()
 */
BT_HIDDEN
enum bt_value_type bt_value_get_type(const struct bt_value *object);

/**
 * Checks whether \p object is a null value object. The only valid null
 * value object is \ref bt_value_null.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is a null value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_null(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_NULL;
}

/**
 * Checks whether \p object is a boolean value object.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is a boolean value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_bool(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_BOOL;
}

/**
 * Checks whether \p object is an integer value object.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is an integer value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_integer(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_INTEGER;
}

/**
 * Checks whether \p object is a floating point number value object.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is a floating point
 *			number value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_float(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_FLOAT;
}

/**
 * Checks whether \p object is a string value object.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is a string value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_string(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_STRING;
}

/**
 * Checks whether \p object is an array value object.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is an array value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_array(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_ARRAY;
}

/**
 * Checks whether \p object is a map value object.
 *
 * @param object	Value object to check
 * @returns		\c true if \p object is a map value object
 *
 * @see bt_value_get_type()
 */
static inline
bool bt_value_is_map(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_MAP;
}

/**
 * Creates a boolean value object. The created boolean value object's
 * initial raw value is \c false.
 *
 * The created value object's reference count is set to 1.
 *
 * @returns	Created value object on success, or \c NULL on error
 *
 * @see bt_value_bool_create_init() (creates an initialized
 *				     boolean value object)
 */
BT_HIDDEN
struct bt_value *bt_value_bool_create(void);

/**
 * Creates a boolean value object with its initial raw value set to
 * \p val.
 *
 * The created value object's reference count is set to 1.
 *
 * @param val	Initial raw value
 * @returns	Created value object on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_bool_create_init(bool val);

/**
 * Creates an integer value object. The created integer value object's
 * initial raw value is 0.
 *
 * The created value object's reference count is set to 1.
 *
 * @returns	Created value object on success, or \c NULL on error
 *
 * @see bt_value_integer_create_init() (creates an initialized
 *					integer value object)
 */
BT_HIDDEN
struct bt_value *bt_value_integer_create(void);

/**
 * Creates an integer value object with its initial raw value set to
 * \p val.
 *
 * The created value object's reference count is set to 1.
 *
 * @param val	Initial raw value
 * @returns	Created value object on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_integer_create_init(int64_t val);

/**
 * Creates a floating point number value object. The created floating
 * point number value object's initial raw value is 0.
 *
 * The created value object's reference count is set to 1.
 *
 * @returns	Created value object on success, or \c NULL on error
 *
 * @see bt_value_float_create_init() (creates an initialized floating
 *				      point number value object)
 */
BT_HIDDEN
struct bt_value *bt_value_float_create(void);

/**
 * Creates a floating point number value object with its initial raw
 * value set to \p val.
 *
 * The created value object's reference count is set to 1.
 *
 * @param val	Initial raw value
 * @returns	Created value object on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_float_create_init(double val);

/**
 * Creates a string value object. The string value object is initially
 * empty.
 *
 * The created value object's reference count is set to 1.
 *
 * @returns	Created value object on success, or \c NULL on error
 *
 * @see bt_value_string_create_init() (creates an initialized
 *				       string value object)
 */
BT_HIDDEN
struct bt_value *bt_value_string_create(void);

/**
 * Creates a string value object with its initial raw value set to
 * \p val.
 *
 * On success, \p val is \em copied.
 *
 * The created value object's reference count is set to 1.
 *
 * @param val	Initial raw value (copied on success)
 * @returns	Created value object on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_string_create_init(const char *val);

/**
 * Creates an empty array value object.
 *
 * The created value object's reference count is set to 1.
 *
 * @returns	Created value object on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_array_create(void);

/**
 * Creates an empty map value object.
 *
 * The created value object's reference count is set to 1.
 *
 * @returns	Created value object on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_map_create(void);

/**
 * Gets the boolean raw value of the boolean value object \p bool_obj.
 *
 * @param bool_obj	Boolean value object
 * @param val		Returned boolean raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_bool_set()
 */
BT_HIDDEN
enum bt_value_status bt_value_bool_get(
		const struct bt_value *bool_obj, bool *val);

/**
 * Sets the boolean raw value of the boolean value object \p bool_obj
 * to \p val.
 *
 * @param bool_obj	Boolean value object
 * @param val		New boolean raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_bool_get()
 */
BT_HIDDEN
enum bt_value_status bt_value_bool_set(struct bt_value *bool_obj,
		bool val);

/**
 * Gets the integer raw value of the integer value object
 * \p integer_obj.
 *
 * @param integer_obj	Integer value object
 * @param val		Returned integer raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_integer_set()
 */
BT_HIDDEN
enum bt_value_status bt_value_integer_get(
		const struct bt_value *integer_obj, int64_t *val);

/**
 * Sets the integer raw value of the integer value object \p integer_obj
 * to \p val.
 *
 * @param integer_obj	Integer value object
 * @param val		New integer raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_integer_get()
 */
BT_HIDDEN
enum bt_value_status bt_value_integer_set(
		struct bt_value *integer_obj, int64_t val);

/**
 * Gets the floating point number raw value of the floating point number
 * value object \p float_obj.
 *
 * @param float_obj	Floating point number value object
 * @param val		Returned floating point number raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_float_set()
 */
BT_HIDDEN
enum bt_value_status bt_value_float_get(
		const struct bt_value *float_obj, double *val);

/**
 * Sets the floating point number raw value of the floating point number
 * value object \p float_obj to \p val.
 *
 * @param float_obj	Floating point number value object
 * @param val		New floating point number raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_float_get()
 */
BT_HIDDEN
enum bt_value_status bt_value_float_set(
		struct bt_value *float_obj, double val);

/**
 * Gets the string raw value of the string value object \p string_obj.
 * The returned string is valid as long as this value object exists and
 * is not modified. The ownership of the returned string is \em not
 * transferred to the caller.
 *
 * @param string_obj	String value object
 * @param val		Returned string raw value
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_string_set()
 */
BT_HIDDEN
enum bt_value_status bt_value_string_get(
		const struct bt_value *string_obj, const char **val);

/**
 * Sets the string raw value of the string value object \p string_obj to
 * \p val.
 *
 * On success, \p val is \em copied.
 *
 * @param string_obj	String value object
 * @param val		New string raw value (copied on successf)
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_string_get()
 */
BT_HIDDEN
enum bt_value_status bt_value_string_set(struct bt_value *string_obj,
		const char *val);

/**
 * Gets the size of the array value object \p array_obj, that is, the
 * number of value objects contained in \p array_obj.
 *
 * @param array_obj	Array value object
 * @returns		Array size if the return value is 0 (empty) or a
 *			positive value, or one of
 *			#bt_value_status negative values otherwise
 *
 * @see bt_value_array_is_empty()
 */
BT_HIDDEN
int bt_value_array_size(const struct bt_value *array_obj);

/**
 * Returns \c true if the array value object \p array_obj is empty.
 *
 * @param array_obj	Array value object
 * @returns		\c true if \p array_obj is empty
 *
 * @see bt_value_array_size()
 */
BT_HIDDEN
bool bt_value_array_is_empty(const struct bt_value *array_obj);

/**
 * Gets the value object of the array value object \p array_obj at the
 * index \p index.
 *
 * The returned value object's reference count is incremented, unless
 * it's a null value object.
 *
 * @param array_obj	Array value object
 * @param index		Index of value object to get
 * @returns		Value object at index \p index on
 *			success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_array_get(const struct bt_value *array_obj,
		size_t index);

/**
 * Appends the value object \p element_obj to the array value
 * object \p array_obj.
 *
 * The appended value object's reference count is incremented, unless
 * it's a null object.
 *
 * @param array_obj	Array value object
 * @param element_obj	Value object to append
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append_bool()
 * @see bt_value_array_append_integer()
 * @see bt_value_array_append_float()
 * @see bt_value_array_append_string()
 * @see bt_value_array_append_empty_array()
 * @see bt_value_array_append_empty_map()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append(struct bt_value *array_obj,
		struct bt_value *element_obj);

/**
 * Appends the boolean raw value \p val to the array value object
 * \p array_obj. This is a convenience function which creates the
 * underlying boolean value object before appending it.
 *
 * The created boolean value object's reference count is set to 1.
 *
 * @param array_obj	Array value object
 * @param val		Boolean raw value to append
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append_bool(
		struct bt_value *array_obj, bool val);

/**
 * Appends the integer raw value \p val to the array value object
 * \p array_obj. This is a convenience function which creates the
 * underlying integer value object before appending it.
 *
 * The created integer value object's reference count is set to 1.
 *
 * @param array_obj	Array value object
 * @param val		Integer raw value to append
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append_integer(
		struct bt_value *array_obj, int64_t val);

/**
 * Appends the floating point number raw value \p val to the array value
 * object \p array_obj. This is a convenience function which creates the
 * underlying floating point number value object before appending it.
 *
 * The created floating point number value object's reference count is
 * set to 1.
 *
 * @param array_obj	Array value object
 * @param val		Floating point number raw value to append
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append_float(
		struct bt_value *array_obj, double val);

/**
 * Appends the string raw value \p val to the array value object
 * \p array_obj. This is a convenience function which creates the
 * underlying string value object before appending it.
 *
 * On success, \p val is \em copied.
 *
 * The created string value object's reference count is set to 1.
 *
 * @param array_obj	Array value object
 * @param val		String raw value to append (copied on success)
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append_string(
		struct bt_value *array_obj, const char *val);

/**
 * Appends an empty array value object to the array value object
 * \p array_obj. This is a convenience function which creates the
 * underlying array value object before appending it.
 *
 * The created array value object's reference count is set to 1.
 *
 * @param array_obj	Array value object
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append_empty_array(
		struct bt_value *array_obj);

/**
 * Appends an empty map value object to the array value object
 * \p array_obj. This is a convenience function which creates the
 * underlying map value object before appending it.
 *
 * The created map value object's reference count is set to 1.
 *
 * @param array_obj	Array value object
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_array_append()
 */
BT_HIDDEN
enum bt_value_status bt_value_array_append_empty_map(
		struct bt_value *array_obj);

/**
 * Replaces the value object at index \p index of the array
 * value object \p array_obj by \p element_obj.
 *
 * The replaced value object's reference count is decremented, unless
 * it's a null value object. The reference count of \p element_obj is
 * incremented, unless it's a null value object.
 *
 * @param array_obj	Array value object
 * @param index		Index of value object to replace
 * @param element_obj	New value object at position \p index of
 *			\p array_obj
 * @returns		One of #bt_value_status values
 */
BT_HIDDEN
enum bt_value_status bt_value_array_set(struct bt_value *array_obj,
		size_t index, struct bt_value *element_obj);

/**
 * Gets the size of a map value object, that is, the number of entries
 * contained in a map value object.
 *
 * @param map_obj	Map value object
 * @returns		Map size if the return value is 0 (empty) or a
 *			positive value, or one of
 *			#bt_value_status negative values otherwise
 *
 * @see bt_value_map_is_empty()
 */
BT_HIDDEN
int bt_value_map_size(const struct bt_value *map_obj);

/**
 * Returns \c true if the map value object \p map_obj is empty.
 *
 * @param map_obj	Map value object
 * @returns		\c true if \p map_obj is empty
 *
 * @see bt_value_map_size()
 */
BT_HIDDEN
bool bt_value_map_is_empty(const struct bt_value *map_obj);

/**
 * Gets the value object associated with the key \p key within the
 * map value object \p map_obj.
 *
 * The returned value object's reference count is incremented, unless
 * it's a null value object.
 *
 * @param map_obj	Map value object
 * @param key		Key of the value object to get
 * @returns		Value object associated with the key \p key
 *			on success, or \c NULL on error
 */
BT_HIDDEN
struct bt_value *bt_value_map_get(const struct bt_value *map_obj,
		const char *key);

/**
 * Calls a provided user function \p cb for each value object of the map
 * value object \p map_obj.
 *
 * The value object passed to the user function is a
 * <b>weak reference</b>: you must call bt_get() on it to obtain your own
 * reference.
 *
 * The key passed to the user function is only valid in the scope of
 * this user function call.
 *
 * The user function must return \c true to continue the loop, or
 * \c false to break it.
 *
 * @param map_obj	Map value object
 * @param cb		User function to call back
 * @param data		User data passed to the user function
 * @returns		One of #bt_value_status values; more
 *			specifically, #BT_VALUE_STATUS_CANCELLED is
 *			returned if the loop was cancelled by the user
 *			function
 */
BT_HIDDEN
enum bt_value_status bt_value_map_foreach(
		const struct bt_value *map_obj, bt_value_map_foreach_cb cb,
		void *data);

/**
 * Returns whether or not the map value object \p map_obj contains the
 * key \p key.
 *
 * @param map_obj	Map value object
 * @param key		Key to check
 * @returns		\c true if \p map_obj contains the key \p key,
 *			or \c false if it doesn't have \p key or
 *			on error
 */
BT_HIDDEN
bool bt_value_map_has_key(const struct bt_value *map_obj,
		const char *key);

/**
 * Inserts the value object \p element_obj associated with the key
 * \p key into the map value object \p map_obj. If \p key exists in
 * \p map_obj, the associated value object is first put, and then
 * replaced by \p element_obj.
 *
 * On success, \p key is \em copied.
 *
 * The inserted value object's reference count is incremented, unless
 * it's a null value object.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of value object to insert
 * @param element_obj	Value object to insert, associated with the
 *			key \p key
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert_bool()
 * @see bt_value_map_insert_integer()
 * @see bt_value_map_insert_float()
 * @see bt_value_map_insert_string()
 * @see bt_value_map_insert_empty_array()
 * @see bt_value_map_insert_empty_map()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert(
		struct bt_value *map_obj, const char *key,
		struct bt_value *element_obj);

/**
 * Inserts the boolean raw value \p val associated with the key \p key
 * into the map value object \p map_obj. This is a convenience function
 * which creates the underlying boolean value object before
 * inserting it.
 *
 * On success, \p key is \em copied.
 *
 * The created boolean value object's reference count is set to 1.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of boolean value object
 *			to insert
 * @param val		Boolean raw value to insert, associated with
 *			the key \p key
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert_bool(
		struct bt_value *map_obj, const char *key, bool val);

/**
 * Inserts the integer raw value \p val associated with the key \p key
 * into the map value object \p map_obj. This is a convenience function
 * which creates the underlying integer value object before inserting it.
 *
 * On success, \p key is \em copied.
 *
 * The created integer value object's reference count is set to 1.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of integer value object
 *			to insert
 * @param val		Integer raw value to insert, associated with
 *			the key \p key
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert_integer(
		struct bt_value *map_obj, const char *key, int64_t val);

/**
 * Inserts the floating point number raw value \p val associated with
 * the key \p key into the map value object \p map_obj. This is a
 * convenience function which creates the underlying floating point
 * number value object before inserting it.
 *
 * On success, \p key is \em copied.
 *
 * The created floating point number value object's reference count is
 * set to 1.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of floating point number
 *			value object to insert
 * @param val		Floating point number raw value to insert,
 *			associated with the key \p key
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert_float(
		struct bt_value *map_obj, const char *key, double val);

/**
 * Inserts the string raw value \p val associated with the key \p key
 * into the map value object \p map_obj. This is a convenience function
 * which creates the underlying string value object before inserting it.
 *
 * On success, \p val and \p key are \em copied.
 *
 * The created string value object's reference count is set to 1.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of string value object
 *			to insert
 * @param val		String raw value to insert (copied on success),
 *			associated with the key \p key
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert_string(
		struct bt_value *map_obj, const char *key, const char *val);

/**
 * Inserts an empty array value object associated with the key \p key
 * into the map value object \p map_obj. This is a convenience function
 * which creates the underlying array value object before inserting it.
 *
 * On success, \p key is \em copied.
 *
 * The created array value object's reference count is set to 1.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of empty array value
 *			object to insert
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert_empty_array(
		struct bt_value *map_obj, const char *key);

/**
 * Inserts an empty map value object associated with the key \p key into
 * the map value object \p map_obj. This is a convenience function which
 * creates the underlying map value object before inserting it.
 *
 * On success, \p key is \em copied.
 *
 * The created map value object's reference count is set to 1.
 *
 * @param map_obj	Map value object
 * @param key		Key (copied on success) of empty map value
 *			object to insert
 * @returns		One of #bt_value_status values
 *
 * @see bt_value_map_insert()
 */
BT_HIDDEN
enum bt_value_status bt_value_map_insert_empty_map(
		struct bt_value *map_obj, const char *key);

/**
 * Creates a deep copy of the value object \p object.
 *
 * The created value object's reference count is set to 1, unless
 * \p object is a null value object.
 *
 * Copying a frozen value object is allowed: the resulting copy is
 * \em not frozen.
 *
 * @param object	Value object to copy
 * @returns		Deep copy of \p object on success, or \c NULL
 *			on error
 */
BT_HIDDEN
struct bt_value *bt_value_copy(const struct bt_value *object);

/**
 * Compares the value objects \p object_a and \p object_b and returns
 * \c true if they have the same \em content (raw values).
 *
 * @param object_a	Value object A
 * @param object_b	Value object B
 * @returns		\c true if \p object_a and \p object_b have the
 *			same content, or \c false if they differ or on
 *			error
 */
BT_HIDDEN
bool bt_value_compare(const struct bt_value *object_a,
		const struct bt_value *object_b);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_VALUES_H */
