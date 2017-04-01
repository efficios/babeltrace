/*
 * babeltrace.c
 *
 * Babeltrace Trace Converter
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/plugin/plugin.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-source.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <stdlib.h>
#include <popt.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <inttypes.h>
#include "babeltrace-cfg.h"
#include "babeltrace-cfg-connect.h"
#include "default-cfg.h"

GPtrArray *loaded_plugins;

static
void init_loaded_plugins_array(void)
{
	loaded_plugins = g_ptr_array_new_full(8, bt_put);
}

static
void fini_loaded_plugins_array(void)
{
	g_ptr_array_free(loaded_plugins, TRUE);
}

static
struct bt_plugin *find_plugin(const char *name)
{
	int i;
	struct bt_plugin *plugin = NULL;

	for (i = 0; i < loaded_plugins->len; i++) {
		plugin = g_ptr_array_index(loaded_plugins, i);

		if (strcmp(name, bt_plugin_get_name(plugin)) == 0) {
			break;
		}

		plugin = NULL;
	}

	return bt_get(plugin);
}

static
struct bt_component_class *find_component_class(const char *plugin_name,
		const char *comp_class_name,
		enum bt_component_class_type comp_class_type)
{
	struct bt_component_class *comp_class = NULL;
	struct bt_plugin *plugin = find_plugin(plugin_name);

	if (!plugin) {
		goto end;
	}

	comp_class = bt_plugin_get_component_class_by_name_and_type(plugin,
			comp_class_name, comp_class_type);
	BT_PUT(plugin);
end:
	return comp_class;
}

static
void print_indent(size_t indent)
{
	size_t i;

	for (i = 0; i < indent; i++) {
		printf(" ");
	}
}

static char *escape_name_for_shell(const char *input)
{
	char *output = g_malloc0(strlen(input) * 5 + 1);
	const char *in;
	char *out = output;

	if (!output) {
		goto end;
	}

	for (in = input; *in != '\0'; in++) {
		switch (*in) {
		case '\\':
			*out++ = '\\';
			*out++ = '\\';
			break;
		case '\'':
			*out++ = '\'';
			*out++ = '"';
			*out++ = '\'';
			*out++ = '"';
			*out++ = '\'';
			break;
		case '.':
			*out++ = '\\';
			*out++ = '.';
			break;
		default:
			*out++ = *in;
			break;
		}
	}

end:
	return output;
}

static
const char *component_type_str(enum bt_component_class_type type)
{
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		return "source";
	case BT_COMPONENT_CLASS_TYPE_SINK:
		return "sink";
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		return "filter";
	case BT_COMPONENT_CLASS_TYPE_UNKNOWN:
	default:
		return "unknown";
	}
}

static void print_plugin_comp_cls_opt(FILE *fh, const char *plugin_name,
		const char *comp_cls_name, enum bt_component_class_type type)
{
	char *shell_plugin_name = NULL;
	char *shell_comp_cls_name = NULL;

	shell_plugin_name = escape_name_for_shell(plugin_name);
	if (!shell_plugin_name) {
		goto end;
	}

	shell_comp_cls_name = escape_name_for_shell(comp_cls_name);
	if (!shell_comp_cls_name) {
		goto end;
	}

	fprintf(fh, "%s%s--%s%s %s'%s%s%s%s.%s%s%s'",
		bt_common_color_bold(),
		bt_common_color_fg_cyan(),
		component_type_str(type),
		bt_common_color_reset(),
		bt_common_color_fg_default(),
		bt_common_color_bold(),
		bt_common_color_fg_blue(),
		shell_plugin_name,
		bt_common_color_fg_default(),
		bt_common_color_fg_yellow(),
		shell_comp_cls_name,
		bt_common_color_reset());

end:
	g_free(shell_plugin_name);
	g_free(shell_comp_cls_name);
}

static
void print_value(struct bt_value *, size_t);

static
void print_value_rec(struct bt_value *, size_t);

static
bool print_map_value(const char *key, struct bt_value *object, void *data)
{
	size_t *indent = data;

	print_indent(*indent);
	printf("%s: ", key);

	if (bt_value_is_array(object) &&
			bt_value_array_is_empty(object)) {
		printf("[ ]\n");
		return true;
	}

	if (bt_value_is_map(object) &&
			bt_value_map_is_empty(object)) {
		printf("{ }\n");
		return true;
	}

	if (bt_value_is_array(object) ||
			bt_value_is_map(object)) {
		printf("\n");
	}

	print_value_rec(object, *indent + 2);
	return true;
}

static
void print_value_rec(struct bt_value *value, size_t indent)
{
	bool bool_val;
	int64_t int_val;
	double dbl_val;
	const char *str_val;
	int size;
	int i;

	if (!value) {
		return;
	}

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_NULL:
		printf("%snull%s\n", bt_common_color_bold(),
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_BOOL:
		bt_value_bool_get(value, &bool_val);
		printf("%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_cyan(), bool_val ? "yes" : "no",
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_INTEGER:
		bt_value_integer_get(value, &int_val);
		printf("%s%s%" PRId64 "%s\n", bt_common_color_bold(),
			bt_common_color_fg_red(), int_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_FLOAT:
		bt_value_float_get(value, &dbl_val);
		printf("%s%s%lf%s\n", bt_common_color_bold(),
			bt_common_color_fg_red(), dbl_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_STRING:
		bt_value_string_get(value, &str_val);
		printf("%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_green(), str_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_ARRAY:
		size = bt_value_array_size(value);
		assert(size >= 0);

		if (size == 0) {
			print_indent(indent);
			printf("[ ]\n");
			break;
		}

		for (i = 0; i < size; i++) {
			struct bt_value *element =
					bt_value_array_get(value, i);

			assert(element);
			print_indent(indent);
			printf("- ");

			if (bt_value_is_array(element) &&
					bt_value_array_is_empty(element)) {
				printf("[ ]\n");
				continue;
			}

			if (bt_value_is_map(element) &&
					bt_value_map_is_empty(element)) {
				printf("{ }\n");
				continue;
			}

			if (bt_value_is_array(element) ||
					bt_value_is_map(element)) {
				printf("\n");
			}

			print_value_rec(element, indent + 2);
			BT_PUT(element);
		}
		break;
	case BT_VALUE_TYPE_MAP:
		if (bt_value_map_is_empty(value)) {
			print_indent(indent);
			printf("{ }\n");
			break;
		}

		bt_value_map_foreach(value, print_map_value, &indent);
		break;
	default:
		assert(false);
	}
}

static
void print_value(struct bt_value *value, size_t indent)
{
	if (!bt_value_is_array(value) && !bt_value_is_map(value)) {
		print_indent(indent);
	}

	print_value_rec(value, indent);
}

static
void print_bt_config_component(struct bt_config_component *bt_config_component)
{
	printf("    ");
	print_plugin_comp_cls_opt(stdout, bt_config_component->plugin_name->str,
		bt_config_component->comp_cls_name->str,
		bt_config_component->type);
	printf(":\n");

	if (bt_config_component->instance_name->len > 0) {
		printf("      Name: %s\n",
			bt_config_component->instance_name->str);
	}

	printf("      Parameters:\n");
	print_value(bt_config_component->params, 8);
}

static
void print_bt_config_components(GPtrArray *array)
{
	size_t i;

	for (i = 0; i < array->len; i++) {
		struct bt_config_component *cfg_component =
				bt_config_get_component(array, i);
		print_bt_config_component(cfg_component);
		BT_PUT(cfg_component);
	}
}

static
void print_plugin_paths(struct bt_value *plugin_paths)
{
	printf("  Plugin paths:\n");
	print_value(plugin_paths, 4);
}

static
void print_cfg_run(struct bt_config *cfg)
{
	size_t i;

	print_plugin_paths(cfg->plugin_paths);
	printf("  Source component instances:\n");
	print_bt_config_components(cfg->cmd_data.run.sources);

	if (cfg->cmd_data.run.filters->len > 0) {
		printf("  Filter component instances:\n");
		print_bt_config_components(cfg->cmd_data.run.filters);
	}

	printf("  Sink component instances:\n");
	print_bt_config_components(cfg->cmd_data.run.sinks);
	printf("  Connections:\n");

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *cfg_connection =
			g_ptr_array_index(cfg->cmd_data.run.connections,
				i);

		printf("    %s%s%s -> %s%s%s\n",
			cfg_connection->src_instance_name->str,
			cfg_connection->src_port_name->len > 0 ? "." : "",
			cfg_connection->src_port_name->str,
			cfg_connection->dst_instance_name->str,
			cfg_connection->dst_port_name->len > 0 ? "." : "",
			cfg_connection->dst_port_name->str);
	}
}

static
void print_cfg_list_plugins(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
}

static
void print_cfg_help(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
}

static
void print_cfg_print_ctf_metadata(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
	printf("  Path: %s\n", cfg->cmd_data.print_ctf_metadata.path->str);
}

static
void print_cfg_print_lttng_live_sessions(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
	printf("  URL: %s\n", cfg->cmd_data.print_lttng_live_sessions.url->str);
}

static
void print_cfg_query(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
	printf("  Object: `%s`\n", cfg->cmd_data.query.object->str);
	printf("  Component class:\n");
	print_bt_config_component(cfg->cmd_data.query.cfg_component);
}

static
void print_cfg(struct bt_config *cfg)
{
	if (!babeltrace_verbose) {
		return;
	}

	printf("Configuration:\n");
	printf("  Debug mode: %s\n", cfg->debug ? "yes" : "no");
	printf("  Verbose mode: %s\n", cfg->verbose ? "yes" : "no");

	switch (cfg->command) {
	case BT_CONFIG_COMMAND_RUN:
		print_cfg_run(cfg);
		break;
	case BT_CONFIG_COMMAND_LIST_PLUGINS:
		print_cfg_list_plugins(cfg);
		break;
	case BT_CONFIG_COMMAND_HELP:
		print_cfg_help(cfg);
		break;
	case BT_CONFIG_COMMAND_QUERY:
		print_cfg_query(cfg);
		break;
	case BT_CONFIG_COMMAND_PRINT_CTF_METADATA:
		print_cfg_print_ctf_metadata(cfg);
		break;
	case BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS:
		print_cfg_print_lttng_live_sessions(cfg);
		break;
	default:
		assert(false);
	}
}

static
struct bt_component *create_trimmer(struct bt_config_component *source_cfg)
{
	struct bt_component *trimmer = NULL;
	struct bt_component_class *trimmer_class = NULL;
	struct bt_value *trimmer_params = NULL;
	struct bt_value *value;

	trimmer_params = bt_value_map_create();
	if (!trimmer_params) {
		goto end;
	}

	value = bt_value_map_get(source_cfg->params, "begin");
	if (value) {
	        enum bt_value_status ret;

		ret = bt_value_map_insert(trimmer_params, "begin",
				value);
	        BT_PUT(value);
		if (ret) {
			goto end;
		}
	}
	value = bt_value_map_get(source_cfg->params, "end");
	if (value) {
		enum bt_value_status ret;

		ret = bt_value_map_insert(trimmer_params, "end",
				value);
		BT_PUT(value);
		if (ret) {
			goto end;
		}
	}
	value = bt_value_map_get(source_cfg->params, "clock-gmt");
	if (value) {
		enum bt_value_status ret;

		ret = bt_value_map_insert(trimmer_params, "clock-gmt",
				value);
		BT_PUT(value);
		if (ret) {
			goto end;
		}
	}

	trimmer_class = find_component_class("utils", "trimmer",
		BT_COMPONENT_CLASS_TYPE_FILTER);
	if (!trimmer_class) {
		fprintf(stderr, "Could not find trimmer component class. Aborting...\n");
		goto end;
	}
	trimmer = bt_component_create(trimmer_class, "source_trimmer",
			trimmer_params);
	if (!trimmer) {
		goto end;
	}
end:
	bt_put(trimmer_params);
	bt_put(trimmer_class);
	return trimmer;
}

static
int connect_source_sink(struct bt_graph *graph,
		struct bt_component *source,
		struct bt_config_component *source_cfg,
		struct bt_component *sink)
{
	int ret = 0;
	struct bt_connection *connection = NULL;
	struct bt_component *trimmer = NULL;
	struct bt_port *source_port =
			bt_component_source_get_default_output_port(source);
	struct bt_port *sink_port =
			bt_component_sink_get_default_input_port(sink);
	struct bt_port *to_sink_port = NULL;
	struct bt_port *trimmer_input_port = NULL;

	if (!source_port) {
		fprintf(stderr, "Failed to find default source output port. Aborting...\n");
		ret = -1;
		goto end;
	}
	if (!sink_port) {
		fprintf(stderr, "Failed to find default sink input port. Aborting...\n");
		ret = -1;
		goto end;
	}

	if (bt_value_map_has_key(source_cfg->params, "begin")
			|| bt_value_map_has_key(source_cfg->params, "end")) {
		/* A trimmer must be inserted in the graph. */
		trimmer = create_trimmer(source_cfg);
		if (!trimmer) {
			fprintf(stderr, "Failed to create trimmer component. Aborting...\n");
			ret = -1;
			goto end;
		}

		trimmer_input_port = bt_component_filter_get_default_input_port(
				trimmer);
		if (!trimmer_input_port) {
			fprintf(stderr, "Failed to find trimmer input port. Aborting...\n");
			ret = -1;
			goto end;
		}
		to_sink_port = bt_component_filter_get_default_output_port(
				trimmer);
		if (!to_sink_port) {
			fprintf(stderr, "Failed to find trimmer output port. Aborting...\n");
			ret = -1;
			goto end;
		}

		connection = bt_graph_connect_ports(graph, source_port,
				trimmer_input_port);
		if (!connection) {
			fprintf(stderr, "Failed to connect source to trimmer. Aborting...\n");
			ret = -1;
			goto end;
		}
		BT_PUT(connection);
	} else {
		BT_MOVE(to_sink_port, source_port);
	}

	connection = bt_graph_connect_ports(graph, to_sink_port, sink_port);
	if (!connection) {
		fprintf(stderr, "Failed to connect to sink. Aborting...\n");
		ret = -1;
		goto end;
	}
end:
	bt_put(trimmer);
	bt_put(source_port);
	bt_put(sink_port);
	bt_put(to_sink_port);
	bt_put(connection);
	return ret;
}

static
void add_to_loaded_plugins(struct bt_plugin **plugins)
{
	while (*plugins) {
		struct bt_plugin *plugin = *plugins;
		/* Check if it's already loaded (from another path). */
		struct bt_plugin *loaded_plugin =
				find_plugin(bt_plugin_get_name(plugin));

		if (loaded_plugin) {
			printf_verbose("Not loading plugin `%s`: already loaded from `%s`\n",
					bt_plugin_get_path(plugin),
					bt_plugin_get_path(loaded_plugin));
			BT_PUT(loaded_plugin);
			BT_PUT(plugin);
		} else {
			/* Transfer ownership to global array. */
			g_ptr_array_add(loaded_plugins, plugin);
		}
		*(plugins++) = NULL;
	}
}

static
int load_dynamic_plugins(struct bt_value *plugin_paths)
{
	int nr_paths, i, ret = 0;

	nr_paths = bt_value_array_size(plugin_paths);
	if (nr_paths < 0) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < nr_paths; i++) {
		struct bt_value *plugin_path_value = NULL;
		const char *plugin_path;
		struct bt_plugin **plugins;

		plugin_path_value = bt_value_array_get(plugin_paths, i);
		if (bt_value_string_get(plugin_path_value,
				&plugin_path)) {
			BT_PUT(plugin_path_value);
			continue;
		}

		plugins = bt_plugin_create_all_from_dir(plugin_path, false);
		if (!plugins) {
			printf_debug("Unable to dynamically load plugins from path %s.\n",
				plugin_path);
			BT_PUT(plugin_path_value);
			continue;
		}

		add_to_loaded_plugins(plugins);
		free(plugins);

		BT_PUT(plugin_path_value);
	}
end:
	return ret;
}

static
int load_static_plugins(void)
{
	int ret = 0;
	struct bt_plugin **plugins;

	plugins = bt_plugin_create_all_from_static();
	if (!plugins) {
		printf_debug("Unable to load static plugins.\n");
		ret = -1;
		goto end;
	}

	add_to_loaded_plugins(plugins);
	free(plugins);
end:
	return ret;
}

static int load_all_plugins(struct bt_value *plugin_paths)
{
	int ret = 0;

	if (load_dynamic_plugins(plugin_paths)) {
		fprintf(stderr, "Failed to load dynamic plugins.\n");
		ret = -1;
		goto end;
	}

	if (load_static_plugins()) {
		fprintf(stderr, "Failed to load static plugins.\n");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static void print_plugin_info(struct bt_plugin *plugin)
{
	unsigned int major, minor, patch;
	const char *extra;
	enum bt_plugin_status version_status;
	const char *plugin_name;
	const char *path;
	const char *author;
	const char *license;
	const char *plugin_description;

	plugin_name = bt_plugin_get_name(plugin);
	path = bt_plugin_get_path(plugin);
	author = bt_plugin_get_author(plugin);
	license = bt_plugin_get_license(plugin);
	plugin_description = bt_plugin_get_description(plugin);
	version_status = bt_plugin_get_version(plugin, &major, &minor,
		&patch, &extra);
	printf("%s%s%s%s:\n", bt_common_color_bold(),
		bt_common_color_fg_blue(), plugin_name,
		bt_common_color_reset());
	printf("  %sPath%s: %s\n", bt_common_color_bold(),
		bt_common_color_reset(), path ? path : "(None)");

	if (version_status == BT_PLUGIN_STATUS_OK) {
		printf("  %sVersion%s: %u.%u.%u",
			bt_common_color_bold(), bt_common_color_reset(),
			major, minor, patch);

		if (extra) {
			printf("%s", extra);
		}

		printf("\n");
	}

	printf("  %sDescription%s: %s\n", bt_common_color_bold(),
		bt_common_color_reset(),
		plugin_description ? plugin_description : "(None)");
	printf("  %sAuthor%s: %s\n", bt_common_color_bold(),
		bt_common_color_reset(), author ? author : "(Unknown)");
	printf("  %sLicense%s: %s\n", bt_common_color_bold(),
		bt_common_color_reset(),
		license ? license : "(Unknown)");
}

static int cmd_query(struct bt_config *cfg)
{
	int ret;
	struct bt_component_class *comp_cls = NULL;
	struct bt_value *results = NULL;

	ret = load_all_plugins(cfg->plugin_paths);
	if (ret) {
		goto end;
	}

	comp_cls = find_component_class(cfg->cmd_data.query.cfg_component->plugin_name->str,
		cfg->cmd_data.query.cfg_component->comp_cls_name->str,
		cfg->cmd_data.query.cfg_component->type);
	if (!comp_cls) {
		fprintf(stderr, "%s%sCannot find component class %s",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			bt_common_color_reset());
		print_plugin_comp_cls_opt(stderr,
			cfg->cmd_data.query.cfg_component->plugin_name->str,
			cfg->cmd_data.query.cfg_component->comp_cls_name->str,
			cfg->cmd_data.query.cfg_component->type);
		fprintf(stderr, "\n");
		ret = -1;
		goto end;
	}

	results = bt_component_class_query(comp_cls,
		cfg->cmd_data.query.object->str,
		cfg->cmd_data.query.cfg_component->params);
	if (!results) {
		fprintf(stderr, "%s%sFailed to query info to %s",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			bt_common_color_reset());
		print_plugin_comp_cls_opt(stderr,
			cfg->cmd_data.query.cfg_component->plugin_name->str,
			cfg->cmd_data.query.cfg_component->comp_cls_name->str,
			cfg->cmd_data.query.cfg_component->type);
		fprintf(stderr, "%s%s with object `%s`%s\n",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			cfg->cmd_data.query.object->str,
			bt_common_color_reset());
		ret = -1;
		goto end;
	}

	print_value(results, 0);

end:
	bt_put(comp_cls);
	bt_put(results);
	return ret;
}

static int cmd_help(struct bt_config *cfg)
{
	int ret;
	struct bt_plugin *plugin = NULL;
	size_t i;

	ret = load_all_plugins(cfg->plugin_paths);
	if (ret) {
		goto end;
	}

	plugin = find_plugin(cfg->cmd_data.help.cfg_component->plugin_name->str);
	if (!plugin) {
		fprintf(stderr, "%s%sCannot find plugin %s%s%s\n",
			bt_common_color_bold(), bt_common_color_fg_red(),
			bt_common_color_fg_blue(),
			cfg->cmd_data.help.cfg_component->plugin_name->str,
			bt_common_color_reset());
		ret = -1;
		goto end;
	}

	print_plugin_info(plugin);
	printf("  %sComponent classes%s: %d\n",
			bt_common_color_bold(),
			bt_common_color_reset(),
			bt_plugin_get_component_class_count(plugin));


	if (cfg->cmd_data.help.cfg_component->type !=
			BT_COMPONENT_CLASS_TYPE_UNKNOWN) {
		struct bt_component_class *needed_comp_cls =
			find_component_class(
				cfg->cmd_data.help.cfg_component->plugin_name->str,
				cfg->cmd_data.help.cfg_component->comp_cls_name->str,
				cfg->cmd_data.help.cfg_component->type);

		if (!needed_comp_cls) {
			fprintf(stderr, "\n%s%sCannot find component class %s",
				bt_common_color_bold(),
				bt_common_color_fg_red(),
				bt_common_color_reset());
			print_plugin_comp_cls_opt(stderr,
				cfg->cmd_data.help.cfg_component->plugin_name->str,
				cfg->cmd_data.help.cfg_component->comp_cls_name->str,
				cfg->cmd_data.help.cfg_component->type);
			fprintf(stderr, "\n");
			ret = -1;
			goto end;
		}

		bt_put(needed_comp_cls);
	}

	for (i = 0; i < bt_plugin_get_component_class_count(plugin); i++) {
		struct bt_component_class *comp_cls =
			bt_plugin_get_component_class(plugin, i);
		const char *comp_class_name =
			bt_component_class_get_name(comp_cls);
		const char *comp_class_description =
			bt_component_class_get_description(comp_cls);
		const char *comp_class_help =
			bt_component_class_get_help(comp_cls);
		enum bt_component_class_type type =
			bt_component_class_get_type(comp_cls);

		assert(comp_cls);

		if (cfg->cmd_data.help.cfg_component->type !=
				BT_COMPONENT_CLASS_TYPE_UNKNOWN) {
			if (strcmp(cfg->cmd_data.help.cfg_component->comp_cls_name->str,
					comp_class_name) != 0 &&
					type ==
					cfg->cmd_data.help.cfg_component->type) {
				bt_put(comp_cls);
				continue;
			}
		}

		printf("\n");
		print_plugin_comp_cls_opt(stdout,
			cfg->cmd_data.help.cfg_component->plugin_name->str,
			comp_class_name,
			type);
		printf("\n");
		printf("  %sDescription%s: %s\n", bt_common_color_bold(),
			bt_common_color_reset(),
			comp_class_description ? comp_class_description : "(None)");

		if (comp_class_help) {
			printf("\n%s\n", comp_class_help);
		}

		bt_put(comp_cls);
	}

end:
	bt_put(plugin);
	return ret;
}

static int cmd_list_plugins(struct bt_config *cfg)
{
	int ret;
	int plugins_count, component_classes_count = 0, i;

	ret = load_all_plugins(cfg->plugin_paths);
	if (ret) {
		goto end;
	}

	printf("From the following plugin paths:\n\n");
	print_value(cfg->plugin_paths, 2);
	printf("\n");
	plugins_count = loaded_plugins->len;
	if (plugins_count == 0) {
		fprintf(stderr, "%s%sNo plugins found.%s\n",
			bt_common_color_bold(), bt_common_color_fg_red(),
			bt_common_color_reset());
		fprintf(stderr, "\n");
		fprintf(stderr, "Please make sure your plugin search path is set correctly. You can use\n");
		fprintf(stderr, "the --plugin-path command-line option or the BABELTRACE_PLUGIN_PATH\n");
		fprintf(stderr, "environment variable.\n");
		ret = -1;
		goto end;
	}

	for (i = 0; i < plugins_count; i++) {
		struct bt_plugin *plugin = g_ptr_array_index(loaded_plugins, i);

		component_classes_count += bt_plugin_get_component_class_count(plugin);
	}

	printf("Found %s%d%s component classes in %s%d%s plugins.\n",
		bt_common_color_bold(),
		component_classes_count,
		bt_common_color_reset(),
		bt_common_color_bold(),
		plugins_count,
		bt_common_color_reset());

	for (i = 0; i < plugins_count; i++) {
		int j;
		struct bt_plugin *plugin = g_ptr_array_index(loaded_plugins, i);

		component_classes_count =
			bt_plugin_get_component_class_count(plugin);
		printf("\n");
		print_plugin_info(plugin);

		if (component_classes_count == 0) {
			printf("  %sComponent classes%s: (None)\n",
				bt_common_color_bold(),
				bt_common_color_reset());
		} else {
			printf("  %sComponent classes%s:\n",
				bt_common_color_bold(),
				bt_common_color_reset());
		}

		for (j = 0; j < component_classes_count; j++) {
			struct bt_component_class *comp_class =
				bt_plugin_get_component_class(plugin, j);
			const char *comp_class_name =
				bt_component_class_get_name(comp_class);
			const char *comp_class_description =
				bt_component_class_get_description(comp_class);
			enum bt_component_class_type type =
				bt_component_class_get_type(comp_class);

			printf("    ");
			print_plugin_comp_cls_opt(stdout,
				bt_plugin_get_name(plugin), comp_class_name,
				type);

			if (comp_class_description) {
				printf(": %s", comp_class_description);
			}

			printf("\n");
			bt_put(comp_class);
		}
	}

end:
	return ret;
}

static int cmd_print_lttng_live_sessions(struct bt_config *cfg)
{
	printf("TODO\n");
	return -1;
}

static int cmd_print_ctf_metadata(struct bt_config *cfg)
{
	int ret = 0;
	struct bt_component_class *comp_cls = NULL;
	struct bt_value *results = NULL;
	struct bt_value *params = NULL;
	struct bt_value *metadata_text_value = NULL;
	const char *metadata_text = NULL;
	static const char * const plugin_name = "ctf";
	static const char * const comp_cls_name = "fs";
	static const enum bt_component_class_type comp_cls_type =
		BT_COMPONENT_CLASS_TYPE_SOURCE;

	assert(cfg->cmd_data.print_ctf_metadata.path);
	comp_cls = find_component_class(plugin_name, comp_cls_name,
		comp_cls_type);
	if (!comp_cls) {
		fprintf(stderr, "%s%sCannot find component class %s",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			bt_common_color_reset());
		print_plugin_comp_cls_opt(stderr, plugin_name,
			comp_cls_name, comp_cls_type);
		fprintf(stderr, "\n");
		ret = -1;
		goto end;
	}

	params = bt_value_map_create();
	if (!params) {
		ret = -1;
		goto end;
	}

	ret = bt_value_map_insert_string(params, "path",
		cfg->cmd_data.print_ctf_metadata.path->str);
	if (ret) {
		ret = -1;
		goto end;
	}

	results = bt_component_class_query(comp_cls, "metadata-info",
		params);
	if (!results) {
		ret = -1;
		fprintf(stderr, "%s%sFailed to request metadata info%s\n",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			bt_common_color_reset());
		goto end;
	}

	metadata_text_value = bt_value_map_get(results, "text");
	if (!metadata_text_value) {
		ret = -1;
		goto end;
	}

	ret = bt_value_string_get(metadata_text_value, &metadata_text);
	assert(ret == 0);
	printf("%s\n", metadata_text);

end:
	bt_put(results);
	bt_put(params);
	bt_put(metadata_text_value);
	bt_put(comp_cls);
	return 0;
}

static int cmd_run(struct bt_config *cfg)
{
	int ret = 0;
	struct bt_component_class *source_class = NULL;
	struct bt_component_class *sink_class = NULL;
	struct bt_component *source = NULL, *sink = NULL;
	struct bt_value *source_params = NULL, *sink_params = NULL;
	struct bt_config_component *source_cfg = NULL, *sink_cfg = NULL;
	struct bt_graph *graph = NULL;

	ret = load_all_plugins(cfg->plugin_paths);
	if (ret) {
		goto end;
	}

	/* TODO handle more than 1 source and 1 sink. */
	if (cfg->cmd_data.run.sources->len != 1 ||
			cfg->cmd_data.run.sinks->len != 1) {
		fprintf(stderr, "Only one source and one sink component class are supported. Aborting...\n");
		ret = -1;
		goto end;
	}

	source_cfg = bt_config_get_component(cfg->cmd_data.run.sources, 0);
	source_params = bt_get(source_cfg->params);
	source_class = find_component_class(source_cfg->plugin_name->str,
			source_cfg->comp_cls_name->str,
			BT_COMPONENT_CLASS_TYPE_SOURCE);
	if (!source_class) {
		fprintf(stderr, "Could not find ");
		print_plugin_comp_cls_opt(stderr, source_cfg->plugin_name->str,
			source_cfg->comp_cls_name->str, BT_COMPONENT_CLASS_TYPE_SOURCE);
		fprintf(stderr, ". Aborting...\n");
		ret = -1;
		goto end;
	}

	sink_cfg = bt_config_get_component(cfg->cmd_data.run.sinks, 0);
	sink_params = bt_get(sink_cfg->params);
	sink_class = find_component_class(sink_cfg->plugin_name->str,
			sink_cfg->comp_cls_name->str,
			BT_COMPONENT_CLASS_TYPE_SINK);
	if (!sink_class) {
		fprintf(stderr, "Could not find ");
		print_plugin_comp_cls_opt(stderr, sink_cfg->plugin_name->str,
			sink_cfg->comp_cls_name->str, BT_COMPONENT_CLASS_TYPE_SINK);
		fprintf(stderr, ". Aborting...\n");
		ret = -1;
		goto end;
	}

	graph = bt_graph_create();
	if (!graph) {
		ret = -1;
		goto end;
	}

	source = bt_component_create(source_class, "source", source_params);
	if (!source) {
		fprintf(stderr, "Failed to instantiate selected source component. Aborting...\n");
                ret = -1;
                goto end;
        }

	sink = bt_component_create(sink_class, "sink", sink_params);
	if (!sink) {
		fprintf(stderr, "Failed to instantiate selected output component. Aborting...\n");
		ret = -1;
		goto end;
	}

	ret = connect_source_sink(graph, source, source_cfg, sink);
	if (ret) {
		ret = -1;
		goto end;
	}

	while (true) {
		enum bt_graph_status graph_status;

		graph_status = bt_graph_run(graph);
		switch (graph_status) {
		case BT_GRAPH_STATUS_AGAIN:
			/* Wait for an arbitraty 500 ms. */
			usleep(500000);
			break;
		case BT_COMPONENT_STATUS_END:
			goto end;
		default:
			fprintf(stderr, "Sink component returned an error, aborting...\n");
			ret = -1;
			goto end;
		}
	}

end:
	bt_put(sink_class);
	bt_put(source_class);
	bt_put(source);
	bt_put(sink);
	bt_put(source_params);
	bt_put(sink_params);
	bt_put(sink_cfg);
	bt_put(source_cfg);
	bt_put(graph);
	return ret;
}

static void warn_command_name_and_directory_clash(struct bt_config *cfg)
{
	if (!cfg->command_name) {
		return;
	}

	if (g_file_test(cfg->command_name,
			G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		fprintf(stderr, "\nNOTE: The `%s` command was executed. If you meant to convert a\n",
			cfg->command_name);
		fprintf(stderr, "trace located in the local `%s` directory, please use:\n",
			cfg->command_name);
		fprintf(stderr, "\n");
		fprintf(stderr, "    babeltrace convert %s [OPTIONS]\n",
			cfg->command_name);
	}
}

int main(int argc, const char **argv)
{
	int ret;
	int retcode;
	struct bt_config *cfg;

	init_loaded_plugins_array();
	cfg = bt_config_from_args_with_defaults(argc, argv, &retcode);

	if (retcode < 0) {
		/* Quit without errors; typically usage/version */
		retcode = 0;
		goto end;
	}

	if (retcode > 0) {
		goto end;
	}

	if (!cfg) {
		fprintf(stderr, "Failed to create Babeltrace configuration\n");
		retcode = 1;
		goto end;
	}

	babeltrace_debug = cfg->debug;
	babeltrace_verbose = cfg->verbose;
	print_cfg(cfg);

	if (cfg->command_needs_plugins) {
		ret = load_all_plugins(cfg->plugin_paths);
		if (ret) {
			retcode = 1;
			goto end;
		}
	}

	switch (cfg->command) {
	case BT_CONFIG_COMMAND_RUN:
		ret = cmd_run(cfg);
		break;
	case BT_CONFIG_COMMAND_LIST_PLUGINS:
		ret = cmd_list_plugins(cfg);
		break;
	case BT_CONFIG_COMMAND_HELP:
		ret = cmd_help(cfg);
		break;
	case BT_CONFIG_COMMAND_QUERY:
		ret = cmd_query(cfg);
		break;
	case BT_CONFIG_COMMAND_PRINT_CTF_METADATA:
		ret = cmd_print_ctf_metadata(cfg);
		break;
	case BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS:
		ret = cmd_print_lttng_live_sessions(cfg);
		break;
	default:
		assert(false);
	}

	warn_command_name_and_directory_clash(cfg);
	retcode = ret ? 1 : 0;

end:
	BT_PUT(cfg);
	fini_loaded_plugins_array();
	return retcode;
}
