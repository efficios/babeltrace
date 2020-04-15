/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/UTIL"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <babeltrace2/babeltrace.h>
#include "lib/trace-ir/utils.h"

bt_util_clock_cycles_to_ns_from_origin_status
bt_util_clock_cycles_to_ns_from_origin(uint64_t cycles,
		uint64_t frequency, int64_t offset_seconds,
		uint64_t offset_cycles, int64_t *ns)
{
	bool overflows;
	int64_t base_offset_ns;
	bt_util_clock_cycles_to_ns_from_origin_status status =
		BT_FUNC_STATUS_OK;
	int ret;

	BT_ASSERT_PRE_NO_ERROR();
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
		status = BT_FUNC_STATUS_OVERFLOW_ERROR;
		goto end;
	}

	ret = bt_util_ns_from_origin_inline(base_offset_ns,
		offset_seconds, offset_cycles,
		frequency, cycles, ns);
	if (ret) {
		status = BT_FUNC_STATUS_OVERFLOW_ERROR;
	}

end:
	return status;
}
