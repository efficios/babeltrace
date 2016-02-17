#ifndef BABELTRACE_PLUGIN_INTERNAL_H
#define BABELTRACE_PLUGIN_INTERNAL_H

/*
 * BabelTrace - Plug-in Internal
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
#include <babeltrace/ref-internal.h>
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/plugin-system.h>
#include <babeltrace/object-internal.h>
#include <gmodule.h>

/**
 * Plug-ins are owned by bt_component_factory and the bt_component_class-es
 * it provides. This means that its lifetime bound by either the component
 * factory's, or the concrete components' lifetime which may be in use and which
 * have hold a reference to their bt_component_class which, in turn, have a
 * reference to their plugin.
 *
 * This ensures that a plugin's library is not closed while it is being used
 * even if the bt_component_factory, which created its components, is destroyed.
 */
struct bt_plugin {
	struct bt_object base;
	const char *name;
	const char *author;
	const char *license;
	const char *description;
	GString *path;
        bt_plugin_init_func init;
	bt_plugin_exit_func exit;
	GModule *module;
};

BT_HIDDEN
struct bt_plugin *bt_plugin_create(GModule *module, const char *path);

BT_HIDDEN
enum bt_component_status bt_plugin_register_component_classes(
		struct bt_plugin *plugin, struct bt_component_factory *factory);

#endif /* BABELTRACE_PLUGIN_INTERNAL_H */
