#ifndef _BABELTRACE_ALIGN_H
#define _BABELTRACE_ALIGN_H

/*
 * BabelTrace align.h - alignment header
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#include <babeltrace/compiler.h>
#include <unistd.h>
#include <limits.h>

#ifndef PAGE_SIZE		/* Cygwin limits.h defines its own PAGE_SIZE */
#define PAGE_SIZE		sysconf(_SC_PAGE_SIZE)
#endif

#define ALIGN(x, a)		__ALIGN_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define PTR_ALIGN(p, a)		((typeof(p)) ALIGN((unsigned long) (p), a))
#define ALIGN_FLOOR(x, a)	__ALIGN_FLOOR_MASK(x, (typeof(x)) (a) - 1)
#define __ALIGN_FLOOR_MASK(x, mask)	((x) & ~(mask))
#define PTR_ALIGN_FLOOR(p, a) \
			((typeof(p)) ALIGN_FLOOR((unsigned long) (p), a))
#define IS_ALIGNED(x, a)	(((x) & ((typeof(x)) (a) - 1)) == 0)

/*
 * Align pointer on natural object alignment.
 */
#define object_align(obj)	PTR_ALIGN(obj, __alignof__(*(obj)))
#define object_align_floor(obj)	PTR_ALIGN_FLOOR(obj, __alignof__(*(obj)))

/**
 * offset_align - Calculate the offset needed to align an object on its natural
 *                alignment towards higher addresses.
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
 * offset_align_floor - Calculate the offset needed to align an object
 *                      on its natural alignment towards lower addresses.
 * @align_drift:  object offset from an "alignment"-aligned address.
 * @alignment:    natural object alignment. Must be non-zero, power of 2.
 *
 * Returns the offset that must be substracted to align towards lower addresses.
 */
#define offset_align_floor(align_drift, alignment)			       \
	({								       \
		MAYBE_BUILD_BUG_ON((alignment) == 0			       \
				   || ((alignment) & ((alignment) - 1)));      \
		(((align_drift) - (alignment)) & ((alignment) - 1);	       \
	})

#endif /* _BABELTRACE_ALIGN_H */
