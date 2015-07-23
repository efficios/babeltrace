/*
 * plugin.c
 *
 * Babeltrace Plugin
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

#include <babeltrace/compiler.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <glib.h>

static
void bt_plugin_destroy(struct bt_ref *ref)
{
	struct bt_plugin *plugin;

	assert(ref);
	plugin = container_of(ref, struct bt_plugin, ref);
	if (plugin->module) {
		if (!g_module_close(plugin->module)) {
			printf_error("Module close error: %s",
				g_module_error());

		}
	}
	g_free(plugin);
}

BT_HIDDEN
struct bt_plugin *bt_plugin_create(GModule *module)
{
	struct bt_plugin *plugin;

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto end;
	}

	bt_ref_init(&plugin->ref, bt_plugin_destroy);
end:
	return plugin;
}

BT_HIDDEN
enum bt_component_status bt_plugin_register_component_classes(
		struct bt_plugin *plugin, struct bt_component_factory *factory)
{
	assert(plugin && factory);
	return plugin->init(factory);
}

BT_HIDDEN
void bt_plugin_get(struct bt_plugin *plugin)
{
	if (!plugin) {
		return;
	}

	bt_ref_get(&plugin->ref);
}

BT_HIDDEN
void bt_plugin_put(struct bt_plugin *plugin)
{
	if (!plugin) {
		return;
	}

	bt_ref_put(&plugin->ref);
}
