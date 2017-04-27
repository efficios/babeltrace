#ifndef BABELTRACE_PLUGIN_PLUGIN_H
#define BABELTRACE_PLUGIN_PLUGIN_H

/*
 * BabelTrace - Babeltrace Plug-in Interface
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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <babeltrace/graph/component-class.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_plugin;
struct bt_plugin_set;
struct bt_component_class;

/**
 * Status code. Errors are always negative.
 */
enum bt_plugin_status {
	/** No error, okay. */
	BT_PLUGIN_STATUS_OK =		0,
	/** General error. */
	BT_PLUGIN_STATUS_ERROR =	-1,
	/** Memory allocation failure. */
	BT_PLUGIN_STATUS_NOMEM =	-4,
};

extern struct bt_plugin *bt_plugin_find(const char *plugin_name);

extern struct bt_component_class *bt_plugin_find_component_class(
		const char *plugin_name, const char *component_class_name,
		enum bt_component_class_type component_class_type);

extern struct bt_plugin_set *bt_plugin_create_all_from_file(const char *path);

extern struct bt_plugin_set *bt_plugin_create_all_from_dir(const char *path,
		bool recurse);

extern struct bt_plugin_set *bt_plugin_create_all_from_static(void);

/**
 * Get the name of a plug-in.
 *
 * @param plugin	An instance of a plug-in
 * @returns		Plug-in name or NULL on error
 */
extern const char *bt_plugin_get_name(struct bt_plugin *plugin);

/**
 * Get the name of a plug-in's author.
 *
 * @param plugin	An instance of a plug-in
 * @returns		Plug-in author or NULL on error
 */
extern const char *bt_plugin_get_author(struct bt_plugin *plugin);

/**
 * Get the license of a plug-in.
 *
 * @param plugin	An instance of a plug-in
 * @returns		Plug-in license or NULL on error
 */
extern const char *bt_plugin_get_license(struct bt_plugin *plugin);

/**
 * Get the decription of a plug-in.
 *
 * @param plugin	An instance of a plug-in
 * @returns		Plug-in description or NULL if none is available
 */
extern const char *bt_plugin_get_description(struct bt_plugin *plugin);

/**
 * Get the path of a plug-in.
 *
 * @param plugin	An instance of a plug-in
 * @returns		Plug-in path or NULL on error
 */
extern const char *bt_plugin_get_path(struct bt_plugin *plugin);

extern enum bt_plugin_status bt_plugin_get_version(struct bt_plugin *plugin,
		unsigned int *major, unsigned int *minor, unsigned int *patch,
		const char **extra);

extern int64_t bt_plugin_get_component_class_count(struct bt_plugin *plugin);

extern struct bt_component_class *bt_plugin_get_component_class_by_index(
		struct bt_plugin *plugin, uint64_t index);

extern
struct bt_component_class *bt_plugin_get_component_class_by_name_and_type(
		struct bt_plugin *plugin, const char *name,
		enum bt_component_class_type type);

extern
int64_t bt_plugin_set_get_plugin_count(struct bt_plugin_set *plugin_set);

extern
struct bt_plugin *bt_plugin_set_get_plugin(struct bt_plugin_set *plugin_set,
		uint64_t index);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_PLUGIN_H */
