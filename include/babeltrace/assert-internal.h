#ifndef BABELTRACE_ASSERT_INTERNAL_H
#define BABELTRACE_ASSERT_INTERNAL_H

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

#include <assert.h>
#include <babeltrace/babeltrace-internal.h>

#ifdef BT_DEBUG_MODE
/*
 * Internal assertion (to detect logic errors on which the library user
 * has no influence). Use BT_ASSERT_PRE() to check a precondition which
 * must be directly or indirectly satisfied by the library user.
 */
# define BT_ASSERT(_cond)	do { assert(_cond); } while (0)

/*
 * Marks a function as being only used within a BT_ASSERT() context.
 */
# define BT_ASSERT_FUNC
#else
/*
 * When BT_DEBUG_MODE is not defined, define BT_ASSERT() macro to the following
 * to trick the compiler into thinking that the variable passed as condition to
 * the assertion is used. This is to prevent set-but-not-used warnings from the
 * compiler when assertions are disabled. The `sizeof` operator also makes sure
 * that the `_cond` expression is not evaluated, thus preventing unwanted side
 * effects.
 *
 * In-depth explanation: https://stackoverflow.com/questions/37411809/how-to-elegantly-fix-this-unused-variable-warning/37412551#37412551
 */
# define BT_ASSERT(_cond)	((void) sizeof((void) (_cond), 0))
# define BT_ASSERT_FUNC		BT_UNUSED
#endif /* BT_DEBUG_MODE */

#endif /* BABELTRACE_ASSERT_INTERNAL_H */
