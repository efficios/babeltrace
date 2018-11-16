/*
 * Babeltrace trace converter - parameter parsing
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

#include <babeltrace/common-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/private-values.h>
#include <glib.h>
#include "babeltrace-cfg.h"

static
void destroy_gstring(void *data)
{
	g_string_free(data, TRUE);
}

/*
 * Extracts the various paths from the string arg, delimited by ':',
 * and appends them to the array value object plugin_paths.
 */
enum bt_value_status bt_config_append_plugin_paths(
		struct bt_private_value *plugin_paths, const char *arg)
{
	enum bt_value_status status = BT_VALUE_STATUS_OK;
	GPtrArray *dirs = g_ptr_array_new_with_free_func(destroy_gstring);
	int ret;
	size_t i;

	if (!dirs) {
		status = BT_VALUE_STATUS_ERROR;
		goto end;
	}

	ret = bt_common_append_plugin_path_dirs(arg, dirs);
	if (ret) {
		status = BT_VALUE_STATUS_ERROR;
		goto end;
	}

	for (i = 0; i < dirs->len; i++) {
		GString *dir = g_ptr_array_index(dirs, i);

		status = bt_private_value_array_append_string_element(
			plugin_paths, dir->str);
		if (status != BT_VALUE_STATUS_OK) {
			break;
		}
	}

end:
	g_ptr_array_free(dirs, TRUE);
	return status;
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
