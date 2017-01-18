/*
 * plugin.c
 *
 * Babeltrace Plugin
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
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/component/component-class-internal.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <gmodule.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#define PLUGIN_SYMBOL_NAME		"__bt_plugin_name"
#define PLUGIN_SYMBOL_AUTHOR		"__bt_plugin_author"
#define PLUGIN_SYMBOL_LICENSE		"__bt_plugin_license"
#define PLUGIN_SYMBOL_INIT		"__bt_plugin_init"
#define PLUGIN_SYMBOL_EXIT		"__bt_plugin_exit"
#define PLUGIN_SYMBOL_DESCRIPTION	"__bt_plugin_description"
#define NATIVE_PLUGIN_SUFFIX		".so"
#define NATIVE_PLUGIN_SUFFIX_LEN	sizeof(NATIVE_PLUGIN_SUFFIX)
#define LIBTOOL_PLUGIN_SUFFIX		".la"
#define LIBTOOL_PLUGIN_SUFFIX_LEN	sizeof(LIBTOOL_PLUGIN_SUFFIX)

#define PLUGIN_SUFFIX_LEN	max_t(size_t, sizeof(NATIVE_PLUGIN_SUFFIX), \
					sizeof(LIBTOOL_PLUGIN_SUFFIX))

#define SECTION_BEGIN(_name)		&__start_##_name
#define SECTION_END(_name)		&__stop_##_name
#define SECTION_ELEMENT_COUNT(_name) (SECTION_END(_name) - SECTION_BEGIN(_name))

#define DECLARE_SECTION(_type, _name)				\
	extern _type const __start_##_name __attribute((weak));	\
	extern _type const __stop_##_name __attribute((weak))

DECLARE_SECTION(bt_plugin_init_func, __bt_plugin_init_funcs);
DECLARE_SECTION(bt_plugin_exit_func, __bt_plugin_exit_funcs);
DECLARE_SECTION(const char *, __bt_plugin_names);
DECLARE_SECTION(const char *, __bt_plugin_authors);
DECLARE_SECTION(const char *, __bt_plugin_licenses);
DECLARE_SECTION(const char *, __bt_plugin_descriptions);

#define PRINT_SECTION(_printer, _name)						\
	do {									\
		_printer("Section " #_name " [%p - %p], (%zu elements)\n",	\
			SECTION_BEGIN(_name), SECTION_END(_name),		\
			SECTION_ELEMENT_COUNT(_name));				\
	} while (0)

#define PRINT_PLUG_IN_SECTIONS(_printer)				\
	do {								\
		PRINT_SECTION(_printer, __bt_plugin_init_funcs);	\
		PRINT_SECTION(_printer, __bt_plugin_exit_funcs);	\
		PRINT_SECTION(_printer, __bt_plugin_names);		\
		PRINT_SECTION(_printer, __bt_plugin_authors);		\
		PRINT_SECTION(_printer, __bt_plugin_licenses);		\
		PRINT_SECTION(_printer, __bt_plugin_descriptions);	\
	} while (0)

/*
 * This hash table, global to the library, maps component class pointers
 * to shared library handles.
 *
 * The keys (component classes) are NOT owned by this hash table, whereas
 * the values (shared library handles) are owned by this hash table.
 *
 * The keys are the component classes created with
 * bt_plugin_add_component_class(). They keep the shared library handle
 * object created by their plugin alive so that the plugin's code is
 * not discarded when it could still be in use by living components
 * created from those component classes:
 *
 *     [component] --ref-> [component class] --through this HT-> [shlib handle]
 *
 * This hash table exists for two reasons:
 *
 * 1. To allow this application:
 *
 *        my_plugin = bt_plugin_create_from_file("/path/to/my-plugin.so");
 *        // instantiate components from the plugin's component classes
 *        BT_PUT(my_plugin);
 *        // user code of instantiated components still exists
 *
 * 2. To decouple the plugin subsystem from the component subsystem:
 *    while plugins objects need to know component class objects, the
 *    opposite is not necessary, thus it makes no sense for a component
 *    class to keep a reference to the plugin object from which it was
 *    created.
 *
 * An entry is removed from this HT when a component class is destroyed
 * thanks to a custom destroy listener. When the entry is removed, the
 * GLib function calls the value destroy notifier of the HT, which is
 * bt_put(). This decreases the reference count of the mapped shared
 * library handle. Assuming the original plugin object which contained
 * some component classes is put first, when the last component class is
 * removed from this HT, the shared library handle object's reference
 * count falls to zero and the shared library is finally closed.
 */
static
GHashTable *comp_classes_to_shlib_handles;

__attribute__((constructor)) static
void init_comp_classes_to_shlib_handles(void) {
	comp_classes_to_shlib_handles = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, bt_put);
	assert(comp_classes_to_shlib_handles);
}

__attribute__((destructor)) static
void fini_comp_classes_to_shlib_handles(void) {
	if (comp_classes_to_shlib_handles) {
		g_hash_table_destroy(comp_classes_to_shlib_handles);
	}
}

static
void bt_plugin_shared_lib_handle_destroy(struct bt_object *obj)
{
	struct bt_plugin_shared_lib_handle *shared_lib_handle;

	assert(obj);
	shared_lib_handle = container_of(obj,
		struct bt_plugin_shared_lib_handle, base);

	if (shared_lib_handle->init_called && shared_lib_handle->exit) {
		enum bt_plugin_status status = shared_lib_handle->exit();

		if (status < 0) {
			printf_verbose("Plugin `%s` exited with error %d\n",
				shared_lib_handle->name, status);
		}
	}

	if (shared_lib_handle->module) {
		if (!g_module_close(shared_lib_handle->module)) {
			printf_error("Module close error: %s\n",
					g_module_error());
		}
	}

	if (shared_lib_handle->path) {
		g_string_free(shared_lib_handle->path, TRUE);
	}

	g_free(shared_lib_handle);
}

static
struct bt_plugin_shared_lib_handle *bt_plugin_shared_lib_handle_create(
		const char *path)
{
	struct bt_plugin_shared_lib_handle *shared_lib_handle = NULL;
	gpointer symbol = NULL;

	shared_lib_handle = g_new0(struct bt_plugin_shared_lib_handle, 1);
	if (!shared_lib_handle) {
		goto error;
	}

	bt_object_init(shared_lib_handle, bt_plugin_shared_lib_handle_destroy);

	if (!path) {
		goto end;
	}

	shared_lib_handle->path = g_string_new(path);
	if (!shared_lib_handle->path) {
		goto error;
	}

	shared_lib_handle->module = g_module_open(path, 0);
	if (!shared_lib_handle->module) {
		printf_verbose("Module open error: %s\n", g_module_error());
		goto error;
	}

	if (!g_module_symbol(shared_lib_handle->module, PLUGIN_SYMBOL_NAME,
			(gpointer *) &shared_lib_handle->name)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			PLUGIN_SYMBOL_NAME,
			g_module_name(shared_lib_handle->module));
		goto error;
	}

	if (!g_module_symbol(shared_lib_handle->module, PLUGIN_SYMBOL_LICENSE,
			(gpointer *) &shared_lib_handle->license)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			PLUGIN_SYMBOL_LICENSE,
			g_module_name(shared_lib_handle->module));
		goto error;
	}

	if (!g_module_symbol(shared_lib_handle->module, PLUGIN_SYMBOL_AUTHOR,
			(gpointer *) &shared_lib_handle->author)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			PLUGIN_SYMBOL_AUTHOR,
			g_module_name(shared_lib_handle->module));
		goto error;
	}

	if (!g_module_symbol(shared_lib_handle->module, PLUGIN_SYMBOL_DESCRIPTION,
			(gpointer *) &shared_lib_handle->description)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			PLUGIN_SYMBOL_DESCRIPTION,
			g_module_name(shared_lib_handle->module));
		goto error;
	}

	if (!g_module_symbol(shared_lib_handle->module, PLUGIN_SYMBOL_INIT,
			&symbol)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			PLUGIN_SYMBOL_INIT,
			g_module_name(shared_lib_handle->module));
		goto error;
	} else {
		shared_lib_handle->init = *((bt_plugin_init_func *) symbol);
		if (!shared_lib_handle->init) {
			printf_verbose("NULL %s symbol target\n",
				PLUGIN_SYMBOL_INIT);
			goto error;
		}
	}

	if (!g_module_symbol(shared_lib_handle->module, PLUGIN_SYMBOL_EXIT,
			&symbol)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			PLUGIN_SYMBOL_EXIT,
			g_module_name(shared_lib_handle->module));
		goto error;
	} else {
		shared_lib_handle->exit = *((bt_plugin_exit_func *) symbol);
		if (!shared_lib_handle->exit) {
			printf_verbose("NULL %s symbol target\n",
				PLUGIN_SYMBOL_EXIT);
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(shared_lib_handle);

end:
	return shared_lib_handle;
}

static
void bt_plugin_destroy(struct bt_object *obj)
{
	struct bt_plugin *plugin;

	assert(obj);
	plugin = container_of(obj, struct bt_plugin, base);

	BT_PUT(plugin->shared_lib_handle);

	if (plugin->comp_classes) {
		g_ptr_array_free(plugin->comp_classes, TRUE);
	}

	g_free(plugin);
}

static
enum bt_plugin_status init_plugin(struct bt_plugin *plugin)
{
	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;

	if (plugin->shared_lib_handle->init) {
		status = plugin->shared_lib_handle->init(plugin);

		if (status < 0) {
			printf_verbose("Plugin `%s` initialization error: %d\n",
				plugin->shared_lib_handle->name, status);
			goto end;
		}
	}

	plugin->shared_lib_handle->init_called = true;

	/*
	 * The initialization function should have added the component
	 * classes at this point. We freeze the plugin so that it's not
	 * possible to add component classes to this plugin object after
	 * this stage (plugin object becomes immutable).
	 */
	plugin->frozen = true;

end:
	return status;
}

struct bt_plugin *bt_plugin_create_from_file(const char *path)
{
	size_t path_len;
	struct bt_plugin *plugin = NULL;
	bool is_libtool_wrapper = false, is_shared_object = false;

	if (!path) {
		goto error;
	}

	path_len = strlen(path);
	if (path_len <= PLUGIN_SUFFIX_LEN) {
		goto error;
	}

	path_len++;
	/*
	 * Check if the file ends with a known plugin file type suffix (i.e. .so
	 * or .la on Linux).
	 */
	is_libtool_wrapper = !strncmp(LIBTOOL_PLUGIN_SUFFIX,
		path + path_len - LIBTOOL_PLUGIN_SUFFIX_LEN,
		LIBTOOL_PLUGIN_SUFFIX_LEN);
	is_shared_object = !strncmp(NATIVE_PLUGIN_SUFFIX,
		path + path_len - NATIVE_PLUGIN_SUFFIX_LEN,
		NATIVE_PLUGIN_SUFFIX_LEN);
	if (!is_shared_object && !is_libtool_wrapper) {
		/* Name indicates that this is not a plugin file. */
		goto error;
	}

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto error;
	}

	bt_object_init(plugin, bt_plugin_destroy);

	/* Create shared lib handle */
	plugin->shared_lib_handle = bt_plugin_shared_lib_handle_create(path);
	if (!plugin->shared_lib_handle) {
		printf_verbose("Failed to create a shared library handle (path `%s`)\n",
			path);
		goto error;
	}

	/* Create empty array of component classes */
	plugin->comp_classes =
		g_ptr_array_new_with_free_func((GDestroyNotify) bt_put);
	if (!plugin->comp_classes) {
		goto error;
	}

	/* Initialize plugin */
	if (init_plugin(plugin) < 0) {
		goto error;
	}

	goto end;

error:
	BT_PUT(plugin);

end:
	return plugin;
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
	size_t path_len = strlen(path);
	enum bt_plugin_status ret = BT_PLUGIN_STATUS_OK;

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
			struct bt_plugin *plugin = bt_plugin_create_from_file(file_path);

			if (plugin) {
				/* Transfer ownership to array */
				g_ptr_array_add(plugins, plugin);
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
	GPtrArray *plugins_array = NULL;
	struct bt_plugin **plugins = NULL;
	enum bt_plugin_status status;

	if (!path) {
		goto error;
	}

	plugins_array = g_ptr_array_new();
	if (!plugins_array) {
		goto error;
	}

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

static
struct bt_plugin *bt_plugin_create_from_static_at_index(size_t i)
{
	struct bt_plugin *plugin = NULL;

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto error;
	}

	bt_object_init(plugin, bt_plugin_destroy);

	/* Create shared lib handle */
	plugin->shared_lib_handle = bt_plugin_shared_lib_handle_create(NULL);
	if (!plugin->shared_lib_handle) {
		goto error;
	}

	/* Fill shared lib handle */
	plugin->shared_lib_handle->init =
		(SECTION_BEGIN(__bt_plugin_init_funcs))[i];
	if (!plugin->shared_lib_handle->init) {
		goto error;
	}

	plugin->shared_lib_handle->exit =
		(SECTION_BEGIN(__bt_plugin_exit_funcs))[i];
	if (!plugin->shared_lib_handle->exit) {
		goto error;
	}

	plugin->shared_lib_handle->name = (SECTION_BEGIN(__bt_plugin_names))[i];
	plugin->shared_lib_handle->author =
		(SECTION_BEGIN(__bt_plugin_authors))[i];
	plugin->shared_lib_handle->license =
		(SECTION_BEGIN(__bt_plugin_licenses))[i];
	plugin->shared_lib_handle->description =
		(SECTION_BEGIN(__bt_plugin_descriptions))[i];

	/* Create empty array of component classes */
	plugin->comp_classes =
		g_ptr_array_new_with_free_func((GDestroyNotify) bt_put);
	if (!plugin->comp_classes) {
		goto error;
	}

	/* Initialize plugin */
	if (init_plugin(plugin) < 0) {
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
	size_t count, i;
	struct bt_plugin **plugins = NULL;

	PRINT_PLUG_IN_SECTIONS(printf_verbose);
	count = SECTION_ELEMENT_COUNT(__bt_plugin_init_funcs);
	if (SECTION_ELEMENT_COUNT(__bt_plugin_exit_funcs) != count ||
			SECTION_ELEMENT_COUNT(__bt_plugin_names) != count ||
			SECTION_ELEMENT_COUNT(__bt_plugin_authors) != count ||
			SECTION_ELEMENT_COUNT(__bt_plugin_licenses) != count ||
			SECTION_ELEMENT_COUNT(__bt_plugin_descriptions) != count) {
		printf_error("Some statically-linked plug-ins do not define all the mandatory symbols\n");
		goto error;
	}

	printf_verbose("Detected %zu statically-linked plug-ins\n", count);
	plugins = g_new0(struct bt_plugin *, count + 1);
	if (!plugins) {
		goto error;
	}

	for (i = 0; i < count; i++) {
		struct bt_plugin *plugin =
			bt_plugin_create_from_static_at_index(i);

		if (!plugin) {
			printf_error("Cannot create statically-linked plug-in at index %zu\n",
				i);
			goto error;
		}

		/* Transfer ownership to the array */
		plugins[i] = plugin;
	}

	goto end;

error:
	g_free(plugins);

end:
	return plugins;
}

const char *bt_plugin_get_name(struct bt_plugin *plugin)
{
	return plugin ? plugin->shared_lib_handle->name : NULL;
}

const char *bt_plugin_get_author(struct bt_plugin *plugin)
{
	return plugin ? plugin->shared_lib_handle->author : NULL;
}

const char *bt_plugin_get_license(struct bt_plugin *plugin)
{
	return plugin ? plugin->shared_lib_handle->license : NULL;
}

const char *bt_plugin_get_path(struct bt_plugin *plugin)
{
	return (plugin && plugin->shared_lib_handle->path) ?
		plugin->shared_lib_handle->path->str : NULL;
}

const char *bt_plugin_get_description(struct bt_plugin *plugin)
{
	return plugin ? plugin->shared_lib_handle->description : NULL;
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
		enum bt_component_type type)
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
		enum bt_component_type comp_class_cand_type =
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

static
void plugin_comp_class_destroy_listener(struct bt_component_class *comp_class,
		void *data)
{
	gboolean exists = g_hash_table_remove(comp_classes_to_shlib_handles,
		comp_class);
	assert(exists);
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
			plugin->shared_lib_handle->name,
			bt_component_class_get_name(comp_class),
			bt_component_class_get_type(comp_class));
		goto error;
	}

	/* Add new component class */
	comp_class_index = plugin->comp_classes->len;
	g_ptr_array_add(plugin->comp_classes, bt_get(comp_class));

	/* Map component class pointer to shared lib handle in global HT */
	g_hash_table_insert(comp_classes_to_shlib_handles, comp_class,
		bt_get(plugin->shared_lib_handle));

	/* Add our custom destroy listener */
	ret = bt_component_class_add_destroy_listener(comp_class,
		plugin_comp_class_destroy_listener, NULL);
	if (ret) {
		goto error;
	}

	goto end;

error:
	/* Remove entry from global hash table (if exists) */
	g_hash_table_remove(comp_classes_to_shlib_handles,
		comp_class);

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
