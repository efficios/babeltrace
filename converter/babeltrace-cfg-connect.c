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

#include <babeltrace/values.h>
#include "babeltrace-cfg.h"
#include "babeltrace-cfg-connect.h"

static bool all_named_in_array(GPtrArray *comps)
{
	size_t i;
	bool all_named = true;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (comp->instance_name->len == 0) {
			all_named = false;
			goto end;
		}
	}

end:
	return all_named;
}

static bool all_named(struct bt_config *cfg)
{
	return all_named_in_array(cfg->cmd_data.convert.sources) &&
		all_named_in_array(cfg->cmd_data.convert.filters) &&
		all_named_in_array(cfg->cmd_data.convert.sinks);
}

void bt_config_connection_destroy(struct bt_config_connection *connection)
{
	if (!connection) {
		return;
	}

	if (connection->src_instance_name) {
		g_string_free(connection->src_instance_name, TRUE);
	}

	if (connection->dst_instance_name) {
		g_string_free(connection->dst_instance_name, TRUE);
	}

	if (connection->src_port_name) {
		g_string_free(connection->src_port_name, TRUE);
	}

	if (connection->dst_port_name) {
		g_string_free(connection->dst_port_name, TRUE);
	}

	if (connection->arg) {
		g_string_free(connection->arg, TRUE);
	}

	g_free(connection);
}

static struct bt_config_connection *bt_config_connection_create(const char *arg)
{
	struct bt_config_connection *cfg_connection;

	cfg_connection = g_new0(struct bt_config_connection, 1);
	if (!cfg_connection) {
		goto error;
	}

	cfg_connection->src_instance_name = g_string_new(NULL);
	if (!cfg_connection->src_instance_name) {
		goto error;
	}

	cfg_connection->dst_instance_name = g_string_new(NULL);
	if (!cfg_connection->dst_instance_name) {
		goto error;
	}

	cfg_connection->src_port_name = g_string_new(NULL);
	if (!cfg_connection->src_port_name) {
		goto error;
	}

	cfg_connection->dst_port_name = g_string_new(NULL);
	if (!cfg_connection->dst_port_name) {
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

static struct bt_config_connection *bt_config_connection_create_full(
	const char *src_instance_name, const char *src_port_name,
	const char *dst_instance_name, const char *dst_port_name,
	const char *arg)
{
	struct bt_config_connection *cfg_connection =
		bt_config_connection_create(arg);

	if (!cfg_connection) {
		goto end;
	}

	g_string_assign(cfg_connection->src_instance_name, src_instance_name);
	g_string_assign(cfg_connection->dst_instance_name, dst_instance_name);
	g_string_assign(cfg_connection->src_port_name, src_port_name);
	g_string_assign(cfg_connection->dst_port_name, dst_port_name);

end:
	return cfg_connection;
}

static GScanner *create_connection_arg_scanner(void)
{
	GScannerConfig scanner_config = {
		.cset_skip_characters = " \t\n",
		.cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "_-",
		.cset_identifier_nth = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "_-",
		.case_sensitive = TRUE,
		.cpair_comment_single = NULL,
		.skip_comment_multi = TRUE,
		.skip_comment_single = TRUE,
		.scan_comment_multi = FALSE,
		.scan_identifier = TRUE,
		.scan_identifier_1char = TRUE,
		.scan_identifier_NULL = FALSE,
		.scan_symbols = FALSE,
		.symbol_2_token = FALSE,
		.scope_0_fallback = FALSE,
		.scan_binary = FALSE,
		.scan_octal = FALSE,
		.scan_float = FALSE,
		.scan_hex = FALSE,
		.scan_hex_dollar = FALSE,
		.numbers_2_int = FALSE,
		.int_2_float = FALSE,
		.store_int64 = FALSE,
		.scan_string_sq = FALSE,
		.scan_string_dq = FALSE,
		.identifier_2_string = FALSE,
		.char_2_token = TRUE,
	};

	return g_scanner_new(&scanner_config);
}

static struct bt_config_connection *cfg_connection_from_arg(const char *arg)
{
	struct bt_config_connection *connection = NULL;
	GScanner *scanner = NULL;
	enum {
		EXPECTING_SRC,
		EXPECTING_SRC_DOT,
		EXPECTING_SRC_PORT,
		EXPECTING_COLON,
		EXPECTING_DST,
		EXPECTING_DST_DOT,
		EXPECTING_DST_PORT,
		DONE,
	} state = EXPECTING_SRC;

	connection = bt_config_connection_create(arg);
	if (!connection) {
		goto error;
	}

	scanner = create_connection_arg_scanner();
	if (!scanner) {
		goto error;
	}

	g_scanner_input_text(scanner, arg, strlen(arg));

	while (true) {
		GTokenType token_type = g_scanner_get_next_token(scanner);

		if (token_type == G_TOKEN_EOF) {
			goto after_scan;
		}

		switch (state) {
		case EXPECTING_SRC:
			if (token_type != G_TOKEN_IDENTIFIER) {
				goto error;
			}

			g_string_assign(connection->src_instance_name,
				scanner->value.v_identifier);
			state = EXPECTING_SRC_DOT;
			break;
		case EXPECTING_SRC_DOT:
			if (token_type == ':') {
				state = EXPECTING_DST;
				break;
			}

			if (token_type != '.') {
				goto error;
			}

			state = EXPECTING_SRC_PORT;
			break;
		case EXPECTING_SRC_PORT:
			if (token_type != G_TOKEN_IDENTIFIER) {
				goto error;
			}

			g_string_assign(connection->src_port_name,
				scanner->value.v_identifier);
			state = EXPECTING_COLON;
			break;
		case EXPECTING_COLON:
			if (token_type != ':') {
				goto error;
			}

			state = EXPECTING_DST;
			break;
		case EXPECTING_DST:
			if (token_type != G_TOKEN_IDENTIFIER) {
				goto error;
			}

			g_string_assign(connection->dst_instance_name,
				scanner->value.v_identifier);
			state = EXPECTING_DST_DOT;
			break;
		case EXPECTING_DST_DOT:
			if (token_type != '.') {
				goto error;
			}

			state = EXPECTING_DST_PORT;
			break;
		case EXPECTING_DST_PORT:
			if (token_type != G_TOKEN_IDENTIFIER) {
				goto error;
			}

			g_string_assign(connection->dst_port_name,
				scanner->value.v_identifier);
			state = DONE;
			break;
		default:
			goto error;
		}
	}

after_scan:
	if (state != EXPECTING_DST_DOT && state != DONE) {
		goto error;
	}

	goto end;

error:
	bt_config_connection_destroy(connection);
	connection = NULL;

end:
	if (scanner) {
		g_scanner_destroy(scanner);
	}

	return connection;
}

static struct bt_config_component *find_component_in_array(GPtrArray *comps,
		const char *name)
{
	size_t i;
	struct bt_config_component *found_comp = NULL;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (strcmp(name, comp->instance_name->str) == 0) {
			found_comp = bt_get(comp);
			goto end;
		}
	}

end:
	return found_comp;
}

static struct bt_config_component *find_component(struct bt_config *cfg,
		const char *name, enum bt_component_class_type *type)
{
	struct bt_config_component *comp;

	comp = find_component_in_array(cfg->cmd_data.convert.sources, name);
	if (comp) {
		*type = BT_COMPONENT_CLASS_TYPE_SOURCE;
		goto end;
	}

	comp = find_component_in_array(cfg->cmd_data.convert.filters, name);
	if (comp) {
		*type = BT_COMPONENT_CLASS_TYPE_FILTER;
		goto end;
	}

	comp = find_component_in_array(cfg->cmd_data.convert.sinks, name);
	if (comp) {
		*type = BT_COMPONENT_CLASS_TYPE_SINK;
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

	for (i = 0; i < cfg->cmd_data.convert.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.convert.connections, i);
		struct bt_config_component *comp;
		enum bt_component_class_type type;

		comp = find_component(cfg, connection->src_instance_name->str,
			&type);
		bt_put(comp);
		if (!comp) {
			comp = find_component(cfg,
				connection->dst_instance_name->str, &type);
			bt_put(comp);
			if (!comp) {
				snprintf(error_buf, error_buf_size,
					"Invalid connection: cannot find component `%s`:\n    %s\n",
					connection->dst_instance_name->str,
					connection->arg->str);
				ret = -1;
				goto end;
			}
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

	for (i = 0; i < cfg->cmd_data.convert.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.convert.connections, i);
		enum bt_component_class_type src_type;
		enum bt_component_class_type dst_type;

		src_comp = find_component(cfg,
			connection->src_instance_name->str, &src_type);
		assert(src_comp);
		dst_comp = find_component(cfg,
			connection->dst_instance_name->str, &dst_type);
		assert(dst_comp);

		if (src_type == BT_COMPONENT_CLASS_TYPE_SOURCE) {
			if (dst_type != BT_COMPONENT_CLASS_TYPE_FILTER &&
					dst_type != BT_COMPONENT_CLASS_TYPE_SINK) {
				snprintf(error_buf, error_buf_size,
					"Invalid connection: source component `%s` not connected to filter or sink component:\n    %s\n",
					connection->src_instance_name->str,
					connection->arg->str);
				ret = -1;
				goto end;
			}
		} else if (src_type == BT_COMPONENT_CLASS_TYPE_FILTER) {
			if (dst_type != BT_COMPONENT_CLASS_TYPE_FILTER &&
					dst_type != BT_COMPONENT_CLASS_TYPE_SINK) {
				snprintf(error_buf, error_buf_size,
					"Invalid connection: filter component `%s` not connected to filter or sink component:\n    %s\n",
					connection->src_instance_name->str,
					connection->arg->str);
				ret = -1;
				goto end;
			}
		} else {
			snprintf(error_buf, error_buf_size,
				"Invalid connection: cannot connect sink component `%s` to component `%s`:\n    %s\n",
				connection->src_instance_name->str,
				connection->dst_instance_name->str,
				connection->arg->str);
			ret = -1;
			goto end;
		}

		BT_PUT(src_comp);
		BT_PUT(dst_comp);
	}

end:
	bt_put(src_comp);
	bt_put(dst_comp);
	return ret;
}

static int validate_self_connections(struct bt_config *cfg, char *error_buf,
		size_t error_buf_size)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < cfg->cmd_data.convert.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.convert.connections, i);

		if (strcmp(connection->src_instance_name->str,
				connection->dst_instance_name->str) == 0) {
			snprintf(error_buf, error_buf_size,
				"Invalid connection: component `%s` is connected to itself:\n    %s\n",
				connection->src_instance_name->str,
				connection->arg->str);
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

static int validate_all_components_connected_in_array(GPtrArray *comps,
		struct bt_value *connected_components,
		char *error_buf, size_t error_buf_size)
{
	int ret = 0;
	size_t i;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (!bt_value_map_has_key(connected_components,
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

	for (i = 0; i < cfg->cmd_data.convert.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.convert.connections, i);

		ret = bt_value_map_insert(connected_components,
			connection->src_instance_name->str, bt_value_null);
		if (ret) {
			goto end;
		}

		ret = bt_value_map_insert(connected_components,
			connection->dst_instance_name->str, bt_value_null);
		if (ret) {
			goto end;
		}
	}

	ret = validate_all_components_connected_in_array(
		cfg->cmd_data.convert.sources, connected_components,
		error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_all_components_connected_in_array(
		cfg->cmd_data.convert.filters, connected_components,
		error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

	ret = validate_all_components_connected_in_array(
		cfg->cmd_data.convert.sinks, connected_components,
		error_buf, error_buf_size);
	if (ret) {
		goto end;
	}

end:
	bt_put(connected_components);
	return ret;
}

static int validate_no_duplicate_connection(struct bt_config *cfg,
		char *error_buf, size_t error_buf_size)
{
	size_t i;
	int ret = 0;
	struct bt_value *flat_connection_names = bt_value_map_create();
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

	for (i = 0; i < cfg->cmd_data.convert.connections->len; i++) {
		struct bt_config_connection *connection =
			g_ptr_array_index(cfg->cmd_data.convert.connections, i);

		g_string_printf(flat_connection_name, "%s.%s:%s.%s",
			connection->src_instance_name->str,
			connection->src_port_name->str,
			connection->dst_instance_name->str,
			connection->dst_port_name->str);

		if (bt_value_map_has_key(flat_connection_names,
				flat_connection_name->str)) {
			snprintf(error_buf, error_buf_size,
				"Duplicate connection:\n    %s\n",
				connection->arg->str);
			ret = -1;
			goto end;
		}

		ret = bt_value_map_insert(flat_connection_names,
			flat_connection_name->str, bt_value_null);
		if (ret) {
			goto end;
		}
	}

end:
	bt_put(flat_connection_names);

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

	ret = validate_self_connections(cfg, error_buf, error_buf_size);
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

end:
	return ret;
}

static bool component_name_exists_in_array(GPtrArray *comps, const char *name)
{
	size_t i;
	bool exists = false;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (comp->instance_name->len > 0) {
			if (strcmp(comp->instance_name->str, name) == 0) {
				exists = true;
				break;
			}
		}
	}

	return exists;
}

static bool component_name_exists_at_all(struct bt_config *cfg,
		const char *name)
{
	return component_name_exists_in_array(cfg->cmd_data.convert.sources,
		name) || component_name_exists_in_array(
			cfg->cmd_data.convert.filters, name) ||
		component_name_exists_in_array(
			cfg->cmd_data.convert.sinks, name);
}

static int auto_name_component(struct bt_config *cfg, const char *prefix,
		struct bt_config_component *comp)
{
	int ret = 0;
	unsigned int i = 0;
	GString *new_name;

	assert(comp->instance_name->len == 0);
	new_name = g_string_new(NULL);
	if (!new_name) {
		ret = -1;
		goto end;
	}

	do {
		g_string_printf(new_name, "%s-%s.%s-%d", prefix,
			comp->plugin_name->str, comp->component_name->str, i);
		i++;
	} while (component_name_exists_at_all(cfg, new_name->str));

	g_string_assign(comp->instance_name, new_name->str);

end:
	if (new_name) {
		g_string_free(new_name, TRUE);
	}

	return ret;
}

static int auto_name_components_in_array(struct bt_config *cfg,
		GPtrArray *comps, const char *prefix)
{
	int ret = 0;
	size_t i;

	for (i = 0; i < comps->len; i++) {
		struct bt_config_component *comp = g_ptr_array_index(comps, i);

		if (comp->instance_name->len == 0) {
			ret = auto_name_component(cfg, prefix, comp);
			if (ret) {
				goto end;
			}
		}
	}

end:
	return ret;
}

static int auto_name_components(struct bt_config *cfg)
{
	int ret;

	ret = auto_name_components_in_array(cfg, cfg->cmd_data.convert.sources,
		"source");
	if (ret) {
		goto end;
	}

	ret = auto_name_components_in_array(cfg, cfg->cmd_data.convert.filters,
		"filter");
	if (ret) {
		goto end;
	}

	ret = auto_name_components_in_array(cfg, cfg->cmd_data.convert.sinks,
		"sink");
	if (ret) {
		goto end;
	}

end:
	return ret;
}

static int auto_connect(struct bt_config *cfg)
{
	int ret = 0;
	struct bt_config_component *muxer_cfg_comp = NULL;
	size_t i;
	const char *last_filter_comp_name;

	/* Make sure all components have a unique instance name */
	ret = auto_name_components(cfg);
	if (ret) {
		goto error;
	}

	/* Add an implicit muxer filter */
	muxer_cfg_comp = bt_config_component_from_arg("utils.muxer");
	if (!muxer_cfg_comp) {
		goto error;
	}

	auto_name_component(cfg, "filter", muxer_cfg_comp);
	g_ptr_array_add(cfg->cmd_data.convert.filters, bt_get(muxer_cfg_comp));

	/* Connect all sources to this mux */
	for (i = 0; i < cfg->cmd_data.convert.sources->len; i++) {
		struct bt_config_component *comp =
			g_ptr_array_index(cfg->cmd_data.convert.sources, i);
		struct bt_config_connection *cfg_connection =
			bt_config_connection_create_full(
				comp->instance_name->str, "",
				muxer_cfg_comp->instance_name->str, "",
				"(auto)");

		if (!cfg_connection) {
			goto error;
		}

		g_ptr_array_add(cfg->cmd_data.convert.connections,
			cfg_connection);
	}

	/* Connect this mux to the filter components, in order */
	last_filter_comp_name = muxer_cfg_comp->instance_name->str;

	for (i = 0; i < cfg->cmd_data.convert.filters->len - 1; i++) {
		struct bt_config_component *comp =
			g_ptr_array_index(cfg->cmd_data.convert.filters, i);
		struct bt_config_connection *cfg_connection;

		cfg_connection = bt_config_connection_create_full(
				last_filter_comp_name, "",
				comp->instance_name->str, "",
				"(auto)");

		if (!cfg_connection) {
			goto error;
		}

		g_ptr_array_add(cfg->cmd_data.convert.connections,
			cfg_connection);
		last_filter_comp_name = comp->instance_name->str;
	}

	/* Connect the last filter component to all sink components */
	for (i = 0; i < cfg->cmd_data.convert.sinks->len; i++) {
		struct bt_config_component *comp =
			g_ptr_array_index(cfg->cmd_data.convert.sinks, i);
		struct bt_config_connection *cfg_connection =
			bt_config_connection_create_full(
				last_filter_comp_name, "",
				comp->instance_name->str, "",
				"(auto)");

		if (!cfg_connection) {
			goto error;
		}

		g_ptr_array_add(cfg->cmd_data.convert.connections,
			cfg_connection);
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(muxer_cfg_comp);
	return ret;
}

int bt_config_create_connections(struct bt_config *cfg,
		struct bt_value *connection_args,
		char *error_buf, size_t error_buf_size)
{
	int ret;
	size_t i;

	if (bt_value_array_is_empty(connection_args)) {
		/* No explicit connections: do automatic connection */
		ret = auto_connect(cfg);
		if (ret) {
			goto error;
		}
	}

	if (!all_named(cfg)) {
		snprintf(error_buf, error_buf_size, "At least one connection (--connect) specified, but not all component\ninstances are named (use --name)\n");
		goto error;
	}

	for (i = 0; i < bt_value_array_size(connection_args); i++) {
		struct bt_value *arg_value =
			bt_value_array_get(connection_args, i);
		const char *arg;
		struct bt_config_connection *cfg_connection;

		ret = bt_value_string_get(arg_value, &arg);
		BT_PUT(arg_value);
		assert(ret == 0);
		cfg_connection = cfg_connection_from_arg(arg);
		if (!cfg_connection) {
			snprintf(error_buf, error_buf_size, "Cannot parse --connect option's argument:\n    %s\n",
				arg);
			goto error;
		}

		g_ptr_array_add(cfg->cmd_data.convert.connections,
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
