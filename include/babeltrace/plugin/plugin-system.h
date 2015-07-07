#ifndef BABELTRACE_PLUGIN_SYSTEM_H
#define BABELTRACE_PLUGIN_SYSTEM_H

/*
 * BabelTrace - Babeltrace Plug-in System Interface
 *
 * This interface is provided for plug-ins to use the Babeltrace
 * plug-in system facilities.
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

#include <babeltrace/objects.h>
#include <babeltrace/plugin/notification/iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;

/**
 * Component private data deallocation function type.
 *
 * @param component	Component instance
 */
typedef void (*bt_component_destroy_cb)(struct bt_component *component);

/**
 * Iterator creation function type.
 *
 * @param component	Component instance
 */
typedef struct bt_notification_iterator *(
		*bt_component_source_iterator_create_cb)(
		struct bt_component *component);

/**
 * Notification handling function type.
 *
 * @param component	Component instance
 * @param notificattion	Notification to handle
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_sink_handle_notification_cb)(
		struct bt_component *, struct bt_notification *);

typedef struct bt_notification *(bt_notification_iterator_get_notification_cb)(
		struct bt_notification_iterator *);

typedef enum bt_notification_iterator_status (bt_notification_iterator_next_cb)(
		struct bt_notification_iterator *);

/**
 * Get a component's private (implementation) data.
 *
 * @param component	Component of which to get the private data
 */
extern void *bt_component_get_private_data(struct bt_component *component);

/**
 * Set a component's private (implementation) data.
 *
 * @param component	Component of which to set the private data
 * @param data	Component private data
 */
extern enum bt_component_status bt_component_set_private_data(struct bt_component *component,
		void *data);


/** Notification iterator functions */
/**
 * Allocate a notification iterator.
 *
 * @param component		Component instance
 * @param next_cb		Callback advancing to the next notification
 * @param notification_cb	Callback providing the current notification
 * @returns			A notification iterator instance
 */
extern struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_component *component,
		bt_notification_iterator_next_cb next_cb,
		bt_notification_iterator_get_notification_cb notification_cb);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_SYSTEM_H */
