#ifndef BABELTRACE_PLUGIN_COMPONENT_FACTORY_H
#define BABELTRACE_PLUGIN_COMPONENT_FACTORY_H

/*
 * Babeltrace - Component Factory Class Interface.
 *
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

#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/source.h>
#include <babeltrace/plugin/sink.h>
#include <babeltrace/plugin/filter.h>
#include <babeltrace/plugin/plugin-system.h>
#include <babeltrace/values.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Status code. Errors are always negative.
 */
enum bt_component_factory_status {
	/** General error. */
	BT_COMPONENT_FACTORY_STATUS_ERROR =		-128,

	/** Invalid plugin. */
	BT_COMPONENT_FACTORY_STATUS_INVAL_PLUGIN =	-6,

	/** Invalid arguments. */
	BT_COMPONENT_FACTORY_STATUS_INVAL =		-5,

	/** Memory allocation failure. */
	BT_COMPONENT_FACTORY_STATUS_NOMEM =		-4,

	/** I/O error. */
	BT_COMPONENT_FACTORY_STATUS_IO =		-3,

	/** No such file or directory. */
	BT_COMPONENT_FACTORY_STATUS_NOENT =		-2,

	/** Operation not permitted. */
	BT_COMPONENT_FACTORY_STATUS_PERM =		-1,

	/** No error, okay. */
	BT_COMPONENT_FACTORY_STATUS_OK =		0,
};

struct bt_component_factory;

/**
 * Create a component factory.
 *
 * @returns	An instance of component factory
 */
extern struct bt_component_factory *bt_component_factory_create(void);

/**
 * Get the number of component classes registered to the component factory.
 *
 * @param factory	A component factory instance
 * @returns		The number of component classes registered to the
 *			component factory, or a negative value on error.
 */
extern int bt_component_factory_get_component_class_count(
		struct bt_component_factory *factory);

/**
 * Get component class at index.
 *
 * @param factory	A component factory instance
 * @param index		Index of the component class to return
 * @returns		A component class instance, NULL on error.
 */
extern struct bt_component_class *bt_component_factory_get_component_class_index(
		struct bt_component_factory *factory, int index);

/**
 * Look-up component class.
 *
 * @param factory		A component factory instance
 * @param plugin_name		Name of the plug-in which registered the
 *				component class
 * @param type			Component type (@see #bt_component_type)
 * @param component_name	Component name
 * @returns			A component class instance, NULL on error.
 */
extern struct bt_component_class *bt_component_factory_get_component_class(
		struct bt_component_factory *factory,
		const char *plugin_name, enum bt_component_type type,
		const char *component_name);

/**
 * Recursively load and register Babeltrace plugins under a given path.
 *
 * Path will be traversed recursively if it is a directory, otherwise only the
 * provided file will be loaded.
 *
 * @param factory	A component factory instance
 * @param path		A path to a file or directory
 * @returns		One of #bt_component_factory_status values
 */
extern enum bt_component_factory_status bt_component_factory_load(
		struct bt_component_factory *factory, const char *path);

extern enum bt_component_factory_status
bt_component_factory_register_source_component_class(
		struct bt_component_factory *factory, const char *name,
		const char *description, bt_component_init_cb init);

extern enum bt_component_factory_status
bt_component_factory_register_sink_component_class(
		struct bt_component_factory *factory, const char *name,
		const char *description, bt_component_init_cb init);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_COMPONENT_FACTORY_H */
