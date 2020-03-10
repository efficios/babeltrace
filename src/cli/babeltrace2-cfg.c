/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Babeltrace trace converter - parameter parsing
 */

#include "common/common.h"
#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include "babeltrace2-cfg.h"

static
void destroy_gstring(void *data)
{
	g_string_free(data, TRUE);
}

/*
 * Extracts the various paths from the string arg, delimited by ':' on UNIX,
 * ';' on Windows, and appends them to the array value object `plugin_paths`.
 */
int bt_config_append_plugin_paths(
		bt_value *plugin_paths, const char *arg)
{
	int ret = 0;
	GPtrArray *dirs = g_ptr_array_new_with_free_func(destroy_gstring);
	size_t i;

	if (!dirs) {
		ret = -1;
		goto end;
	}

	ret = bt_common_append_plugin_path_dirs(arg, dirs);
	if (ret) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < dirs->len; i++) {
		GString *dir = g_ptr_array_index(dirs, i);

		ret = bt_value_array_append_string_element(
			plugin_paths, dir->str);
		if (ret < 0) {
			ret = -1;
			goto end;
		}
	}

end:
	g_ptr_array_free(dirs, TRUE);
	return ret;
}

void bt_config_connection_destroy(struct bt_config_connection *connection)
{
	if (!connection) {
		return;
	}

	if (connection->upstream_comp_name) {
		g_string_free(connection->upstream_comp_name, TRUE);
	}

	if (connection->downstream_comp_name) {
		g_string_free(connection->downstream_comp_name, TRUE);
	}

	if (connection->upstream_port_glob) {
		g_string_free(connection->upstream_port_glob, TRUE);
	}

	if (connection->downstream_port_glob) {
		g_string_free(connection->downstream_port_glob, TRUE);
	}

	if (connection->arg) {
		g_string_free(connection->arg, TRUE);
	}

	g_free(connection);
}
