/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef _BABELTRACE_LIMITS_H
#define _BABELTRACE_LIMITS_H

#include <limits.h>

#ifdef __linux__

#define BABELTRACE_HOST_NAME_MAX HOST_NAME_MAX

#elif defined(__FreeBSD__)

#define BABELTRACE_HOST_NAME_MAX MAXHOSTNAMELEN

#elif defined(_POSIX_HOST_NAME_MAX)

#define BABELTRACE_HOST_NAME_MAX _POSIX_HOST_NAME_MAX

#else

#define BABELTRACE_HOST_NAME_MAX 256

#endif /* __linux__, __FreeBSD__, _POSIX_HOST_NAME_MAX */

/* GNU Hurd has no PATH_MAX, use a sensible default */
#ifdef __GNU__
#define PATH_MAX 4096
#endif /* __GNU__ */

#endif /* _BABELTRACE_LIMITS_H */
