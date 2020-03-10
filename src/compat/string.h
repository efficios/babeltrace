/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _BABELTRACE_COMPAT_STRING_H
#define _BABELTRACE_COMPAT_STRING_H

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_STRNLEN
static inline
size_t bt_strnlen(const char *str, size_t max)
{
	return strnlen(str, max);
}
#else
static inline
size_t bt_strnlen(const char *str, size_t max)
{
	size_t ret;
	const char *end;

	end = memchr(str, 0, max);

	if (end) {
		ret = (size_t) (end - str);
	} else {
		ret = max;
	}

	return ret;
}
#endif /* HAVE_STRNLEN */

#ifdef HAVE_STRNDUP
static inline
char *bt_strndup(const char *s, size_t n)
{
	return strndup(s, n);
}
#else
static inline
char *bt_strndup(const char *s, size_t n)
{
	char *ret;
	size_t navail;

	if (!s) {
		ret = NULL;
		goto end;
	}

	/* min() */
	navail = strlen(s) + 1;
	if ((n + 1) < navail) {
		navail = n + 1;
	}

	ret = malloc(navail);
	if (!ret) {
		goto end;
	}

	memcpy(ret, s, navail);
	ret[navail - 1] = '\0';
end:
	return ret;
}
#endif /* HAVE_STRNDUP */

#endif /* _BABELTRACE_COMPAT_STRING_H */
