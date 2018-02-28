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
 * The macros in this header use macros defined in
 * <babeltrace/lib-logging-internal.h>. We don't want this header to
 * automatically include <babeltrace/lib-logging-internal.h> because you
 * need to manually define BT_LOG_TAG before including
 * <babeltrace/lib-logging-internal.h> and it is unexpected that you
 * also need to define it before including this header.
 *
 * This is a reminder that in order to use
 * <babeltrace/assert-internal.hpp>, you also need to use logging
 * explicitly.
 */
#ifndef BABELTRACE_LIB_LOGGING_INTERNAL_H
# error Include <babeltrace/lib-logging-internal.h> before this header.
#endif

#include <stdlib.h>

#ifdef BT_DEV_MODE
/*
 * Asserts that the library precondition _cond is satisfied.
 *
 * If _cond is false, log a fatal statement using _fmt and the optional
 * arguments using BT_LIB_LOGF(), and abort.
 *
 * To assert that a postcondition is satisfied or that some internal
 * object/context/value is in the expected state, use BT_ASSERT().
 */
# define BT_ASSERT_PRE(_cond, _fmt, ...)				\
	do {								\
		if (!(_cond)) {						\
			BT_LOGF_STR("Library precondition not satisfied:"); \
			BT_LIB_LOGF((_fmt), ##__VA_ARGS__);		\
			BT_LOGF_STR("Aborting...");			\
			abort();					\
		}							\
	} while (0)

/*
 * Marks a function as being only used within a BT_ASSERT_PRE() context.
 */
# define BT_ASSERT_PRE_FUNC

/*
 * Prints the details of an unsatisfied precondition without immediately
 * aborting. You should use this within a function which checks
 * preconditions, but which is called from a BT_ASSERT_PRE() context, so
 * that the function can still return its result for BT_ASSERT_PRE() to
 * evaluate it.
 *
 * Example:
 *
 *     BT_ASSERT_PRE_FUNC
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
# define BT_ASSERT_PRE_MSG	BT_LIB_LOGF
#else
# define BT_ASSERT_PRE(_cond, _fmt, ...)
# define BT_ASSERT_PRE_FUNC	__attribute__((unused))
# define BT_ASSERT_PRE_MSG(_fmt, ...)
#endif /* BT_DEV_MODE */

/*
 * Developer mode: asserts that a given variable is not NULL.
 */
#define BT_ASSERT_PRE_NON_NULL(_obj, _obj_name)				\
	BT_ASSERT_PRE((_obj) != NULL, "%s is NULL: ", _obj_name)

/*
 * Developer mode: asserts that a given object is NOT frozen. This macro
 * checks the `frozen` field of _obj.
 */
#define BT_ASSERT_PRE_HOT(_obj, _obj_name, _fmt, ...)			\
	BT_ASSERT_PRE(!(_obj)->frozen, "%s is frozen" _fmt, _obj_name,	\
		##__VA_ARGS__)

#endif /* BABELTRACE_ASSERT_PRE_INTERNAL_H */
