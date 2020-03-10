/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2015 Michael Jeanson <mjeanson@efficios.com>
 */

#ifndef _BABELTRACE_COMPAT_STDLIB_H
#define _BABELTRACE_COMPAT_STDLIB_H

/*
 * This compat wrapper can be removed and replaced by g_mkdtemp() when we bump
 * the requirement on glib to version 2.30.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <glib.h>

#ifdef HAVE_MKDTEMP

static inline
char *bt_mkdtemp(char *template)
{
	return mkdtemp(template);
}

#elif GLIB_CHECK_VERSION(2,30,0)

#include <glib/gstdio.h>
static inline
char *bt_mkdtemp(char *template)
{
	return g_mkdtemp(template);
}

#else

static inline
char *bt_mkdtemp(char *template)
{
	char *ret;

	ret = mktemp(template);
	if (!ret) {
		goto end;
	}

	if(mkdir(template, 0700)) {
		ret = NULL;
		goto end;
	}

	ret = template;
end:
	return ret;
}

#endif

#endif /* _BABELTRACE_COMPAT_STDLIB_H */
