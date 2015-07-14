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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Status code. Errors are always negative.
 */
enum bt_component_factory_status {
	/** General error. */
	BT_COMPONENT_FACTORY_STATUS_ERROR =	-128,

	/** Invalid arguments. */
	/* -22 for compatibility with -EINVAL */
	BT_COMPONENT_FACTORY_STATUS_INVAL =	-22,

	/** Memory allocation failure. */
	/* -12 for compatibility with -ENOMEM */
	BT_COMPONENT_FACTORY_STATUS_NOMEM =	-12,

	/** I/O error. */
	/* -5 for compatibility with -EIO */
	BT_COMPONENT_FACTORY_STATUS_IO =	-5,

	/** No such file or directory. */
	/* -2 for compatibility with -ENOENT */
	BT_COMPONENT_FACTORY_STATUS_NOENT =	-2,

	/** Operation not permitted. */
	/* -1 for compatibility with -EPERM */
	BT_COMPONENT_FACTORY_STATUS_PERM =	-1,

	/** No error, okay. */
	BT_COMPONENT_FACTORY_STATUS_OK =	0,
};

struct bt_component_factory;

/**
 * Create a component factory.
 *
 * @returns	An instance of component factory
 */
extern
struct bt_component_factory *bt_component_factory_create(void);

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
extern
enum bt_component_factory_status bt_component_factory_load(
		struct bt_component_factory *factory, const char *path);

extern
enum bt_component_factory_status
bt_component_factory_register_source_component_class(
	struct bt_component_factory *factory, const char *name,
	bt_component_source_init_cb init);

extern
enum bt_component_factory_status bt_component_factory_register_sink_component_class(
	struct bt_component_factory *factory, const char *name,
	bt_component_sink_init_cb init);

extern
void bt_component_factory_destroy(struct bt_component_factory *factory);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_COMPONENT_FACTORY_H */
