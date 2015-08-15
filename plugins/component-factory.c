/*
 * component-factory.c
 *
 * Babeltrace Plugin Component Factory
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

#include <babeltrace/plugin/component-factory.h>
#include <babeltrace/plugin/component-factory-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/ref.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <gmodule.h>

#define NATIVE_PLUGIN_SUFFIX ".so"
#define NATIVE_PLUGIN_SUFFIX_LEN sizeof(NATIVE_PLUGIN_SUFFIX)
#define LIBTOOL_PLUGIN_SUFFIX ".la"
#define LIBTOOL_PLUGIN_SUFFIX_LEN sizeof(LIBTOOL_PLUGIN_SUFFIX)
#define PLUGIN_SUFFIX_LEN max_t(size_t, sizeof(NATIVE_PLUGIN_SUFFIX), \
		sizeof(LIBTOOL_PLUGIN_SUFFIX))

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
enum bt_component_factory_status
bt_component_factory_load_file(struct bt_component_factory *factory,
		const char *path)
{
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;
	enum bt_component_status component_status;
	size_t path_len;
	GModule *module;
	struct bt_plugin *plugin;

	if (!factory || !path) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	path_len = strlen(path);
	if (path_len <= PLUGIN_SUFFIX_LEN) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	path_len++;
	/*
	 * Check if the file ends with a known plugin file type suffix (i.e. .so
	 * or .la on Linux).
	 */
	if (strncmp(NATIVE_PLUGIN_SUFFIX,
			path + path_len - NATIVE_PLUGIN_SUFFIX_LEN,
			NATIVE_PLUGIN_SUFFIX_LEN) &&
			strncmp(LIBTOOL_PLUGIN_SUFFIX,
			path + path_len - LIBTOOL_PLUGIN_SUFFIX_LEN,
			LIBTOOL_PLUGIN_SUFFIX_LEN)) {
		/* Name indicates that this is not a plugin file. */
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	module = g_module_open(path, 0);
	if (!module) {
		printf_error("Module open error: %s", g_module_error());
		ret = BT_COMPONENT_FACTORY_STATUS_ERROR;
		goto end;
	}

	/* Load plugin and make sure it defines the required entry points */
	plugin = bt_plugin_create(module);
	if (!plugin) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL_PLUGIN;
		if (!g_module_close(module)) {
			printf_error("Module close error: %s",
				g_module_error());
		}
		goto end;
	}

	component_status = bt_plugin_register_component_classes(plugin,
			factory);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		switch (component_status) {
		case BT_COMPONENT_STATUS_NOMEM:
			ret = BT_COMPONENT_FACTORY_STATUS_NOMEM;
			break;
		default:
			ret = BT_COMPONENT_FACTORY_STATUS_ERROR;
			break;
		}

		BT_PUT(plugin);
		goto end;
	}
	g_ptr_array_add(factory->plugins, plugin);
end:
	return ret;
}

static
enum bt_component_factory_status
bt_component_factory_load_dir_recursive(struct bt_component_factory *factory,
		const char *path)
{
	DIR *directory = NULL;
	struct dirent *entry = NULL, *result = NULL;
	char *file_path = NULL;
	size_t path_len = strlen(path);
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	if (path_len >= PATH_MAX) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	entry = alloc_dirent(path);
	if (!entry) {
		ret = BT_COMPONENT_FACTORY_STATUS_NOMEM;
		goto end;
	}

	file_path = zmalloc(PATH_MAX);
	if (!file_path) {
		ret = BT_COMPONENT_FACTORY_STATUS_NOMEM;
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
		ret = BT_COMPONENT_FACTORY_STATUS_ERROR;
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
			printf_perror("Failed to stat() plugin file");
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			ret = bt_component_factory_load_dir_recursive(factory,
					file_path);
			if (ret != BT_COMPONENT_FACTORY_STATUS_OK) {
				goto end;
			}
		} else if (S_ISREG(st.st_mode)) {
		        bt_component_factory_load_file(factory, file_path);
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

static
void bt_component_factory_destroy(struct bt_object *obj)
{
	struct bt_component_factory *factory = NULL;

	assert(obj);
	factory = container_of(obj, struct bt_component_factory, base);

	if (factory->plugins) {
		g_ptr_array_free(factory->plugins, TRUE);
	}
	if (factory->components) {
		g_ptr_array_free(factory->components, TRUE);
	}
	g_free(factory);
}

struct bt_component_factory *bt_component_factory_create(void)
{
	struct bt_component_factory *factory;

	factory = g_new0(struct bt_component_factory, 1);
	if (!factory) {
		goto end;
	}

	bt_object_init(factory, bt_component_factory_destroy);
	factory->plugins = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	if (!factory->plugins) {
		goto error;
	}
	factory->components = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	if (!factory->components) {
		goto error;
	}
end:
	return factory;
error:
        BT_PUT(factory);
	return factory;
}

enum bt_component_factory_status bt_component_factory_load(
		struct bt_component_factory *factory, const char *path)
{
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	if (!factory || !path) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		ret = BT_COMPONENT_FACTORY_STATUS_NOENT;
		goto end;
	}

	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		ret = bt_component_factory_load_dir_recursive(factory, path);
	} else if (g_file_test(path, G_FILE_TEST_IS_REGULAR) ||
			g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
		ret = bt_component_factory_load_file(factory, path);
	} else {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}
end:
	return ret;
}
