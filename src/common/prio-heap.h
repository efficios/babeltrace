/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Static-sized priority heap containing pointers. Based on CLRS,
 * chapter 6.
 */

#ifndef BABELTRACE_COMMON_PRIO_HEAP_H
#define BABELTRACE_COMMON_PRIO_HEAP_H

#include <unistd.h>
#include "common/macros.h"

struct ptr_heap {
	size_t len, alloc_len;
	void **ptrs;
	int (*gt)(void *a, void *b);
};

#ifdef DEBUG_HEAP
void check_heap(const struct ptr_heap *heap);
#else
static inline
void check_heap(const struct ptr_heap *heap __attribute__((unused)))
{
}
#endif

/**
 * bt_heap_maximum - return the largest element in the heap
 * @heap: the heap to be operated on
 *
 * Returns the largest element in the heap, without performing any modification
 * to the heap structure. Returns NULL if the heap is empty.
 */
static inline void *bt_heap_maximum(const struct ptr_heap *heap)
{
	check_heap(heap);
	return G_LIKELY(heap->len) ? heap->ptrs[0] : NULL;
}

/**
 * bt_heap_init - initialize the heap
 * @heap: the heap to initialize
 * @alloc_len: number of elements initially allocated
 * @gt: function to compare the elements
 *
 * Returns -ENOMEM if out of memory.
 */
extern int bt_heap_init(struct ptr_heap *heap,
		     size_t alloc_len,
		     int gt(void *a, void *b));

/**
 * bt_heap_free - free the heap
 * @heap: the heap to free
 */
extern void bt_heap_free(struct ptr_heap *heap);

/**
 * bt_heap_insert - insert an element into the heap
 * @heap: the heap to be operated on
 * @p: the element to add
 *
 * Insert an element into the heap.
 *
 * Returns -ENOMEM if out of memory.
 */
extern int bt_heap_insert(struct ptr_heap *heap, void *p);

/**
 * bt_heap_remove - remove the largest element from the heap
 * @heap: the heap to be operated on
 *
 * Returns the largest element in the heap. It removes this element from the
 * heap. Returns NULL if the heap is empty.
 */
extern void *bt_heap_remove(struct ptr_heap *heap);

/**
 * bt_heap_cherrypick - remove a given element from the heap
 * @heap: the heap to be operated on
 * @p: the element
 *
 * Remove the given element from the heap. Return the element if present, else
 * return NULL. This algorithm has a complexity of O(n), which is higher than
 * O(log(n)) provided by the rest of this API.
 */
extern void *bt_heap_cherrypick(struct ptr_heap *heap, void *p);

/**
 * bt_heap_replace_max - replace the the largest element from the heap
 * @heap: the heap to be operated on
 * @p: the pointer to be inserted as topmost element replacement
 *
 * Returns the largest element in the heap. It removes this element from the
 * heap. The heap is rebalanced only once after the insertion. Returns NULL if
 * the heap is empty.
 *
 * This is the equivalent of calling bt_heap_remove() and then bt_heap_insert(), but
 * it only rebalances the heap once. It never allocates memory.
 */
extern void *bt_heap_replace_max(struct ptr_heap *heap, void *p);

/**
 * bt_heap_copy - copy a heap
 * @dst: the destination heap (must be allocated)
 * @src: the source heap
 *
 * Returns -ENOMEM if out of memory.
 */
extern int bt_heap_copy(struct ptr_heap *dst, struct ptr_heap *src);

#endif /* BABELTRACE_COMMON_PRIO_HEAP_H */
