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

#define NATIVE_PLUGIN_SUFFIX		".so"
#define NATIVE_PLUGIN_SUFFIX_LEN	sizeof(NATIVE_PLUGIN_SUFFIX)
#define LIBTOOL_PLUGIN_SUFFIX		".la"
#define LIBTOOL_PLUGIN_SUFFIX_LEN	sizeof(LIBTOOL_PLUGIN_SUFFIX)

#define PLUGIN_SUFFIX_LEN	max_t(size_t, sizeof(NATIVE_PLUGIN_SUFFIX), \
					sizeof(LIBTOOL_PLUGIN_SUFFIX))

#define SECTION_BEGIN(_name)		(&(__start_##_name))
#define SECTION_END(_name)		(&(__stop_##_name))
#define SECTION_ELEMENT_COUNT(_name) (SECTION_END(_name) - SECTION_BEGIN(_name))

#define DECLARE_SECTION(_type, _name)				\
	extern _type __start_##_name __attribute((weak));	\
	extern _type __stop_##_name __attribute((weak))

DECLARE_SECTION(struct __bt_plugin_descriptor const *, __bt_plugin_descriptors);
DECLARE_SECTION(struct __bt_plugin_descriptor_attribute const *, __bt_plugin_descriptor_attributes);
DECLARE_SECTION(struct __bt_plugin_component_class_descriptor const *, __bt_plugin_component_class_descriptors);
DECLARE_SECTION(struct __bt_plugin_component_class_descriptor_attribute const *, __bt_plugin_component_class_descriptor_attributes);

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
 *        my_plugins = bt_plugin_create_all_from_file("/path/to/my-plugin.so");
 *        // instantiate components from a plugin's component classes
 *        // put plugins and free my_plugins here
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
			const char *path = shared_lib_handle->path ?
				shared_lib_handle->path->str : "[built-in]";

			printf_verbose("Plugin in module `%s` exited with error %d\n",
				path, status);
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
struct bt_plugin *bt_plugin_create_empty(
		struct bt_plugin_shared_lib_handle *shared_lib_handle)
{
	struct bt_plugin *plugin = NULL;

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		goto error;
	}

	bt_object_init(plugin, bt_plugin_destroy);
	plugin->shared_lib_handle = bt_get(shared_lib_handle);

	/* Create empty array of component classes */
	plugin->comp_classes =
		g_ptr_array_new_with_free_func((GDestroyNotify) bt_put);
	if (!plugin->comp_classes) {
		goto error;
	}

	goto end;

error:
	BT_PUT(plugin);

end:
	return plugin;
}

static
enum bt_plugin_status bt_plugin_init(
		struct bt_plugin *plugin,
		const struct __bt_plugin_descriptor *descriptor,
		struct __bt_plugin_descriptor_attribute const * const *attrs_begin,
		struct __bt_plugin_descriptor_attribute const * const *attrs_end,
		struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_begin,
		struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_end,
		struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_begin,
		struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_end)
{
	/*
	 * This structure's members point to the plugin's memory
	 * (do NOT free).
	 */
	struct comp_class_full_descriptor {
		const struct __bt_plugin_component_class_descriptor *descriptor;
		const char *description;
	};

	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;
	struct __bt_plugin_descriptor_attribute const * const *cur_attr_ptr;
	struct __bt_plugin_component_class_descriptor const * const *cur_cc_descr_ptr;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cur_cc_descr_attr_ptr;
	GArray *comp_class_full_descriptors;
	size_t i;

	comp_class_full_descriptors = g_array_new(FALSE, TRUE,
		sizeof(struct comp_class_full_descriptor));
	if (!comp_class_full_descriptors) {
		status = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	/* Set mandatory attributes */
	plugin->descriptor = descriptor;
	plugin->name = descriptor->name;

	/*
	 * Find and set optional attributes attached to this plugin
	 * descriptor.
	 */
	for (cur_attr_ptr = attrs_begin; cur_attr_ptr != attrs_end; cur_attr_ptr++) {
		const struct __bt_plugin_descriptor_attribute *cur_attr =
			*cur_attr_ptr;

		if (cur_attr->plugin_descriptor != descriptor) {
			continue;
		}

		switch (cur_attr->type) {
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_INIT:
			plugin->init = cur_attr->value.init;
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT:
			plugin->shared_lib_handle->exit = cur_attr->value.exit;
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR:
			plugin->author = cur_attr->value.author;
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE:
			plugin->license = cur_attr->value.license;
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION:
			plugin->description = cur_attr->value.description;
			break;
		default:
			printf_verbose("WARNING: Unknown attribute \"%s\" (type %d) for plugin %s\n",
				cur_attr->type_name, cur_attr->type,
				descriptor->name);
			break;
		}
	}

	/*
	 * Find component class descriptors attached to this plugin
	 * descriptor and initialize corresponding full component class
	 * descriptors in the array.
	 */
	for (cur_cc_descr_ptr = cc_descriptors_begin; cur_cc_descr_ptr != cc_descriptors_end; cur_cc_descr_ptr++) {
		const struct __bt_plugin_component_class_descriptor *cur_cc_descr =
			*cur_cc_descr_ptr;
		struct comp_class_full_descriptor full_descriptor;

		if (cur_cc_descr->plugin_descriptor != descriptor) {
			continue;
		}

		full_descriptor.descriptor = cur_cc_descr;
		full_descriptor.description = NULL;
		g_array_append_val(comp_class_full_descriptors,
			full_descriptor);
	}

	/*
	 * Find component class descriptor attributes attached to this
	 * plugin descriptor and update corresponding full component
	 * class descriptors in the array.
	 */
	for (cur_cc_descr_attr_ptr = cc_descr_attrs_begin; cur_cc_descr_attr_ptr != cc_descr_attrs_end; cur_cc_descr_attr_ptr++) {
		const struct __bt_plugin_component_class_descriptor_attribute *cur_cc_descr_attr =
			*cur_cc_descr_attr_ptr;

		if (cur_cc_descr_attr->comp_class_descriptor->plugin_descriptor !=
				descriptor) {
			continue;
		}

		/* Find the corresponding component class descriptor entry */
		for (i = 0; i < comp_class_full_descriptors->len; i++) {
			struct comp_class_full_descriptor *cc_full_descr =
				&g_array_index(comp_class_full_descriptors,
					struct comp_class_full_descriptor, i);

			if (cur_cc_descr_attr->comp_class_descriptor ==
					cc_full_descr->descriptor) {
				switch (cur_cc_descr_attr->type) {
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION:
					cc_full_descr->description =
						cur_cc_descr_attr->value.description;
					break;
				default:
					printf_verbose("WARNING: Unknown attribute \"%s\" (type %d) for component class %s (type %d) in plugin %s\n",
						cur_cc_descr_attr->type_name,
						cur_cc_descr_attr->type,
						cur_cc_descr_attr->comp_class_descriptor->name,
						cur_cc_descr_attr->comp_class_descriptor->type,
						descriptor->name);
					break;
				}
			}
		}
	}

	/* Initialize plugin */
	if (plugin->init) {
		status = plugin->init(plugin);
		if (status < 0) {
			printf_verbose("Plugin `%s` initialization error: %d\n",
				plugin->name, status);
			goto end;
		}
	}

	plugin->shared_lib_handle->init_called = true;

	for (i = 0; i < comp_class_full_descriptors->len; i++) {
		struct comp_class_full_descriptor *cc_full_descr =
			&g_array_index(comp_class_full_descriptors,
				struct comp_class_full_descriptor, i);
		struct bt_component_class *comp_class;

		comp_class = bt_component_class_create(
			cc_full_descr->descriptor->type,
			cc_full_descr->descriptor->name,
			cc_full_descr->description,
			cc_full_descr->descriptor->init_cb);
		if (!comp_class) {
			status = BT_PLUGIN_STATUS_ERROR;
			goto end;
		}

		status = bt_plugin_add_component_class(plugin,
			comp_class);
		BT_PUT(comp_class);
		if (status < 0) {
			printf_verbose("Cannot add component class %s (type %d) to plugin `%s`: status = %d\n",
				cc_full_descr->descriptor->name,
				cc_full_descr->descriptor->type,
				plugin->name, status);
			goto end;
		}
	}

	/*
	 * All the plugin's component classes should be added at this
	 * point. We freeze the plugin so that it's not possible to add
	 * component classes to this plugin object after this stage
	 * (plugin object becomes immutable).
	 */
	plugin->frozen = true;

end:
	g_array_free(comp_class_full_descriptors, TRUE);
	return status;
}

static
struct bt_plugin **bt_plugin_create_all_from_sections(
		struct bt_plugin_shared_lib_handle *shared_lib_handle,
		struct __bt_plugin_descriptor const * const *descriptors_begin,
		struct __bt_plugin_descriptor const * const *descriptors_end,
		struct __bt_plugin_descriptor_attribute const * const *attrs_begin,
		struct __bt_plugin_descriptor_attribute const * const *attrs_end,
		struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_begin,
		struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_end,
		struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_begin,
		struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_end)
{
	size_t descriptor_count;
	size_t attrs_count;
	size_t cc_descriptors_count;
	size_t cc_descr_attrs_count;
	size_t i;
	struct bt_plugin **plugins = NULL;

	descriptor_count = descriptors_end - descriptors_begin;
	attrs_count = attrs_end - attrs_begin;
	cc_descriptors_count = cc_descriptors_end - cc_descriptors_begin;
	cc_descr_attrs_count = cc_descr_attrs_end - cc_descr_attrs_begin;
	printf_verbose("Section: Plugin descriptors: [%p - %p], (%zu elements)\n",
		descriptors_begin, descriptors_end, descriptor_count);
	printf_verbose("Section: Plugin descriptor attributes: [%p - %p], (%zu elements)\n",
		attrs_begin, attrs_end, attrs_count);
	printf_verbose("Section: Plugin component class descriptors: [%p - %p], (%zu elements)\n",
		cc_descriptors_begin, cc_descriptors_end, cc_descriptors_count);
	printf_verbose("Section: Plugin component class descriptor attributes: [%p - %p], (%zu elements)\n",
		cc_descr_attrs_begin, cc_descr_attrs_end, attrs_count);
	plugins = calloc(descriptor_count + 1, sizeof(*plugins));
	if (!plugins) {
		goto error;
	}

	for (i = 0; i < descriptor_count; i++) {
		enum bt_plugin_status status;
		const struct __bt_plugin_descriptor *descriptor =
			descriptors_begin[i];
		struct bt_plugin *plugin;

		printf_verbose("Loading plugin %s (%d.%d)\n", descriptor->name,
			descriptor->major, descriptor->minor);

		if (descriptor->major > __BT_PLUGIN_VERSION_MAJOR) {
			printf_error("Unknown plugin's major version: %d\n",
				descriptor->major);
			goto error;
		}

		plugin = bt_plugin_create_empty(shared_lib_handle);
		if (!plugin) {
			printf_error("Cannot allocate plugin object for plugin %s\n",
				descriptor->name);
			goto error;
		}

		status = bt_plugin_init(plugin, descriptor, attrs_begin,
			attrs_end, cc_descriptors_begin, cc_descriptors_end,
			cc_descr_attrs_begin, cc_descr_attrs_end);
		if (status < 0) {
			printf_error("Cannot initialize plugin object %s\n",
				descriptor->name);
			BT_PUT(plugin);
			goto error;
		}

		/* Transfer ownership to the array */
		plugins[i] = plugin;
	}

	goto end;

error:
	g_free(plugins);
	plugins = NULL;

end:
	return plugins;
}

struct bt_plugin **bt_plugin_create_all_from_static(void)
{
	struct bt_plugin **plugins = NULL;
	struct bt_plugin_shared_lib_handle *shared_lib_handle =
		bt_plugin_shared_lib_handle_create(NULL);

	if (!shared_lib_handle) {
		goto end;
	}

	plugins = bt_plugin_create_all_from_sections(shared_lib_handle,
		SECTION_BEGIN(__bt_plugin_descriptors),
		SECTION_END(__bt_plugin_descriptors),
		SECTION_BEGIN(__bt_plugin_descriptor_attributes),
		SECTION_END(__bt_plugin_descriptor_attributes),
		SECTION_BEGIN(__bt_plugin_component_class_descriptors),
		SECTION_END(__bt_plugin_component_class_descriptors),
		SECTION_BEGIN(__bt_plugin_component_class_descriptor_attributes),
		SECTION_END(__bt_plugin_component_class_descriptor_attributes));

end:
	BT_PUT(shared_lib_handle);

	return plugins;
}

struct bt_plugin **bt_plugin_create_all_from_file(const char *path)
{
	size_t path_len;
	struct bt_plugin **plugins = NULL;
	struct __bt_plugin_descriptor const * const *descriptors_begin = NULL;
	struct __bt_plugin_descriptor const * const *descriptors_end = NULL;
	struct __bt_plugin_descriptor_attribute const * const *attrs_begin = NULL;
	struct __bt_plugin_descriptor_attribute const * const *attrs_end = NULL;
	struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_begin = NULL;
	struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_end = NULL;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_begin = NULL;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_end = NULL;
	bool is_libtool_wrapper = false, is_shared_object = false;
	struct bt_plugin_shared_lib_handle *shared_lib_handle = NULL;

	if (!path) {
		goto end;
	}

	path_len = strlen(path);
	if (path_len <= PLUGIN_SUFFIX_LEN) {
		goto end;
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
		goto end;
	}

	shared_lib_handle = bt_plugin_shared_lib_handle_create(path);
	if (!shared_lib_handle) {
		goto end;
	}

	if (!g_module_symbol(shared_lib_handle->module, "__start___bt_plugin_descriptors",
			(gpointer *) &descriptors_begin)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__start___bt_plugin_descriptors",
			g_module_name(shared_lib_handle->module));
		goto end;
	}

	if (!g_module_symbol(shared_lib_handle->module, "__stop___bt_plugin_descriptors",
			(gpointer *) &descriptors_end)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__stop___bt_plugin_descriptors",
			g_module_name(shared_lib_handle->module));
		goto end;
	}

	if (!g_module_symbol(shared_lib_handle->module, "__start___bt_plugin_descriptor_attributes",
			(gpointer *) &attrs_begin)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__start___bt_plugin_descriptor_attributes",
			g_module_name(shared_lib_handle->module));
	}

	if (!g_module_symbol(shared_lib_handle->module, "__stop___bt_plugin_descriptor_attributes",
			(gpointer *) &attrs_end)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__stop___bt_plugin_descriptor_attributes",
			g_module_name(shared_lib_handle->module));
	}

	if ((!!attrs_begin - !!attrs_end) != 0) {
		printf_verbose("Found __start___bt_plugin_descriptor_attributes or __stop___bt_plugin_descriptor_attributes symbol, but not both in %s\n",
			g_module_name(shared_lib_handle->module));
		goto end;
	}

	if (!g_module_symbol(shared_lib_handle->module, "__start___bt_plugin_component_class_descriptors",
			(gpointer *) &cc_descriptors_begin)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__start___bt_plugin_component_class_descriptors",
			g_module_name(shared_lib_handle->module));
	}

	if (!g_module_symbol(shared_lib_handle->module, "__stop___bt_plugin_component_class_descriptors",
			(gpointer *) &cc_descriptors_end)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__stop___bt_plugin_component_class_descriptors",
			g_module_name(shared_lib_handle->module));
	}

	if ((!!cc_descriptors_begin - !!cc_descriptors_end) != 0) {
		printf_verbose("Found __start___bt_plugin_component_class_descriptors or __stop___bt_plugin_component_class_descriptors symbol, but not both in %s\n",
			g_module_name(shared_lib_handle->module));
		goto end;
	}

	if (!g_module_symbol(shared_lib_handle->module, "__start___bt_plugin_component_class_descriptor_attributes",
			(gpointer *) &cc_descr_attrs_begin)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__start___bt_plugin_component_class_descriptor_attributes",
			g_module_name(shared_lib_handle->module));
	}

	if (!g_module_symbol(shared_lib_handle->module, "__stop___bt_plugin_component_class_descriptor_attributes",
			(gpointer *) &cc_descr_attrs_end)) {
		printf_verbose("Unable to resolve plugin symbol %s from %s\n",
			"__stop___bt_plugin_component_class_descriptor_attributes",
			g_module_name(shared_lib_handle->module));
	}

	if ((!!cc_descr_attrs_begin - !!cc_descr_attrs_end) != 0) {
		printf_verbose("Found __start___bt_plugin_component_class_descriptor_attributes or __stop___bt_plugin_component_class_descriptor_attributes symbol, but not both in %s\n",
			g_module_name(shared_lib_handle->module));
		goto end;
	}

	/* Initialize plugin */
	plugins = bt_plugin_create_all_from_sections(shared_lib_handle,
		descriptors_begin, descriptors_end, attrs_begin, attrs_end,
		cc_descriptors_begin, cc_descriptors_end,
		cc_descr_attrs_begin, cc_descr_attrs_end);

end:
	BT_PUT(shared_lib_handle);
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
	return (plugin && plugin->shared_lib_handle->path) ?
		plugin->shared_lib_handle->path->str : NULL;
}

const char *bt_plugin_get_description(struct bt_plugin *plugin)
{
	return plugin ? plugin->description : NULL;
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
			plugin->name,
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
