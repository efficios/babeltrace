/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2017 Philippe Proulx <pproulx@efficios.com>
 *
 * Babeltrace trace converter - CLI tool's configuration
 */

#ifndef CLI_BABELTRACE_CFG_H
#define CLI_BABELTRACE_CFG_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "lib/object.h"
#include "compat/compiler.h"
#include <glib.h>

enum bt_config_command {
	BT_CONFIG_COMMAND_RUN,
	BT_CONFIG_COMMAND_PRINT_CTF_METADATA,
	BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS,
	BT_CONFIG_COMMAND_LIST_PLUGINS,
	BT_CONFIG_COMMAND_HELP,
	BT_CONFIG_COMMAND_QUERY,
};

struct bt_config_component {
	bt_object base;
	bt_component_class_type type;
	GString *plugin_name;
	GString *comp_cls_name;
	bt_value *params;
	GString *instance_name;
	int log_level;
};

struct bt_config_connection {
	GString *upstream_comp_name;
	GString *downstream_comp_name;
	GString *upstream_port_glob;
	GString *downstream_port_glob;
	GString *arg;
};

struct bt_config {
	bt_object base;
	bt_value *plugin_paths;
	bool omit_system_plugin_path;
	bool omit_home_plugin_path;
	bool command_needs_plugins;
	const char *command_name;
	int log_level;
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

			/*
			 * Number of microseconds to sleep when we need
			 * to retry to run the graph.
			 */
			uint64_t retry_duration_us;

			/*
			 * Whether or not to trim the source trace to the
			 * intersection of its streams.
			 */
			bool stream_intersection_mode;
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
			GString *output_path;
		} print_ctf_metadata;

		/* BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS */
		struct {
			GString *url;
			GString *output_path;
		} print_lttng_live_sessions;
	} cmd_data;
};

static inline
struct bt_config_component *bt_config_get_component(GPtrArray *array,
		size_t index)
{
	struct bt_config_component *comp = g_ptr_array_index(array, index);

	bt_object_get_ref(comp);
	return comp;
}

int bt_config_append_plugin_paths(bt_value *plugin_paths,
		const char *arg);

void bt_config_connection_destroy(struct bt_config_connection *connection);

#endif /* CLI_BABELTRACE_CFG_H */
