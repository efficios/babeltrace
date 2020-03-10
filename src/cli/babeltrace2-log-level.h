/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CLI_BABELTRACE_LOG_LEVEL_H
#define CLI_BABELTRACE_LOG_LEVEL_H

#include <babeltrace2/babeltrace.h>

#define ENV_BABELTRACE_CLI_LOG_LEVEL "BABELTRACE_CLI_LOG_LEVEL"

/*
 * Return the minimal (most verbose) log level between `a` and `b`.
 *
 * If one of the parameter has the value -1, it is ignored in the comparison
 * and the other value is returned.  If both parameters are -1, -1 is returned.
 */
static inline
int logging_level_min(int a, int b)
{
	if (a == -1) {
		return b;
	} else if (b == -1) {
		return a;
	} else {
		return MIN(a, b);
	}
}

void set_auto_log_levels(int *logging_level);

#endif /* CLI_BABELTRACE_LOG_LEVEL_H */
