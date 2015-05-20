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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plug-in private data deallocation function type.
 *
 * @param plugin	Plug-in instance
 */
typedef void (*bt_plugin_destroy_cb)(struct bt_plugin *plugin);

/**
 * Get a plug-in's private (implementation) data.
 *
 * @param plugin	Plug-in of which to get the private data
 * @returns		Plug-in private data
 */
extern void *bt_plugin_get_private_data(struct bt_plugin *plugin);

	
/** Plug-in initialization functions */
/**
 * Allocate a source plug-in.
 *
 * @param name			Plug-in instance name (will be copied)
 * @param private_data		Private plug-in implementation data
 * @param destroy_cb		Plug-in private data clean-up callback
 * @param iterator_create_cb	Iterator creation callback
 * @returns			A source plug-in instance
 */
extern struct bt_plugin *bt_plugin_source_create(const char *name,
		void *private_data, bt_plugin_destroy_func destroy_func,
		bt_plugin_source_iterator_create_cb iterator_create_cb);

/**
 * Allocate a sink plug-in.
 *
 * @param name			Plug-in instance name (will be copied)
 * @param private_data		Private plug-in implementation data
 * @param destroy_cb		Plug-in private data clean-up callback
 * @param notification_cb	Notification handling callback
 * @returns			A sink plug-in instance
 */
extern struct bt_plugin *bt_plugin_sink_create(const char *name,
		void *private_data, bt_plugin_destroy_func destroy_func,
		bt_plugin_sink_handle_notification_cb notification_cb);


/** Notification iterator functions */
/**
 * Allocate a notification iterator.
 *
 * @param plugin		Plug-in instance
 * @param next_cb		Callback advancing to the next notification
 * @param notification_cb	Callback providing the current notification
 * @returns			A notification iterator instance
 */
extern struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_plugin *plugin,
		bt_notification_iterator_next_cb next_cb,
		bt_notification_iterator_get_notification_cb notification_cb);

/**
 * Set an iterator's private data.
 *
 * @param plugin		Plug-in instance on which to iterate
 * @param data			Iterator private data
 * @returns			One of #bt_notification_iterator_status values
 */
extern enum bt_notification_iterator_status *bt_notification_iterator_create(
		struct bt_plugin *plugin, void *data);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_SYSTEM_H */
