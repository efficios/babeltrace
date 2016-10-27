/*
 * plugin.c
 *
 * Babeltrace Plugin
 * 
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/ref.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/component-factory-internal.h>
#include <glib.h>

#define PLUGIN_SYMBOL_NAME		"__bt_plugin_name"
#define PLUGIN_SYMBOL_AUTHOR		"__bt_plugin_author"
#define PLUGIN_SYMBOL_LICENSE		"__bt_plugin_license"
#define PLUGIN_SYMBOL_REGISTER		"__bt_plugin_register"
#define PLUGIN_SYMBOL_DESCRIPTION	"__bt_plugin_description"

DECLARE_PLUG_IN_SECTIONS;

static
void bt_plugin_destroy(struct bt_object *obj)
{
	struct bt_plugin *plugin;

	assert(obj);
	plugin = container_of(obj, struct bt_plugin, base);

	if (plugin->module) {
		if (!g_module_close(plugin->module)) {
			printf_error("Module close error: %s\n",
					g_module_error());
		}
	}

	if (plugin->path) {
		g_string_free(plugin->path, TRUE);
	}
	g_free(plugin);
}

BT_HIDDEN
struct bt_plugin *bt_plugin_create_from_module(GModule *module,
		const char *path)
{
	struct bt_plugin *plugin = NULL;
	gpointer symbol = NULL;

	if (!module || !path) {
		goto error;
	}

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto error;
	}

	bt_object_init(plugin, bt_plugin_destroy);
	plugin->path = g_string_new(path);
	if (!plugin->path) {
		goto error;
	}

	if (!g_module_symbol(module, PLUGIN_SYMBOL_NAME,
			(gpointer *) &plugin->name)) {
		printf_error("Unable to resolve plugin symbol %s from %s",
				PLUGIN_SYMBOL_NAME, g_module_name(module));
		goto error;
	}

	if (!g_module_symbol(module, PLUGIN_SYMBOL_LICENSE,
			(gpointer *) &plugin->license)) {
		printf_error("Unable to resolve plugin symbol %s from %s",
				PLUGIN_SYMBOL_LICENSE, g_module_name(module));
		goto error;
	}
	if (!g_module_symbol(module, PLUGIN_SYMBOL_AUTHOR,
			(gpointer *) &plugin->author)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
				PLUGIN_SYMBOL_AUTHOR, g_module_name(module));
		goto error;
	}
	if (!g_module_symbol(module, PLUGIN_SYMBOL_DESCRIPTION,
			(gpointer *) &plugin->description)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
				PLUGIN_SYMBOL_DESCRIPTION,
				g_module_name(module));
		goto error;
	}
	if (!g_module_symbol(module, PLUGIN_SYMBOL_REGISTER, &symbol)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
				PLUGIN_SYMBOL_REGISTER, g_module_name(module));
		goto error;
	} else {
		plugin->_register = *((bt_plugin_register_func *) symbol);
		if (!plugin->_register) {
			printf_verbose("NULL %s symbol target\n",
					PLUGIN_SYMBOL_REGISTER);
			goto error;
		}
	}

	return plugin;
error:
	BT_PUT(plugin);
	return plugin;
}

BT_HIDDEN
struct bt_plugin *bt_plugin_create_from_static(size_t i)
{
	struct bt_plugin *plugin = NULL;

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto error;
	}

	bt_object_init(plugin, bt_plugin_destroy);
	plugin->_register = (SECTION_BEGIN(__plugin_register_funcs))[i];
	if (!plugin->_register) {
		goto error;
	}
	plugin->name = (SECTION_BEGIN(__plugin_names))[i];
	plugin->author = (SECTION_BEGIN(__plugin_authors))[i];
	plugin->license = (SECTION_BEGIN(__plugin_licenses))[i];
	plugin->description = (SECTION_BEGIN(__plugin_descriptions))[i];
	return plugin;
error:
	BT_PUT(plugin);
	return plugin;
}

BT_HIDDEN
enum bt_component_status bt_plugin_register_component_classes(
		struct bt_plugin *plugin, struct bt_component_factory *factory)
{
	assert(plugin && factory);
	return plugin->_register(factory);
}

const char *bt_plugin_get_name(struct bt_plugin *plugin)
{
	return plugin ? plugin->name : NULL;
}

const char *bt_plugin_get_author(struct bt_plugin *plugin)
{
	return plugin ? plugin->author : NULL;
}

const char *bt_plugin_get_license(struct bt_plugin *plugin)
{
	return plugin ? plugin->license : NULL;
}

const char *bt_plugin_get_path(struct bt_plugin *plugin)
{
	return (plugin && plugin->path) ? plugin->path->str : NULL;
}

const char *bt_plugin_get_description(struct bt_plugin *plugin)
{
	return plugin ? plugin->description : NULL;
}
