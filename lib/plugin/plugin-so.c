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

#define BT_LOG_TAG "PLUGIN-SO"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/plugin-so-internal.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/list-internal.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>

#define NATIVE_PLUGIN_SUFFIX		"." G_MODULE_SUFFIX
#define NATIVE_PLUGIN_SUFFIX_LEN	sizeof(NATIVE_PLUGIN_SUFFIX)
#define LIBTOOL_PLUGIN_SUFFIX		".la"
#define LIBTOOL_PLUGIN_SUFFIX_LEN	sizeof(LIBTOOL_PLUGIN_SUFFIX)

#define PLUGIN_SUFFIX_LEN	max_t(size_t, sizeof(NATIVE_PLUGIN_SUFFIX), \
					sizeof(LIBTOOL_PLUGIN_SUFFIX))

BT_PLUGIN_MODULE();

/*
 * This list, global to the library, keeps all component classes that
 * have a reference to their shared library handles. It allows iteration
 * on all component classes still present when the destructor executes
 * to release the shared library handle references they might still have.
 *
 * The list items are the component classes created with
 * bt_plugin_add_component_class(). They keep the shared library handle
 * object created by their plugin alive so that the plugin's code is
 * not discarded when it could still be in use by living components
 * created from those component classes:
 *
 *     [component] --ref-> [component class]-> [shlib handle]
 *
 * It allows this use-case:
 *
 *        my_plugins = bt_plugin_create_all_from_file("/path/to/my-plugin.so");
 *        // instantiate components from a plugin's component classes
 *        // put plugins and free my_plugins here
 *        // user code of instantiated components still exists
 *
 * An entry is removed from this list when a component class is
 * destroyed thanks to a custom destroy listener. When the entry is
 * removed, the entry is removed from the list, and we release the
 * reference on the shlib handle. Assuming the original plugin object
 * which contained some component classes is put first, when the last
 * component class is removed from this list, the shared library handle
 * object's reference count falls to zero and the shared library is
 * finally closed.
 */

static
BT_LIST_HEAD(component_class_list);

__attribute__((destructor)) static
void fini_comp_class_list(void)
{
	struct bt_component_class *comp_class, *tmp;

	bt_list_for_each_entry_safe(comp_class, tmp, &component_class_list, node) {
		bt_list_del(&comp_class->node);
		BT_PUT(comp_class->so_handle);
	}
	BT_LOGD_STR("Released references from all component classes to shared library handles.");
}

static
void bt_plugin_so_shared_lib_handle_destroy(struct bt_object *obj)
{
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle;

	assert(obj);
	shared_lib_handle = container_of(obj,
		struct bt_plugin_so_shared_lib_handle, base);
	const char *path = shared_lib_handle->path ?
		shared_lib_handle->path->str : NULL;

	BT_LOGD("Destroying shared library handle: addr=%p, path=\"%s\"",
		shared_lib_handle, path);

	if (shared_lib_handle->init_called && shared_lib_handle->exit) {
		enum bt_plugin_status status;

		BT_LOGD_STR("Calling user's plugin exit function.");
		status = shared_lib_handle->exit();
		BT_LOGD("User function returned: %s",
			bt_plugin_status_string(status));

		if (status < 0) {
			BT_LOGW("User's plugin exit function failed: "
				"path=\"%s\"", path);
		}
	}

	if (shared_lib_handle->module) {
#ifndef NDEBUG
		/*
		 * Valgrind shows incomplete stack traces when
		 * dynamically loaded libraries are closed before it
		 * finishes. Use the BABELTRACE_NO_DLCLOSE in a debug
		 * build to avoid this.
		 */
		const char *var = getenv("BABELTRACE_NO_DLCLOSE");

		if (!var || strcmp(var, "1") != 0) {
#endif
			BT_LOGD("Closing GModule: path=\"%s\"", path);

			if (!g_module_close(shared_lib_handle->module)) {
				BT_LOGE("Cannot close GModule: %s: path=\"%s\"",
					g_module_error(), path);
			}
#ifndef NDEBUG
		} else {
			BT_LOGD("Not closing GModule because `BABELTRACE_NO_DLCLOSE=1`: "
				"path=\"%s\"", path);
		}
#endif
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

	BT_LOGD("Creating shared library handle: path=\"%s\"", path);
	shared_lib_handle = g_new0(struct bt_plugin_so_shared_lib_handle, 1);
	if (!shared_lib_handle) {
		BT_LOGE_STR("Failed to allocate one shared library handle.");
		goto error;
	}

	bt_object_init(shared_lib_handle, bt_plugin_so_shared_lib_handle_destroy);

	if (!path) {
		goto end;
	}

	shared_lib_handle->path = g_string_new(path);
	if (!shared_lib_handle->path) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	shared_lib_handle->module = g_module_open(path, G_MODULE_BIND_LOCAL);
	if (!shared_lib_handle->module) {
		/*
		 * DEBUG-level logging because we're only _trying_ to
		 * open this file as a Babeltrace plugin: if it's not,
		 * it's not an error. And because this can be tried
		 * during bt_plugin_create_all_from_dir(), it's not even
		 * a warning.
		 */
		BT_LOGD("Cannot open GModule: %s: path=\"%s\"",
			g_module_error(), path);
		goto error;
	}

	goto end;

error:
	BT_PUT(shared_lib_handle);

end:
	if (shared_lib_handle) {
		BT_LOGD("Created shared library handle: path=\"%s\", addr=%p",
			path, shared_lib_handle);
	}

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
		struct bt_component_class_notification_iterator_methods iterator_methods;
	};

	enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;
	struct __bt_plugin_descriptor_attribute const * const *cur_attr_ptr;
	struct __bt_plugin_component_class_descriptor const * const *cur_cc_descr_ptr;
	struct __bt_plugin_component_class_descriptor_attribute const * const *cur_cc_descr_attr_ptr;
	struct bt_plugin_so_spec_data *spec = plugin->spec_data;
	GArray *comp_class_full_descriptors;
	size_t i;
	int ret;

	BT_LOGD("Initializing plugin object from descriptors found in sections: "
		"plugin-addr=%p, plugin-path=\"%s\", "
		"attrs-begin-addr=%p, attrs-end-addr=%p, "
		"cc-descr-begin-addr=%p, cc-descr-end-addr=%p, "
		"cc-descr-attrs-begin-addr=%p, cc-descr-attrs-end-addr=%p",
		plugin,
		spec->shared_lib_handle->path ?
			spec->shared_lib_handle->path->str : NULL,
		attrs_begin, attrs_end,
		cc_descriptors_begin, cc_descriptors_end,
		cc_descr_attrs_begin, cc_descr_attrs_end);
	comp_class_full_descriptors = g_array_new(FALSE, TRUE,
		sizeof(struct comp_class_full_descriptor));
	if (!comp_class_full_descriptors) {
		BT_LOGE_STR("Failed to allocate a GArray.");
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

		if (cur_attr == NULL) {
			continue;
		}

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
			/*
			 * WARN-level logging because this should not
			 * happen with the appropriate ABI version. If
			 * we're here, we know that for the reported
			 * version of the ABI, this attribute is
			 * unknown.
			 */
			BT_LOGW("Ignoring unknown plugin descriptor attribute: "
				"plugin-path=\"%s\", plugin-name=\"%s\", "
				"attr-type-name=\"%s\", attr-type-id=%d",
				spec->shared_lib_handle->path ?
					spec->shared_lib_handle->path->str :
					NULL,
				descriptor->name, cur_attr->type_name,
				cur_attr->type);
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

		if (cur_cc_descr == NULL) {
			continue;
		}

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

		if (cur_cc_descr_attr == NULL) {
			continue;
		}

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
				default:
					/*
					 * WARN-level logging because
					 * this should not happen with
					 * the appropriate ABI version.
					 * If we're here, we know that
					 * for the reported version of
					 * the ABI, this attribute is
					 * unknown.
					 */
					BT_LOGW("Ignoring unknown component class descriptor attribute: "
						"plugin-path=\"%s\", "
						"plugin-name=\"%s\", "
						"comp-class-name=\"%s\", "
						"comp-class-type=%s, "
						"attr-type-name=\"%s\", "
						"attr-type-id=%d",
						spec->shared_lib_handle->path ?
							spec->shared_lib_handle->path->str :
							NULL,
						descriptor->name,
						cur_cc_descr_attr->comp_class_descriptor->name,
						bt_component_class_type_string(
							cur_cc_descr_attr->comp_class_descriptor->type),
						cur_cc_descr_attr->type_name,
						cur_cc_descr_attr->type);
					break;
				}
			}
		}
	}

	/* Initialize plugin */
	if (spec->init) {
		BT_LOGD_STR("Calling user's plugin initialization function.");
		status = spec->init(plugin);
		BT_LOGD("User function returned: %s",
			bt_plugin_status_string(status));

		if (status < 0) {
			BT_LOGW_STR("User's plugin initialization function failed.");
			goto end;
		}
	}

	spec->shared_lib_handle->init_called = BT_TRUE;

	/* Add described component classes to plugin */
	for (i = 0; i < comp_class_full_descriptors->len; i++) {
		struct comp_class_full_descriptor *cc_full_descr =
			&g_array_index(comp_class_full_descriptors,
				struct comp_class_full_descriptor, i);
		struct bt_component_class *comp_class;

		BT_LOGD("Creating and setting properties of plugin's component class: "
			"plugin-path=\"%s\", plugin-name=\"%s\", "
			"comp-class-name=\"%s\", comp-class-type=%s",
			spec->shared_lib_handle->path ?
				spec->shared_lib_handle->path->str :
				NULL,
			descriptor->name,
			cc_full_descr->descriptor->name,
			bt_component_class_type_string(
				cc_full_descr->descriptor->type));

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
			/*
			 * WARN-level logging because this should not
			 * happen with the appropriate ABI version. If
			 * we're here, we know that for the reported
			 * version of the ABI, this component class type
			 * is unknown.
			 */
			BT_LOGW("Ignoring unknown component class type: "
				"plugin-path=\"%s\", plugin-name=\"%s\", "
				"comp-class-name=\"%s\", comp-class-type=%d",
				spec->shared_lib_handle->path->str ?
					spec->shared_lib_handle->path->str :
					NULL,
				descriptor->name,
				cc_full_descr->descriptor->name,
				cc_full_descr->descriptor->type);
			continue;
		}

		if (!comp_class) {
			BT_LOGE_STR("Cannot create component class.");
			status = BT_PLUGIN_STATUS_ERROR;
			goto end;
		}

		if (cc_full_descr->description) {
			ret = bt_component_class_set_description(comp_class,
				cc_full_descr->description);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's description.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->help) {
			ret = bt_component_class_set_help(comp_class,
				cc_full_descr->help);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's help string.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->init_method) {
			ret = bt_component_class_set_init_method(comp_class,
				cc_full_descr->init_method);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's initialization method.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->finalize_method) {
			ret = bt_component_class_set_finalize_method(comp_class,
				cc_full_descr->finalize_method);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's finalization method.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->query_method) {
			ret = bt_component_class_set_query_method(
				comp_class, cc_full_descr->query_method);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's query method.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->accept_port_connection_method) {
			ret = bt_component_class_set_accept_port_connection_method(
				comp_class, cc_full_descr->accept_port_connection_method);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's \"accept port connection\" method.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->port_connected_method) {
			ret = bt_component_class_set_port_connected_method(
				comp_class, cc_full_descr->port_connected_method);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's \"port connected\" method.");
				status = BT_PLUGIN_STATUS_ERROR;
				BT_PUT(comp_class);
				goto end;
			}
		}

		if (cc_full_descr->port_disconnected_method) {
			ret = bt_component_class_set_port_disconnected_method(
				comp_class, cc_full_descr->port_disconnected_method);
			if (ret) {
				BT_LOGE_STR("Cannot set component class's \"port disconnected\" method.");
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
					BT_LOGE_STR("Cannot set component class's notification iterator initialization method.");
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
					BT_LOGE_STR("Cannot set source component class's notification iterator finalization method.");
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
					BT_LOGE_STR("Cannot set filter component class's notification iterator initialization method.");
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
					BT_LOGE_STR("Cannot set filter component class's notification iterator finalization method.");
					status = BT_PLUGIN_STATUS_ERROR;
					BT_PUT(comp_class);
					goto end;
				}
			}
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			break;
		default:
			abort();
		}

		/*
		 * Add component class to the plugin object.
		 *
		 * This will call back
		 * bt_plugin_so_on_add_component_class() so that we can
		 * add a mapping in the component class list when
		 * we know the component class is successfully added.
		 */
		status = bt_plugin_add_component_class(plugin,
			comp_class);
		BT_PUT(comp_class);
		if (status < 0) {
			BT_LOGE("Cannot add component class to plugin.");
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
		BT_LOGE_STR("Failed to allocate one SO plugin specific data structure.");
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
size_t count_non_null_items_in_section(const void *begin, const void *end)
{
	size_t count = 0;
	const int * const *begin_int = (const int * const *) begin;
	const int * const *end_int = (const int * const *) end;
	const int * const *iter;

	for (iter = begin_int; iter != end_int; iter++) {
		if (*iter) {
			count++;
		}
	}

	return count;
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

	descriptor_count = count_non_null_items_in_section(descriptors_begin, descriptors_end);
	attrs_count = count_non_null_items_in_section(attrs_begin, attrs_end);
	cc_descriptors_count = count_non_null_items_in_section(cc_descriptors_begin, cc_descriptors_end);
	cc_descr_attrs_count =  count_non_null_items_in_section(cc_descr_attrs_begin, cc_descr_attrs_end);

	BT_LOGD("Creating all SO plugins from sections: "
		"plugin-path=\"%s\", "
		"descr-begin-addr=%p, descr-end-addr=%p, "
		"attrs-begin-addr=%p, attrs-end-addr=%p, "
		"cc-descr-begin-addr=%p, cc-descr-end-addr=%p, "
		"cc-descr-attrs-begin-addr=%p, cc-descr-attrs-end-addr=%p, "
		"descr-count=%zu, attrs-count=%zu, "
		"cc-descr-count=%zu, cc-descr-attrs-count=%zu",
		shared_lib_handle->path ? shared_lib_handle->path->str : NULL,
		descriptors_begin, descriptors_end,
		attrs_begin, attrs_end,
		cc_descriptors_begin, cc_descriptors_end,
		cc_descr_attrs_begin, cc_descr_attrs_end,
		descriptor_count, attrs_count,
		cc_descriptors_count, cc_descr_attrs_count);
	plugin_set = bt_plugin_set_create();
	if (!plugin_set) {
		BT_LOGE_STR("Cannot create empty plugin set.");
		goto error;
	}

	for (i = 0; i < descriptors_end - descriptors_begin; i++) {
		enum bt_plugin_status status;
		const struct __bt_plugin_descriptor *descriptor =
			descriptors_begin[i];
		struct bt_plugin *plugin;

		if (descriptor == NULL) {
			continue;
		}

		BT_LOGD("Creating plugin object for plugin: "
			"name=\"%s\", abi-major=%d, abi-minor=%d",
			descriptor->name, descriptor->major, descriptor->minor);

		if (descriptor->major > __BT_PLUGIN_VERSION_MAJOR) {
			/*
			 * DEBUG-level logging because we're only
			 * _trying_ to open this file as a compatible
			 * Babeltrace plugin: if it's not, it's not an
			 * error. And because this can be tried during
			 * bt_plugin_create_all_from_dir(), it's not
			 * even a warning.
			 */
			BT_LOGD("Unknown ABI major version: abi-major=%d",
				descriptor->major);
			goto error;
		}

		plugin = bt_plugin_so_create_empty(shared_lib_handle);
		if (!plugin) {
			BT_LOGE_STR("Cannot create empty shared library handle.");
			goto error;
		}

		if (shared_lib_handle && shared_lib_handle->path) {
			bt_plugin_set_path(plugin, shared_lib_handle->path->str);
		}

		status = bt_plugin_so_init(plugin, descriptor, attrs_begin,
			attrs_end, cc_descriptors_begin, cc_descriptors_end,
			cc_descr_attrs_begin, cc_descr_attrs_end);
		if (status < 0) {
			/*
			 * DEBUG-level logging because we're only
			 * _trying_ to open this file as a compatible
			 * Babeltrace plugin: if it's not, it's not an
			 * error. And because this can be tried during
			 * bt_plugin_create_all_from_dir(), it's not
			 * even a warning.
			 */
			BT_LOGD_STR("Cannot initialize SO plugin object from sections.");
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

	BT_LOGD_STR("Creating all SO plugins from built-in plugins.");
	plugin_set = bt_plugin_so_create_all_from_sections(shared_lib_handle,
		__bt_get_begin_section_plugin_descriptors(),
		__bt_get_end_section_plugin_descriptors(),
		__bt_get_begin_section_plugin_descriptor_attributes(),
		__bt_get_end_section_plugin_descriptor_attributes(),
		__bt_get_begin_section_component_class_descriptors(),
		__bt_get_end_section_component_class_descriptors(),
		__bt_get_begin_section_component_class_descriptor_attributes(),
		__bt_get_end_section_component_class_descriptor_attributes());

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
	struct __bt_plugin_descriptor const * const *(*get_begin_section_plugin_descriptors)(void);
	struct __bt_plugin_descriptor const * const *(*get_end_section_plugin_descriptors)(void);
	struct __bt_plugin_descriptor_attribute const * const *(*get_begin_section_plugin_descriptor_attributes)(void);
	struct __bt_plugin_descriptor_attribute const * const *(*get_end_section_plugin_descriptor_attributes)(void);
	struct __bt_plugin_component_class_descriptor const * const *(*get_begin_section_component_class_descriptors)(void);
	struct __bt_plugin_component_class_descriptor const * const *(*get_end_section_component_class_descriptors)(void);
	struct __bt_plugin_component_class_descriptor_attribute const * const *(*get_begin_section_component_class_descriptor_attributes)(void);
	struct __bt_plugin_component_class_descriptor_attribute const * const *(*get_end_section_component_class_descriptor_attributes)(void);
	bt_bool is_libtool_wrapper = BT_FALSE, is_shared_object = BT_FALSE;
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle = NULL;

	if (!path) {
		BT_LOGW_STR("Invalid parameter: path is NULL.");
		goto end;
	}

	BT_LOGD("Creating all SO plugins from file: path=\"%s\"", path);
	path_len = strlen(path);
	if (path_len <= PLUGIN_SUFFIX_LEN) {
		BT_LOGW("Invalid parameter: path length is too short: "
			"path-length=%zu", path_len);
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
		/* Name indicates this is not a plugin file; not an error */
		BT_LOGV("File is not a SO plugin file: path=\"%s\"", path);
		goto end;
	}

	shared_lib_handle = bt_plugin_so_shared_lib_handle_create(path);
	if (!shared_lib_handle) {
		BT_LOGD_STR("Cannot create shared library handle.");
		goto end;
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_begin_section_plugin_descriptors",
			(gpointer *) &get_begin_section_plugin_descriptors)) {
		descriptors_begin = get_begin_section_plugin_descriptors();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_begin_section_plugin_descriptors");
		goto end;
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_end_section_plugin_descriptors",
			(gpointer *) &get_end_section_plugin_descriptors)) {
		descriptors_end = get_end_section_plugin_descriptors();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_end_section_plugin_descriptors");
		goto end;
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_begin_section_plugin_descriptor_attributes",
			(gpointer *) &get_begin_section_plugin_descriptor_attributes)) {
		 attrs_begin = get_begin_section_plugin_descriptor_attributes();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_begin_section_plugin_descriptor_attributes");
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_end_section_plugin_descriptor_attributes",
			(gpointer *) &get_end_section_plugin_descriptor_attributes)) {
		attrs_end = get_end_section_plugin_descriptor_attributes();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_end_section_plugin_descriptor_attributes");
	}

	if ((!!attrs_begin - !!attrs_end) != 0) {
		BT_LOGD("Found section start or end symbol, but not both: "
			"path=\"%s\", symbol-start=\"%s\", "
			"symbol-end=\"%s\", symbol-start-addr=%p, "
			"symbol-end-addr=%p",
			path, "__bt_get_begin_section_plugin_descriptor_attributes",
			"__bt_get_end_section_plugin_descriptor_attributes",
			attrs_begin, attrs_end);
		goto end;
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_begin_section_component_class_descriptors",
			(gpointer *) &get_begin_section_component_class_descriptors)) {
		cc_descriptors_begin = get_begin_section_component_class_descriptors();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_begin_section_component_class_descriptors");
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_end_section_component_class_descriptors",
			(gpointer *) &get_end_section_component_class_descriptors)) {
		cc_descriptors_end = get_end_section_component_class_descriptors();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_end_section_component_class_descriptors");
	}

	if ((!!cc_descriptors_begin - !!cc_descriptors_end) != 0) {
		BT_LOGD("Found section start or end symbol, but not both: "
			"path=\"%s\", symbol-start=\"%s\", "
			"symbol-end=\"%s\", symbol-start-addr=%p, "
			"symbol-end-addr=%p",
			path, "__bt_get_begin_section_component_class_descriptors",
			"__bt_get_end_section_component_class_descriptors",
			cc_descriptors_begin, cc_descriptors_end);
		goto end;
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_begin_section_component_class_descriptor_attributes",
			(gpointer *) &get_begin_section_component_class_descriptor_attributes)) {
		cc_descr_attrs_begin = get_begin_section_component_class_descriptor_attributes();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_begin_section_component_class_descriptor_attributes");
	}

	if (g_module_symbol(shared_lib_handle->module, "__bt_get_end_section_component_class_descriptor_attributes",
			(gpointer *) &get_end_section_component_class_descriptor_attributes)) {
		cc_descr_attrs_end = get_end_section_component_class_descriptor_attributes();
	} else {
		BT_LOGD("Cannot resolve plugin symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_end_section_component_class_descriptor_attributes");
	}

	if ((!!cc_descr_attrs_begin - !!cc_descr_attrs_end) != 0) {
		BT_LOGD("Found section start or end symbol, but not both: "
			"path=\"%s\", symbol-start=\"%s\", "
			"symbol-end=\"%s\", symbol-start-addr=%p, "
			"symbol-end-addr=%p",
			path, "__bt_get_begin_section_component_class_descriptor_attributes",
			"__bt_get_end_section_component_class_descriptor_attributes",
			cc_descr_attrs_begin, cc_descr_attrs_end);
		goto end;
	}

	/* Initialize plugin */
	BT_LOGD_STR("Initializing plugin object.");
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
	bt_list_del(&comp_class->node);
	BT_PUT(comp_class->so_handle);
	BT_LOGV("Component class destroyed: removed entry from list: "
		"comp-cls-addr=%p", comp_class);
}

BT_HIDDEN
void bt_plugin_so_on_add_component_class(struct bt_plugin *plugin,
		struct bt_component_class *comp_class)
{
	struct bt_plugin_so_spec_data *spec = plugin->spec_data;

	assert(plugin->spec_data);
	assert(plugin->type == BT_PLUGIN_TYPE_SO);

	bt_list_add(&comp_class->node, &component_class_list);
	comp_class->so_handle = bt_get(spec->shared_lib_handle);

	/* Add our custom destroy listener */
	bt_component_class_add_destroy_listener(comp_class,
		plugin_comp_class_destroy_listener, NULL);
}
