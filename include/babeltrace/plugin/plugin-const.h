#ifndef BABELTRACE_PLUGIN_PLUGIN_CONST_H
#define BABELTRACE_PLUGIN_PLUGIN_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_bool */
#include <babeltrace/types.h>

/* For enum bt_component_class_type */
#include <babeltrace/graph/component-class.h>

/* For enum bt_component_class_source */
#include <babeltrace/graph/component-class-source.h>

/* For enum bt_component_class_filter */
#include <babeltrace/graph/component-class-filter.h>

/* For enum bt_component_class_sink */
#include <babeltrace/graph/component-class-sink.h>

/* For enum bt_property_availability */
#include <babeltrace/property.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_plugin;
struct bt_plugin_set;
struct bt_component_class;
struct bt_component_class_source;
struct bt_component_class_filter;
struct bt_component_class_sink;

enum bt_plugin_status {
	BT_PLUGIN_STATUS_OK =		0,
	BT_PLUGIN_STATUS_ERROR =	-1,
	BT_PLUGIN_STATUS_NOMEM =	-12,
};

extern const struct bt_plugin *bt_plugin_find(const char *plugin_name);

extern const struct bt_plugin_set *bt_plugin_create_all_from_file(
		const char *path);

extern const struct bt_plugin_set *bt_plugin_create_all_from_dir(
		const char *path, bt_bool recurse);

extern const struct bt_plugin_set *bt_plugin_create_all_from_static(void);

extern const char *bt_plugin_get_name(const struct bt_plugin *plugin);

extern const char *bt_plugin_get_author(const struct bt_plugin *plugin);

extern const char *bt_plugin_get_license(const struct bt_plugin *plugin);

extern const char *bt_plugin_get_description(const struct bt_plugin *plugin);

extern const char *bt_plugin_get_path(const struct bt_plugin *plugin);

extern enum bt_property_availability bt_plugin_get_version(
		const struct bt_plugin *plugin, unsigned int *major,
		unsigned int *minor, unsigned int *patch, const char **extra);

extern uint64_t bt_plugin_get_source_component_class_count(
		const struct bt_plugin *plugin);

extern uint64_t bt_plugin_get_filter_component_class_count(
		const struct bt_plugin *plugin);

extern uint64_t bt_plugin_get_sink_component_class_count(
		const struct bt_plugin *plugin);

extern const struct bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const_const(
		const struct bt_plugin *plugin, uint64_t index);

extern const struct bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const struct bt_plugin *plugin, uint64_t index);

extern const struct bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const struct bt_plugin *plugin, uint64_t index);

extern const struct bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const struct bt_plugin *plugin, const char *name);

extern const struct bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const struct bt_plugin *plugin, const char *name);

extern const struct bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const struct bt_plugin *plugin, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_PLUGIN_CONST_H */
