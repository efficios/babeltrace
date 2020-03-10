/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2016 Michael Jeanson <mjeanson@efficios.com>
 */

#ifndef _BABELTRACE_COMPAT_UNISTD_H
#define _BABELTRACE_COMPAT_UNISTD_H

#include <unistd.h>

#ifdef __MINGW32__
#include <windows.h>
#include <errno.h>

#define _SC_PAGESIZE 30

static inline
long bt_sysconf(int name)
{
	SYSTEM_INFO si;

	switch(name) {
	case _SC_PAGESIZE:
		GetNativeSystemInfo(&si);
		return si.dwPageSize;
	default:
		errno = EINVAL;
		return -1;
	}
}

#else

static inline
long bt_sysconf(int name)
{
	return sysconf(name);
}

#endif
#endif /* _BABELTRACE_COMPAT_UNISTD_H */
