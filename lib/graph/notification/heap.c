/*
 * Babeltrace - CTF notification priority heap
 *
 * Static-sized priority heap containing pointers. Based on CLRS,
 * chapter 6.
 *
 * Copyright (c) 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright (c) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stddef.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/notification-heap-internal.h>
#include <babeltrace/assert-internal.h>

#ifdef DEBUG_HEAP
static
void check_heap(struct bt_notification_heap *heap)
{
	size_t i;

	if (!heap->count) {
		return;
	}

	for (i = 1; i < heap->count; i++) {
		BT_ASSERT(!heap->compare(g_ptr_array_index(heap->ptrs, i),
				g_ptr_array_index(heap->ptrs, 0),
				heap->compare_data));
	}
}
#else
void check_heap(struct bt_notification_heap *heap)
{
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
int heap_grow(struct bt_notification_heap *heap, size_t new_len)
{
	size_t alloc_len;

	if (likely(heap->ptrs->len >= new_len)) {
		goto end;
	}

        alloc_len = max_t(size_t, new_len, heap->ptrs->len << 1);
	g_ptr_array_set_size(heap->ptrs, alloc_len);
end:
	return 0;
}

static
int heap_set_count(struct bt_notification_heap *heap, size_t new_count)
{
	int ret = 0;

	ret = heap_grow(heap, new_count);
	if (unlikely(ret)) {
		goto end;
	}
	heap->count = new_count;
end:
	return ret;
}

static
void heapify(struct bt_notification_heap *heap, size_t i)
{
	struct bt_notification **ptrs =
			(struct bt_notification **) heap->ptrs->pdata;

	for (;;) {
		void *tmp;
		size_t l, r, largest;

		l = left(i);
		r = right(i);
		if (l < heap->count && heap->compare(ptrs[l], ptrs[i],
				heap->compare_data)) {
			largest = l;
		} else {
			largest = i;
		}
		if (r < heap->count && heap->compare(ptrs[r], ptrs[largest],
				heap->compare_data)) {
			largest = r;
		}
		if (unlikely(largest == i)) {
			break;
		}
		tmp = ptrs[i];
		ptrs[i] = ptrs[largest];
		ptrs[largest] = tmp;
		i = largest;
	}
	check_heap(heap);
}

static
struct bt_notification *heap_replace_max(struct bt_notification_heap *heap,
	        struct bt_notification *notification)
{
	struct bt_notification *res = NULL;

	if (unlikely(!heap->count)) {
		(void) heap_set_count(heap, 1);
		g_ptr_array_index(heap->ptrs, 0) = notification;
		check_heap(heap);
		goto end;
	}

	/* Replace the current max and heapify. */
	res = g_ptr_array_index(heap->ptrs, 0);
	g_ptr_array_index(heap->ptrs, 0) = notification;
	heapify(heap, 0);
end:
	return res;
}

static
void bt_notification_heap_destroy(struct bt_object *obj)
{
	struct bt_notification_heap *heap = container_of(obj,
		        struct bt_notification_heap, base);

	if (heap->ptrs) {
		size_t i;

		for (i = 0; i < heap->count; i++) {
			bt_put(g_ptr_array_index(heap->ptrs, i));
		}
		g_ptr_array_free(heap->ptrs, TRUE);
	}
	g_free(heap);
}

struct bt_notification_heap *bt_notification_heap_create(
		bt_notification_time_compare_func comparator, void *user_data)
{
	struct bt_notification_heap *heap = NULL;

	if (!comparator) {
		goto end;
	}

	heap = g_new0(struct bt_notification_heap, 1);
	if (!heap) {
		goto end;
	}

	bt_object_init(&heap->base, bt_notification_heap_destroy);
	heap->ptrs = g_ptr_array_new();
	if (!heap->ptrs) {
		BT_PUT(heap);
		goto end;
	}

	heap->compare = comparator;
	heap->compare_data = user_data;
end:
	return heap;
}

struct bt_notification *bt_notification_heap_peek(
		struct bt_notification_heap *heap)
{
	check_heap(heap);
	return bt_get(likely(heap->count) ?
			g_ptr_array_index(heap->ptrs, 0) : NULL);
}

int bt_notification_heap_insert(struct bt_notification_heap *heap,
	        struct bt_notification *notification)
{
	int ret;
	size_t pos;
        struct bt_notification **ptrs;

	ret = heap_set_count(heap, heap->count + 1);
	if (unlikely(ret)) {
		goto end;
	}

	ptrs = (struct bt_notification **) heap->ptrs->pdata;
	pos = heap->count - 1;
	while (pos > 0 && heap->compare(notification, ptrs[parent(pos)],
		        heap->compare_data)) {
		/* Move parent down until we find the right spot. */
		ptrs[pos] = ptrs[parent(pos)];
		pos = parent(pos);
	}
	ptrs[pos] = bt_get(notification);
	check_heap(heap);
end:
	return ret;
}

struct bt_notification *bt_notification_heap_pop(
		struct bt_notification_heap *heap)
{
        struct bt_notification *ret = NULL;

	switch (heap->count) {
	case 0:
		goto end;
	case 1:
		(void) heap_set_count(heap, 0);
		ret = g_ptr_array_index(heap->ptrs, 0);
		goto end;
	}
	/*
	 * Shrink, replace the current max by previous last entry and heapify.
	 */
	heap_set_count(heap, heap->count - 1);
	/* count changed. previous last entry is at heap->count. */
	ret = heap_replace_max(heap, g_ptr_array_index(heap->ptrs,
			heap->count));
end:
	/*
	 * Not taking a supplementary reference since we are relinquishing our
	 * own to the caller.
	 */
	return ret;
}
