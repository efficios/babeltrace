/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _BABELTRACE_ALIGN_H
#define _BABELTRACE_ALIGN_H

#include "compat/compiler.h"
#include "compat/limits.h"

#define BT_ALIGN(x, a)		__BT_ALIGN_MASK(x, (__typeof__(x))(a) - 1)
#define __BT_ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define BT_PTR_ALIGN(p, a)		((__typeof__(p)) BT_ALIGN((unsigned long) (p), a))
#define BT_ALIGN_FLOOR(x, a)	__BT_ALIGN_FLOOR_MASK(x, (__typeof__(x)) (a) - 1)
#define __BT_ALIGN_FLOOR_MASK(x, mask)	((x) & ~(mask))
#define BT_PTR_ALIGN_FLOOR(p, a) \
			((__typeof__(p)) BT_ALIGN_FLOOR((unsigned long) (p), a))
#define BT_IS_ALIGNED(x, a)	(((x) & ((__typeof__(x)) (a) - 1)) == 0)

#endif /* _BABELTRACE_ALIGN_H */
