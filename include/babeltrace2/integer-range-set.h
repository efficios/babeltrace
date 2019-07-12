#ifndef BABELTRACE2_INTEGER_RANGE_SET_H
#define BABELTRACE2_INTEGER_RANGE_SET_H

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
#include <babeltrace2/integer-range-set-const.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_integer_range_set_unsigned *bt_integer_range_set_unsigned_create(void);

typedef enum bt_integer_range_set_add_range_status {
	BT_RANGE_SET_ADD_RANGE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_RANGE_SET_ADD_RANGE_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_integer_range_set_add_range_status;

extern bt_integer_range_set_add_range_status bt_integer_range_set_unsigned_add_range(
		bt_integer_range_set_unsigned *range_set,
		uint64_t lower, uint64_t upper);

extern bt_integer_range_set_signed *bt_integer_range_set_signed_create(void);

extern bt_integer_range_set_add_range_status bt_integer_range_set_signed_add_range(
		bt_integer_range_set_signed *range_set,
		int64_t lower, int64_t upper);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_INTEGER_RANGE_SET_H */
