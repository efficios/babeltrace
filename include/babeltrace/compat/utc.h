#ifndef _BABELTRACE_UTC_H
#define _BABELTRACE_UTC_H

/*
 * Copyright (C) 2011-2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <time.h>

/* If set, use GNU or BSD timegm(3) */
#if defined(_BSD_SOURCE) || defined(_SVID_SOURCE)

static inline
time_t babeltrace_timegm(struct tm *tm)
{
	return timegm(tm);
}

#else

#include <string.h>
#include <stdlib.h>

/*
 * Note: Below implementation of timegm() is not thread safe
 * as it changes the environment
 * variable TZ. It is OK as long as it is kept in self-contained program,
 * but should not be used within thread-safe library code.
 */

static inline
time_t babeltrace_timegm(struct tm *tm)
{
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	/*
	 * Make a temporary copy, as the environment variable will be
	 * modified.
	 */
	if (tz) {
		tz = strdup(tz);
		if (!tz) {
			/*
			 * Memory allocation error.
			 */
			return (time_t) -1;
		}
	}

	/* Temporarily setting TZ to UTC */
	setenv("TZ", "UTC", 1);
	tzset();
	ret = mktime(tm);
	if (tz) {
		setenv("TZ", tz, 1);
		free(tz);
	} else {
		unsetenv("TZ");
	}
	tzset();
	return ret;
}

#endif

#endif /* _BABELTRACE_UTC_H */
