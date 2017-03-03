#ifndef BABELTRACE_CONVERTER_CFG_H
#define BABELTRACE_CONVERTER_CFG_H

/*
 * Babeltrace trace converter - configuration
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <stdlib.h>
#include <stdint.h>
#include <babeltrace/values.h>
#include <babeltrace/ref.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/component/component-class.h>
#include <glib.h>

struct bt_config_component {
	struct bt_object base;
	enum bt_component_class_type type;
	GString *plugin_name;
	GString *comp_cls_name;
	struct bt_value *params;
	GString *instance_name;
};

enum bt_config_command {
	BT_CONFIG_COMMAND_RUN,
	BT_CONFIG_COMMAND_PRINT_CTF_METADATA,
	BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS,
	BT_CONFIG_COMMAND_LIST_PLUGINS,
	BT_CONFIG_COMMAND_HELP,
	BT_CONFIG_COMMAND_QUERY,
};

struct bt_config {
	struct bt_object base;
	bool debug;
	bool verbose;
	struct bt_value *plugin_paths;
	bool omit_system_plugin_path;
	bool omit_home_plugin_path;
	bool command_needs_plugins;
	const char *command_name;
	enum bt_config_command command;
	union {
		/* BT_CONFIG_COMMAND_RUN */
		struct {
			/* Array of pointers to struct bt_config_component */
			GPtrArray *sources;

			/* Array of pointers to struct bt_config_component */
			GPtrArray *filters;

			/* Array of pointers to struct bt_config_component */
			GPtrArray *sinks;

			/* Array of pointers to struct bt_config_connection */
			GPtrArray *connections;
		} run;

		/* BT_CONFIG_COMMAND_HELP */
		struct {
			struct bt_config_component *cfg_component;
		} help;

		/* BT_CONFIG_COMMAND_QUERY */
		struct {
			GString *object;
			struct bt_config_component *cfg_component;
		} query;

		/* BT_CONFIG_COMMAND_PRINT_CTF_METADATA */
		struct {
			GString *path;
		} print_ctf_metadata;

		/* BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS */
		struct {
			GString *url;
		} print_lttng_live_sessions;
	} cmd_data;
};

static inline
struct bt_config_component *bt_config_get_component(GPtrArray *array,
		size_t index)
{
	return bt_get(g_ptr_array_index(array, index));
}

struct bt_config *bt_config_from_args(int argc, const char *argv[],
		int *retcode, bool omit_system_plugin_path,
		bool omit_home_plugin_path,
		struct bt_value *initial_plugin_paths);

struct bt_config_component *bt_config_component_from_arg(
		enum bt_component_class_type type, const char *arg);

enum bt_value_status bt_config_append_plugin_paths(
		struct bt_value *plugin_paths, const char *arg);

#endif /* BABELTRACE_CONVERTER_CFG_H */
