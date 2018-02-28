/*
 * plugin.c
 *
 * Babeltrace Plugin (generic)
 *
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

#define BT_LOG_TAG "PLUGIN"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/plugin-so-internal.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <ftw.h>
#include <pthread.h>

#define PYTHON_PLUGIN_PROVIDER_FILENAME	"libbabeltrace-python-plugin-provider." G_MODULE_SUFFIX
#define PYTHON_PLUGIN_PROVIDER_SYM_NAME	bt_plugin_python_create_all_from_file
#define PYTHON_PLUGIN_PROVIDER_SYM_NAME_STR	TOSTRING(PYTHON_PLUGIN_PROVIDER_SYM_NAME)

#define APPEND_ALL_FROM_DIR_NFDOPEN_MAX	8

#ifdef BT_BUILT_IN_PYTHON_PLUGIN_SUPPORT
#include <babeltrace/plugin/python-plugin-provider-internal.h>

static
struct bt_plugin_set *(*bt_plugin_python_create_all_from_file_sym)(const char *path) =
	bt_plugin_python_create_all_from_file;

static
void init_python_plugin_provider(void) {}
#else /* BT_BUILT_IN_PYTHON_PLUGIN_SUPPORT */
static GModule *python_plugin_provider_module;
static
struct bt_plugin_set *(*bt_plugin_python_create_all_from_file_sym)(const char *path);

static
void init_python_plugin_provider(void) {
	if (bt_plugin_python_create_all_from_file_sym != NULL) {
		return;
	}

	BT_LOGD_STR("Loading Python plugin provider module.");
	python_plugin_provider_module =
		g_module_open(PYTHON_PLUGIN_PROVIDER_FILENAME, 0);
	if (!python_plugin_provider_module) {
		BT_LOGI("Cannot open `%s`: %s: continuing without Python plugin support.",
			PYTHON_PLUGIN_PROVIDER_FILENAME, g_module_error());
		return;
	}

	if (!g_module_symbol(python_plugin_provider_module,
			PYTHON_PLUGIN_PROVIDER_SYM_NAME_STR,
			(gpointer) &bt_plugin_python_create_all_from_file_sym)) {
		BT_LOGI("Cannot find the Python plugin provider loading symbol: continuing without Python plugin support: "
			"file=\"%s\", symbol=\"%s\"",
			PYTHON_PLUGIN_PROVIDER_FILENAME,
			PYTHON_PLUGIN_PROVIDER_SYM_NAME_STR);
		return;
	}

	BT_LOGI("Loaded Python plugin provider module: addr=%p",
		python_plugin_provider_module);
}

__attribute__((destructor)) static
void fini_python_plugin_provider(void) {
	if (python_plugin_provider_module) {
		BT_LOGD("Unloading Python plugin provider module.");

		if (!g_module_close(python_plugin_provider_module)) {
			BT_LOGE("Failed to close the Python plugin provider module: %s.",
				g_module_error());
		}

		python_plugin_provider_module = NULL;
	}
}
#endif

extern
int64_t bt_plugin_set_get_plugin_count(struct bt_plugin_set *plugin_set)
{
	int64_t count = -1;

	if (!plugin_set) {
		BT_LOGW_STR("Invalid parameter: plugin set is NULL.");
		goto end;
	}

	count = (int64_t) plugin_set->plugins->len;

end:
	return count;
}

extern
struct bt_plugin *bt_plugin_set_get_plugin(struct bt_plugin_set *plugin_set,
		uint64_t index)
{
	struct bt_plugin *plugin = NULL;

	if (!plugin_set) {
		BT_LOGW_STR("Invalid parameter: plugin set is NULL.");
		goto end;
	}

	if (index >= plugin_set->plugins->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			plugin_set, index, plugin_set->plugins->len);
		goto end;
	}

	plugin = bt_get(g_ptr_array_index(plugin_set->plugins, index));

end:
	return plugin;
}

struct bt_plugin_set *bt_plugin_create_all_from_static(void)
{
	/* bt_plugin_so_create_all_from_static() logs errors */
	return bt_plugin_so_create_all_from_static();
}

struct bt_plugin_set *bt_plugin_create_all_from_file(const char *path)
{
	struct bt_plugin_set *plugin_set = NULL;

	if (!path) {
		BT_LOGW_STR("Invalid parameter: path is NULL.");
		goto end;
	}

	BT_LOGD("Creating plugins from file: path=\"%s\"", path);

	/* Try shared object plugins */
	plugin_set = bt_plugin_so_create_all_from_file(path);
	if (plugin_set) {
		goto end;
	}

	/* Try Python plugins if support is available */
	init_python_plugin_provider();
	if (bt_plugin_python_create_all_from_file_sym) {
		plugin_set = bt_plugin_python_create_all_from_file_sym(path);
		if (plugin_set) {
			goto end;
		}
	}

end:
	if (plugin_set) {
		BT_LOGD("Created %u plugins from file: "
			"path=\"%s\", count=%u, plugin-set-addr=%p",
			plugin_set->plugins->len, path,
			plugin_set->plugins->len, plugin_set);
	} else {
		BT_LOGD("Found no plugins in file: path=\"%s\"", path);
	}

	return plugin_set;
}

static void destroy_gstring(void *data)
{
	g_string_free(data, TRUE);
}

struct bt_plugin *bt_plugin_find(const char *plugin_name)
{
	const char *system_plugin_dir;
	char *home_plugin_dir = NULL;
	const char *envvar;
	struct bt_plugin *plugin = NULL;
	struct bt_plugin_set *plugin_set = NULL;
	GPtrArray *dirs = NULL;
	int ret;
	size_t i, j;

	if (!plugin_name) {
		BT_LOGW_STR("Invalid parameter: plugin name is NULL.");
		goto end;
	}

	BT_LOGD("Finding named plugin in standard directories and built-in plugins: "
		"name=\"%s\"", plugin_name);
	dirs = g_ptr_array_new_with_free_func((GDestroyNotify) destroy_gstring);
	if (!dirs) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto end;
	}

	/*
	 * Search order is:
	 *
	 * 1. BABELTRACE_PLUGIN_PATH environment variable
	 *    (colon-separated list of directories)
	 * 2. ~/.local/lib/babeltrace/plugins
	 * 3. Default system directory for Babeltrace plugins, usually
	 *    /usr/lib/babeltrace/plugins or
	 *    /usr/local/lib/babeltrace/plugins if installed
	 *    locally
	 * 4. Built-in plugins (static)
	 *
	 * Directories are searched non-recursively.
	 */
	envvar = getenv("BABELTRACE_PLUGIN_PATH");
	if (envvar) {
		ret = bt_common_append_plugin_path_dirs(envvar, dirs);
		if (ret) {
			BT_LOGE_STR("Failed to append plugin path to array of directories.");
			goto end;
		}
	}

	home_plugin_dir = bt_common_get_home_plugin_path();
	if (home_plugin_dir) {
		GString *home_plugin_dir_str =
			g_string_new(home_plugin_dir);

		if (!home_plugin_dir_str) {
			BT_LOGE_STR("Failed to allocate a GString.");
			goto end;
		}

		g_ptr_array_add(dirs, home_plugin_dir_str);
	}

	system_plugin_dir = bt_common_get_system_plugin_path();
	if (system_plugin_dir) {
		GString *system_plugin_dir_str =
			g_string_new(system_plugin_dir);

		if (!system_plugin_dir_str) {
			BT_LOGE_STR("Failed to allocate a GString.");
			goto end;
		}

		g_ptr_array_add(dirs, system_plugin_dir_str);
	}

	for (i = 0; i < dirs->len; i++) {
		GString *dir = g_ptr_array_index(dirs, i);

		BT_PUT(plugin_set);

		/*
		 * Skip this if the directory does not exist because
		 * bt_plugin_create_all_from_dir() would log a warning.
		 */
		if (!g_file_test(dir->str, G_FILE_TEST_IS_DIR)) {
			BT_LOGV("Skipping nonexistent directory path: "
				"path=\"%s\"", dir->str);
			continue;
		}

		/* bt_plugin_create_all_from_dir() logs details/errors */
		plugin_set = bt_plugin_create_all_from_dir(dir->str, BT_FALSE);
		if (!plugin_set) {
			BT_LOGD("No plugins found in directory: path=\"%s\"",
				dir->str);
			continue;
		}

		for (j = 0; j < plugin_set->plugins->len; j++) {
			struct bt_plugin *candidate_plugin =
				g_ptr_array_index(plugin_set->plugins, j);

			if (strcmp(bt_plugin_get_name(candidate_plugin),
					plugin_name) == 0) {
				BT_LOGD("Plugin found in directory: name=\"%s\", path=\"%s\"",
					plugin_name, dir->str);
				plugin = bt_get(candidate_plugin);
				goto end;
			}
		}

		BT_LOGD("Plugin not found in directory: name=\"%s\", path=\"%s\"",
			plugin_name, dir->str);
	}

	bt_put(plugin_set);
	plugin_set = bt_plugin_create_all_from_static();
	if (plugin_set) {
		for (j = 0; j < plugin_set->plugins->len; j++) {
			struct bt_plugin *candidate_plugin =
				g_ptr_array_index(plugin_set->plugins, j);

			if (strcmp(bt_plugin_get_name(candidate_plugin),
					plugin_name) == 0) {
				BT_LOGD("Plugin found in built-in plugins: "
					"name=\"%s\"", plugin_name);
				plugin = bt_get(candidate_plugin);
				goto end;
			}
		}
	}

end:
	free(home_plugin_dir);
	bt_put(plugin_set);

	if (dirs) {
		g_ptr_array_free(dirs, TRUE);
	}

	if (plugin) {
		BT_LOGD("Found plugin in standard directories and built-in plugins: "
			"addr=%p, name=\"%s\", path=\"%s\"",
			plugin, plugin_name, bt_plugin_get_path(plugin));
	} else {
		BT_LOGD("No plugin found in standard directories and built-in plugins: "
			"name=\"%s\"", plugin_name);
	}

	return plugin;
}

struct bt_component_class *bt_plugin_find_component_class(
		const char *plugin_name, const char *comp_cls_name,
		enum bt_component_class_type comp_cls_type)
{
	struct bt_plugin *plugin = NULL;
	struct bt_component_class *comp_cls = NULL;

	if (!plugin_name) {
		BT_LOGW_STR("Invalid parameter: plugin name is NULL.");
		goto end;
	}

	if (!comp_cls_name) {
		BT_LOGW_STR("Invalid parameter: component class name is NULL.");
		goto end;
	}

	BT_LOGD("Finding named plugin and component class in standard directories and built-in plugins: "
		"plugin-name=\"%s\", comp-class-name=\"%s\"",
		plugin_name, comp_cls_name);
	plugin = bt_plugin_find(plugin_name);
	if (!plugin) {
		BT_LOGD_STR("Plugin not found.");
		goto end;
	}

	comp_cls = bt_plugin_get_component_class_by_name_and_type(
		plugin, comp_cls_name, comp_cls_type);
	if (!comp_cls) {
		BT_LOGD("Component class not found in plugin: "
			"plugin-addr=%p, plugin-path=\"%s\"",
			plugin, bt_plugin_get_path(plugin));
	}

end:
	bt_put(plugin);
	return comp_cls;
}

static struct {
	pthread_mutex_t lock;
	struct bt_plugin_set *plugin_set;
	bt_bool recurse;
} append_all_from_dir_info = {
	.lock = PTHREAD_MUTEX_INITIALIZER
};

static
int nftw_append_all_from_dir(const char *file, const struct stat *sb, int flag,
		struct FTW *s)
{
	int ret = 0;
	const char *name = file + s->base;

	/* Check for recursion */
	if (!append_all_from_dir_info.recurse && s->level > 1) {
		goto end;
	}

	switch (flag) {
	case FTW_F:
	{
		struct bt_plugin_set *plugins_from_file;

		if (name[0] == '.') {
			/* Skip hidden files */
			BT_LOGV("Skipping hidden file: path=\"%s\"", file);
			goto end;
		}

		plugins_from_file = bt_plugin_create_all_from_file(file);

		if (plugins_from_file) {
			size_t j;

			for (j = 0; j < plugins_from_file->plugins->len; j++) {
				struct bt_plugin *plugin =
					g_ptr_array_index(plugins_from_file->plugins, j);

				BT_LOGD("Adding plugin to plugin set: "
					"plugin-path=\"%s\", plugin-addr=%p, plugin-name=\"%s\"",
					file, plugin, bt_plugin_get_name(plugin));
				bt_plugin_set_add_plugin(append_all_from_dir_info.plugin_set, plugin);
			}

			bt_put(plugins_from_file);
		}
		break;
	}
	case FTW_DNR:
		/* Continue to next file / directory. */
		BT_LOGW("Cannot enter directory: continuing: path=\"%s\"", file);
		break;
	case FTW_NS:
		/* Continue to next file / directory. */
		BT_LOGD("Cannot get file information: continuing: path=\"%s\"", file);
		break;
	}

end:
	return ret;
}

static
enum bt_plugin_status bt_plugin_create_append_all_from_dir(
		struct bt_plugin_set *plugin_set, const char *path,
		bt_bool recurse)
{
	int nftw_flags = FTW_PHYS;
	size_t path_len;
	enum bt_plugin_status ret = BT_PLUGIN_STATUS_OK;

	if (!path) {
		BT_LOGW_STR("Invalid parameter: path is NULL.");
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	path_len = strlen(path);
	if (path_len >= PATH_MAX) {
		BT_LOGW("Invalid parameter: path length is too large: "
			"path-length=%zu", path_len);
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	pthread_mutex_lock(&append_all_from_dir_info.lock);

	append_all_from_dir_info.plugin_set = plugin_set;
	append_all_from_dir_info.recurse = recurse;
	ret = nftw(path, nftw_append_all_from_dir,
		APPEND_ALL_FROM_DIR_NFDOPEN_MAX, nftw_flags);

	pthread_mutex_unlock(&append_all_from_dir_info.lock);

	if (ret != 0) {
		BT_LOGW("Cannot open directory: %s: path=\"%s\", errno=%d",
			strerror(errno), path, errno);
		ret = BT_PLUGIN_STATUS_ERROR;
	}

end:
	return ret;
}

struct bt_plugin_set *bt_plugin_create_all_from_dir(const char *path,
		bt_bool recurse)
{
	struct bt_plugin_set *plugin_set;
	enum bt_plugin_status status;

	BT_LOGD("Creating all plugins in directory: path=\"%s\", recurse=%d",
		path, recurse);
	plugin_set = bt_plugin_set_create();
	if (!plugin_set) {
		BT_LOGE_STR("Cannot create empty plugin set.");
		goto error;
	}

	/* Append found plugins to array */
	status = bt_plugin_create_append_all_from_dir(plugin_set, path,
		recurse);
	if (status < 0) {
		BT_LOGW("Cannot append plugins found in directory: "
			"path=\"%s\", status=%s",
			path, bt_plugin_status_string(status));
		goto error;
	}

	BT_LOGD("Created %u plugins from directory: count=%u, path=\"%s\"",
		plugin_set->plugins->len, plugin_set->plugins->len, path);
	goto end;

error:
	BT_PUT(plugin_set);

end:
	return plugin_set;
}

const char *bt_plugin_get_name(struct bt_plugin *plugin)
{
	const char *val = NULL;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto end;
	}

	if (plugin->info.name_set) {
		val = plugin->info.name->str;
	}

end:
	return val;
}

const char *bt_plugin_get_author(struct bt_plugin *plugin)
{
	const char *val = NULL;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto end;
	}

	if (plugin->info.author_set) {
		val = plugin->info.author->str;
	}

end:
	return val;
}

const char *bt_plugin_get_license(struct bt_plugin *plugin)
{
	const char *val = NULL;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto end;
	}

	if (plugin->info.license_set) {
		val = plugin->info.license->str;
	}

end:
	return val;
}

const char *bt_plugin_get_path(struct bt_plugin *plugin)
{
	const char *val = NULL;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto end;
	}

	if (plugin->info.path_set) {
		val = plugin->info.path->str;
	}

end:
	return val;
}

const char *bt_plugin_get_description(struct bt_plugin *plugin)
{
	const char *val = NULL;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto end;
	}

	if (plugin->info.description_set) {
		val = plugin->info.description->str;
	}

end:
	return val;
}

enum bt_plugin_status bt_plugin_get_version(struct bt_plugin *plugin,
		unsigned int *major, unsigned int *minor, unsigned int *patch,
		const char **extra)
{
	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		status = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	if (!plugin->info.version_set) {
		BT_LOGV("Plugin's version is not set: addr=%p", plugin);
		status = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	if (major) {
		*major = plugin->info.version.major;
	}

	if (minor) {
		*minor = plugin->info.version.minor;
	}

	if (patch) {
		*patch = plugin->info.version.patch;
	}

	if (extra) {
		*extra = plugin->info.version.extra->str;
	}

end:
	return status;
}

int64_t bt_plugin_get_component_class_count(struct bt_plugin *plugin)
{
	return plugin ? plugin->comp_classes->len : (int64_t) -1;
}

struct bt_component_class *bt_plugin_get_component_class_by_index(
		struct bt_plugin *plugin, uint64_t index)
{
	struct bt_component_class *comp_class = NULL;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto error;
	}

	if (index >= plugin->comp_classes->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			plugin, index, plugin->comp_classes->len);
		goto error;
	}

	comp_class = g_ptr_array_index(plugin->comp_classes, index);
	bt_get(comp_class);
	goto end;

error:
	BT_PUT(comp_class);

end:
	return comp_class;
}

struct bt_component_class *bt_plugin_get_component_class_by_name_and_type(
		struct bt_plugin *plugin, const char *name,
		enum bt_component_class_type type)
{
	struct bt_component_class *comp_class = NULL;
	size_t i;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto error;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto error;
	}

	for (i = 0; i < plugin->comp_classes->len; i++) {
		struct bt_component_class *comp_class_candidate =
			g_ptr_array_index(plugin->comp_classes, i);
		const char *comp_class_cand_name =
			bt_component_class_get_name(comp_class_candidate);
		enum bt_component_class_type comp_class_cand_type =
			bt_component_class_get_type(comp_class_candidate);

		BT_ASSERT(comp_class_cand_name);
		BT_ASSERT(comp_class_cand_type >= 0);

		if (strcmp(name, comp_class_cand_name) == 0 &&
				comp_class_cand_type == type) {
			comp_class = bt_get(comp_class_candidate);
			break;
		}
	}

	goto end;

error:
	BT_PUT(comp_class);

end:
	return comp_class;
}

enum bt_plugin_status bt_plugin_add_component_class(
	struct bt_plugin *plugin, struct bt_component_class *comp_class)
{
	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;
	struct bt_component_class *comp_class_dup = NULL;
	int comp_class_index = -1;

	if (!plugin) {
		BT_LOGW_STR("Invalid parameter: plugin is NULL.");
		goto error;
	}

	if (!comp_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		goto error;
	}

	if (plugin->frozen) {
		BT_LOGW("Invalid parameter: plugin is frozen: "
			"addr=%p, name=\"%s\"", plugin,
			bt_plugin_get_name(plugin));
		goto error;
	}

	/* Check for duplicate */
	comp_class_dup = bt_plugin_get_component_class_by_name_and_type(plugin,
		bt_component_class_get_name(comp_class),
		bt_component_class_get_type(comp_class));
	if (comp_class_dup) {
		BT_LOGW("Invalid parameter: a component class with this name and type already exists in the plugin: "
			"plugin-addr=%p, plugin-name=\"%s\", plugin-path=\"%s\", "
			"comp-class-name=\"%s\", comp-class-type=%s",
			plugin, bt_plugin_get_name(plugin),
			bt_plugin_get_path(plugin),
			bt_component_class_get_name(comp_class),
			bt_component_class_type_string(
				bt_component_class_get_type(comp_class)));
		goto error;
	}

	/* Add new component class */
	comp_class_index = plugin->comp_classes->len;
	g_ptr_array_add(plugin->comp_classes, bt_get(comp_class));

	/* Special case for a shared object plugin */
	if (plugin->type == BT_PLUGIN_TYPE_SO) {
		bt_plugin_so_on_add_component_class(plugin, comp_class);
	}

	BT_LOGD("Added component class to plugin: "
		"plugin-addr=%p, plugin-name=\"%s\", plugin-path=\"%s\", "
		"comp-class-addr=%p, comp-class-name=\"%s\", comp-class-type=%s",
		plugin, bt_plugin_get_name(plugin),
		bt_plugin_get_path(plugin),
		comp_class,
		bt_component_class_get_name(comp_class),
		bt_component_class_type_string(
			bt_component_class_get_type(comp_class)));
	goto end;

error:
	/* Remove entry from plugin's component classes (if added) */
	if (comp_class_index >= 0) {
		g_ptr_array_remove_index(plugin->comp_classes,
			comp_class_index);
	}

	status = BT_PLUGIN_STATUS_ERROR;

end:
	bt_put(comp_class_dup);
	return status;
}
