#ifndef BABELTRACE_GRAPH_CLOCK_CLASS_PRIORITY_MAP_H
#define BABELTRACE_GRAPH_CLOCK_CLASS_PRIORITY_MAP_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_clock_class;

/**
@defgroup graphclockclassprioritymap Clock class priority map
@ingroup graph
@brief Clock class priority map.

@code
#include <babeltrace/graph/clock-class-priority-map.h>
@endcode

A <strong><em>clock class priority map</em></strong> object associates
CTF IR clock classes to priorities. A clock class priority indicates
which clock class you should choose to sort notifications by time.

You need a clock class priority map object when you create
an \link graphnotifevent event notification\endlink or
an \link graphnotifinactivity inactivity notification\endlink.

A priority is a 64-bit unsigned integer. A lower value has a
\em higher priority. Multiple clock classes can have the same priority
within a given clock class priority map.

The following functions can \em freeze clock class priority map objects:

- bt_notification_event_create() freezes its clock class priority
  map parameter.
- bt_notification_inactivity_create() freezes its clock class priority
  map parameter.

You cannot modify a frozen clock class priority map object: it is
considered immutable, except for \link refs reference counting\endlink.

As with any Babeltrace object, clock class priority map objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

@file
@brief Clock class priority map type and functions.
@sa graphclockclassprioritymap

@addtogroup graphclockclassprioritymap
@{
*/

/**
@struct bt_clock_class_priority_map
@brief A clock class priority map.
@sa graphclockclassprioritymap
*/
struct bt_clock_class_priority_map;

/**
@brief  Creates an empty clock class priority map.

@returns	Created clock class priority map object, or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_clock_class_priority_map *bt_clock_class_priority_map_create(void);

/**
@brief	Returns the number of CTF IR clock classes contained in the
	clock class priority map \p clock_class_priority_map.

@param[in] clock_class_priority_map	Clock class priority map of
					which to get the number of
					clock classes.
@returns				Number of clock classes contained
					in \p clock_class_priority_map,
					or a negative value on error.

@prenotnull{clock_class_priority_map}
@postrefcountsame{clock_class_priority_map}
*/
extern int64_t bt_clock_class_priority_map_get_clock_class_count(
		struct bt_clock_class_priority_map *clock_class_priority_map);

extern struct bt_clock_class *
bt_clock_class_priority_map_borrow_clock_class_by_index(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		uint64_t index);

/**
@brief  Returns the CTF IR clock class at index \p index in the clock
	class priority map \p clock_class_priority_map.

@param[in] clock_class_priority_map	Clock class priority map of which
					to get the clock class at index
					\p index.
@param[in] index			Index of the clock class to find
					in \p clock_class_priority_map.
@returns				Clock class at index \p index in
					\p clock_class_priority_map,
					or \c NULL on error.

@prenotnull{clock_class_priority_map}
@pre \p index is lesser than the number of clock classes contained in
	the clock class priority map \p clock_class_priority_map (see
	bt_clock_class_priority_map_get_clock_class_count()).
@postrefcountsame{clock_class_priority_map}

@sa bt_clock_class_priority_map_get_clock_class_by_name(): Finds a clock
	class by name in a given clock class priority map.
@sa bt_clock_class_priority_map_get_highest_priority_clock_class():
	Returns the clock class with the highest priority contained
	in a given clock class priority map.
@sa bt_clock_class_priority_map_add_clock_class(): Adds a clock class
	to a clock class priority map.
*/
static inline
struct bt_clock_class *
bt_clock_class_priority_map_get_clock_class_by_index(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		uint64_t index)
{
	return bt_get(bt_clock_class_priority_map_borrow_clock_class_by_index(
		clock_class_priority_map, index));
}

extern struct bt_clock_class *
bt_clock_class_priority_map_borrow_clock_class_by_name(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		const char *name);

/**
@brief  Returns the CTF IR clock class named \c name found in the clock
	class priority map \p clock_class_priority_map.

@param[in] clock_class_priority_map	Clock class priority map of
					which to get the clock class
					named \p name.
@param[in] name				Name of the clock class to find
					in \p clock_class_priority_map.
@returns				Clock class named \p name in
					\p clock_class_priority_map,
					or \c NULL on error.

@prenotnull{clock_class_priority_map}
@prenotnull{name}
@postrefcountsame{clock_class_priority_map}
@postsuccessrefcountretinc

@sa bt_clock_class_priority_map_get_clock_class_by_index(): Returns the clock
	class contained in a given clock class priority map at
	a given index.
@sa bt_clock_class_priority_map_get_highest_priority_clock_class():
	Returns the clock class with the highest priority contained in
	a given clock class priority map.
@sa bt_clock_class_priority_map_add_clock_class(): Adds a clock class
	to a clock class priority map.
*/
static inline
struct bt_clock_class *bt_clock_class_priority_map_get_clock_class_by_name(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		const char *name)
{
	return bt_get(bt_clock_class_priority_map_borrow_clock_class_by_name(
		clock_class_priority_map, name));
}

extern struct bt_clock_class *
bt_clock_class_priority_map_borrow_highest_priority_clock_class(
		struct bt_clock_class_priority_map *clock_class_priority_map);

/**
@brief  Returns the CTF IR clock class with the currently highest
	priority within the clock class priority map
	\p clock_class_priority_map.

If multiple clock classes share the same highest priority in
\p clock_class_priority_map, you cannot deterministically know which one
this function returns.

@param[in] clock_class_priority_map	Clock class priority map of which
					to get the clock class with the
					highest priority.
@returns				Clock class with the highest
					priority within
					\p clock_class_priority_map, or
					\c NULL on error or if there are
					no clock classes in
					\p clock_class_priority_map.

@prenotnull{clock_class_priority_map}
@postrefcountsame{clock_class_priority_map}
@postsuccessrefcountretinc

@sa bt_clock_class_priority_map_get_clock_class_by_index(): Returns the clock
	class contained in a given clock class priority map at
	a given index.
@sa bt_clock_class_priority_map_get_clock_class_by_name(): Finds a
	clock class by name in a given clock class priority map.
@sa bt_clock_class_priority_map_add_clock_class(): Adds a clock class
	to a clock class priority map.
*/
static inline
struct bt_clock_class *
bt_clock_class_priority_map_get_highest_priority_clock_class(
		struct bt_clock_class_priority_map *clock_class_priority_map)
{
	return bt_get(
		bt_clock_class_priority_map_borrow_highest_priority_clock_class(
			clock_class_priority_map));
}

/**
@brief  Returns the priority of the CTF IR clock class \p clock_class
	contained within the clock class priority map
	\p clock_class_priority_map.

@param[in] clock_class_priority_map	Clock class priority map
					containing \p clock_class.
@param[in] clock_class			Clock class of which to get the
					priority.
@param[out] priority			Returned priority of
					\p clock_class within
					\p clock_class_priority_map.
@returns				0 on success, or a negative
					value on error.

@prenotnull{clock_class_priority_map}
@prenotnull{clock_class}
@prenotnull{priority}
@pre \p clock_class is contained in \p clock_class_priority_map (was
	previously added to \p clock_class_priority_map with
	bt_clock_class_priority_map_add_clock_class()).
@postrefcountsame{clock_class_priority_map}
@postrefcountsame{clock_class}

@sa bt_clock_class_priority_map_get_highest_priority_clock_class():
	Returns the clock class with the highest priority contained
	in a given clock class priority map.
*/
extern int bt_clock_class_priority_map_get_clock_class_priority(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		struct bt_clock_class *clock_class, uint64_t *priority);

/**
@brief	Adds the CTF IR clock class \p clock_class to the clock class
	priority map \p clock_class_priority_map with the
	priority \p priority.

You can call this function even if \p clock_class is frozen.

A lower priority value means a \em higher priority. Multiple clock
classes can have the same priority within a given clock class priority
map.

@param[in] clock_class_priority_map	Clock class priority map to
					which to add \p clock_class.
@param[in] clock_class			Clock class to add to
					\p clock_class_priority_map.
@param[in] priority			Priority of \p clock_class within
					\p clock_class_priority_map,
					where a lower value means a
					higher priority.
@returns				0 on success, or a negative
					value on error.

@prenotnull{clock_class_priority_map}
@prenotnull{clock_class}
@prehot{clock_class_priority_map}
@postrefcountsame{clock_class_priority_map}
@postsuccessrefcountinc{clock_class}

@sa bt_clock_class_priority_map_get_clock_class_by_index(): Returns the clock
	class contained in a given clock class priority map
	at a given index.
@sa bt_clock_class_priority_map_get_clock_class_by_name(): Finds a
	clock class by name in a given clock class priority map.
*/
extern int bt_clock_class_priority_map_add_clock_class(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		struct bt_clock_class *clock_class, uint64_t priority);

/**
@brief	Creates a copy of the clock class priority map
	\p clock_class_priority_map.

You can copy a frozen clock class priority map: the resulting copy is
<em>not frozen</em>.

@param[in] clock_class_priority_map	Clock class priority map to copy.
@returns				Copy of \p clock_class_priority_map
					on success, or a negative value
					on error.

@prenotnull{clock_class_priority_map}
@postrefcountsame{clock_class_priority_map}
@postsuccessrefcountret1
@post <strong>On success</strong>, the returned clock class priority map
	is not frozen.
*/
extern struct bt_clock_class_priority_map *bt_clock_class_priority_map_copy(
		struct bt_clock_class_priority_map *clock_class_priority_map);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_CLOCK_CLASS_PRIORITY_MAP_H */
