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
time_t bt_timegm(struct tm *tm)
{
	return timegm(tm);
}

#elif defined(__MINGW32__)

static inline
time_t bt_timegm(struct tm *tm)
{
	return _mkgmtime(tm);
}

#else

#include <errno.h>

/*
 * This is a simple implementation of timegm() it just turns the "struct tm" into
 * a GMT time_t. It does not normalize any of the fields of the "struct tm", nor
 * does it set tm_wday or tm_yday.
 */

static inline
int bt_leapyear(int year)
{
    return ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0));
}

static inline
time_t bt_timegm(struct tm *tm)
{
	int year, month, total_days;

	int monthlen[2][12] = {
		/* Days per month for a regular year */
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		/* Days per month for a leap year */
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	};

	if ((tm->tm_mon >= 12) ||
			(tm->tm_mday >= 32) ||
			(tm->tm_hour >= 24) ||
			(tm->tm_min >= 60) ||
			(tm->tm_sec >= 61)) {
		errno = EOVERFLOW;
		return (time_t) -1;
	}

	/* Add 365 days for each year since 1970 */
	total_days = 365 * (tm->tm_year - 70);

	/* Add one day for each leap year since 1970 */
	for (year = 70; year < tm->tm_year; year++) {
		if (bt_leapyear(1900 + year)) {
			total_days++;
		}
	}

	/* Add days for each remaining month */
	for (month = 0; month < tm->tm_mon; month++) {
		total_days += monthlen[bt_leapyear(1900 + year)][month];
	}

	/* Add remaining days */
	total_days += tm->tm_mday - 1;

	return ((((total_days * 24) + tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec);
}

#endif

#endif /* _BABELTRACE_UTC_H */
