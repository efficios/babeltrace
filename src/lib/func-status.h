#ifndef BABELTRACE_FUNC_STATUS_INTERNAL_H
#define BABELTRACE_FUNC_STATUS_INTERNAL_H

/*
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

#define __BT_IN_BABELTRACE_H
#include <babeltrace2/func-status.h>

/*
 * Aliases without a `__` prefix for internal code: this is just easier
 * to read.
 */
#define BT_FUNC_STATUS_AGAIN		__BT_FUNC_STATUS_AGAIN
#define BT_FUNC_STATUS_END		__BT_FUNC_STATUS_END
#define BT_FUNC_STATUS_USER_ERROR	__BT_FUNC_STATUS_USER_ERROR
#define BT_FUNC_STATUS_ERROR		__BT_FUNC_STATUS_ERROR
#define BT_FUNC_STATUS_INTERRUPTED	__BT_FUNC_STATUS_INTERRUPTED
#define BT_FUNC_STATUS_UNKNOWN_OBJECT	__BT_FUNC_STATUS_UNKNOWN_OBJECT
#define BT_FUNC_STATUS_MEMORY_ERROR	__BT_FUNC_STATUS_MEMORY_ERROR
#define BT_FUNC_STATUS_NO_MATCH		__BT_FUNC_STATUS_NO_MATCH
#define BT_FUNC_STATUS_NOT_FOUND	__BT_FUNC_STATUS_NOT_FOUND
#define BT_FUNC_STATUS_OK		__BT_FUNC_STATUS_OK
#define BT_FUNC_STATUS_OVERFLOW_ERROR	__BT_FUNC_STATUS_OVERFLOW_ERROR

#endif /* BABELTRACE_FUNC_STATUS_INTERNAL_H */
