/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013 JP Ikaheimonen <jp_ikaheimonen@mentor.com>
 * Copyright (C) 2016 Michael Jeanson <mjeanson@efficios.com>
 */

#ifndef _BABELTRACE_INCLUDE_COMPAT_TIME_H
#define _BABELTRACE_INCLUDE_COMPAT_TIME_H

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
