/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Common Trace Format Object Stack.
 */

#ifndef _OBJSTACK_H
#define _OBJSTACK_H

#include "common/macros.h"

struct objstack;

BT_HIDDEN
struct objstack *objstack_create(void);
BT_HIDDEN
void objstack_destroy(struct objstack *objstack);

/*
 * Allocate len bytes of zeroed memory.
 * Return NULL on error.
 */
BT_HIDDEN
void *objstack_alloc(struct objstack *objstack, size_t len);

#endif /* _OBJSTACK_H */
