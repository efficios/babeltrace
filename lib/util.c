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

#define BT_LOG_TAG "UTIL"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <inttypes.h>
#include <babeltrace/util.h>
#include <babeltrace/trace-ir/utils-internal.h>

bt_util_status bt_util_clock_cycles_to_ns_from_origin(uint64_t cycles,
		uint64_t frequency, int64_t offset_seconds,
		uint64_t offset_cycles, int64_t *ns)
{
	bool overflows;
	int64_t base_offset_ns;
	bt_util_status status = BT_UTIL_STATUS_OK;
	int ret;

	BT_ASSERT_PRE_NON_NULL(ns, "Nanoseconds (output)");
	BT_ASSERT_PRE(frequency != UINT64_C(-1) && frequency != 0,
		"Invalid frequency: freq=%" PRIu64, frequency);
	BT_ASSERT_PRE(offset_cycles < frequency,
		"Offset (cycles) is greater than frequency: "
		"offset-cycles=%" PRIu64 ", freq=%" PRIu64,
		offset_cycles, frequency);

	overflows = bt_util_get_base_offset_ns(offset_seconds, offset_cycles,
		frequency, &base_offset_ns);
	if (overflows) {
		status = BT_UTIL_STATUS_OVERFLOW;
		goto end;
	}

	ret = bt_util_ns_from_origin_inline(base_offset_ns,
		offset_seconds, offset_cycles,
		frequency, cycles, ns);
	if (ret) {
		status = BT_UTIL_STATUS_OVERFLOW;
	}

end:
	return status;
}
