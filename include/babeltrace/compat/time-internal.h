#ifndef _BABELTRACE_INCLUDE_COMPAT_TIME_H
#define _BABELTRACE_INCLUDE_COMPAT_TIME_H

/*
 * Copyright (C) 2013 JP Ikaheimonen <jp_ikaheimonen@mentor.com>
 *               2016 Michael Jeanson <mjeanson@efficios.com>
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
#include <stdlib.h>

#ifdef __MINGW32__

#include <string.h>

/*
 * The Windows version of the time functions use one common tm structure per
 * thread which makes them thread-safe. Implement the POSIX _r variants by
 * copying this to a user supplied struct.
 */

static inline
struct tm *bt_gmtime_r(const time_t *timep, struct tm *result)
{
	struct tm *local_res;

	if (!result) {
		goto error;
	}

	local_res = gmtime(timep);
	if (!local_res) {
		result = NULL;
		goto error;
	}

	memcpy(result, local_res, sizeof(struct tm));

error:
	return result;
}

static inline
struct tm *bt_localtime_r(const time_t *timep, struct tm *result)
{
	struct tm *local_res;

	if (!result) {
		goto error;
	}

	local_res = localtime(timep);
	if (!local_res) {
		result = NULL;
		goto error;
	}

	memcpy(result, local_res, sizeof(struct tm));

error:
	return result;
}

#else /* __MINGW32__ */

static inline
struct tm *bt_gmtime_r(const time_t *timep, struct tm *result)
{
	return gmtime_r(timep, result);
}

static inline
struct tm *bt_localtime_r(const time_t *timep, struct tm *result)
{
	return localtime_r(timep, result);
}

#endif /* __MINGW32__ */
#endif /* _BABELTRACE_INCLUDE_COMPAT_TIME_H */
