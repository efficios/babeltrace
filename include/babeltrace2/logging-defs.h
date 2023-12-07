/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

/*
 * No include guards here: it is safe to include this file multiple
 * times.
 */

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

/*
 * This is NOT part of the API.
 *
 * These macros are used internally in preprocessor conditions to define
 * logging macros.
 */

#ifndef __BT_LOGGING_LEVEL_TRACE
# define __BT_LOGGING_LEVEL_TRACE	1
#endif

#ifndef __BT_LOGGING_LEVEL_DEBUG
# define __BT_LOGGING_LEVEL_DEBUG	2
#endif

#ifndef __BT_LOGGING_LEVEL_INFO
# define __BT_LOGGING_LEVEL_INFO	3
#endif

#ifndef __BT_LOGGING_LEVEL_WARNING
# define __BT_LOGGING_LEVEL_WARNING	4
#endif

#ifndef __BT_LOGGING_LEVEL_ERROR
# define __BT_LOGGING_LEVEL_ERROR	5
#endif

#ifndef __BT_LOGGING_LEVEL_FATAL
# define __BT_LOGGING_LEVEL_FATAL	6
#endif

#ifndef __BT_LOGGING_LEVEL_NONE
# define __BT_LOGGING_LEVEL_NONE	0xff
#endif
