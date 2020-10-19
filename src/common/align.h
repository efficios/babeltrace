/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _BABELTRACE_ALIGN_H
#define _BABELTRACE_ALIGN_H

#include "compat/compiler.h"
#include "compat/limits.h"

#define BT_ALIGN(x, a)		__BT_ALIGN_MASK(x, (typeof(x))(a) - 1)
#define __BT_ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define BT_PTR_ALIGN(p, a)		((typeof(p)) BT_ALIGN((unsigned long) (p), a))
#define BT_ALIGN_FLOOR(x, a)	__BT_ALIGN_FLOOR_MASK(x, (typeof(x)) (a) - 1)
#define __BT_ALIGN_FLOOR_MASK(x, mask)	((x) & ~(mask))
#define BT_PTR_ALIGN_FLOOR(p, a) \
			((typeof(p)) BT_ALIGN_FLOOR((unsigned long) (p), a))
#define BT_IS_ALIGNED(x, a)	(((x) & ((typeof(x)) (a) - 1)) == 0)

/*
 * Align pointer on natural object alignment.
 */
#define bt_object_align(obj)		BT_PTR_ALIGN(obj, __alignof__(*(obj)))
#define bt_object_align_floor(obj)	BT_PTR_ALIGN_FLOOR(obj, __alignof__(*(obj)))

/**
 * bt_offset_align - Calculate the offset needed to align an object on its
 *                   natural alignment towards higher addresses.
 * @align_drift:  object offset from an "alignment"-aligned address.
 * @alignment:    natural object alignment. Must be non-zero, power of 2.
 *
 * Returns the offset that must be added to align towards higher
 * addresses.
 */
#define offset_align(align_drift, alignment)				       \
	({								       \
		MAYBE_BUILD_BUG_ON((alignment) == 0			       \
				   || ((alignment) & ((alignment) - 1)));      \
		(((alignment) - (align_drift)) & ((alignment) - 1));	       \
	})

/**
 * bt_offset_align_floor - Calculate the offset needed to align an object
 *                         on its natural alignment towards lower addresses.
 * @align_drift:  object offset from an "alignment"-aligned address.
 * @alignment:    natural object alignment. Must be non-zero, power of 2.
 *
 * Returns the offset that must be substracted to align towards lower addresses.
 */
#define bt_offset_align_floor(align_drift, alignment)			       \
	({								       \
		MAYBE_BUILD_BUG_ON((alignment) == 0			       \
				   || ((alignment) & ((alignment) - 1)));      \
		(((align_drift) - (alignment)) & ((alignment) - 1));	       \
	})

#endif /* _BABELTRACE_ALIGN_H */
