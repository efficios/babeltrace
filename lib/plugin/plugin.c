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

#include <babeltrace/compiler.h>
#include <babeltrace/ref.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/plugin-so-internal.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef WITH_PYTHON_PLUGINS
# include <babeltrace/plugin/plugin-python-enabled-internal.h>
#else
# include <babeltrace/plugin/plugin-python-disabled-internal.h>
#endif

static
void bt_plugin_destroy(struct bt_object *obj)
{
	struct bt_plugin *plugin;

	assert(obj);
	plugin = container_of(obj, struct bt_plugin, base);

	if (plugin->type == BT_PLUGIN_TYPE_SO) {
		bt_plugin_so_destroy_spec_data(plugin);
	} else if (plugin->type == BT_PLUGIN_TYPE_PYTHON) {
		bt_plugin_python_destroy_spec_data(plugin);
	} else {
		assert(false);
	}

	if (plugin->comp_classes) {
		g_ptr_array_free(plugin->comp_classes, TRUE);
	}

	if (plugin->info.name) {
		g_string_free(plugin->info.name, TRUE);
	}

	if (plugin->info.path) {
		g_string_free(plugin->info.path, TRUE);
	}

	if (plugin->info.description) {
		g_string_free(plugin->info.description, TRUE);
	}

	if (plugin->info.author) {
		g_string_free(plugin->info.author, TRUE);
	}

	if (plugin->info.license) {
		g_string_free(plugin->info.license, TRUE);
	}

	if (plugin->info.version.extra) {
		g_string_free(plugin->info.version.extra, TRUE);
	}

	g_free(plugin);
}

BT_HIDDEN
struct bt_plugin *bt_plugin_create_empty(enum bt_plugin_type type)
{
	struct bt_plugin *plugin = NULL;

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto error;
	}

	bt_object_init(plugin, bt_plugin_destroy);
	plugin->type = type;

	/* Create empty array of component classes */
	plugin->comp_classes =
		g_ptr_array_new_with_free_func((GDestroyNotify) bt_put);
	if (!plugin->comp_classes) {
		goto error;
	}

	/* Create empty info */
	plugin->info.name = g_string_new(NULL);
	if (!plugin->info.name) {
		goto error;
	}

	plugin->info.path = g_string_new(NULL);
	if (!plugin->info.path) {
		goto error;
	}

	plugin->info.description = g_string_new(NULL);
	if (!plugin->info.description) {
		goto error;
	}

	plugin->info.author = g_string_new(NULL);
	if (!plugin->info.author) {
		goto error;
	}

	plugin->info.license = g_string_new(NULL);
	if (!plugin->info.license) {
		goto error;
	}

	plugin->info.version.extra = g_string_new(NULL);
	if (!plugin->info.version.extra) {
		goto error;
	}

	goto end;

error:
	BT_PUT(plugin);

end:
	return plugin;
}

struct bt_plugin **bt_plugin_create_all_from_static(void)
{
	return bt_plugin_so_create_all_from_static();
}

struct bt_plugin **bt_plugin_create_all_from_file(const char *path)
{
	struct bt_plugin **plugins = NULL;

	if (!path) {
		goto end;
	}

	printf_verbose("Trying to load plugins from `%s`\n", path);

	/* Try shared object plugins */
	plugins = bt_plugin_so_create_all_from_file(path);
	if (plugins) {
		goto end;
	}

	plugins = bt_plugin_python_create_all_from_file(path);
	if (plugins) {
		goto end;
	}

end:
	return plugins;
}

/* Allocate dirent as recommended by READDIR(3), NOTES on readdir_r */
static
struct dirent *alloc_dirent(const char *path)
{
	size_t len;
	long name_max;
	struct dirent *entry;

	name_max = pathconf(path, _PC_NAME_MAX);
	if (name_max == -1) {
		name_max = PATH_MAX;
	}
	len = offsetof(struct dirent, d_name) + name_max + 1;
	entry = zmalloc(len);
	return entry;
}

static
enum bt_plugin_status bt_plugin_create_append_all_from_dir(
		GPtrArray *plugins, const char *path, bool recurse)
{
	DIR *directory = NULL;
	struct dirent *entry = NULL, *result = NULL;
	char *file_path = NULL;
	size_t path_len;
	enum bt_plugin_status ret = BT_PLUGIN_STATUS_OK;

	if (!path) {
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	path_len = strlen(path);

	if (path_len >= PATH_MAX) {
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	entry = alloc_dirent(path);
	if (!entry) {
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	file_path = zmalloc(PATH_MAX);
	if (!file_path) {
		ret = BT_PLUGIN_STATUS_NOMEM;
		goto end;
	}

	strncpy(file_path, path, path_len);
	/* Append a trailing '/' to the path */
	if (file_path[path_len - 1] != '/') {
		file_path[path_len++] = '/';
	}

	directory = opendir(file_path);
	if (!directory) {
		perror("Failed to open plug-in directory");
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	/* Recursively walk directory */
	while (!readdir_r(directory, entry, &result) && result) {
		struct stat st;
		int stat_ret;
		size_t file_name_len;

		if (result->d_name[0] == '.') {
			/* Skip hidden files, . and .. */
			continue;
		}

		file_name_len = strlen(result->d_name);

		if (path_len + file_name_len >= PATH_MAX) {
			continue;
		}

		strncpy(file_path + path_len, result->d_name, file_name_len);
		file_path[path_len + file_name_len] = '\0';
		stat_ret = stat(file_path, &st);
		if (stat_ret < 0) {
			/* Continue to next file / directory. */
			printf_perror("Failed to stat() plugin file\n");
			continue;
		}

		if (S_ISDIR(st.st_mode) && recurse) {
			ret = bt_plugin_create_append_all_from_dir(plugins,
				file_path, true);
			if (ret < 0) {
				goto end;
			}
		} else if (S_ISREG(st.st_mode)) {
			struct bt_plugin **plugins_from_file =
				bt_plugin_create_all_from_file(file_path);

			if (plugins_from_file) {
				struct bt_plugin **plugin;

				for (plugin = plugins_from_file; *plugin; plugin++) {
					/* Transfer ownership to array */
					g_ptr_array_add(plugins, *plugin);
				}

				free(plugins_from_file);
			}
		}
	}
end:
	if (directory) {
		if (closedir(directory)) {
			/*
			 * We don't want to override the error since there is
			 * nothing could do.
			 */
		        perror("Failed to close plug-in directory");
		}
	}
	free(entry);
	free(file_path);
	return ret;
}

struct bt_plugin **bt_plugin_create_all_from_dir(const char *path,
		bool recurse)
{
	GPtrArray *plugins_array = g_ptr_array_new();
	struct bt_plugin **plugins = NULL;
	enum bt_plugin_status status;

	/* Append found plugins to array */
	status = bt_plugin_create_append_all_from_dir(plugins_array, path,
		recurse);
	if (status < 0) {
		goto error;
	}

	/* Add sentinel to array */
	g_ptr_array_add(plugins_array, NULL);
	plugins = (struct bt_plugin **) plugins_array->pdata;
	goto end;

error:
	if (plugins_array) {
		g_ptr_array_free(plugins_array, TRUE);
		plugins_array = NULL;
	}

end:
	if (plugins_array) {
		g_ptr_array_free(plugins_array, FALSE);
	}

	return plugins;
}

const char *bt_plugin_get_name(struct bt_plugin *plugin)
{
	return plugin && plugin->info.name_set ? plugin->info.name->str : NULL;
}

const char *bt_plugin_get_author(struct bt_plugin *plugin)
{
	return plugin && plugin->info.author_set ? plugin->info.author->str : NULL;
}

const char *bt_plugin_get_license(struct bt_plugin *plugin)
{
	return plugin && plugin->info.license_set ? plugin->info.license->str : NULL;
}

const char *bt_plugin_get_path(struct bt_plugin *plugin)
{
	return plugin && plugin->info.path_set ? plugin->info.path->str : NULL;
}

const char *bt_plugin_get_description(struct bt_plugin *plugin)
{
	return plugin && plugin->info.description_set ? plugin->info.description->str : NULL;
}

enum bt_plugin_status bt_plugin_get_version(struct bt_plugin *plugin,
		unsigned int *major, unsigned int *minor, unsigned int *patch,
		const char **extra)
{
	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;

	if (!plugin || !plugin->info.version_set) {
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

int bt_plugin_get_component_class_count(struct bt_plugin *plugin)
{
	return plugin ? plugin->comp_classes->len : -1;
}

struct bt_component_class *bt_plugin_get_component_class(
		struct bt_plugin *plugin, size_t index)
{
	struct bt_component_class *comp_class = NULL;

	if (!plugin || index >= plugin->comp_classes->len) {
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

	if (!plugin || !name) {
		goto error;
	}

	for (i = 0; i < plugin->comp_classes->len; i++) {
		struct bt_component_class *comp_class_candidate =
			g_ptr_array_index(plugin->comp_classes, i);
		const char *comp_class_cand_name =
			bt_component_class_get_name(comp_class_candidate);
		enum bt_component_class_type comp_class_cand_type =
			bt_component_class_get_type(comp_class_candidate);

		assert(comp_class_cand_name);
		assert(comp_class_cand_type >= 0);

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
	int ret;
	int comp_class_index = -1;

	if (!plugin || !comp_class || plugin->frozen) {
		goto error;
	}

	/* Check for duplicate */
	comp_class_dup = bt_plugin_get_component_class_by_name_and_type(plugin,
		bt_component_class_get_name(comp_class),
		bt_component_class_get_type(comp_class));
	if (comp_class_dup) {
		printf_verbose("Plugin `%s`: adding component class with existing name `%s` and type %d\n",
			bt_plugin_get_name(plugin),
			bt_component_class_get_name(comp_class),
			bt_component_class_get_type(comp_class));
		goto error;
	}

	/* Add new component class */
	comp_class_index = plugin->comp_classes->len;
	g_ptr_array_add(plugin->comp_classes, bt_get(comp_class));

	/* Special case for a shared object plugin */
	if (plugin->type == BT_PLUGIN_TYPE_SO) {
		ret = bt_plugin_so_on_add_component_class(plugin, comp_class);
		if (ret) {
			goto error;
		}
	}

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
