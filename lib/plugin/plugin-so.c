/*
 * plugin-so.c
 *
 * Babeltrace Plugin (shared object)
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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/plugin-so-internal.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <gmodule.h>

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
void bt_plugin_so_shared_lib_handle_destroy(struct bt_object *obj)
{
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle;

	assert(obj);
	shared_lib_handle = container_of(obj,
		struct bt_plugin_so_shared_lib_handle, base);

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
struct bt_plugin_so_shared_lib_handle *bt_plugin_so_shared_lib_handle_create(
		const char *path)
{
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle = NULL;

	shared_lib_handle = g_new0(struct bt_plugin_so_shared_lib_handle, 1);
	if (!shared_lib_handle) {
		goto error;
	}

	bt_object_init(shared_lib_handle, bt_plugin_so_shared_lib_handle_destroy);

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
void bt_plugin_so_destroy_spec_data(struct bt_plugin *plugin)
{
	struct bt_plugin_so_spec_data *spec = plugin->spec_data;

	if (!plugin->spec_data) {
		return;
	}

	assert(plugin->type == BT_PLUGIN_TYPE_SO);
	assert(spec);
	BT_PUT(spec->shared_lib_handle);
	g_free(plugin->spec_data);
	plugin->spec_data = NULL;
}

/*
 * This function does the following:
 *
 * 1. Iterate on the plugin descriptor attributes section and set the
 *    plugin's attributes depending on the attribute types. This
 *    includes the name of the plugin, its description, and its
 *    initialization function, for example.
 *
 * 2. Iterate on the component class descriptors section and create one
 *    "full descriptor" (temporary structure) for each one that is found
 *    and attached to our plugin descriptor.
 *
 * 3. Iterate on the component class descriptor attributes section and
 *    set the corresponding full descriptor's attributes depending on
 *    the attribute types. This includes the description of the
 *    component class, as well as its initialization and destroy
 *    methods.
 *
 * 4. Call the user's plugin initialization function, if any is
 *    defined.
 *
 * 5. For each full component class descriptor, create a component class
 *    object, set its optional attributes, and add it to the plugin
 *    object.
 *
 * 6. Freeze the plugin object.
 */
static
enum bt_plugin_status bt_plugin_so_init(
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
		const char *help;
		bt_component_class_init_method init_method;
		bt_component_class_finalize_method finalize_method;
		bt_component_class_query_method query_method;
		bt_component_class_accept_port_connection_method accept_port_connection_method;
		bt_component_class_port_connected_method port_connected_method;
		bt_component_class_port_disconnected_method port_disconnected_method;
		struct bt_component_class_iterator_methods iterator_methods;
	};

	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;
	struct __bt_plugin_descriptor_attribute const * const *cur_attr_ptr;
	struct __bt_plugin_component_class_descriptor const * const *cur_cc_descr_ptr;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cur_cc_descr_attr_ptr;
	struct bt_plugin_so_spec_data *spec = plugin->spec_data;
	GArray *comp_class_full_descriptors;
	size_t i;
	int ret;

	comp_class_full_descriptors = g_array_new(FALSE, TRUE,
		sizeof(struct comp_class_full_descriptor));
	if (!comp_class_full_descriptors) {
		status = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	/* Set mandatory attributes */
	spec->descriptor = descriptor;
	bt_plugin_set_name(plugin, descriptor->name);

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
			spec->init = cur_attr->value.init;
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT:
			spec->shared_lib_handle->exit = cur_attr->value.exit;
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR:
			bt_plugin_set_author(plugin, cur_attr->value.author);
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE:
			bt_plugin_set_license(plugin, cur_attr->value.license);
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION:
			bt_plugin_set_description(plugin, cur_attr->value.description);
			break;
		case BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION:
			bt_plugin_set_version(plugin,
				(unsigned int) cur_attr->value.version.major,
				(unsigned int) cur_attr->value.version.minor,
				(unsigned int) cur_attr->value.version.patch,
				cur_attr->value.version.extra);
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
		struct comp_class_full_descriptor full_descriptor = {0};

		if (cur_cc_descr->plugin_descriptor != descriptor) {
			continue;
		}

		full_descriptor.descriptor = cur_cc_descr;
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
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP:
					cc_full_descr->help =
						cur_cc_descr_attr->value.help;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INIT_METHOD:
					cc_full_descr->init_method =
						cur_cc_descr_attr->value.init_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD:
					cc_full_descr->finalize_method =
						cur_cc_descr_attr->value.finalize_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD:
					cc_full_descr->query_method =
						cur_cc_descr_attr->value.query_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_PORT_CONNECTION_METHOD:
					cc_full_descr->accept_port_connection_method =
						cur_cc_descr_attr->value.accept_port_connection_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_PORT_CONNECTED_METHOD:
					cc_full_descr->port_connected_method =
						cur_cc_descr_attr->value.port_connected_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_PORT_DISCONNECTED_METHOD:
					cc_full_descr->port_disconnected_method =
						cur_cc_descr_attr->value.port_disconnected_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_NOTIF_ITER_INIT_METHOD:
					cc_full_descr->iterator_methods.init =
						cur_cc_descr_attr->value.notif_iter_init_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_NOTIF_ITER_FINALIZE_METHOD:
					cc_full_descr->iterator_methods.finalize =
						cur_cc_descr_attr->value.notif_iter_finalize_method;
					break;
				case BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_NOTIF_ITER_SEEK_TIME_METHOD:
					cc_full_descr->iterator_methods.seek_time =
						cur_cc_descr_attr->value.notif_iter_seek_time_method;
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
	if (spec->init) {
		status = spec->init(plugin);
		if (status < 0) {
			printf_verbose("Plugin `%s` initialization error: %d\n",
				bt_plugin_get_name(plugin), status);
			goto end;
		}
	}

	spec->shared_lib_handle->init_called = true;

	/* Add described component classes to plugin */
	for (i = 0; i < comp_class_full_descriptors->len; i++) {
		struct comp_class_full_descriptor *cc_full_descr =
			&g_array_index(comp_class_full_descriptors,
				struct comp_class_full_descriptor, i);
		struct bt_component_class *comp_class;

		switch (cc_full_descr->descriptor->type) {
		case BT_COMPONENT_CLASS_TYPE_SOURCE:
			comp_class = bt_component_class_source_create(
				cc_full_descr->descriptor->name,
				cc_full_descr->descriptor->methods.source.notif_iter_next);
			break;
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			comp_class = bt_component_class_filter_create(
				cc_full_descr->descriptor->name,
				cc_full_descr->descriptor->methods.source.notif_iter_next);
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			comp_class = bt_component_class_sink_create(
				cc_full_descr->descriptor->name,
				cc_full_descr->descriptor->methods.sink.consume);
			break;
		default:
			printf_verbose("WARNING: Unknown component class type %d for component class %s in plugin %s\n",
				cc_full_descr->descriptor->type,
				cc_full_descr->descriptor->name,
				descriptor->name);
			continue;
		}

		if (!comp_class) {
			status = BT_PLUGIN_STATUS_ERROR;
			goto end;
		}

		if (cc_full_descr->description) {
			ret = bt_component_class_set_description(comp_class,
				cc_full_descr->description);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->help) {
			ret = bt_component_class_set_help(comp_class,
				cc_full_descr->help);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->init_method) {
			ret = bt_component_class_set_init_method(comp_class,
				cc_full_descr->init_method);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->finalize_method) {
			ret = bt_component_class_set_finalize_method(comp_class,
				cc_full_descr->finalize_method);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->query_method) {
			ret = bt_component_class_set_query_method(
				comp_class, cc_full_descr->query_method);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->accept_port_connection_method) {
			ret = bt_component_class_set_accept_port_connection_method(
				comp_class, cc_full_descr->accept_port_connection_method);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->port_connected_method) {
			ret = bt_component_class_set_port_connected_method(
				comp_class, cc_full_descr->port_connected_method);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->port_disconnected_method) {
			ret = bt_component_class_set_port_disconnected_method(
				comp_class, cc_full_descr->port_disconnected_method);
			if (ret) {
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		switch (cc_full_descr->descriptor->type) {
		case BT_COMPONENT_CLASS_TYPE_SOURCE:
			if (cc_full_descr->iterator_methods.init) {
				ret = bt_component_class_source_set_notification_iterator_init_method(
					comp_class,
					cc_full_descr->iterator_methods.init);
				if (ret) {
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}

			if (cc_full_descr->iterator_methods.finalize) {
				ret = bt_component_class_source_set_notification_iterator_finalize_method(
					comp_class,
					cc_full_descr->iterator_methods.finalize);
				if (ret) {
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}

			if (cc_full_descr->iterator_methods.seek_time) {
				ret = bt_component_class_source_set_notification_iterator_seek_time_method(
					comp_class,
					cc_full_descr->iterator_methods.seek_time);
				if (ret) {
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}
			break;
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			if (cc_full_descr->iterator_methods.init) {
				ret = bt_component_class_filter_set_notification_iterator_init_method(
					comp_class,
					cc_full_descr->iterator_methods.init);
				if (ret) {
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}

			if (cc_full_descr->iterator_methods.finalize) {
				ret = bt_component_class_filter_set_notification_iterator_finalize_method(
					comp_class,
					cc_full_descr->iterator_methods.finalize);
				if (ret) {
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}

			if (cc_full_descr->iterator_methods.seek_time) {
				ret = bt_component_class_filter_set_notification_iterator_seek_time_method(
					comp_class,
					cc_full_descr->iterator_methods.seek_time);
				if (ret) {
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			break;
		default:
			assert(false);
			break;
		}

		/*
		 * Add component class to the plugin object.
		 *
		 * This will call back
		 * bt_plugin_so_on_add_component_class() so that we can
		 * add a mapping in comp_classes_to_shlib_handles when
		 * we know the component class is successfully added.
		 */
		status = bt_plugin_add_component_class(plugin,
			comp_class);
		BT_PUT(comp_class);
		if (status < 0) {
			printf_verbose("Cannot add component class %s (type %d) to plugin `%s`: status = %d\n",
				cc_full_descr->descriptor->name,
				cc_full_descr->descriptor->type,
				bt_plugin_get_name(plugin), status);
			goto end;
		}
	}

	/*
	 * All the plugin's component classes should be added at this
	 * point. We freeze the plugin so that it's not possible to add
	 * component classes to this plugin object after this stage
	 * (plugin object becomes immutable).
	 */
	bt_plugin_freeze(plugin);

end:
	g_array_free(comp_class_full_descriptors, TRUE);
	return status;
}

static
struct bt_plugin *bt_plugin_so_create_empty(
		struct bt_plugin_so_shared_lib_handle *shared_lib_handle)
{
	struct bt_plugin *plugin;
	struct bt_plugin_so_spec_data *spec;

	plugin = bt_plugin_create_empty(BT_PLUGIN_TYPE_SO);
	if (!plugin) {
		goto error;
	}

	plugin->destroy_spec_data = bt_plugin_so_destroy_spec_data;
	plugin->spec_data = g_new0(struct bt_plugin_so_spec_data, 1);
	if (!plugin->spec_data) {
		goto error;
	}

	spec = plugin->spec_data;
	spec->shared_lib_handle = bt_get(shared_lib_handle);
	goto end;

error:
	BT_PUT(plugin);

end:
	return plugin;
}

static
struct bt_plugin_set *bt_plugin_so_create_all_from_sections(
		struct bt_plugin_so_shared_lib_handle *shared_lib_handle,
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
	struct bt_plugin_set *plugin_set = NULL;

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
		cc_descr_attrs_begin, cc_descr_attrs_end, cc_descr_attrs_count);
	plugin_set = bt_plugin_set_create();
	if (!plugin_set) {
		goto error;
	}

	for (i = 0; i < descriptor_count; i++) {
		enum bt_plugin_status status;
		const struct __bt_plugin_descriptor *descriptor =
			descriptors_begin[i];
		struct bt_plugin *plugin;

		printf_verbose("Loading plugin %s (ABI %d.%d)\n", descriptor->name,
			descriptor->major, descriptor->minor);

		if (descriptor->major > __BT_PLUGIN_VERSION_MAJOR) {
			printf_error("Unknown plugin's major version: %d\n",
				descriptor->major);
			goto error;
		}

		plugin = bt_plugin_so_create_empty(shared_lib_handle);
		if (!plugin) {
			printf_error("Cannot allocate plugin object for plugin %s\n",
				descriptor->name);
			goto error;
		}

		if (shared_lib_handle && shared_lib_handle->path) {
			bt_plugin_set_path(plugin, shared_lib_handle->path->str);
		}

		status = bt_plugin_so_init(plugin, descriptor, attrs_begin,
			attrs_end, cc_descriptors_begin, cc_descriptors_end,
			cc_descr_attrs_begin, cc_descr_attrs_end);
		if (status < 0) {
			printf_error("Cannot initialize plugin object %s\n",
				descriptor->name);
			BT_PUT(plugin);
			goto error;
		}

		/* Add to plugin set */
		bt_plugin_set_add_plugin(plugin_set, plugin);
		bt_put(plugin);
	}

	goto end;

error:
	BT_PUT(plugin_set);

end:
	return plugin_set;
}

BT_HIDDEN
struct bt_plugin_set *bt_plugin_so_create_all_from_static(void)
{
	struct bt_plugin_set *plugin_set = NULL;
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle =
		bt_plugin_so_shared_lib_handle_create(NULL);

	if (!shared_lib_handle) {
		goto end;
	}

	plugin_set = bt_plugin_so_create_all_from_sections(shared_lib_handle,
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

	return plugin_set;
}

BT_HIDDEN
struct bt_plugin_set *bt_plugin_so_create_all_from_file(const char *path)
{
	size_t path_len;
	struct bt_plugin_set *plugin_set = NULL;
	struct __bt_plugin_descriptor const * const *descriptors_begin = NULL;
	struct __bt_plugin_descriptor const * const *descriptors_end = NULL;
	struct __bt_plugin_descriptor_attribute const * const *attrs_begin = NULL;
	struct __bt_plugin_descriptor_attribute const * const *attrs_end = NULL;
	struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_begin = NULL;
	struct __bt_plugin_component_class_descriptor const * const *cc_descriptors_end = NULL;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_begin = NULL;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cc_descr_attrs_end = NULL;
	bool is_libtool_wrapper = false, is_shared_object = false;
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle = NULL;

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

	shared_lib_handle = bt_plugin_so_shared_lib_handle_create(path);
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
	plugin_set = bt_plugin_so_create_all_from_sections(shared_lib_handle,
		descriptors_begin, descriptors_end, attrs_begin, attrs_end,
		cc_descriptors_begin, cc_descriptors_end,
		cc_descr_attrs_begin, cc_descr_attrs_end);

end:
	BT_PUT(shared_lib_handle);
	return plugin_set;
}

static
void plugin_comp_class_destroy_listener(struct bt_component_class *comp_class,
		void *data)
{
	gboolean exists = g_hash_table_remove(comp_classes_to_shlib_handles,
		comp_class);
	assert(exists);
}

BT_HIDDEN
void bt_plugin_so_on_add_component_class(struct bt_plugin *plugin,
		struct bt_component_class *comp_class)
{
	struct bt_plugin_so_spec_data *spec = plugin->spec_data;

	assert(plugin->spec_data);
	assert(plugin->type == BT_PLUGIN_TYPE_SO);

	/* Map component class pointer to shared lib handle in global HT */
	g_hash_table_insert(comp_classes_to_shlib_handles, comp_class,
		bt_get(spec->shared_lib_handle));

	/* Add our custom destroy listener */
	bt_component_class_add_destroy_listener(comp_class,
		plugin_comp_class_destroy_listener, NULL);
}
