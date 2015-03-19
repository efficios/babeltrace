#ifndef _BABELTRACE_OBJECTS_H
#define _BABELTRACE_OBJECTS_H

/*
 * Babeltrace
 *
 * Basic object system
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
 * @file objects.h
 * @brief Basic object system
 *
 * This is a basic object system API. The following functions allow you
 * to create, modify, and destroy:
 *
 *   - \link bt_object_null null objects\endlink
 *   - \link bt_object_bool_create() boolean objects\endlink
 *   - \link bt_object_integer_create() integer objects\endlink
 *   - \link bt_object_float_create() floating point number
 *     objects\endlink
 *   - \link bt_object_string_create() string objects\endlink
 *   - \link bt_object_array_create() array objects\endlink,
 *     containing zero or more objects
 *   - \link bt_object_map_create() map objects\endlink, mapping
 *     string keys to objects
 *
 * All the object types above, except for null objects (which always
 * point to the same \link bt_object_null singleton\endlink), have a
 * reference count property. Once an object is created, its reference
 * count is set to 1. When \link bt_object_array_append() appending an
 * object to an array object\endlink, or \link bt_object_map_insert()
 * inserting an object into a map object\endlink, its reference count
 * is incremented, as well as when getting an object back from those
 * structures. The bt_object_get() and bt_object_put() functions exist
 * to deal with reference counting. Once you are done with an object,
 * pass it to bt_object_put().
 *
 * A common action with objects is to create or get one, do something
 * with it, and then put it. To avoid putting it a second time later
 * (if an error occurs, for example), the variable is often reset to
 * \c NULL after putting the object it points to. Since this is so
 * common, you can use the BT_OBJECT_PUT() macro, which does just that:
 *
 * \code{.c}
 *     struct bt_object *int_obj = bt_object_integer_create_init(34);
 *
 *     if (!int_obj) {
 *         goto error;
 *     }
 *
 *     // stuff, which could jump to error
 *
 *     BT_OBJECT_PUT(int_obj);
 *
 *     // stuff, which could jump to error
 *
 *     return 0;
 *
 * error:
 *     // safe, even if int_obj is NULL
 *     BT_OBJECT_PUT(int_obj);
 *
 *     // ...
 * \endcode
 *
 * Another common manipulation is to move the ownership of an object
 * from one variable to another: since the reference count is not
 * incremented, and since, to avoid errors, two variables should not
 * point to same object without each of them having their own reference,
 * it is best practice to set the original variable to \c NULL. This
 * too can be accomplished in a single step using the BT_OBJECT_MOVE()
 * macro:
 *
 * \code{.c}
 *     struct bt_object *int_obj2 = NULL;
 *     struct bt_object *int_obj = bt_object_integer_create_init(-23);
 *
 *     if (!int_obj) {
 *         goto error;
 *     }
 *
 *     // stuff, which could jump to error
 *
 *     BT_OBJECT_MOVE(int_obj2, int_obj);
 *
 *     // stuff, which could jump to error
 *
 *     return 0;
 *
 * error:
 *     // safe, since only one of int_obj/int_obj2 (or none)
 *     // points to the object
 *     BT_OBJECT_PUT(int_obj);
 *     BT_OBJECT_PUT(int_obj2);
 *
 *     // ...
 * \endcode
 *
 * Most functions return a status code, one of the values in
 * #bt_object_status.
 *
 * You can create a deep copy of any object using the bt_object_copy()
 * function. You can compare two given objects using
 * bt_object_compare().
 *
 * Any object may be frozen using bt_object_freeze(). You may get the
 * value of a frozen object, but you cannot modify it. Reference
 * counting still works on frozen objects. You may also copy and compare
 * frozen objects.
 *
 * @author	Philippe Proulx <pproulx@efficios.com>
 * @bug		No known bugs
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Object type.
 */
enum bt_object_type {
	/** Unknown object, used as an error code. */
	BT_OBJECT_TYPE_UNKNOWN =	-1,

	/** Null object. */
	BT_OBJECT_TYPE_NULL =		0,

	/** Boolean object (holds \c true or \c false). */
	BT_OBJECT_TYPE_BOOL =		1,

	/** Integer (holds a signed 64-bit integer value). */
	BT_OBJECT_TYPE_INTEGER =	2,

	/**
	 * Floating point number object (holds a \c double value).
	 */
	BT_OBJECT_TYPE_FLOAT =		3,

	/** String object. */
	BT_OBJECT_TYPE_STRING =		4,

	/** Array object. */
	BT_OBJECT_TYPE_ARRAY =		5,

	/** Map object. */
	BT_OBJECT_TYPE_MAP =		6,
};

/**
 * Status code.
 */
enum bt_object_status {
	/** Object cannot be altered because it's frozen. */
	BT_OBJECT_STATUS_FROZEN =	-4,

	/** Operation cancelled. */
	BT_OBJECT_STATUS_CANCELLED =	-3,

	/** Invalid arguments. */
	/* -22 for compatibility with -EINVAL */
	BT_OBJECT_STATUS_INVAL =	-22,

	/** General error. */
	BT_OBJECT_STATUS_ERROR =	-1,

	/** Okay, no error. */
	BT_OBJECT_STATUS_OK =		0,
};

/**
 * Object.
 */
struct bt_object;

/**
 * The null object singleton.
 *
 * Use this everytime you need a null object.
 *
 * The null object singleton has no reference count; there's only one.
 * You may directly compare any object to the null object singleton to
 * find out if it's a null object, or otherwise use bt_object_is_null().
 *
 * The null object singleton is always frozen (see bt_object_freeze()
 * and bt_object_is_frozen()).
 *
 * Functions of this API return this when the object is actually a
 * null object (of type #BT_OBJECT_TYPE_NULL), whereas \c NULL means an
 * error of some sort.
 */
extern struct bt_object *bt_object_null;

/**
 * User function type for bt_object_map_foreach().
 *
 * \p object is a \em weak reference; you must pass it to
 * bt_object_get() to get your own reference.
 *
 * Return \c true to continue the loop, or \c false to break it.
 *
 * @param key		Key of map entry
 * @param object	Object of map entry (weak reference)
 * @param data		User data
 * @returns		\c true to continue the loop
 */
typedef bool (* bt_object_map_foreach_cb)(const char *key,
	struct bt_object *object, void *data);

/**
 * Puts the object \p _object (calls bt_object_put() on it), and resets
 * the variable to \c NULL.
 *
 * This is something that is often done when putting and object;
 * resetting the variable to \c NULL makes sure it cannot be put a
 * second time later.
 *
 * @param _object	Object to put
 *
 * @see BT_OBJECT_MOVE() (moves an object from one variable to the other
 *			 without putting it)
 */
#define BT_OBJECT_PUT(_object)				\
	do {						\
		bt_object_put(_object);			\
		(_object) = NULL;			\
	} while (0)

/**
 * Moves the object referenced by the variable \p _src_object to the
 * \p _dst_object variable, then resets \p _src_object to \c NULL.
 *
 * The object's reference count is <b>not changed</b>. Resetting
 * \p _src_object to \c NULL ensures the object will not be put
 * twice later; its ownership is indeed \em moved from the source
 * variable to the destination variable.
 *
 * @param _src_object	Source object variable
 * @param _dst_object	Destination object variable
 */
#define BT_OBJECT_MOVE(_dst_object, _src_object)	\
	do {						\
		(_dst_object) = (_src_object);		\
		(_src_object) = NULL;			\
	} while (0)

/**
 * Increments the reference count of \p object.
 *
 * @param object	Object of which to increment the reference count
 */
extern void bt_object_get(struct bt_object *object);

/**
 * Decrements the reference count of \p object, destroying it when this
 * count reaches 0.
 *
 * @param object	Object of which to decrement the reference count
 *
 * @see BT_OBJECT_PUT() (puts an object and resets the variable to
 *			\c NULL)
 */
extern void bt_object_put(struct bt_object *object);

/**
 * Recursively freezes the object \p object.
 *
 * A frozen object cannot be modified; it is considered immutable.
 * Reference counting still works on a frozen object though: you may
 * pass a frozen object to bt_object_get() and bt_object_put().
 *
 * @param object	Object to freeze
 * @returns		One of #bt_object_status values; if \p object
 *			is already frozen, though, #BT_OBJECT_STATUS_OK
 *			is returned anyway (i.e. this function never
 *			returns #BT_OBJECT_STATUS_FROZEN)
 */
extern enum bt_object_status bt_object_freeze(struct bt_object *object);

/**
 * Checks whether \p object is frozen or not.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is frozen
 */
extern bool bt_object_is_frozen(const struct bt_object *object);

/**
 * Returns the type of \p object.
 *
 * @param object	Object of which to get the type
 * @returns		Object's type, or #BT_OBJECT_TYPE_UNKNOWN
 *			on error
 *
 * @see #bt_object_type (object types)
 */
extern enum bt_object_type bt_object_get_type(const struct bt_object *object);

/**
 * Checks whether \p object is a null object. The only valid null
 * object is \ref bt_object_null.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is a null object
 */
static inline
bool bt_object_is_null(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_NULL;
}

/**
 * Checks whether \p object is a boolean object.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is a boolean object
 */
static inline
bool bt_object_is_bool(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_BOOL;
}

/**
 * Checks whether \p object is an integer object.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is an integer object
 */
static inline
bool bt_object_is_integer(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_INTEGER;
}

/**
 * Checks whether \p object is a floating point number object.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is a floating point number object
 */
static inline
bool bt_object_is_float(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_FLOAT;
}

/**
 * Checks whether \p object is a string object.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is a string object
 */
static inline
bool bt_object_is_string(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_STRING;
}

/**
 * Checks whether \p object is an array object.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is an array object
 */
static inline
bool bt_object_is_array(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_ARRAY;
}

/**
 * Checks whether \p object is a map object.
 *
 * @param object	Object to check
 * @returns		\c true if \p object is a map object
 */
static inline
bool bt_object_is_map(const struct bt_object *object)
{
	return bt_object_get_type(object) == BT_OBJECT_TYPE_MAP;
}

/**
 * Creates a boolean object. The created boolean object's initial value
 * is \c false.
 *
 * The created object's reference count is set to 1.
 *
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_bool_create(void);

/**
 * Creates a boolean object with its initial value set to \p val.
 *
 * The created object's reference count is set to 1.
 *
 * @param val	Initial value
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_bool_create_init(bool val);

/**
 * Creates an integer object. The created integer object's initial value
 * is 0.
 *
 * The created object's reference count is set to 1.
 *
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_integer_create(void);

/**
 * Creates an integer object with its initial value set to \p val.
 *
 * The created object's reference count is set to 1.
 *
 * @param val	Initial value
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_integer_create_init(int64_t val);

/**
 * Creates a floating point number object. The created floating point
 * number object's initial value is 0.
 *
 * The created object's reference count is set to 1.
 *
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_float_create(void);

/**
 * Creates a floating point number object with its initial value set
 * to \p val.
 *
 * The created object's reference count is set to 1.
 *
 * @param val	Initial value
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_float_create_init(double val);

/**
 * Creates a string object. The string object is initially empty.
 *
 * The created object's reference count is set to 1.
 *
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_string_create(void);

/**
 * Creates a string object with its initial value set to \p val.
 *
 * \p val is copied.
 *
 * The created object's reference count is set to 1.
 *
 * @param val	Initial value (will be copied)
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_string_create_init(const char *val);

/**
 * Creates an empty array object.
 *
 * The created object's reference count is set to 1.
 *
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_array_create(void);

/**
 * Creates an empty map object.
 *
 * The created object's reference count is set to 1.
 *
 * @returns	Created object on success, or \c NULL on error
 */
extern struct bt_object *bt_object_map_create(void);

/**
 * Gets the boolean value of the boolean object \p bool_obj.
 *
 * @param bool_obj	Boolean object
 * @param val		Returned boolean value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_bool_get(
	const struct bt_object *bool_obj, bool *val);

/**
 * Sets the boolean value of the boolean object \p bool_obj to \p val.
 *
 * @param bool_obj	Boolean object
 * @param val		New boolean value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_bool_set(struct bt_object *bool_obj,
	bool val);

/**
 * Gets the integer value of the integer object \p integer_obj.
 *
 * @param integer_obj	Integer object
 * @param val		Returned integer value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_integer_get(
	const struct bt_object *integer_obj, int64_t *val);

/**
 * Sets the integer value of the integer object \p integer_obj to
 * \p val.
 *
 * @param integer_obj	Integer object
 * @param val		New integer value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_integer_set(
	struct bt_object *integer_obj, int64_t val);

/**
 * Gets the floating point number value of the floating point number
 * object \p float_obj.
 *
 * @param float_obj	Floating point number object
 * @param val		Returned floating point number value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_float_get(
	const struct bt_object *float_obj, double *val);

/**
 * Sets the floating point number value of the floating point number
 * object \p float_obj to \p val.
 *
 * @param float_obj	Floating point number object
 * @param val		New floating point number value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_float_set(
	struct bt_object *float_obj, double val);

/**
 * Gets the string value of the string object \p string_obj. The
 * returned string is valid as long as this object exists and is not
 * modified. The ownership of the returned string is \em not
 * transferred to the caller.
 *
 * @param string_obj	String object
 * @param val		Returned string value
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_string_get(
	const struct bt_object *string_obj, const char **val);

/**
 * Sets the string value of the string object \p string_obj to
 * \p val.
 *
 * \p val is copied.
 *
 * @param string_obj	String object
 * @param val		New string value (copied)
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_string_set(struct bt_object *string_obj,
	const char *val);

/**
 * Gets the size of the array object \p array_obj, that is, the number
 * of elements contained in \p array_obj.
 *
 * @param array_obj	Array object
 * @returns		Array size if the return value is 0 (empty) or a
 *			positive value, or one of
 *			#bt_object_status negative values otherwise
 */
extern int bt_object_array_size(const struct bt_object *array_obj);

/**
 * Returns \c true if the array object \p array_obj.
 *
 * @param array_obj	Array object
 * @returns		\c true if \p array_obj is empty
 */
extern bool bt_object_array_is_empty(const struct bt_object *array_obj);

/**
 * Gets the element object of the array object \p array_obj at the
 * index \p index.
 *
 * The returned object's reference count is incremented, unless it's
 * a null object.
 *
 * @param array_obj	Array object
 * @param index		Index of element to get
 * @returns		Element object at index \p index on success,
 *			or \c NULL on error
 */
extern struct bt_object *bt_object_array_get(const struct bt_object *array_obj,
	size_t index);

/**
 * Appends the element object \p element_obj to the array object
 * \p array_obj.
 *
 * The appended object's reference count is incremented, unless it's
 * a null object.
 *
 * @param array_obj	Array object
 * @param element_obj	Element object to append
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append(struct bt_object *array_obj,
	struct bt_object *element_obj);

/**
 * Appends the boolean value \p val to the array object \p array_obj.
 * This is a convenience function which creates the underlying boolean
 * object before appending it.
 *
 * The created boolean object's reference count is set to 1.
 *
 * @param array_obj	Array object
 * @param val		Boolean value to append
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append_bool(
	struct bt_object *array_obj, bool val);

/**
 * Appends the integer value \p val to the array object \p array_obj.
 * This is a convenience function which creates the underlying integer
 * object before appending it.
 *
 * The created integer object's reference count is set to 1.
 *
 * @param array_obj	Array object
 * @param val		Integer value to append
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append_integer(
	struct bt_object *array_obj, int64_t val);

/**
 * Appends the floating point number value \p val to the array object
 * \p array_obj. This is a convenience function which creates the
 * underlying floating point number object before appending it.
 *
 * The created floating point number object's reference count is
 * set to 1.
 *
 * @param array_obj	Array object
 * @param val		Floating point number value to append
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append_float(
	struct bt_object *array_obj, double val);

/**
 * Appends the string value \p val to the array object \p array_obj.
 * This is a convenience function which creates the underlying string
 * object before appending it.
 *
 * \p val is copied.
 *
 * The created string object's reference count is set to 1.
 *
 * @param array_obj	Array object
 * @param val		String value to append (copied)
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append_string(
	struct bt_object *array_obj, const char *val);

/**
 * Appends an empty array object to the array object \p array_obj.
 * This is a convenience function which creates the underlying array
 * object before appending it.
 *
 * The created array object's reference count is set to 1.
 *
 * @param array_obj	Array object
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append_array(
	struct bt_object *array_obj);

/**
 * Appends an empty map object to the array object \p array_obj. This
 * is a convenience function which creates the underlying map object
 * before appending it.
 *
 * The created map object's reference count is set to 1.
 *
 * @param array_obj	Array object
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_append_map(
	struct bt_object *array_obj);

/**
 * Replaces the element object at index \p index of the array object
 * \p array_obj by \p element_obj.
 *
 * The replaced object's reference count is decremented, unless it's
 * a null object. The reference count of \p element_obj is incremented,
 * unless it's a null object.
 *
 * @param array_obj	Array object
 * @param index		Index of element object to replace
 * @param element_obj	New element object at position \p index of
 *			\p array_obj
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_array_set(struct bt_object *array_obj,
	size_t index, struct bt_object *element_obj);

/**
 * Gets the size of a map object, that is, the number of elements
 * contained in a map object.
 *
 * @param map_obj	Map object
 * @returns		Map size if the return value is 0 (empty) or a
 *			positive value, or one of
 *			#bt_object_status negative values otherwise
 */
extern int bt_object_map_size(const struct bt_object *map_obj);

/**
 * Returns \c true if the map object \p map_obj.
 *
 * @param map_obj	Map object
 * @returns		\c true if \p map_obj is empty
 */
extern bool bt_object_map_is_empty(const struct bt_object *map_obj);

/**
 * Gets the element object associated with the key \p key within the
 * map object \p map_obj.
 *
 * The returned object's reference count is incremented, unless it's
 * a null object.
 *
 * @param map_obj	Map object
 * @param key		Key of the element to get
 * @returns		Element object associated with the key \p key
 *			on success, or \c NULL on error
 */
extern struct bt_object *bt_object_map_get(const struct bt_object *map_obj,
	const char *key);

/**
 * Calls a provided user function \p cb for each element of the map
 * object \p map_obj.
 *
 * The object passed to the user function is a <b>weak reference</b>:
 * you must call bt_object_get() on it to obtain your own reference.
 *
 * The key passed to the user function is only valid in the scope of
 * this user function.
 *
 * The user function must return \c true to continue the loop, or
 * \c false to break it.
 *
 * @param map_obj	Map object
 * @param cb		User function to call back
 * @param data		User data passed to the user function
 * @returns		One of #bt_object_status values; more
 *			specifically, #BT_OBJECT_STATUS_CANCELLED is
 *			returned if the loop was cancelled by the user
 *			function
 */
extern enum bt_object_status bt_object_map_foreach(
	const struct bt_object *map_obj, bt_object_map_foreach_cb cb,
	void *data);

/**
 * Returns whether or not the map object \p map_obj contains the
 * key \p key.
 *
 * @param map_obj	Map object
 * @param key		Key to check
 * @returns		\c true if \p map_obj contains the key \p key,
 *			or \c false if it doesn't have \p key or
 *			on error
 */
extern bool bt_object_map_has_key(const struct bt_object *map_obj,
	const char *key);

/**
 * Inserts the element object \p element_obj associated with the key
 * \p key into the map object \p map_obj. If \p key exists in
 * \p map_obj, the associated element object is first put, and then
 * replaced by \p element_obj.
 *
 * \p key is copied.
 *
 * The inserted object's reference count is incremented, unless it's
 * a null object.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of object to insert
 * @param element_obj	Element object to insert, associated with the
 *			key \p key
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert(
	struct bt_object *map_obj, const char *key,
	struct bt_object *element_obj);

/**
 * Inserts the boolean value \p val associated with the key \p key into
 * the map object \p map_obj. This is a convenience function which
 * creates the underlying boolean object before inserting it.
 *
 * \p key is copied.
 *
 * The created boolean object's reference count is set to 1.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of boolean value to insert
 * @param val		Boolean value to insert, associated with the
 *			key \p key
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert_bool(
	struct bt_object *map_obj, const char *key, bool val);

/**
 * Inserts the integer value \p val associated with the key \p key into
 * the map object \p map_obj. This is a convenience function which
 * creates the underlying integer object before inserting it.
 *
 * \p key is copied.
 *
 * The created integer object's reference count is set to 1.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of integer value to insert
 * @param val		Integer value to insert, associated with the
 *			key \p key
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert_integer(
	struct bt_object *map_obj, const char *key, int64_t val);

/**
 * Inserts the floating point number value \p val associated with the
 * key \p key into the map object \p map_obj. This is a convenience
 * function which creates the underlying floating point number object
 * before inserting it.
 *
 * \p key is copied.
 *
 * The created floating point number object's reference count is
 * set to 1.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of floating point number value to
 *			insert
 * @param val		Floating point number value to insert,
 *			associated with the key \p key
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert_float(
	struct bt_object *map_obj, const char *key, double val);

/**
 * Inserts the string value \p val associated with the key \p key into
 * the map object \p map_obj. This is a convenience function which
 * creates the underlying string object before inserting it.
 *
 * \p val and \p key are copied.
 *
 * The created string object's reference count is set to 1.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of string value to insert
 * @param val		String value to insert, associated with the
 *			key \p key
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert_string(
	struct bt_object *map_obj, const char *key, const char *val);

/**
 * Inserts an empty array object associated with the key \p key into
 * the map object \p map_obj. This is a convenience function which
 * creates the underlying array object before inserting it.
 *
 * \p key is copied.
 *
 * The created array object's reference count is set to 1.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of empty array to insert
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert_array(
	struct bt_object *map_obj, const char *key);

/**
 * Inserts an empty map object associated with the key \p key into
 * the map object \p map_obj. This is a convenience function which
 * creates the underlying map object before inserting it.
 *
 * \p key is copied.
 *
 * The created map object's reference count is set to 1.
 *
 * @param map_obj	Map object
 * @param key		Key (copied) of empty map to insert
 * @returns		One of #bt_object_status values
 */
extern enum bt_object_status bt_object_map_insert_map(
	struct bt_object *map_obj, const char *key);

/**
 * Creates a deep copy of the object \p object.
 *
 * The created object's reference count is set to 1, unless
 * \p object is a null object.
 *
 * @param object	Object to copy
 * @returns		Deep copy of \p object on success, or \c NULL
 *			on error
 */
extern struct bt_object *bt_object_copy(const struct bt_object *object);

/**
 * Compares the objects \p object_a and \p object_b and returns \c true
 * if they have the same content.
 *
 * @param object_a	Object A
 * @param object_B	Object B
 * @returns		\c true if \p object_a and \p object_b have the
 *			same content, or \c false if they differ or on
 *			error
 */
extern bool bt_object_compare(const struct bt_object *object_a,
	const struct bt_object *object_b);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_OBJECTS_H */
