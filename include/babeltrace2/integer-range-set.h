#ifndef BABELTRACE2_INTEGER_RANGE_SET_H
#define BABELTRACE2_INTEGER_RANGE_SET_H

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
@defgroup api-int-rs Integer range sets

@brief
    Sets of unsigned and signed 64-bit integer ranges.

An <strong><em>integer range set</em></strong>
is an \em unordered set of integer ranges.

An <strong><em>integer range</em></strong> represents all the
integers \b 𝑥 which satisfy
(<em>lower&nbsp;value</em>&nbsp;≤&nbsp;<strong>𝑥</strong>&nbsp;≤&nbsp;<em>upper&nbsp;value</em>).

For example, an unsigned integer range set could contain the ranges
[5,&nbsp;14], [199,&nbsp;2001], and [1976,&nbsp;3000].

This integer range set API offers unsigned and signed 64-bit integer
ranges and integer range sets with dedicated C&nbsp;types:

- #bt_integer_range_unsigned
- #bt_integer_range_signed
- #bt_integer_range_set_unsigned
- #bt_integer_range_set_signed

This API uses the \em abstract #bt_integer_range_set type for common
properties and operations (for example,
bt_integer_range_set_get_range_count()).
\ref api-fund-c-typing "Upcast" a specific
integer range set to the #bt_integer_range_set type with
bt_integer_range_set_unsigned_as_range_set_const() or
bt_integer_range_set_signed_as_range_set_const().

An integer range set is a \ref api-fund-shared-object "shared object":
get a new reference with bt_integer_range_set_unsigned_get_ref() or
bt_integer_range_set_signed_get_ref() and put an existing reference with
bt_integer_range_set_unsigned_put_ref() or
bt_integer_range_set_signed_put_ref().

An integer range is a \ref api-fund-unique-object "unique object": it
belongs to the integer range set which contains it.

Some library functions \ref api-fund-freezing "freeze" integer range
sets on success. The documentation of those functions indicate this
postcondition.

Create an empty integer range set with
bt_integer_range_set_unsigned_create() or
bt_integer_range_set_signed_create().

Add an integer range to an integer range set with
bt_integer_range_set_unsigned_add_range() or
bt_integer_range_set_signed_add_range(). Although integer ranges can
overlap, specific functions of the \bt_api expect an integer range set
with non-overlapping integer ranges.

As of \bt_name_version_min_maj, you cannot remove an existing
integer range from an integer range set.

Check that two integer ranges are equal with
bt_integer_range_unsigned_is_equal() or
bt_integer_range_signed_is_equal().

Check that two integer range sets are equal with
bt_integer_range_set_unsigned_is_equal() or
bt_integer_range_set_signed_is_equal().
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_integer_range_unsigned bt_integer_range_unsigned

@brief
    Unsigned 64-bit integer range.


@typedef struct bt_integer_range_signed bt_integer_range_signed

@brief
    Signed 64-bit integer range.


@typedef struct bt_integer_range_set bt_integer_range_set

@brief
    Set of 64-bit integer ranges.

This is an abstract type for common properties and operations. See \ref
api-fund-c-typing to learn more about conceptual object type
inheritance.


@typedef struct bt_integer_range_set_unsigned bt_integer_range_set_unsigned;

@brief
    Set of unsigned 64-bit integer ranges.


@typedef struct bt_integer_range_set_signed bt_integer_range_set_signed;

@brief
    Set of signed 64-bit integer ranges.

@}
*/

/*!
@name Unsigned integer range
@{
*/

/*!
@brief
    Returns the lower value of the unsigned integer range
    \bt_p{int_range}.

The returned lower value is included in \bt_p{int_range}.

@param[in] int_range
    Unsigned integer range of which to get the lower value.

@returns
    Lower value of \bt_p{int_range}.

@bt_pre_not_null{int_range}
@bt_pre_is_bool_val{int_range}
*/
extern uint64_t bt_integer_range_unsigned_get_lower(
		const bt_integer_range_unsigned *int_range);

/*!
@brief
    Returns the upper value of the unsigned integer range
    \bt_p{int_range}.

The returned upper value is included in \bt_p{int_range}.

@param[in] int_range
    Unsigned integer range of which to get the upper value.

@returns
    Upper value of \bt_p{int_range}.

@bt_pre_not_null{int_range}
@bt_pre_is_bool_val{int_range}
*/
extern uint64_t bt_integer_range_unsigned_get_upper(
		const bt_integer_range_unsigned *int_range);

/*!
@brief
    Returns whether or not the unsigned integer range
    \bt_p{a_int_range} is equal to \bt_p{b_int_range}.

Two unsigned integer ranges are considered equal if they have the same
lower and upper values.

@param[in] a_int_range
    Unsigned integer range A.
@param[in] b_int_range
    Unsigned integer range B.

@returns
    #BT_TRUE if \bt_p{a_int_range} is equal to
    \bt_p{b_int_range}.

@bt_pre_not_null{a_int_range}
@bt_pre_not_null{b_int_range}
*/
extern bt_bool bt_integer_range_unsigned_is_equal(
		const bt_integer_range_unsigned *a_int_range,
		const bt_integer_range_unsigned *b_int_range);

/*! @} */

/*!
@name Signed integer range
@{
*/

/*!
@brief
    Returns the lower value of the signed integer range
    \bt_p{int_range}.

The returned lower value is included in \bt_p{int_range}.

@param[in] int_range
    Signed integer range of which to get the lower value.

@returns
    Lower value of \bt_p{int_range}.

@bt_pre_not_null{int_range}
@bt_pre_is_bool_val{int_range}
*/
extern int64_t bt_integer_range_signed_get_lower(
		const bt_integer_range_signed *int_range);

/*!
@brief
    Returns the upper value of the signed integer range
    \bt_p{int_range}.

The returned upper value is included in \bt_p{int_range}.

@param[in] int_range
    Signed integer range of which to get the upper value.

@returns
    Upper value of \bt_p{int_range}.

@bt_pre_not_null{int_range}
@bt_pre_is_bool_val{int_range}
*/
extern int64_t bt_integer_range_signed_get_upper(
		const bt_integer_range_signed *int_range);

/*!
@brief
    Returns whether or not the signed integer range
    \bt_p{a_int_range} is equal to \bt_p{b_int_range}.

Two signed integer ranges are considered equal if they have the same
lower and upper values.

@param[in] a_int_range
    Signed integer range A.
@param[in] b_int_range
    Signed integer range B.

@returns
    #BT_TRUE if \bt_p{a_int_range} is equal to
    \bt_p{b_int_range}.

@bt_pre_not_null{a_int_range}
@bt_pre_not_null{b_int_range}
*/
extern bt_bool bt_integer_range_signed_is_equal(
		const bt_integer_range_signed *a_int_range,
		const bt_integer_range_signed *b_int_range);

/*! @} */

/*!
@name Integer range set: common
@{
*/

/*!
@brief
    Status codes for bt_integer_range_set_unsigned_add_range() and
    bt_integer_range_set_signed_add_range().
*/
typedef enum bt_integer_range_set_add_range_status {
	/*!
	@brief
	    Success.
	*/
	BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_integer_range_set_add_range_status;

/*!
@brief
    Returns the number of integer ranges contained in the integer
    range set \bt_p{int_range_set}.

@note
    The parameter \bt_p{int_range_set} has the abstract type
    #bt_integer_range_set: use
    bt_integer_range_set_unsigned_as_range_set_const() or
    bt_integer_range_set_signed_as_range_set_const() to upcast a
    specific integer range set to this type.

@param[in] int_range_set
    Integer range set of which to get the number of contained integer
    ranges.

@returns
    Number of contained integer ranges in \bt_p{int_range_set}.

@bt_pre_not_null{int_range_set}
*/
extern uint64_t bt_integer_range_set_get_range_count(
		const bt_integer_range_set *int_range_set);

/*! @} */

/*!
@name Unsigned integer range set
@{
*/

/*!
@brief
    Creates and returns an empty set of unsigned 64-bit integer ranges.

@returns
    New unsigned integer range set, or \c NULL on memory error.
*/
extern bt_integer_range_set_unsigned *bt_integer_range_set_unsigned_create(void);

/*!
@brief
    Adds an unsigned 64-bit integer range having the lower value
    \bt_p{lower} and the upper value \bt_p{upper} to the unsigned
    integer range set
    \bt_p{int_range_set}.

The values \bt_p{lower} and \bt_p{upper} are included in the unsigned
integer range to add to \bt_p{int_range_set}.

@param[in] int_range_set
    Unsigned integer range set to which to add an unsigned integer
    range.
@param[in] lower
    Lower value (included) of the unsigned integer range to add to
    \bt_p{int_range_set}.
@param[in] upper
    Upper value (included) of the unsigned integer range to add to
    \bt_p{int_range_set}.

@retval #BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_OK
    Success.
@retval #BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{int_range_set}
@bt_pre_hot{int_range_set}
@pre
    \bt_p{lower} ≤ \bt_p{upper}.
*/
extern bt_integer_range_set_add_range_status
bt_integer_range_set_unsigned_add_range(
		bt_integer_range_set_unsigned *int_range_set,
		uint64_t lower, uint64_t upper);

/*!
@brief
    Borrows the unsigned integer range at index \bt_p{index} from the
    unsigned integer range set \bt_p{int_range_set}.

@param[in] int_range_set
    Unsigned integer range set from which to borrow the unsigned integer
    range at index \bt_p{index}.
@param[in] index
    Index of the unsigned integer range to borrow from
    \bt_p{int_range_set}.

@returns
    @parblock
    \em Borrowed reference of the unsigned integer range of
    \bt_p{int_range_set} at index \bt_p{index}.

    The returned pointer remains valid until \bt_p{int_range_set} is
    modified.
    @endparblock

@bt_pre_not_null{int_range_set}
@pre
    \bt_p{index} is less than the number of unsigned integer ranges in
    \bt_p{int_range_set} (as returned by
    bt_integer_range_set_get_range_count()).
*/
extern const bt_integer_range_unsigned *
bt_integer_range_set_unsigned_borrow_range_by_index_const(
		const bt_integer_range_set_unsigned *int_range_set,
		uint64_t index);

/*!
@brief
    Returns whether or not the unsigned integer range set
    \bt_p{int_range_set_a} is equal to \bt_p{int_range_set_b}.

Two unsigned integer range sets are considered equal if they contain the
exact same unsigned integer ranges, whatever the order. In other words,
an unsigned integer range set containing [2,&nbsp;9] and [10,&nbsp;15]
is \em not equal to an unsigned integer range containing [2,&nbsp;15].

@param[in] int_range_set_a
    Unsigned integer range set A.
@param[in] int_range_set_b
    Unsigned integer range set B.

@returns
    #BT_TRUE if \bt_p{int_range_set_a} is equal to
    \bt_p{int_range_set_b}.

@bt_pre_not_null{int_range_set_a}
@bt_pre_not_null{int_range_set_b}
*/
extern bt_bool bt_integer_range_set_unsigned_is_equal(
		const bt_integer_range_set_unsigned *int_range_set_a,
		const bt_integer_range_set_unsigned *int_range_set_b);

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the unsigned integer range set
    \bt_p{int_range_set} to the abstract #bt_integer_range_set type.

@param[in] int_range_set
    @parblock
    Unsigned integer range set to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{int_range_set} as an abstract integer range set.
*/
static inline
const bt_integer_range_set *bt_integer_range_set_unsigned_as_range_set_const(
		const bt_integer_range_set_unsigned *int_range_set)
{
	return __BT_UPCAST_CONST(bt_integer_range_set, int_range_set);
}

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the unsigned integer range set \bt_p{int_range_set}.

@param[in] int_range_set
    @parblock
    Unsigned integer range set of which to increment the reference
    count.

    Can be \c NULL.
    @endparblock

@sa bt_integer_range_set_unsigned_put_ref() &mdash;
    Decrements the reference count of an unsigned integer range set.
*/
extern void bt_integer_range_set_unsigned_get_ref(
		const bt_integer_range_set_unsigned *int_range_set);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the unsigned integer range set \bt_p{int_range_set}.

@param[in] int_range_set
    @parblock
    Unsigned integer range set of which to decrement the reference
    count.

    Can be \c NULL.
    @endparblock

@sa bt_integer_range_set_unsigned_get_ref() &mdash;
    Increments the reference count of an unsigned integer range set.
*/
extern void bt_integer_range_set_unsigned_put_ref(
		const bt_integer_range_set_unsigned *int_range_set);

/*!
@brief
    Decrements the reference count of the unsigned integer range set
    \bt_p{_int_range_set}, and then sets \bt_p{_int_range_set} to \c
    NULL.

@param _int_range_set
    @parblock
    Unsigned integer range set of which to decrement the reference
    count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_int_range_set}
*/
#define BT_INTEGER_RANGE_SET_UNSIGNED_PUT_REF_AND_RESET(_int_range_set)	\
	do {								\
		bt_integer_range_set_unsigned_put_ref(_int_range_set);	\
		(_int_range_set) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the unsigned integer range set
    \bt_p{_dst}, sets \bt_p{_dst} to \bt_p{_src}, and then sets
    \bt_p{_src} to \c NULL.

This macro effectively moves an unsigned integer range set reference
from the expression \bt_p{_src} to the expression \bt_p{_dst}, putting
the existing \bt_p{_dst} reference.

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
#define BT_INTEGER_RANGE_SET_UNSIGNED_MOVE_REF(_dst, _src)	\
	do {							\
		bt_integer_range_set_unsigned_put_ref(_dst);	\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*!
@name Signed integer range set
@{
*/

/*!
@brief
    Creates and returns an empty set of signed 64-bit integer ranges.

@returns
    New signed integer range set, or \c NULL on memory error.
*/
extern bt_integer_range_set_signed *bt_integer_range_set_signed_create(void);

/*!
@brief
    Adds a signed 64-bit integer range having the lower value
    \bt_p{lower} and the upper value \bt_p{upper} to the signed
    integer range set
    \bt_p{int_range_set}.

The values \bt_p{lower} and \bt_p{upper} are included in the signed
integer range to add to \bt_p{int_range_set}.

@param[in] int_range_set
    Signed integer range set to which to add a signed integer
    range.
@param[in] lower
    Lower value (included) of the signed integer range to add to
    \bt_p{int_range_set}.
@param[in] upper
    Upper value (included) of the signed integer range to add to
    \bt_p{int_range_set}.

@retval #BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_OK
    Success.
@retval #BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{int_range_set}
@bt_pre_hot{int_range_set}
@pre
    \bt_p{lower} ≤ \bt_p{upper}.
*/
extern bt_integer_range_set_add_range_status
bt_integer_range_set_signed_add_range(
		bt_integer_range_set_signed *int_range_set,
		int64_t lower, int64_t upper);

/*!
@brief
    Borrows the signed integer range at index \bt_p{index} from the
    signed integer range set \bt_p{int_range_set}.

@param[in] int_range_set
    Signed integer range set from which to borrow the signed integer
    range at index \bt_p{index}.
@param[in] index
    Index of the signed integer range to borrow from
    \bt_p{int_range_set}.

@returns
    @parblock
    \em Borrowed reference of the signed integer range of
    \bt_p{int_range_set} at index \bt_p{index}.

    The returned pointer remains valid until \bt_p{int_range_set} is
    modified.
    @endparblock

@bt_pre_not_null{int_range_set}
@pre
    \bt_p{index} is less than the number of signed integer ranges in
    \bt_p{int_range_set} (as returned by
    bt_integer_range_set_get_range_count()).
*/
extern const bt_integer_range_signed *
bt_integer_range_set_signed_borrow_range_by_index_const(
		const bt_integer_range_set_signed *int_range_set, uint64_t index);

/*!
@brief
    Returns whether or not the signed integer range set
    \bt_p{int_range_set_a} is equal to \bt_p{int_range_set_b}.

Two signed integer range sets are considered equal if they contain the
exact same signed integer ranges, whatever the order. In other words,
a signed integer range set containing [-57,&nbsp;23] and [24,&nbsp;42]
is \em not equal to a signed integer range containing [-57,&nbsp;42].

@param[in] int_range_set_a
    Signed integer range set A.
@param[in] int_range_set_b
    Signed integer range set B.

@returns
    #BT_TRUE if \bt_p{int_range_set_a} is equal to
    \bt_p{int_range_set_b}.

@bt_pre_not_null{int_range_set_a}
@bt_pre_not_null{int_range_set_b}
*/
extern bt_bool bt_integer_range_set_signed_is_equal(
		const bt_integer_range_set_signed *int_range_set_a,
		const bt_integer_range_set_signed *int_range_set_b);

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the signed integer range set
    \bt_p{int_range_set} to the abstract #bt_integer_range_set type.

@param[in] int_range_set
    @parblock
    Signed integer range set to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{int_range_set} as an abstract integer range set.
*/
static inline
const bt_integer_range_set *bt_integer_range_set_signed_as_range_set_const(
		const bt_integer_range_set_signed *int_range_set)
{
	return __BT_UPCAST_CONST(bt_integer_range_set, int_range_set);
}

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the signed integer range set \bt_p{int_range_set}.

@param[in] int_range_set
    @parblock
    Signed integer range set of which to increment the reference
    count.

    Can be \c NULL.
    @endparblock

@sa bt_integer_range_set_signed_put_ref() &mdash;
    Decrements the reference count of a signed integer range set.
*/
extern void bt_integer_range_set_signed_get_ref(
		const bt_integer_range_set_signed *int_range_set);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the signed integer range set \bt_p{int_range_set}.

@param[in] int_range_set
    @parblock
    Signed integer range set of which to decrement the reference
    count.

    Can be \c NULL.
    @endparblock

@sa bt_integer_range_set_signed_get_ref() &mdash;
    Increments the reference count of a signed integer range set.
*/
extern void bt_integer_range_set_signed_put_ref(
		const bt_integer_range_set_signed *int_range_set);

/*!
@brief
    Decrements the reference count of the signed integer range set
    \bt_p{_int_range_set}, and then sets \bt_p{_int_range_set} to \c
    NULL.

@param _int_range_set
    @parblock
    Signed integer range set of which to decrement the reference
    count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_int_range_set}
*/
#define BT_INTEGER_RANGE_SET_SIGNED_PUT_REF_AND_RESET(_int_range_set)	\
	do {								\
		bt_integer_range_set_signed_put_ref(_int_range_set);	\
		(_int_range_set) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the signed integer range set
    \bt_p{_dst}, sets \bt_p{_dst} to \bt_p{_src}, and then sets
    \bt_p{_src} to \c NULL.

This macro effectively moves a signed integer range set reference
from the expression \bt_p{_src} to the expression \bt_p{_dst}, putting
the existing \bt_p{_dst} reference.

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
#define BT_INTEGER_RANGE_SET_SIGNED_MOVE_REF(_dst, _src)	\
	do {							\
		bt_integer_range_set_signed_put_ref(_dst);	\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_INTEGER_RANGE_SET_H */
