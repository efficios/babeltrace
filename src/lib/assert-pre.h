#ifndef BABELTRACE_ASSERT_PRE_INTERNAL_H
#define BABELTRACE_ASSERT_PRE_INTERNAL_H

/*
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
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

/*
 * The macros in this header use macros defined in "lib/logging.h".
 * We don't want this header to automatically include
 * "lib/logging.h" because you need to manually define BT_LOG_TAG
 * before including "lib/logging.h" and it is unexpected that you
 * also need to define it before including this header.
 *
 * This is a reminder that in order to use "lib/assert-pre.h", you also
 * need to use logging explicitly.
 */

#ifndef BT_LIB_LOG_SUPPORTED
# error Include "lib/logging.h" before this header.
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include "common/common.h"
#include "common/macros.h"

/*
 * Prints the details of an unsatisfied precondition without immediately
 * aborting. You should use this within a function which checks
 * preconditions, but which is called from a BT_ASSERT_PRE() context, so
 * that the function can still return its result for BT_ASSERT_PRE() to
 * evaluate it.
 *
 * Example:
 *
 *     static inline bool check_complex_precond(...)
 *     {
 *         ...
 *
 *         if (...) {
 *             BT_ASSERT_PRE_MSG("Invalid object: ...", ...);
 *             return false;
 *         }
 *
 *         ...
 *     }
 *
 *     ...
 *
 *     BT_ASSERT_PRE(check_complex_precond(...),
 *                   "Precondition is not satisfied: ...", ...);
 */
#define BT_ASSERT_PRE_MSG(_fmt, ...)					\
	do {								\
		bt_lib_log(_BT_LOG_SRCLOC_FUNCTION, __FILE__,		\
			__LINE__, BT_LOG_FATAL, BT_LOG_TAG,		\
			(_fmt), ##__VA_ARGS__);				\
	} while (0)

/*
 * Asserts that the library precondition `_cond` is satisfied.
 *
 * If `_cond` is false, log a fatal statement using `_fmt` and the
 * optional arguments (same usage as BT_LIB_LOGF()), and abort.
 *
 * To assert that a library postcondition is satisfied (return from user
 * code), use BT_ASSERT_POST().
 *
 * To assert that an internal postcondition is satisfied, use
 * BT_ASSERT() or BT_ASSERT_DBG().
 */
#define BT_ASSERT_PRE(_cond, _fmt, ...)				\
	do {								\
		if (!(_cond)) {						\
			BT_ASSERT_PRE_MSG("Babeltrace 2 library precondition not satisfied; error is:"); \
			BT_ASSERT_PRE_MSG(_fmt, ##__VA_ARGS__);	\
			BT_ASSERT_PRE_MSG("Aborting...");		\
			bt_common_abort();					\
		}							\
	} while (0)

/*
 * Asserts that a given variable `_obj` named `_obj_name` (capitalized)
 * is not `NULL`.
 */
#define BT_ASSERT_PRE_NON_NULL(_obj, _obj_name)				\
	BT_ASSERT_PRE((_obj), "%s is NULL: ", _obj_name)

/*
 * Asserts that a given index `_index` is less than a given length
 * `_length`.
 */
#define BT_ASSERT_PRE_VALID_INDEX(_index, _length)			\
	BT_ASSERT_PRE((_index) < (_length),				\
		"Index is out of bounds: index=%" PRIu64 ", "		\
		"count=%" PRIu64, (uint64_t) (_index), (uint64_t) (_length))

/*
 * Asserts that the current thread has no error set.
 */
#define BT_ASSERT_PRE_NO_ERROR() \
	do {									\
		const struct bt_error *err = bt_current_thread_take_error();	\
		if (err) {							\
			bt_current_thread_move_error(err);			\
		}								\
		BT_ASSERT_PRE(!err,						\
			"API function called while current thread has an "	\
			"error: function=%s", __func__);			\
	} while (0)

#ifdef BT_DEV_MODE
/* Developer mode version of BT_ASSERT_PRE_MSG(). */
# define BT_ASSERT_PRE_DEV_MSG(_fmt, ...)				\
	BT_ASSERT_PRE_MSG(_fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_PRE(). */
# define BT_ASSERT_PRE_DEV(_cond, _fmt, ...)				\
	BT_ASSERT_PRE((_cond), _fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_PRE_NON_NULL() */
# define BT_ASSERT_PRE_DEV_NON_NULL(_obj, _obj_name)			\
	BT_ASSERT_PRE_NON_NULL((_obj), (_obj_name))

/*
 * Developer mode: asserts that a given object `_obj` named `_obj_name`
 * (capitalized) is NOT frozen. This macro checks the `frozen` field of
 * `_obj`.
 *
 * This currently only exists in developer mode because some freezing
 * functions can be called on the fast path, so they too are only
 * enabled in developer mode.
 */
# define BT_ASSERT_PRE_DEV_HOT(_obj, _obj_name, _fmt, ...)		\
	BT_ASSERT_PRE(!(_obj)->frozen, "%s is frozen" _fmt, _obj_name,	\
		##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_PRE_VALID_INDEX() */
# define BT_ASSERT_PRE_DEV_VALID_INDEX(_index, _length)			\
	BT_ASSERT_PRE_VALID_INDEX((_index), (_length))

/* Developer mode version of BT_ASSERT_PRE_NO_ERROR(). */
# define BT_ASSERT_PRE_DEV_NO_ERROR() \
	BT_ASSERT_PRE_NO_ERROR()

/*
 * Marks a function as being only used within a BT_ASSERT_PRE_DEV()
 * context.
 */
# define BT_ASSERT_PRE_DEV_FUNC
#else
# define BT_ASSERT_PRE_DEV_MSG(_fmt, ...)
# define BT_ASSERT_PRE_DEV(_cond, _fmt, ...)	((void) sizeof((void) (_cond), 0))
# define BT_ASSERT_PRE_DEV_NON_NULL(_obj, _obj_name)			\
	((void) sizeof((void) (_obj), (void) (_obj_name), 0))
# define BT_ASSERT_PRE_DEV_HOT(_obj, _obj_name, _fmt, ...)		\
	((void) sizeof((void) (_obj), (void) (_obj_name), 0))
# define BT_ASSERT_PRE_DEV_VALID_INDEX(_index, _length)			\
	((void) sizeof((void) (_index), (void) (_length), 0))
# define BT_ASSERT_PRE_DEV_NO_ERROR()
# define BT_ASSERT_PRE_DEV_FUNC		__attribute__((unused))
#endif /* BT_DEV_MODE */

#define BT_ASSERT_PRE_SUPPORTED

#endif /* BABELTRACE_ASSERT_PRE_INTERNAL_H */
