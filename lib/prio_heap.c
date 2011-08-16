/*
 * prio_heap.c
 *
 * Static-sized priority heap containing pointers. Based on CLRS,
 * chapter 6.
 *
 * Copyright 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/prio_heap.h>
#include <babeltrace/babeltrace-internal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef max_t
#define max_t(type, a, b)	\
	((type) (a) > (type) (b) ? (type) (a) : (type) (b))
#endif

#ifdef DEBUG_HEAP
void check_heap(const struct ptr_heap *heap)
{
	size_t i;

	if (!heap->len)
		return;

	for (i = 1; i < heap->len; i++)
		assert(!heap->gt(heap->ptrs[i], heap->ptrs[0]));
}
#endif

static
size_t parent(size_t i)
{
	return (i - 1) >> 1;
}

static
size_t left(size_t i)
{
	return (i << 1) + 1;
}

static
size_t right(size_t i)
{
	return (i << 1) + 2;
}

/*
 * Copy of heap->ptrs pointer is invalid after heap_grow.
 */
static
int heap_grow(struct ptr_heap *heap, size_t new_len)
{
	void **new_ptrs;

	if (likely(heap->alloc_len >= new_len))
		return 0;

	heap->alloc_len = max_t(size_t, new_len, heap->alloc_len << 1);
	new_ptrs = calloc(heap->alloc_len, sizeof(void *));
	if (unlikely(!new_ptrs))
		return -ENOMEM;
	if (likely(heap->ptrs))
		memcpy(new_ptrs, heap->ptrs, heap->len * sizeof(void *));
	free(heap->ptrs);
	heap->ptrs = new_ptrs;
	return 0;
}

static
int heap_set_len(struct ptr_heap *heap, size_t new_len)
{
	int ret;

	ret = heap_grow(heap, new_len);
	if (unlikely(ret))
		return ret;
	heap->len = new_len;
	return 0;
}

int heap_init(struct ptr_heap *heap, size_t alloc_len,
	      int gt(void *a, void *b))
{
	heap->ptrs = NULL;
	heap->len = 0;
	heap->alloc_len = 0;
	heap->gt = gt;
	/*
	 * Minimum size allocated is 1 entry to ensure memory allocation
	 * never fails within heap_replace_max.
	 */
	return heap_grow(heap, max_t(size_t, 1, alloc_len));
}

void heap_free(struct ptr_heap *heap)
{
	free(heap->ptrs);
}

static void heapify(struct ptr_heap *heap, size_t i)
{
	void **ptrs = heap->ptrs;
	size_t l, r, largest;

	for (;;) {
		void *tmp;

		l = left(i);
		r = right(i);
		if (l < heap->len && heap->gt(ptrs[l], ptrs[i]))
			largest = l;
		else
			largest = i;
		if (r < heap->len && heap->gt(ptrs[r], ptrs[largest]))
			largest = r;
		if (unlikely(largest == i))
			break;
		tmp = ptrs[i];
		ptrs[i] = ptrs[largest];
		ptrs[largest] = tmp;
		i = largest;
	}
	check_heap(heap);
}

void *heap_replace_max(struct ptr_heap *heap, void *p)
{
	void *res;

	if (unlikely(!heap->len)) {
		(void) heap_set_len(heap, 1);
		heap->ptrs[0] = p;
		check_heap(heap);
		return NULL;
	}

	/* Replace the current max and heapify */
	res = heap->ptrs[0];
	heap->ptrs[0] = p;
	heapify(heap, 0);
	return res;
}

int heap_insert(struct ptr_heap *heap, void *p)
{
	void **ptrs;
	size_t pos;
	int ret;

	ret = heap_set_len(heap, heap->len + 1);
	if (unlikely(ret))
		return ret;
	ptrs = heap->ptrs;
	pos = heap->len - 1;
	while (pos > 0 && heap->gt(p, ptrs[parent(pos)])) {
		/* Move parent down until we find the right spot */
		ptrs[pos] = ptrs[parent(pos)];
		pos = parent(pos);
	}
	ptrs[pos] = p;
	check_heap(heap);
	return 0;
}

void *heap_remove(struct ptr_heap *heap)
{
	switch (heap->len) {
	case 0:
		return NULL;
	case 1:
		(void) heap_set_len(heap, 0);
		return heap->ptrs[0];
	}
	/* Shrink, replace the current max by previous last entry and heapify */
	heap_set_len(heap, heap->len - 1);
	/* len changed. previous last entry is at heap->len */
	return heap_replace_max(heap, heap->ptrs[heap->len]);
}

void *heap_cherrypick(struct ptr_heap *heap, void *p)
{
	size_t pos, len = heap->len;

	for (pos = 0; pos < len; pos++)
		if (unlikely(heap->ptrs[pos] == p))
			goto found;
	return NULL;
found:
	if (unlikely(heap->len == 1)) {
		(void) heap_set_len(heap, 0);
		check_heap(heap);
		return heap->ptrs[0];
	}
	/* Replace p with previous last entry and heapify. */
	heap_set_len(heap, heap->len - 1);
	/* len changed. previous last entry is at heap->len */
	heap->ptrs[pos] = heap->ptrs[heap->len];
	heapify(heap, pos);
	return p;
}
