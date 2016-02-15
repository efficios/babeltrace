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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;
struct bt_notification_iterator;
struct bt_component;

/**
 * Component private data deallocation function type.
 *
 * @param component	Component instance
 */
typedef void (*bt_component_destroy_cb)(struct bt_component *component);

/**
 * Source component initialization function type.
 *
 * A source component's iterator initialization callback, private data and
 * deinitialization callback must be set by this function.
 *
 * @param component	Component instance
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_source_init_cb)(
		struct bt_component *component);

/**
 * Sink component initialization function type.
 *
 * A sink component's notification handling callback, private data and
 * deinitialization callback must be set by this function.
 *
 * @param component	Component instance
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_sink_init_cb)(
		struct bt_component *component);

/**
 * Iterator initialization function type.
 *
 * @param component	Component instance
 * @param iterator	Iterator instance
 */
typedef enum bt_component_status (*bt_component_source_iterator_init_cb)(
		struct bt_component *component,
		struct bt_notification_iterator *iterator);

/**
 * Get a component's private data.
 *
 * @param component	Component of which to get the private data
 * @returns		Component's private data
 */
extern void *bt_component_get_private_data(struct bt_component *component);

/**
 * Set a component's private data.
 *
 * @param component	Component of which to set the private data
 * @param data		Component private data
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_private_data(
		struct bt_component *component, void *data);

/**
 * Set a component's private data cleanup function.
 *
 * @param component	Component of which to set the private data destruction
 *			function
 * @param data		Component private data clean-up function
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_destroy_cb(
		struct bt_component *component,
		bt_component_destroy_cb destroy);

/** bt_component_souce */
/**
 * Iterator initialization function type.
 *
 * A notification iterator's private data, deinitialization, next, and get
 * callbacks must be set by this function.
 *
 * @param component	Component instance
 * @param iterator	Notification iterator instance
 */
typedef enum bt_component_status (*bt_component_source_init_iterator_cb)(
		struct bt_component *component,
		struct bt_notification_iterator *iterator);

/**
 * Set a source component's iterator initialization function.
 *
 * @param component	Component instance
 * @param init_iterator	Notification iterator initialization callback
 */
extern enum bt_component_status
bt_component_source_set_iterator_init_cb(struct bt_component *component,
		bt_component_source_init_iterator_cb init_iterator);

/** bt_component_sink */
/**
 * Notification handling function type.
 *
 * @param component	Component instance
 * @param notificattion	Notification to handle
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_sink_handle_notification_cb)(
		struct bt_component *, struct bt_notification *);

/**
 * Set a sink component's notification handling callback.
 *
 * @param component		Component instance
 * @param handle_notification	Notification handling callback
 * @returns			One of #bt_component_status values
 */
extern enum bt_component_status
bt_component_sink_set_handle_notification_cb(struct bt_component *component,
		bt_component_sink_handle_notification_cb handle_notification);

/** bt_component_notification_iterator */
/**
 * Function returning an iterator's current notification.
 *
 * @param iterator	Notification iterator instance
 * @returns 		A notification instance
 */
typedef struct bt_notification *(*bt_notification_iterator_get_cb)(
		struct bt_notification_iterator *iterator);

/**
 * Function advancing an iterator's position of one element.
 *
 * @param iterator	Notification iterator instance
 * @returns 		One of #bt_notification_iterator_status values
 */
typedef enum bt_notification_iterator_status (*bt_notification_iterator_next_cb)(
		struct bt_notification_iterator *iterator);

/**
 * Function cleaning-up an iterator's private data on destruction.
 *
 * @param iterator	Notification iterator instance
 */
typedef void (*bt_notification_iterator_destroy_cb)(
		struct bt_notification_iterator *iterator);

/**
 * Set an iterator's "get" callback which return the current notification.
 *
 * @param iterator	Notification iterator instance
 * @param get		Notification return callback
 * @returns		One of #bt_notification_iterator_status values
 */
extern enum bt_notification_iterator_status
bt_notification_iterator_set_get_cb(struct bt_notification_iterator *iterator,
		bt_notification_iterator_get_cb get);

/**
 * Set an iterator's "next" callback which advances the iterator's position.
 *
 * @param iterator	Notification iterator instance
 * @param next		Iterator "next" callback
 * @returns		One of #bt_notification_iterator_status values
 */
extern enum bt_notification_iterator_status
bt_notification_iterator_set_next_cb(struct bt_notification_iterator *iterator,
		bt_notification_iterator_next_cb next);

/**
 * Set an iterator's "destroy" callback.
 *
 * @param iterator	Notification iterator instance
 * @param next		Iterator destruction callback
 * @returns		One of #bt_notification_iterator_status values
 */
extern enum bt_notification_iterator_status
bt_notification_iterator_set_destroy_cb(
		struct bt_notification_iterator *iterator,
		bt_notification_iterator_destroy_cb destroy);

/**
 * Set an iterator's private data.
 *
 * @param iterator	Notification iterator instance
 * @param data		Iterator private data
 * @returns		One of #bt_notification_iterator_status values
 */
extern enum bt_notification_iterator_status
bt_notification_iterator_set_private_data(
		struct bt_notification_iterator *iterator, void *data);

/**
 * Get an iterator's private data.
 *
 * @param iterator	Notification iterator instance
 * @returns		Iterator instance private data
 */
extern void *bt_notification_iterator_get_private_data(
		struct bt_notification_iterator *iterator);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_SYSTEM_H */
