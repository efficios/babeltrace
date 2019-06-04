#ifndef BABELTRACE_UTIL_H
#define BABELTRACE_UTIL_H

/*
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
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

typedef enum bt_util_status {
	BT_UTIL_STATUS_OK = 0,
	BT_UTIL_STATUS_OVERFLOW = -75,
} bt_util_status;

bt_util_status bt_util_clock_cycles_to_ns_from_origin(uint64_t cycles,
		uint64_t frequency, int64_t offset_seconds,
		uint64_t offset_cycles, int64_t *ns);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_UTIL_H */
