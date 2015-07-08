#ifndef BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H
#define BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H

/*
 * BabelTrace - Component Factory Internal
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
#include <babeltrace/plugin/component-internal.h>
#include <babeltrace/plugin/plugin-system.h>

/** Component initialization functions */
/**
 * Allocate a source component.
 *
 * @param name			Component instance name (will be copied)
 * @param private_data		Private component implementation data
 * @param destroy_cb		Component private data clean-up callback
 * @param iterator_create_cb	Iterator creation callback
 * @returns			A source component instance
 */
BT_HIDDEN
extern struct bt_component *bt_component_source_create(const char *name,
		void *private_data, bt_component_destroy_cb destroy_func,
		bt_component_source_iterator_create_cb iterator_create_cb);

/**
 * Allocate a sink component.
 *
 * @param name			Component instance name (will be copied)
 * @param private_data		Private component implementation data
 * @param destroy_cb		Component private data clean-up callback
 * @param notification_cb	Notification handling callback
 * @returns			A sink component instance
 */
BT_HIDDEN
extern struct bt_component *bt_component_sink_create(const char *name,
		void *private_data, bt_component_destroy_cb destroy_func,
		bt_component_sink_handle_notification_cb notification_cb);

#endif /* BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H */
