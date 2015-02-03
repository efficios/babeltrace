#ifndef BABELTRACE_PLUGIN_NOTIFICATION_H
#define BABELTRACE_PLUGIN_NOTIFICATION_H

/*
 * BabelTrace - Plug-in Notification
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;

enum bt_plugin_notification_type {
	BT_PLUGIN_NOTIFICATION_TYPE_UNKNOWN = -1,
	BT_PLUGIN_NOTIFICATION_TYPE_EVENT = 0, /* see notification/event.h */
};

enum bt_plugin_notification_type bt_plugin_notification_get_type(
		struct bt_plugin_notification *notification);

void bt_plugin_notification_get(struct bt_plugin_notification *notification);
void bt_plugin_notification_put(struct bt_plugin_notification *notification);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_NOTIFICATION_H */
