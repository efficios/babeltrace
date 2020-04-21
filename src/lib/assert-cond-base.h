/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018-2020 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_ASSERT_COND_BASE_INTERNAL_H
#define BABELTRACE_ASSERT_COND_BASE_INTERNAL_H

/*
 * The macros in this header use macros defined in "lib/logging.h". We
 * don't want this header to automatically include "lib/logging.h"
 * because you need to manually define BT_LOG_TAG before including
 * "lib/logging.h" and it is unexpected that you also need to define it
 * before including this header.
 *
 * This is a reminder that in order to use "lib/assert-cond.h", you also
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
 * Prints the details of an unsatisfied precondition or postcondition
 * without immediately aborting. You should use this within a function
 * which checks preconditions or postconditions, but which is called
 * from a BT_ASSERT_PRE() or BT_ASSERT_POST() context, so that the
 * function can still return its result for
 * BT_ASSERT_PRE()/BT_ASSERT_POST() to evaluate it.
 *
 * Example:
 *
 *     static inline bool check_complex_precond(...)
 *     {
 *         ...
 *
 *         if (...) {
 *             BT_ASSERT_COND_MSG("Invalid object: ...", ...);
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
#define BT_ASSERT_COND_MSG(_fmt, ...)					\
	do {								\
		bt_lib_log(_BT_LOG_SRCLOC_FUNCTION, __FILE__,		\
			__LINE__, BT_LOG_FATAL, BT_LOG_TAG,		\
			(_fmt), ##__VA_ARGS__);				\
	} while (0)

/*
 * This function:
 *
 * 1. Generates a condition ID based on `cond_type`, `func`, and
 *    `id_suffix`.
 *
 * 2. Logs (FATAL level) the generated condition ID and function name
 *    (`func`).
 *
 * 3. Logs (FATAL level) a message using `fmt` and the optional
 *    arguments (same usage as BT_LIB_LOGF()).
 *
 * 4. Aborts.
 */
BT_HIDDEN
__attribute__((noreturn))
void bt_lib_assert_cond_failed(const char *cond_type, const char *func,
		const char *id_suffix, const char *fmt, ...);

/*
 * Internal to this file: asserts that the library condition `_cond` of
 * type `_cond_type` (`pre` or `post`), related to function `_func`,
 * and having the ID suffix `_id_suffix` is satisfied.
 *
 * If `_cond` is false, then this macro calls
 * bt_lib_assert_cond_failed().
 *
 * To assert that an _internal_ precondition or postcondition is
 * satisfied, use BT_ASSERT() or BT_ASSERT_DBG().
 */
#define _BT_ASSERT_COND(_cond_type, _func, _id_suffix, _cond, _fmt, ...) \
	do {								\
		if (!(_cond)) {						\
			bt_lib_assert_cond_failed(_cond_type, _func,	\
				_id_suffix, _fmt, ##__VA_ARGS__);	\
		}							\
	} while (0)

/*
 * Asserts that the library precondition `_cond` is satisfied.
 *
 * See _BT_ASSERT_COND() for details about the `_func` and `_id_suffix`
 * parameters.
 */
#define BT_ASSERT_PRE_FROM_FUNC(_func, _id_suffix, _cond, _fmt, ...)	\
	_BT_ASSERT_COND("pre", _func, _id_suffix, (_cond), _fmt, ##__VA_ARGS__)

/*
 * Like BT_ASSERT_PRE_FROM_FUNC(), but uses `__func__` (current function
 * name) as the `_func` parameter.
 */
#define BT_ASSERT_PRE(_id_suffix, _cond, _fmt, ...)			\
	BT_ASSERT_PRE_FROM_FUNC(__func__, _id_suffix, (_cond), _fmt, ##__VA_ARGS__)

/*
 * Asserts that the library postcondition `_cond` is satisfied.
 *
 * See _BT_ASSERT_COND() for details about the `_func` and `_id_suffix`
 * parameters.
 */
#define BT_ASSERT_POST(_func, _id_suffix, _cond, _fmt, ...)				\
	_BT_ASSERT_COND("post", _func, _id_suffix, (_cond), _fmt, ##__VA_ARGS__);

#ifdef BT_DEV_MODE
/* Developer mode version of BT_ASSERT_PRE_FROM_FUNC(). */
# define BT_ASSERT_PRE_DEV_FROM_FUNC(_func, _id_suffix, _cond, _fmt, ...) \
	BT_ASSERT_PRE_FROM_FUNC(_func, _id_suffix, (_cond), _fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_PRE(). */
# define BT_ASSERT_PRE_DEV(_id_suffix, _cond, _fmt, ...)		\
	BT_ASSERT_PRE(_id_suffix, (_cond), _fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_POST(). */
# define BT_ASSERT_POST_DEV(_func, _id_suffix, _cond, _fmt, ...)	\
	BT_ASSERT_POST(_func, _id_suffix, (_cond), _fmt, ##__VA_ARGS__)

/* Developer mode version of BT_ASSERT_COND_MSG(). */
# define BT_ASSERT_COND_DEV_MSG(_fmt, ...)				\
	BT_ASSERT_COND_MSG(_fmt, ##__VA_ARGS__)

/*
 * Marks a function as being only used within a BT_ASSERT_PRE_DEV() or
 * BT_ASSERT_POST_DEV() context.
 */
# define BT_ASSERT_COND_DEV_FUNC
#else
# define BT_ASSERT_COND_DEV_MSG(_fmt, ...)

# define BT_ASSERT_PRE_DEV_FROM_FUNC(_func, _id_suffix, _cond, _fmt, ...) \
	BT_USE_EXPR4(_func, _id_suffix, _cond, _fmt)

# define BT_ASSERT_PRE_DEV(_id_suffix, _cond, _fmt, ...)		\
	BT_USE_EXPR3(_id_suffix, _cond, _fmt)

# define BT_ASSERT_POST_DEV(_func, _id_suffix, _cond, _fmt, ...)	\
	BT_USE_EXPR4(_func, _id_suffix, _cond, _fmt)

# define BT_ASSERT_COND_DEV_FUNC	__attribute__((unused))
#endif /* BT_DEV_MODE */

/*
 * Mark anything that includes this file as supporting precondition and
 * postcondition assertion macros.
 */
#define BT_ASSERT_COND_SUPPORTED

#endif /* BABELTRACE_ASSERT_COND_BASE_INTERNAL_H */
