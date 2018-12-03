/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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
#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include "babeltrace-cfg.h"
#include "babeltrace-cfg-cli-args-connect.h"

static bool all_named_and_printable_in_array(GPtrArray *comps)
{
	size_t i;
	bool all_named_and_printable = true;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (comp->instance_name->len == 0) {
			all_named_and_printable = false;
			goto end;
		}

		if (!bt_common_string_is_printable(comp->instance_name->str)) {
			all_named_and_printable = false;
			goto end;
		}
	}

end:
	return all_named_and_printable;
}

static bool all_named_and_printable(struct bt_config *cfg)
{
	return all_named_and_printable_in_array(cfg->cmd_data.run.sources) &&
		all_named_and_printable_in_array(cfg->cmd_data.run.filters) &&
		all_named_and_printable_in_array(cfg->cmd_data.run.sinks);
}

static struct bt_config_connection *bt_config_connection_create(const char *arg)
{
	struct bt_config_connection *cfg_connection;

	cfg_connection = g_new0(struct bt_config_connection, 1);
	if (!cfg_connection) {
		goto error;
	}

	cfg_connection->upstream_comp_name = g_string_new(NULL);
	if (!cfg_connection->upstream_comp_name) {
		goto error;
	}

	cfg_connection->downstream_comp_name = g_string_new(NULL);
	if (!cfg_connection->downstream_comp_name) {
		goto error;
	}

	cfg_connection->upstream_port_glob = g_string_new("*");
	if (!cfg_connection->upstream_port_glob) {
		goto error;
	}

	cfg_connection->downstream_port_glob = g_string_new("*");
	if (!cfg_connection->downstream_port_glob) {
		goto error;
	}

	cfg_connection->arg = g_string_new(arg);
	if (!cfg_connection->arg) {
		goto error;
	}

	goto end;

error:
	g_free(cfg_connection);
	cfg_connection = NULL;

end:
	return cfg_connection;
}

static bool validate_port_glob(const char *port_glob)
{
	bool is_valid = true;
	const char *ch = port_glob;

	BT_ASSERT(port_glob);

	while (*ch != '\0') {
		switch (*ch) {
		case '\\':
			switch (ch[1]) {
			case '\0':
				goto end;
			default:
				ch += 2;
				continue;
			}
		case '?':
		case '[':
			/*
			 * This is reserved for future use, to support
			 * full globbing patterns. Those characters must
			 * be escaped with `\`.
			 */
			is_valid = false;
			goto end;
		default:
			ch++;
			break;
		}
	}

end:
	return is_valid;
}

static int normalize_glob_pattern(GString *glob_pattern_gs)
{
	int ret = 0;
	char *glob_pattern = strdup(glob_pattern_gs->str);

	if (!glob_pattern) {
		ret = -1;
		goto end;
	}

	bt_common_normalize_star_glob_pattern(glob_pattern);
	g_string_assign(glob_pattern_gs, glob_pattern);
	free(glob_pattern);

end:
	return ret;
}

static struct bt_config_connection *cfg_connection_from_arg(const char *arg)
{
	const char *at = arg;
	size_t end_pos;
	struct bt_config_connection *cfg_conn = NULL;
	GString *gs = NULL;
	enum {
		UPSTREAM_NAME,
		DOWNSTREAM_NAME,
		UPSTREAM_PORT_GLOB,
		DOWNSTREAM_PORT_GLOB,
	} state = UPSTREAM_NAME;

	if (!bt_common_string_is_printable(arg)) {
		goto error;
	}

	cfg_conn = bt_config_connection_create(arg);
	if (!cfg_conn) {
		goto error;
	}

	while (true) {
		switch (state) {
		case UPSTREAM_NAME:
			gs = bt_common_string_until(at, ".:\\", ".:", &end_pos);
			if (!gs || gs->len == 0) {
				goto error;
			}

			g_string_free(cfg_conn->upstream_comp_name, TRUE);
			cfg_conn->upstream_comp_name = gs;
			gs = NULL;

			if (at[end_pos] == ':') {
				state = DOWNSTREAM_NAME;
			} else if (at[end_pos] == '.') {
				state = UPSTREAM_PORT_GLOB;
			} else {
				goto error;
			}

			at += end_pos + 1;
			break;
		case DOWNSTREAM_NAME:
			gs = bt_common_string_until(at, ".:\\", ".:", &end_pos);
			if (!gs || gs->len == 0) {
				goto error;
			}

			g_string_free(cfg_conn->downstream_comp_name, TRUE);
			cfg_conn->downstream_comp_name = gs;
			gs = NULL;

			if (at[end_pos] == '.') {
				state = DOWNSTREAM_PORT_GLOB;
			} else if (at[end_pos] == '\0') {
				goto end;
			} else {
				goto error;
			}

			at += end_pos + 1;
			break;
		case UPSTREAM_PORT_GLOB:
			gs = bt_common_string_until(at, ".:", ".:", &end_pos);
			if (!gs || gs->len == 0) {
				goto error;
			}

			if (!validate_port_glob(gs->str)) {
				goto error;
			}

			if (normalize_glob_pattern(gs)) {
				goto error;
			}

			g_string_free(cfg_conn->upstream_port_glob, TRUE);
			cfg_conn->upstream_port_glob = gs;
			gs = NULL;

			if (at[end_pos] == ':') {
				state = DOWNSTREAM_NAME;
			} else {
				goto error;
			}

			at += end_pos + 1;
			break;
		case DOWNSTREAM_PORT_GLOB:
			gs = bt_common_string_until(at, ".:", ".:", &end_pos);
			if (!gs || gs->len == 0) {
				goto error;
			}

			if (!validate_port_glob(gs->str)) {
				goto error;
			}

			if (normalize_glob_pattern(gs)) {
				goto error;
			}

			g_string_free(cfg_conn->downstream_port_glob, TRUE);
			cfg_conn->downstream_port_glob = gs;
			gs = NULL;

			if (at[end_pos] == '\0') {
				goto end;
			} else {
				goto error;
			}
			break;
		default:
			abort();
		}
	}

error:
	bt_config_connection_destroy(cfg_conn);
	cfg_conn = NULL;

end:
	if (gs) {
		g_string_free(gs, TRUE);
	}

	return cfg_conn;
}

static struct bt_config_component *find_component_in_array(GPtrArray *comps,
		const char *name)
{
	size_t i;
	struct bt_config_component *found_comp = NULL;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (strcmp(name, comp->instance_name->str) == 0) {
			found_comp = comp;
			bt_object_get_ref(found_comp);
			goto end;
		}
	}

end:
	return found_comp;
}

static struct bt_config_component *find_component(struct bt_config *cfg,
		const char *name)
{
	struct bt_config_component *comp;

	comp = find_component_in_array(cfg->cmd_data.run.sources, name);
	if (comp) {
		goto end;
	}

	comp = find_component_in_array(cfg->cmd_data.run.filters, name);
	if (comp) {
		goto end;
	}

	comp = find_component_in_array(cfg->cmd_data.run.sinks, name);
	if (comp) {
		goto end;
	}

end:
	return comp;
}

static int validate_all_endpoints_exist(struct bt_config *cfg, char *error_buf,
		size_t error_buf_size)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.run.connections, i);
		struct bt_config_component *comp;

		comp = find_component(cfg, connection->upstream_comp_name->str);
		bt_object_put_ref(comp);
		if (!comp) {
			snprintf(error_buf, error_buf_size,
				"Invalid connection: cannot find upstream component `%s`:\n    %s\n",
				connection->upstream_comp_name->str,
				connection->arg->str);
			ret = -1;
			goto end;
		}

		comp = find_component(cfg, connection->downstream_comp_name->str);
		bt_object_put_ref(comp);
		if (!comp) {
			snprintf(error_buf, error_buf_size,
				"Invalid connection: cannot find downstream component `%s`:\n    %s\n",
				connection->downstream_comp_name->str,
				connection->arg->str);
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

static int validate_connection_directions(struct bt_config *cfg,
		char *error_buf, size_t error_buf_size)
{
	size_t i;
	int ret = 0;
	struct bt_config_component *src_comp = NULL;
	struct bt_config_component *dst_comp = NULL;

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.run.connections, i);

		src_comp = find_component(cfg,
			connection->upstream_comp_name->str);
		BT_ASSERT(src_comp);
		dst_comp = find_component(cfg,
			connection->downstream_comp_name->str);
		BT_ASSERT(dst_comp);

		if (src_comp->type == BT_COMPONENT_CLASS_TYPE_SOURCE) {
			if (dst_comp->type != BT_COMPONENT_CLASS_TYPE_FILTER &&
					dst_comp->type != BT_COMPONENT_CLASS_TYPE_SINK) {
				snprintf(error_buf, error_buf_size,
					"Invalid connection: source component `%s` not connected to filter or sink component:\n    %s\n",
					connection->upstream_comp_name->str,
					connection->arg->str);
				ret = -1;
				goto end;
			}
		} else if (src_comp->type == BT_COMPONENT_CLASS_TYPE_FILTER) {
			if (dst_comp->type != BT_COMPONENT_CLASS_TYPE_FILTER &&
					dst_comp->type != BT_COMPONENT_CLASS_TYPE_SINK) {
				snprintf(error_buf, error_buf_size,
					"Invalid connection: filter component `%s` not connected to filter or sink component:\n    %s\n",
					connection->upstream_comp_name->str,
					connection->arg->str);
				ret = -1;
				goto end;
			}
		} else {
			snprintf(error_buf, error_buf_size,
				"Invalid connection: cannot connect sink component `%s` to component `%s`:\n    %s\n",
				connection->upstream_comp_name->str,
				connection->downstream_comp_name->str,
				connection->arg->str);
			ret = -1;
			goto end;
		}

		BT_OBJECT_PUT_REF_AND_RESET(src_comp);
		BT_OBJECT_PUT_REF_AND_RESET(dst_comp);
	}

end:
	bt_object_put_ref(src_comp);
	bt_object_put_ref(dst_comp);
	return ret;
}

static int validate_no_cycles_rec(struct bt_config *cfg, GPtrArray *path,
		char *error_buf, size_t error_buf_size)
{
	int ret = 0;
	size_t conn_i;
	const char *src_comp_name;

	BT_ASSERT(path && path->len > 0);
	src_comp_name = g_ptr_array_index(path, path->len - 1);

	for (conn_i = 0; conn_i < cfg->cmd_data.run.connections->len; conn_i++) {
		struct bt_config_connection *conn =
			g_ptr_array_index(cfg->cmd_data.run.connections, conn_i);

		if (strcmp(conn->upstream_comp_name->str, src_comp_name) == 0) {
			size_t path_i;

			for (path_i = 0; path_i < path->len; path_i++) {
				const char *comp_name =
					g_ptr_array_index(path, path_i);

				if (strcmp(comp_name, conn->downstream_comp_name->str) == 0) {
					snprintf(error_buf, error_buf_size,
						"Invalid connection: connection forms a cycle:\n    %s\n",
						conn->arg->str);
					ret = -1;
					goto end;
				}
			}

			g_ptr_array_add(path, conn->downstream_comp_name->str);
			ret = validate_no_cycles_rec(cfg, path, error_buf,
					error_buf_size);
			if (ret) {
				goto end;
			}

			g_ptr_array_remove_index(path, path->len - 1);
		}
	}

end:
	return ret;
}

static int validate_no_cycles(struct bt_config *cfg, char *error_buf,
		size_t error_buf_size)
{
	size_t i;
	int ret = 0;
	GPtrArray *path;

	path = g_ptr_array_new();
	if (!path) {
		ret = -1;
		goto end;
	}

	g_ptr_array_add(path, NULL);

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *conn =
			g_ptr_array_index(cfg->cmd_data.run.connections, i);

		g_ptr_array_index(path, 0) = conn->upstream_comp_name->str;
		ret = validate_no_cycles_rec(cfg, path,
			error_buf, error_buf_size);
		if (ret) {
			goto end;
		}
	}

end:
	if (path) {
		g_ptr_array_free(path, TRUE);
	}

	return ret;
}

static int validate_all_components_connected_in_array(GPtrArray *comps,
		const struct bt_value *connected_components,
		char *error_buf, size_t error_buf_size)
{
	int ret = 0;
	size_t i;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (!bt_value_map_has_entry(connected_components,
				comp->instance_name->str)) {
			snprintf(error_buf, error_buf_size,
				"Component `%s` is not connected\n",
				comp->instance_name->str);
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

static int validate_all_components_connected(struct bt_config *cfg,
		char *error_buf, size_t error_buf_size)
{
	size_t i;
	int ret = 0;
	struct bt_value *connected_components = bt_value_map_create();

	if (!connected_components) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.run.connections, i);

		ret = bt_value_map_insert_entry(connected_components,
			connection->upstream_comp_name->str, bt_value_null);
		if (ret) {
			goto end;
		}

		ret = bt_value_map_insert_entry(connected_components,
			connection->downstream_comp_name->str, bt_value_null);
		if (ret) {
			goto end;
		}
	}

	ret = validate_all_components_connected_in_array(
		cfg->cmd_data.run.sources,
		connected_components,
		error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_all_components_connected_in_array(
		cfg->cmd_data.run.filters,
		connected_components,
		error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_all_components_connected_in_array(
		cfg->cmd_data.run.sinks,
		connected_components,
		error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

end:
	bt_object_put_ref(connected_components);
	return ret;
}

static int validate_no_duplicate_connection(struct bt_config *cfg,
		char *error_buf, size_t error_buf_size)
{
	size_t i;
	int ret = 0;
	struct bt_value *flat_connection_names =
		bt_value_map_create();
	GString *flat_connection_name = NULL;

	if (!flat_connection_names) {
		ret = -1;
		goto end;
	}

	flat_connection_name = g_string_new(NULL);
	if (!flat_connection_name) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.run.connections, i);

		g_string_printf(flat_connection_name, "%s\x01%s\x01%s\x01%s",
			connection->upstream_comp_name->str,
			connection->upstream_port_glob->str,
			connection->downstream_comp_name->str,
			connection->downstream_port_glob->str);

		if (bt_value_map_has_entry(flat_connection_names,
					   flat_connection_name->str)) {
			snprintf(error_buf, error_buf_size,
				"Duplicate connection:\n    %s\n",
				connection->arg->str);
			ret = -1;
			goto end;
		}

		ret = bt_value_map_insert_entry(flat_connection_names,
			flat_connection_name->str, bt_value_null);
		if (ret) {
			goto end;
		}
	}

end:
	bt_object_put_ref(flat_connection_names);

	if (flat_connection_name) {
		g_string_free(flat_connection_name, TRUE);
	}

	return ret;
}

static int validate_connections(struct bt_config *cfg, char *error_buf,
		size_t error_buf_size)
{
	int ret;

	ret = validate_all_endpoints_exist(cfg, error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_connection_directions(cfg, error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_all_components_connected(cfg, error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_no_duplicate_connection(cfg, error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_no_cycles(cfg, error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

end:
	return ret;
}

int bt_config_cli_args_create_connections(struct bt_config *cfg,
		const struct bt_value *connection_args,
		char *error_buf, size_t error_buf_size)
{
	int ret;
	size_t i;

	if (!all_named_and_printable(cfg)) {
		snprintf(error_buf, error_buf_size,
			"One or more components are unnamed (use --name) or contain a non-printable character\n");
		goto error;
	}

	for (i = 0; i < bt_value_array_get_size(connection_args); i++) {
		const struct bt_value *arg_value =
			bt_value_array_borrow_element_by_index_const(
				connection_args, i);
		const char *arg;
		struct bt_config_connection *cfg_connection;

		arg = bt_value_string_get(arg_value);
		cfg_connection = cfg_connection_from_arg(arg);
		if (!cfg_connection) {
			snprintf(error_buf, error_buf_size, "Cannot parse --connect option's argument:\n    %s\n",
				arg);
			goto error;
		}

		g_ptr_array_add(cfg->cmd_data.run.connections,
			cfg_connection);
	}


	ret = validate_connections(cfg, error_buf, error_buf_size);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}
