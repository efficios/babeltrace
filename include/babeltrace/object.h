#ifndef BABELTRACE_OBJECT_H
#define BABELTRACE_OBJECT_H

/*
 * BabelTrace: common reference counting
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup refs Reference counting management
@ingroup apiref
@brief Common reference counting management for all Babeltrace objects.

@code
#include <babeltrace/object.h>
@endcode

The macros and functions of this module are everything that is needed
to handle the <strong><em>reference counting</em></strong> of
Babeltrace objects.

Any Babeltrace object can be shared by multiple owners thanks to
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counting</a>.

The Babeltrace C API complies with the following key principles:

1. When you call an API function which accepts a Babeltrace object
   pointer as a parameter, the API function <strong>borrows the
   reference</strong> for the <strong>duration of the function</strong>.

   @image html ref-count-user-calls.png

   The API function can also get a new reference if the system needs a
   more persistent reference, but the ownership is <strong>never
   transferred</strong> from the caller to the API function.

   In other words, the caller still owns the object after calling any
   API function: no function "steals" the user's reference (except
   bt_object_put_ref()).

2. An API function which \em returns a Babeltrace object pointer to the
   user returns a <strong>new reference</strong>. The caller becomes an
   owner of the object.

   @image html ref-count-api-returns.png

   It is your responsibility to discard the object when you don't
   need it anymore with bt_object_put_ref().

   For example, see bt_value_array_get().

3. A Babeltrace object pointer received as a parameter in a user
   function called back from an API function is a
   <strong>borrowed</strong>, or <strong>weak reference</strong>: if you
   need a reference which is more persistent than the duration of the
   user function, call bt_object_get_ref() on the pointer.

   @image html ref-count-callback.png

   For example, see bt_value_map_foreach_entry().

The two macros BT_OBJECT_PUT_REF_AND_RESET() and BT_OBJECT_MOVE_REF() operate on \em variables rather
than pointer values. You should use BT_OBJECT_PUT_REF_AND_RESET() instead of bt_object_put_ref() when
possible to avoid "double puts". For the same reason, you should use use
BT_OBJECT_MOVE_REF() instead of performing manual reference moves between
variables.

@file
@brief Reference counting management macros and functions.
@sa refs

@addtogroup refs
@{
*/

/**
@brief	Calls bt_object_put_ref() on a variable named \p _var, then
	sets \p _var to \c NULL.

Using this macro is considered safer than calling bt_object_put_ref() because it
makes sure that the variable which used to contain a reference to a
Babeltrace object is set to \c NULL so that a future BT_OBJECT_PUT_REF_AND_RESET() or
bt_object_put_ref() call will not cause another, unwanted reference decrementation.

@param[in,out] _var	Name of a variable containing a
			Babeltrace object's address (this address
			can be \c NULL).

@post <strong>If \p _var does not contain \p NULL</strong>,
	its reference count is decremented.
@post \p _var contains \c NULL.

@sa BT_OBJECT_MOVE_REF(): Transfers the ownership of a Babeltrace object from a
	variable to another.
*/
#define BT_OBJECT_PUT_REF_AND_RESET(_var)	\
	do {					\
		bt_object_put_ref(_var);	\
		(_var) = NULL;			\
	} while (0)

/**
@brief	Transfers the ownership of a Babeltrace object from a variable
	named \p _var_src to a variable named \p _var_dst.

This macro implements the following common pattern:

  1. Call bt_object_put_ref() on \p _var_dst to make sure the previous reference
     held by \p _var_dst is discarded.
  2. Assign \p _var_src to \p _var_dst.
  3. Set \p _var_src to \c NULL to avoid future, unwanted reference
     decrementation of \p _var_src.

@warning
You must \em not use this macro when both \p _var_dst and
\p _var_src contain the same Babeltrace object address and the reference
count of this object is 1. The initial call to bt_object_put_ref() on \p _var_dst
would destroy the object and leave a dangling pointer in \p _var_dst.

@param[in,out] _var_dst	Name of the destination variable, containing
			either the address of a Babeltrace object to
			put first, or \c NULL.
@param[in,out] _var_src	Name of the source variable, containing
			either the address of a Babeltrace object to
			move, or \c NULL.

@pre <strong>If \p _var_dst and \p _var_src contain the same
	value which is not \c NULL</strong>, this object's reference
	count is greater than 1.
@post <strong>If \c _var_dst is not \c NULL</strong>, its reference
	count is decremented.
@post \p _var_dst is equal to the value of \p _var_src \em before
	you called this macro.
@post \p _var_src is \c NULL.

@sa BT_OBJECT_PUT_REF_AND_RESET(): Calls bt_object_put_ref() on a variable, then sets it to \c NULL.
*/
#define BT_OBJECT_MOVE_REF(_var_dst, _var_src)	\
	do {					\
		bt_object_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);	\
		(_var_src) = NULL;		\
	} while (0)

/**
@brief  Increments the reference count of the Babeltrace object \p obj.

@param[in] obj	Babeltrace object of which to get a new reference
		(can be \c NULL).
@returns	\p obj

@post <strong>If \c obj is not \c NULL</strong>, its reference
	count is incremented.

@sa bt_object_put_ref(): Decrements the reference count of a Babeltrace object.
*/
void *bt_object_get_ref(void *obj);

/**
@brief	Decrements the reference count of the Babeltrace object
	\p obj.

When the object's reference count reaches 0, the object can no longer
be accessed and is considered \em destroyed.

@remarks
You should use the BT_OBJECT_PUT_REF_AND_RESET() macro instead of calling bt_object_put_ref() since the
former is generally safer.

@param[in] obj	Babeltrace object of which to drop a reference
		(can be \c NULL).

@post <strong>If \c obj is not \c NULL</strong>, its reference
	count is decremented.

@sa BT_OBJECT_PUT_REF_AND_RESET(): Calls bt_object_put_ref() on a variable, then sets it to \c NULL.
@sa BT_OBJECT_MOVE_REF(): Transfers the ownership of a Babeltrace object from a
	variable to another.
@sa bt_object_get_ref(): Increments the reference count of a Babeltrace object.
*/
void bt_object_put_ref(void *obj);

/**
@}
*/

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_OBJECT_H */
