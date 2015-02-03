#ifndef BABELTRACE_PLUGIN_H
#define BABELTRACE_PLUGIN_H

/*
 * BabelTrace - Plug-in
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

struct bt_plugin;
struct bt_notification;

enum bt_plugin_type {
	BT_PLUGIN_TYPE_UNKNOWN = -1,
	/* A source plug-in is a notification generator. */
	BT_PLUGIN_TYPE_SOURCE = 0,
	/* A sink plug-in handles incoming notifications. */
	BT_PLUGIN_TYPE_SINK = 1,
	/* A filter plug-in implements both SOURCE and SINK interfaces. */
	BT_PLUGIN_TYPE_FILTER = 2,
};

/**
 * Plug-in discovery functions.
 *
 * The Babeltrace plug-in architecture mandates that a given plug-in shared
 * object only define one plug-in. These functions are used to query a about
 * shared object about its attributes.
 *
 * The functions marked as mandatory MUST be exported by the shared object
 * to be considered a valid plug-in.
 */
enum bt_plugin_type bt_plugin_get_type(void);
const char *bt_plugin_get_name(void);

/* TODO: document mandatory fields and their expected types */
int bt_plugin_set_parameters(struct bt_plugin *plugin,
		struct bt_ctf_field *field);

void bt_plugin_get(struct bt_plugin *plugin);
void bt_plugin_put(struct bt_plugin *plugin);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_H */
