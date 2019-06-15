#ifndef BABELTRACE_ERROR_CONST_H
#define BABELTRACE_ERROR_CONST_H

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

#include <stdint.h>
#include <stddef.h>

/* For bt_error, bt_error_cause */
#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern
uint64_t bt_error_get_cause_count(const bt_error *error);

extern
const bt_error_cause *bt_error_borrow_cause_by_index(
		const bt_error *error, uint64_t index);

extern
void bt_error_release(const bt_error *error);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_ERROR_CONST_H */
