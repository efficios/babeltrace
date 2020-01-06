/*
 * Babeltrace trace converter - CLI tool's configuration
 *
 * Copyright 2016-2019 EfficiOS Inc.
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

#define BT_LOG_TAG "CLI/PLUGINS"
#include "logging.h"

#include "babeltrace2-plugins.h"

#include <stdbool.h>
#include <babeltrace2/babeltrace.h>

/* Array of bt_plugin * */
static GPtrArray *loaded_plugins;

void init_loaded_plugins(void)
{
	loaded_plugins = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_plugin_put_ref);
}

void fini_loaded_plugins(void)
{
	g_ptr_array_free(loaded_plugins, TRUE);
}

const bt_plugin *borrow_loaded_plugin_by_name(const char *name)
{
	int i;
	const bt_plugin *plugin = NULL;

	BT_ASSERT(name);
	BT_LOGI("Finding plugin: name=\"%s\"", name);

	for (i = 0; i < loaded_plugins->len; i++) {
		plugin = g_ptr_array_index(loaded_plugins, i);

		if (strcmp(name, bt_plugin_get_name(plugin)) == 0) {
			break;
		}

		plugin = NULL;
	}

	if (plugin) {
		BT_LOGI("Found plugin: name=\"%s\", plugin-addr=%p",
			name, plugin);
	} else {
		BT_LOGI("Cannot find plugin: name=\"%s\"", name);
	}

	return plugin;
}

size_t get_loaded_plugins_count(void)
{
	return loaded_plugins->len;
}

const bt_plugin **borrow_loaded_plugins(void)
{
	return (const bt_plugin **) loaded_plugins->pdata;
}

const bt_plugin *borrow_loaded_plugin_by_index(size_t index)
{
	BT_ASSERT(index < loaded_plugins->len);
	return g_ptr_array_index(loaded_plugins, index);
}

static
void add_to_loaded_plugins(const bt_plugin_set *plugin_set)
{
	int64_t i;
	int64_t count;

	count = bt_plugin_set_get_plugin_count(plugin_set);
	BT_ASSERT(count >= 0);

	for (i = 0; i < count; i++) {
		const bt_plugin *plugin =
			bt_plugin_set_borrow_plugin_by_index_const(plugin_set, i);
		const bt_plugin *loaded_plugin =
			borrow_loaded_plugin_by_name(bt_plugin_get_name(plugin));

		BT_ASSERT(plugin);

		if (loaded_plugin) {
			BT_LOGI("Not using plugin: another one already exists with the same name: "
				"plugin-name=\"%s\", plugin-path=\"%s\", "
				"existing-plugin-path=\"%s\"",
				bt_plugin_get_name(plugin),
				bt_plugin_get_path(plugin),
				bt_plugin_get_path(loaded_plugin));
		} else {
			/* Add to global array. */
			BT_LOGD("Adding plugin to loaded plugins: plugin-path=\"%s\"",
				bt_plugin_get_name(plugin));
			bt_plugin_get_ref(plugin);
			g_ptr_array_add(loaded_plugins, (void *) plugin);
		}
	}
}

static
int load_dynamic_plugins(const bt_value *plugin_paths)
{
	int nr_paths, i, ret = 0;

	nr_paths = bt_value_array_get_length(plugin_paths);
	if (nr_paths == 0) {
		BT_LOGI_STR("No dynamic plugin path.");
		goto end;
	}

	BT_LOGI_STR("Loading dynamic plugins.");

	for (i = 0; i < nr_paths; i++) {
		const bt_value *plugin_path_value = NULL;
		const char *plugin_path;
		const bt_plugin_set *plugin_set = NULL;
		bt_plugin_find_all_from_dir_status status;

		plugin_path_value =
			bt_value_array_borrow_element_by_index_const(
				plugin_paths, i);
		plugin_path = bt_value_string_get(plugin_path_value);

		/*
		 * Skip this if the directory does not exist because
		 * bt_plugin_find_all_from_dir() expects an existing
		 * directory.
		 */
		if (!g_file_test(plugin_path, G_FILE_TEST_IS_DIR)) {
			BT_LOGI("Skipping nonexistent directory path: "
				"path=\"%s\"", plugin_path);
			continue;
		}

		status = bt_plugin_find_all_from_dir(plugin_path, BT_FALSE,
			BT_TRUE, &plugin_set);
		if (status < 0) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Unable to load dynamic plugins from directory: "
				"path=\"%s\"", plugin_path);
			ret = status;
			goto end;
		} else if (status ==
				BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND) {
			BT_LOGI("No plugins found in directory: path=\"%s\"",
				plugin_path);
			continue;
		}

		BT_ASSERT(status == BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK);
		BT_ASSERT(plugin_set);
		add_to_loaded_plugins(plugin_set);
		bt_plugin_set_put_ref(plugin_set);
	}
end:
	return ret;
}

static
int load_static_plugins(void)
{
	int ret = 0;
	const bt_plugin_set *plugin_set;
	bt_plugin_find_all_from_static_status status;

	BT_LOGI("Loading static plugins.");
	status = bt_plugin_find_all_from_static(BT_FALSE, &plugin_set);
	if (status < 0) {
		BT_LOGE("Unable to load static plugins.");
		ret = -1;
		goto end;
	} else if (status ==
			BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND) {
		BT_LOGI("No static plugins found.");
		goto end;
	}

	BT_ASSERT(status == BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_OK);
	BT_ASSERT(plugin_set);
	add_to_loaded_plugins(plugin_set);
	bt_plugin_set_put_ref(plugin_set);

end:
	return ret;
}

int require_loaded_plugins(const bt_value *plugin_paths)
{
	static bool loaded = false;
	static int ret = 0;

	if (loaded) {
		goto end;
	}

	loaded = true;

	if (load_dynamic_plugins(plugin_paths)) {
		ret = -1;
		goto end;
	}

	if (load_static_plugins()) {
		ret = -1;
		goto end;
	}

	BT_LOGI("Loaded all plugins: count=%u", loaded_plugins->len);

end:
	return ret;
}
