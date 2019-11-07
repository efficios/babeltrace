#ifndef BABELTRACE_ASSERT_POST_INTERNAL_H
#define BABELTRACE_ASSERT_POST_INTERNAL_H

/*
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
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
 * This is a reminder that in order to use "lib/assert-post.h", you also
 * need to use logging explicitly.
 */

#ifndef BT_LIB_LOG_SUPPORTED
# error Include "lib/logging.h" before this header.
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include "common/macros.h"
#include "common/common.h"

/*
 * Prints the details of an unsatisfied postcondition without
 * immediately aborting. You should use this within a function which
 * checks postconditions, but which is called from a
 * BT_ASSERT_POST() context, so that the function can still return
 * its result for BT_ASSERT_POST() to evaluate it.
 *
 * Example:
 *
 *     BT_ASSERT_POST_FUNC
 *     static inline bool check_complex_postcond(...)
 *     {
 *         ...
 *
 *         if (...) {
 *             BT_ASSERT_POST_MSG("Unexpected status: ...", ...);
 *             return false;
 *         }
 *
 *         ...
 *     }
 *
 *     ...
 *
 *     BT_ASSERT_POST(check_complex_postcond(...),
 *                         "Postcondition is not satisfied: ...", ...);
 */
#define BT_ASSERT_POST_MSG(_fmt, ...)					\
	do {								\
		bt_lib_log(_BT_LOG_SRCLOC_FUNCTION, __FILE__,		\
			__LINE__, BT_LOG_FATAL, BT_LOG_TAG,		\
			(_fmt), ##__VA_ARGS__);				\
	} while (0)

/*
 * Asserts that the library postcondition `_cond` is satisfied.
 *
 * If `_cond` is false, log a fatal statement using `_fmt` and the
 * optional arguments (same usage as BT_LIB_LOGF()), and abort.
 *
 * To assert that a library precondition is satisfied (parameters from
 * the user), use BT_ASSERT_PRE().
 *
 * To assert that an internal postcondition is satisfied, use
 * BT_ASSERT() or BT_ASSERT_DBG().
 */
#define BT_ASSERT_POST(_cond, _fmt, ...)				\
	do {								\
		if (!(_cond)) {						\
			BT_ASSERT_POST_MSG("Babeltrace 2 library postcondition not satisfied; error is:"); \
			BT_ASSERT_POST_MSG(_fmt, ##__VA_ARGS__);	\
			BT_ASSERT_POST_MSG("Aborting...");		\
			bt_common_abort();					\
		}							\
	} while (0)

/*
 * Asserts that if there's an error on the current thread, an error status code
 * was returned.  Puts back the error in place (if there is one) such that if
 * the assertion hits, it will be possible to inspect it with a debugger.
 */
#define BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(_status)			\
	do {									\
		const struct bt_error *err = bt_current_thread_take_error();	\
		if (err) {							\
			bt_current_thread_move_error(err);			\
		}								\
		BT_ASSERT_POST(_status < 0 || !err,				\
			"Current thread has an error, but user function "	\
			"returned a non-error status: status=%s",		\
			bt_common_func_status_string(_status));			\
	} while (0)

/*
 * Asserts that the current thread has no error.
 */
#define BT_ASSERT_POST_NO_ERROR() \
	BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(0)

/*
 * Marks a function as being only used within a BT_ASSERT_POST()
 * context.
 */
#define BT_ASSERT_POST_FUNC

#ifdef BT_DEV_MODE
/* Developer mode version of BT_ASSERT_POST_MSG(). */
# define BT_ASSERT_POST_DEV_MSG(_fmt, ...)				\
	BT_ASSERT_POST_MSG(_fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_POST(). */
# define BT_ASSERT_POST_DEV(_cond, _fmt, ...)				\
	BT_ASSERT_POST((_cond), _fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(). */
# define BT_ASSERT_POST_DEV_NO_ERROR_IF_NO_ERROR_STATUS(_status) \
	BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(_status)

/* Developer mode version of `BT_ASSERT_POST_FUNC`. */
# define BT_ASSERT_POST_DEV_FUNC BT_ASSERT_POST_FUNC
#else
# define BT_ASSERT_POST_DEV_MSG(_fmt, ...)
# define BT_ASSERT_POST_DEV(_cond, _fmt, ...)	((void) sizeof((void) (_cond), 0))
# define BT_ASSERT_POST_DEV_NO_ERROR_IF_NO_ERROR_STATUS(_status) \
	((void) sizeof((void) (_status), 0))
# define BT_ASSERT_POST_DEV_FUNC	__attribute__((unused))
#endif /* BT_DEV_MODE */

#define BT_ASSERT_POST_SUPPORTED

#endif /* BABELTRACE_ASSERT_POST_INTERNAL_H */
