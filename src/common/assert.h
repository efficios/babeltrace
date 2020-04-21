/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2018-2019 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018-2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_ASSERT_INTERNAL_H
#define BABELTRACE_ASSERT_INTERNAL_H

#include <assert.h>
#include <glib.h>

#include "common/macros.h"

BT_HIDDEN
extern void bt_common_assert_failed(const char *file, int line,
		const char *func, const char *assertion)
		__attribute__((noreturn));

/*
 * Internal assertion (to detect logic errors on which the library user
 * has no influence). Use BT_ASSERT_PRE*() or BT_ASSERT_POST*() to check
 * preconditions or postconditions which must be directly or indirectly
 * satisfied by the library user.
 *
 * BT_ASSERT() is enabled in both debug and non-debug modes.
 */
#define BT_ASSERT(_cond)                                                       \
	do {                                                                   \
		if (!(_cond)) {                                                \
			bt_common_assert_failed(__FILE__, __LINE__, __func__,  \
				G_STRINGIFY(_cond));                           \
		}                                                              \
	} while (0)

/*
 * Marks a function as being only used within a BT_ASSERT() context.
 */
#define BT_ASSERT_FUNC

#ifdef BT_DEBUG_MODE

/*
 * Debug mode internal assertion.
 */
#define BT_ASSERT_DBG(_cond)	BT_ASSERT(_cond)

/*
 * Marks a function as being only used within a BT_ASSERT_DBG() context.
 */
#define BT_ASSERT_DBG_FUNC

#else /* BT_DEBUG_MODE */
# define BT_ASSERT_DBG(_cond)	BT_USE_EXPR(_cond)
# define BT_ASSERT_DBG_FUNC	__attribute__((unused))
#endif /* BT_DEBUG_MODE */

#endif /* BABELTRACE_ASSERT_INTERNAL_H */
