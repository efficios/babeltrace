/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2015 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _BABELTRACE_COMPAT_STDIO_H
#define _BABELTRACE_COMPAT_STDIO_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include "common/assert.h"

#define BT_GETLINE_MINBUFLEN	64

static inline
char * _bt_getline_bufalloc(char **lineptr, size_t *n, size_t linelen)
{
	size_t buflen = *n;
	char *buf = *lineptr;

	if (buflen >= linelen && buf) {
		return buf;
	}
	if (!buf) {
		buflen = BT_GETLINE_MINBUFLEN;
	} else {
		buflen = buflen << 1;
		if (buflen < BT_GETLINE_MINBUFLEN) {
			buflen = BT_GETLINE_MINBUFLEN;
		}
	}
	/* Check below not strictly needed, extra safety. */
	if (buflen < linelen) {
		buflen = linelen;
	}
	buf = realloc(buf, buflen);
	if (!buf) {
		errno = ENOMEM;
		return NULL;
	}
	*n = buflen;
	*lineptr = buf;
	return buf;
}

/*
 * Returns line length (including possible final \n, excluding final
 * \0). On end of file, returns -1 with nonzero feof(stream) and errno
 * set to 0. On error, returns -1 with errno set.
 *
 * This interface is similar to the getline(3) man page part of the
 * Linux man-pages project, release 3.74. One major difference from the
 * Open Group POSIX specification is that this implementation does not
 * necessarily set the ferror() flag on error (because it is internal to
 * libc).
 */
static inline
ssize_t bt_getline(char **lineptr, size_t *n, FILE *stream)
{
	size_t linelen = 0;
	char *buf;
	int found_eof = 0;

	if (!lineptr || !n) {
		errno = EINVAL;
		return -1;
	}
	for (;;) {
		char c;
		int ret;

		ret = fgetc(stream);
		if (ret == EOF) {
			if (ferror(stream)) {
				/* ferror() is set, errno set by fgetc(). */
				return -1;
			}
			BT_ASSERT_DBG(feof(stream));
			found_eof = 1;
			break;
		}
		c = (char) ret;
		if (linelen == SSIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		buf = _bt_getline_bufalloc(lineptr, n, ++linelen);
		if (!buf) {
			return -1;
		}
		buf[linelen - 1] = c;
		if (c == '\n') {
			break;
		}
	}
	if (!linelen && found_eof) {
		/* feof() is set. */
		errno = 0;
		return -1;
	}
	buf = _bt_getline_bufalloc(lineptr, n, ++linelen);
	if (!buf) {
		return -1;
	}
	buf[linelen - 1] = '\0';
	return linelen - 1;	/* Count don't include final \0. */
}

#endif /* _BABELTRACE_COMPAT_STDIO_H */
