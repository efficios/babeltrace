/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Common Trace Format Object Stack.
 */

#ifndef _OBJSTACK_H
#define _OBJSTACK_H

struct objstack *objstack_create(void);
void objstack_destroy(struct objstack *objstack);

/*
 * Allocate len bytes of zeroed memory.
 * Return NULL on error.
 */
void *objstack_alloc(struct objstack *objstack, size_t len);

#endif /* _OBJSTACK_H */
