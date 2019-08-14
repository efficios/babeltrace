#ifndef CLI_BABELTRACE_LOG_LEVEL_H
#define CLI_BABELTRACE_LOG_LEVEL_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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
