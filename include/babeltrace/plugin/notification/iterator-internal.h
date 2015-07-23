#ifndef BABELTRACE_PLUGIN_ITERATOR_INTERNAL_H
#define BABELTRACE_PLUGIN_ITERATOR_INTERNAL_H

/*
 * BabelTrace - Notification Iterator Internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/plugin/plugin-system.h>
#include <babeltrace/ref-internal.h>

struct bt_notification_iterator {
	struct bt_ref ref;
	bt_notification_iterator_get_cb get;
	bt_notification_iterator_next_cb next;
	void *user_data;
	bt_notification_iterator_destroy_cb user_destroy;
};

/**
 * Allocate a notification iterator.
 *
 * @param component		Component instance
 * @returns			A notification iterator instance
 */
BT_HIDDEN
struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_component *component);

/**
 * Validate a notification iterator.
 *
 * @param iterator		Notification iterator instance
 * @returns			One of #bt_component_status values
 */
BT_HIDDEN
enum bt_notification_iterator_status bt_notification_iterator_validate(
		struct bt_notification_iterator *iterator);

#endif /* BABELTRACE_PLUGIN_ITERATOR_INTERNAL_H */
