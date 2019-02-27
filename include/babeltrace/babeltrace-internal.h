#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

/*
 * Copyright 2012 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <stdio.h>
#include <glib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <babeltrace/compat/string-internal.h>
#include <babeltrace/types.h>

#define PERROR_BUFLEN	200

#ifndef likely
# ifdef __GNUC__
#  define likely(x)      __builtin_expect(!!(x), 1)
# else
#  define likely(x)      (!!(x))
# endif
#endif

#ifndef unlikely
# ifdef __GNUC__
#  define unlikely(x)    __builtin_expect(!!(x), 0)
# else
#  define unlikely(x)    (!!(x))
# endif
#endif

#ifndef min
#define min(a, b)	(((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)	(((a) > (b)) ? (a) : (b))
#endif

#ifndef max_t
#define max_t(type, a, b)	\
	((type) (a) > (type) (b) ? (type) (a) : (type) (b))
#endif

static inline
bool bt_safe_to_mul_int64(int64_t a, int64_t b)
{
	if (a == 0 || b == 0) {
		return true;
	}

	return a < INT64_MAX / b;
}

static inline
bool bt_safe_to_mul_uint64(uint64_t a, uint64_t b)
{
	if (a == 0 || b == 0) {
		return true;
	}

	return a < UINT64_MAX / b;
}

static inline
bool bt_safe_to_add_int64(int64_t a, int64_t b)
{
	return a <= INT64_MAX - b;
}

static inline
bool bt_safe_to_add_uint64(uint64_t a, uint64_t b)
{
	return a <= UINT64_MAX - b;
}

/*
 * Memory allocation zeroed
 */
#define zmalloc(x) calloc(1, x)

/*
 * BT_HIDDEN: set the hidden attribute for internal functions
 * On Windows, symbols are local unless explicitly exported,
 * see https://gcc.gnu.org/wiki/Visibility
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#define BT_HIDDEN
#else
#define BT_HIDDEN __attribute__((visibility("hidden")))
#endif

#ifndef __STRINGIFY
#define __STRINGIFY(x)	#x
#endif

#define TOSTRING(x)	__STRINGIFY(x)

#define BT_UNUSED	__attribute__((unused))

#endif
