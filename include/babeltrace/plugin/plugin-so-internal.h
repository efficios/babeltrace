#ifndef BABELTRACE_PLUGIN_PLUGIN_SO_INTERNAL_H
#define BABELTRACE_PLUGIN_PLUGIN_SO_INTERNAL_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <glib.h>
#include <gmodule.h>
#include <babeltrace/types.h>

struct bt_plugin;
struct bt_component_class;

struct bt_plugin_so_shared_lib_handle {
	struct bt_object base;
	GString *path;
	GModule *module;

	/* True if initialization function was called */
	bt_bool init_called;
	bt_plugin_exit_func exit;
};

struct bt_plugin_so_spec_data {
	/* Shared lib. handle: owned by this */
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle;

	/* Pointers to plugin's memory: do NOT free */
	const struct __bt_plugin_descriptor *descriptor;
	bt_plugin_init_func init;
	const struct __bt_plugin_descriptor_version *version;
};

BT_HIDDEN
struct bt_plugin_set *bt_plugin_so_create_all_from_file(const char *path);

BT_HIDDEN
struct bt_plugin_set *bt_plugin_so_create_all_from_static(void);

BT_HIDDEN
void bt_plugin_so_on_add_component_class(struct bt_plugin *plugin,
		struct bt_component_class *comp_class);

#endif /* BABELTRACE_PLUGIN_PLUGIN_SO_INTERNAL_H */
