#ifndef BABELTRACE_GRAPH_NOTIFICATION_HEAP_H
#define BABELTRACE_GRAPH_NOTIFICATION_HEAP_H

/*
 * Babeltrace - Notification Heap
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
#include <stdbool.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/babeltrace-internal.h>

/**
 * bt_notification_time_compare - Compare two notifications' timestamps
 *
 * Compare two notifications in the time domain. Return true if 'a' happened
 * prior to 'b'. In the case where both notifications are deemed to have
 * happened at the same time, an implementation-defined critarion shall be
 * used to order the notifications. This criterion shall ensure a consistent
 * ordering over multiple runs.
 */
typedef bool (*bt_notification_time_compare_func)(
		struct bt_notification *a, struct bt_notification *b,
		void *user_data);

/**
 * bt_notification_heap_create - create a new bt_notification heap.
 *
 * @comparator: Function to use for notification comparisons.
 *
 * Returns a new notification heap, NULL on error.
 */
extern struct bt_notification_heap *bt_notification_heap_create(
		bt_notification_time_compare_func comparator, void *user_data);

/**
 * bt_notification_heap_insert - insert an element into the heap
 *
 * @heap: the heap to be operated on
 * @notification: the notification to add
 *
 * Insert a notification into the heap.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_notification_heap_insert(struct bt_notification_heap *heap,
		struct bt_notification *notification);

/**
 * bt_notification_heap_peek - return the element on top of the heap.
 *
 * @heap: the heap to be operated on
 *
 * Returns the top element of the heap, without performing any modification
 * to the heap structure. Returns NULL if the heap is empty. The returned
 * notification must be bt_put() by the caller.
 */
extern struct bt_notification *bt_notification_heap_peek(
		struct bt_notification_heap *heap);

/**
 * bt_notification_heap_pop - remove the element sitting on top of the heap.
 * @heap: the heap to be operated on
 *
 * Returns the top element of the heap. The element is removed from the
 * heap. Returns NULL if the heap is empty. The returned notification must be
 * bt_put() by the caller.
 */
extern struct bt_notification *bt_notification_heap_pop(
		struct bt_notification_heap *heap);

#endif /* BABELTRACE_GRAPH_NOTIFICATION_HEAP_H */
