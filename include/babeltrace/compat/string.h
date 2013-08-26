#ifndef _BABELTRACE_COMPAT_STRING_H
#define _BABELTRACE_COMPAT_STRING_H

/*
 * Copyright (C) 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <string.h>

#if !defined(__linux__) || ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !defined(_GNU_SOURCE))

/* XSI-compliant strerror_r */
static inline
int compat_strerror_r(int errnum, char *buf, size_t buflen)
{
	return strerror_r(errnum, buf, buflen);
}

#else

/* GNU-compliant strerror_r */
static inline
int compat_strerror_r(int errnum, char *buf, size_t buflen)
{
	char *retbuf;

	retbuf = strerror_r(errnum, buf, buflen);
	if (retbuf != buf)
		strncpy(buf, retbuf, buflen);
	buf[buflen - 1] = '\0';
	return 0;
}

#endif

#endif /* _BABELTRACE_COMPAT_STRING_H */
