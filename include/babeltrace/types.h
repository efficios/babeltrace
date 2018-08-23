#ifndef BABELTRACE_TYPES_H
#define BABELTRACE_TYPES_H

/*
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctypes Babeltrace C types
@ingroup apiref
@brief Babeltrace C types.

@code
#include <babeltrace/types.h>
@endcode

This header contains custom type definitions used across the library.

@file
@brief Babeltrace C types.
@sa ctypes

@addtogroup ctypes
@{
*/

/// False boolean value for the #bt_bool type.
#define BT_FALSE	0

/// True boolean value for the #bt_bool type.
#define BT_TRUE		1

/**
@brief	Babeltrace's boolean type.

Use only the #BT_FALSE and #BT_TRUE definitions for #bt_bool parameters.
It is guaranteed that the library functions which return a #bt_bool
value return either #BT_FALSE or #BT_TRUE.

You can always test the truthness of a #bt_bool value directly, without
comparing it to #BT_TRUE directly:

@code
bt_bool ret = bt_some_function(...);

if (ret) {
	// ret is true
}
@endcode
*/
typedef int bt_bool;

typedef const uint8_t *bt_uuid;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TYPES_H */
