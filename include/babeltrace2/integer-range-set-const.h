#ifndef BABELTRACE2_INTEGER_RANGE_SET_CONST_H
#define BABELTRACE2_INTEGER_RANGE_SET_CONST_H

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

extern uint64_t bt_integer_range_unsigned_get_lower(
		const bt_integer_range_unsigned *range);

extern uint64_t bt_integer_range_unsigned_get_upper(
		const bt_integer_range_unsigned *range);

extern int64_t bt_integer_range_signed_get_lower(
		const bt_integer_range_signed *range);

extern int64_t bt_integer_range_signed_get_upper(
		const bt_integer_range_signed *range);

extern bt_bool bt_integer_range_unsigned_compare(
		const bt_integer_range_unsigned *range_a,
		const bt_integer_range_unsigned *range_b);

extern bt_bool bt_integer_range_signed_compare(
		const bt_integer_range_signed *range_a,
		const bt_integer_range_signed *range_b);

static inline
const bt_integer_range_set *bt_integer_range_set_unsigned_as_range_set_const(
		const bt_integer_range_set_unsigned *range_set)
{
	return __BT_UPCAST_CONST(bt_integer_range_set, range_set);
}

static inline
const bt_integer_range_set *bt_integer_range_set_signed_as_range_set_const(
		const bt_integer_range_set_signed *range_set)
{
	return __BT_UPCAST_CONST(bt_integer_range_set, range_set);
}

extern uint64_t bt_integer_range_set_get_range_count(const bt_integer_range_set *range_set);

extern const bt_integer_range_unsigned *
bt_integer_range_set_unsigned_borrow_range_by_index_const(
		const bt_integer_range_set_unsigned *range_set, uint64_t index);

extern const bt_integer_range_signed *
bt_integer_range_set_signed_borrow_range_by_index_const(
		const bt_integer_range_set_signed *range_set, uint64_t index);

extern bt_bool bt_integer_range_set_unsigned_compare(
		const bt_integer_range_set_unsigned *range_set_a,
		const bt_integer_range_set_unsigned *range_set_b);

extern bt_bool bt_integer_range_set_signed_compare(
		const bt_integer_range_set_signed *range_set_a,
		const bt_integer_range_set_signed *range_set_b);

extern void bt_integer_range_set_unsigned_get_ref(
		const bt_integer_range_set_unsigned *range_set);

extern void bt_integer_range_set_unsigned_put_ref(
		const bt_integer_range_set_unsigned *range_set);

#define BT_RANGE_SET_UNSIGNED_PUT_REF_AND_RESET(_var)			\
	do {								\
		bt_integer_range_set_unsigned_put_ref(_var);		\
		(_var) = NULL;						\
	} while (0)

#define BT_RANGE_SET_UNSIGNED_MOVE_REF(_var_dst, _var_src)		\
	do {								\
		bt_integer_range_set_unsigned_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);				\
		(_var_src) = NULL;					\
	} while (0)

extern void bt_integer_range_set_signed_get_ref(
		const bt_integer_range_set_signed *range_set);

extern void bt_integer_range_set_signed_put_ref(
		const bt_integer_range_set_signed *range_set);

#define BT_RANGE_SET_SIGNED_PUT_REF_AND_RESET(_var)			\
	do {								\
		bt_integer_range_set_signed_put_ref(_var);		\
		(_var) = NULL;						\
	} while (0)

#define BT_RANGE_SET_SIGNED_MOVE_REF(_var_dst, _var_src)		\
	do {								\
		bt_integer_range_set_signed_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);				\
		(_var_src) = NULL;					\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_INTEGER_RANGE_SET_CONST_H */
