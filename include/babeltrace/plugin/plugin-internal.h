#ifndef BABELTRACE_PLUGIN_PLUGIN_INTERNAL_H
#define BABELTRACE_PLUGIN_PLUGIN_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/plugin/plugin-const.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/plugin/plugin-so-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/lib-logging-internal.h>
#include <glib.h>

enum bt_plugin_type {
	BT_PLUGIN_TYPE_SO = 0,
	BT_PLUGIN_TYPE_PYTHON = 1,
};

enum bt_plugin_status {
	BT_PLUGIN_STATUS_OK = 0,
	BT_PLUGIN_STATUS_ERROR = -1,
	BT_PLUGIN_STATUS_NOMEM = -12,
};

struct bt_plugin {
	struct bt_object base;
	enum bt_plugin_type type;

	/* Arrays of `struct bt_component_class *` (owned by this) */
	GPtrArray *src_comp_classes;
	GPtrArray *flt_comp_classes;
	GPtrArray *sink_comp_classes;

	/* Info (owned by this) */
	struct {
		GString *path;
		GString *name;
		GString *author;
		GString *license;
		GString *description;
		struct {
			unsigned int major;
			unsigned int minor;
			unsigned int patch;
			GString *extra;
		} version;
		bool path_set;
		bool name_set;
		bool author_set;
		bool license_set;
		bool description_set;
		bool version_set;
	} info;

	/* Value depends on the specific plugin type */
	void *spec_data;
	void (*destroy_spec_data)(struct bt_plugin *);
};

struct bt_plugin_set {
	struct bt_object base;

	/* Array of struct bt_plugin * */
	GPtrArray *plugins;
};

static inline
const char *bt_plugin_status_string(enum bt_plugin_status status)
{
	switch (status) {
	case BT_PLUGIN_STATUS_OK:
		return "BT_PLUGIN_STATUS_OK";
	case BT_PLUGIN_STATUS_ERROR:
		return "BT_PLUGIN_STATUS_ERROR";
	case BT_PLUGIN_STATUS_NOMEM:
		return "BT_PLUGIN_STATUS_NOMEM";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_plugin_type_string(enum bt_plugin_type type)
{
	switch (type) {
	case BT_PLUGIN_TYPE_SO:
		return "BT_PLUGIN_TYPE_SO";
	case BT_PLUGIN_TYPE_PYTHON:
		return "BT_PLUGIN_TYPE_PYTHON";
	default:
		return "(unknown)";
	}
}

static inline
void bt_plugin_destroy(struct bt_object *obj)
{
	struct bt_plugin *plugin;

	BT_ASSERT(obj);
	plugin = container_of(obj, struct bt_plugin, base);
	BT_LIB_LOGD("Destroying plugin object: %!+l", plugin);

	if (plugin->destroy_spec_data) {
		plugin->destroy_spec_data(plugin);
	}

	if (plugin->src_comp_classes) {
		BT_LOGD_STR("Putting source component classes.");
		g_ptr_array_free(plugin->src_comp_classes, TRUE);
		plugin->src_comp_classes = NULL;
	}

	if (plugin->flt_comp_classes) {
		BT_LOGD_STR("Putting filter component classes.");
		g_ptr_array_free(plugin->flt_comp_classes, TRUE);
		plugin->flt_comp_classes = NULL;
	}

	if (plugin->sink_comp_classes) {
		BT_LOGD_STR("Putting sink component classes.");
		g_ptr_array_free(plugin->sink_comp_classes, TRUE);
		plugin->sink_comp_classes = NULL;
	}

	if (plugin->info.name) {
		g_string_free(plugin->info.name, TRUE);
		plugin->info.name = NULL;
	}

	if (plugin->info.path) {
		g_string_free(plugin->info.path, TRUE);
		plugin->info.path = NULL;
	}

	if (plugin->info.description) {
		g_string_free(plugin->info.description, TRUE);
		plugin->info.description = NULL;
	}

	if (plugin->info.author) {
		g_string_free(plugin->info.author, TRUE);
		plugin->info.author = NULL;
	}

	if (plugin->info.license) {
		g_string_free(plugin->info.license, TRUE);
		plugin->info.license = NULL;
	}

	if (plugin->info.version.extra) {
		g_string_free(plugin->info.version.extra, TRUE);
		plugin->info.version.extra = NULL;
	}

	g_free(plugin);
}

static inline
struct bt_plugin *bt_plugin_create_empty(enum bt_plugin_type type)
{
	struct bt_plugin *plugin = NULL;

	BT_LOGD("Creating empty plugin object: type=%s",
		bt_plugin_type_string(type));

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		BT_LOGE_STR("Failed to allocate one plugin.");
		goto error;
	}

	bt_object_init_shared(&plugin->base, bt_plugin_destroy);
	plugin->type = type;

	/* Create empty arrays of component classes */
	plugin->src_comp_classes =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_put_ref);
	if (!plugin->src_comp_classes) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	plugin->flt_comp_classes =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_put_ref);
	if (!plugin->flt_comp_classes) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	plugin->sink_comp_classes =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_put_ref);
	if (!plugin->sink_comp_classes) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	/* Create empty info */
	plugin->info.name = g_string_new(NULL);
	if (!plugin->info.name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.path = g_string_new(NULL);
	if (!plugin->info.path) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.description = g_string_new(NULL);
	if (!plugin->info.description) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.author = g_string_new(NULL);
	if (!plugin->info.author) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.license = g_string_new(NULL);
	if (!plugin->info.license) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.version.extra = g_string_new(NULL);
	if (!plugin->info.version.extra) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	BT_LIB_LOGD("Created empty plugin object: %!+l", plugin);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(plugin);

end:
	return plugin;
}

static inline
void bt_plugin_set_path(struct bt_plugin *plugin, const char *path)
{
	BT_ASSERT(plugin);
	BT_ASSERT(path);
	g_string_assign(plugin->info.path, path);
	plugin->info.path_set = BT_TRUE;
	BT_LIB_LOGV("Set plugin's path: %![plugin-]+l, path=\"%s\"",
		plugin, path);
}

static inline
void bt_plugin_set_name(struct bt_plugin *plugin, const char *name)
{
	BT_ASSERT(plugin);
	BT_ASSERT(name);
	g_string_assign(plugin->info.name, name);
	plugin->info.name_set = BT_TRUE;
	BT_LIB_LOGV("Set plugin's name: %![plugin-]+l, name=\"%s\"",
		plugin, name);
}

static inline
void bt_plugin_set_description(struct bt_plugin *plugin,
		const char *description)
{
	BT_ASSERT(plugin);
	BT_ASSERT(description);
	g_string_assign(plugin->info.description, description);
	plugin->info.description_set = BT_TRUE;
	BT_LIB_LOGV("Set plugin's description: %![plugin-]+l", plugin);
}

static inline
void bt_plugin_set_author(struct bt_plugin *plugin, const char *author)
{
	BT_ASSERT(plugin);
	BT_ASSERT(author);
	g_string_assign(plugin->info.author, author);
	plugin->info.author_set = BT_TRUE;
	BT_LIB_LOGV("Set plugin's author: %![plugin-]+l, author=\"%s\"",
		plugin, author);
}

static inline
void bt_plugin_set_license(struct bt_plugin *plugin, const char *license)
{
	BT_ASSERT(plugin);
	BT_ASSERT(license);
	g_string_assign(plugin->info.license, license);
	plugin->info.license_set = BT_TRUE;
	BT_LIB_LOGV("Set plugin's path: %![plugin-]+l, license=\"%s\"",
		plugin, license);
}

static inline
void bt_plugin_set_version(struct bt_plugin *plugin, unsigned int major,
		unsigned int minor, unsigned int patch, const char *extra)
{
	BT_ASSERT(plugin);
	plugin->info.version.major = major;
	plugin->info.version.minor = minor;
	plugin->info.version.patch = patch;

	if (extra) {
		g_string_assign(plugin->info.version.extra, extra);
	}

	plugin->info.version_set = BT_TRUE;
	BT_LIB_LOGV("Set plugin's version: %![plugin-]+l, "
		"major=%u, minor=%u, patch=%u, extra=\"%s\"",
		plugin, major, minor, patch, extra);
}

static inline
enum bt_plugin_status bt_plugin_add_component_class(
	struct bt_plugin *plugin, struct bt_component_class *comp_class)
{
	GPtrArray *comp_classes;

	BT_ASSERT(plugin);
	BT_ASSERT(comp_class);

	switch (comp_class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		comp_classes = plugin->src_comp_classes;
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		comp_classes = plugin->flt_comp_classes;
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		comp_classes = plugin->sink_comp_classes;
		break;
	default:
		abort();
	}

	/* Add new component class */
	bt_object_get_ref(comp_class);
	g_ptr_array_add(comp_classes, comp_class);

	/* Special case for a shared object plugin */
	if (plugin->type == BT_PLUGIN_TYPE_SO) {
		bt_plugin_so_on_add_component_class(plugin, comp_class);
	}

	BT_LIB_LOGD("Added component class to plugin: "
		"%![plugin-]+l, %![cc-]+C", plugin, comp_class);
	return BT_PLUGIN_STATUS_OK;
}

static
void bt_plugin_set_destroy(struct bt_object *obj)
{
	struct bt_plugin_set *plugin_set =
		container_of(obj, struct bt_plugin_set, base);

	if (!plugin_set) {
		return;
	}

	BT_LOGD("Destroying plugin set: addr=%p", plugin_set);

	if (plugin_set->plugins) {
		BT_LOGD_STR("Putting plugins.");
		g_ptr_array_free(plugin_set->plugins, TRUE);
	}

	g_free(plugin_set);
}

static inline
struct bt_plugin_set *bt_plugin_set_create(void)
{
	struct bt_plugin_set *plugin_set = g_new0(struct bt_plugin_set, 1);

	if (!plugin_set) {
		goto end;
	}

	BT_LOGD_STR("Creating empty plugin set.");
	bt_object_init_shared(&plugin_set->base, bt_plugin_set_destroy);

	plugin_set->plugins = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!plugin_set->plugins) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		BT_OBJECT_PUT_REF_AND_RESET(plugin_set);
		goto end;
	}

	BT_LOGD("Created empty plugin set: addr=%p", plugin_set);

end:
	return plugin_set;
}

static inline
void bt_plugin_set_add_plugin(struct bt_plugin_set *plugin_set,
		struct bt_plugin *plugin)
{
	BT_ASSERT(plugin_set);
	BT_ASSERT(plugin);
	bt_object_get_ref(plugin);
	g_ptr_array_add(plugin_set->plugins, plugin);
	BT_LIB_LOGV("Added plugin to plugin set: "
		"plugin-set-addr=%p, %![plugin-]+l",
		plugin_set, plugin);
}

#endif /* BABELTRACE_PLUGIN_PLUGIN_INTERNAL_H */
