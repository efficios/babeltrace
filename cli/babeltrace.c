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
#include <babeltrace/logging.h>
#include <unistd.h>
#include <stdlib.h>
#include <popt.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include "babeltrace-cfg.h"
#include "babeltrace-cfg-cli-args.h"
#include "babeltrace-cfg-cli-args-default.h"

#define BT_LOG_TAG "CLI"
#include "logging.h"

#define ENV_BABELTRACE_WARN_COMMAND_NAME_DIRECTORY_CLASH "BABELTRACE_CLI_WARN_COMMAND_NAME_DIRECTORY_CLASH"

/* Application's processing graph (weak) */
static struct bt_graph *the_graph;
static bool canceled = false;

GPtrArray *loaded_plugins;

BT_HIDDEN
int bt_cli_log_level = BT_LOG_NONE;

static
void sigint_handler(int signum)
{
	if (signum != SIGINT) {
		return;
	}

	if (the_graph) {
		bt_graph_cancel(the_graph);
	}

	canceled = true;
}

static
void init_static_data(void)
{
	loaded_plugins = g_ptr_array_new_with_free_func(bt_put);
}

static
void fini_static_data(void)
{
	g_ptr_array_free(loaded_plugins, TRUE);
}

static
struct bt_plugin *find_plugin(const char *name)
{
	int i;
	struct bt_plugin *plugin = NULL;

	assert(name);
	BT_LOGD("Finding plugin: name=\"%s\"", name);

	for (i = 0; i < loaded_plugins->len; i++) {
		plugin = g_ptr_array_index(loaded_plugins, i);

		if (strcmp(name, bt_plugin_get_name(plugin)) == 0) {
			break;
		}

		plugin = NULL;
	}

	if (BT_LOG_ON_DEBUG) {
		if (plugin) {
			BT_LOGD("Found plugin: plugin-addr=%p", plugin);
		} else {
			BT_LOGD("Cannot find plugin.");
		}
	}

	return bt_get(plugin);
}

static
struct bt_component_class *find_component_class(const char *plugin_name,
		const char *comp_class_name,
		enum bt_component_class_type comp_class_type)
{
	struct bt_component_class *comp_class = NULL;
	struct bt_plugin *plugin;

	BT_LOGD("Finding component class: plugin-name=\"%s\", "
		"comp-cls-name=\"%s\", comp-cls-type=%d",
		plugin_name, comp_class_name, comp_class_type);

	plugin = find_plugin(plugin_name);

	if (!plugin) {
		goto end;
	}

	comp_class = bt_plugin_get_component_class_by_name_and_type(plugin,
			comp_class_name, comp_class_type);
	BT_PUT(plugin);

end:
	if (BT_LOG_ON_DEBUG) {
		if (comp_class) {
			BT_LOGD("Found component class: comp-cls-addr=%p",
				comp_class);
		} else {
			BT_LOGD("Cannot find component class.");
		}
	}

	return comp_class;
}

static
void print_indent(FILE *fp, size_t indent)
{
	size_t i;

	for (i = 0; i < indent; i++) {
		fprintf(fp, " ");
	}
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

static
void print_plugin_comp_cls_opt(FILE *fh, const char *plugin_name,
		const char *comp_cls_name, enum bt_component_class_type type)
{
	GString *shell_plugin_name = NULL;
	GString *shell_comp_cls_name = NULL;

	shell_plugin_name = bt_common_shell_quote(plugin_name, false);
	if (!shell_plugin_name) {
		goto end;
	}

	shell_comp_cls_name = bt_common_shell_quote(comp_cls_name, false);
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
		shell_plugin_name->str,
		bt_common_color_fg_default(),
		bt_common_color_fg_yellow(),
		shell_comp_cls_name->str,
		bt_common_color_reset());

end:
	if (shell_plugin_name) {
		g_string_free(shell_plugin_name, TRUE);
	}

	if (shell_comp_cls_name) {
		g_string_free(shell_comp_cls_name, TRUE);
	}
}

static
void print_value(FILE *, struct bt_value *, size_t);

static
void print_value_rec(FILE *, struct bt_value *, size_t);

struct print_map_value_data {
	size_t indent;
	FILE *fp;
};

static
bt_bool print_map_value(const char *key, struct bt_value *object, void *data)
{
	struct print_map_value_data *print_map_value_data = data;

	print_indent(print_map_value_data->fp, print_map_value_data->indent);
	fprintf(print_map_value_data->fp, "%s: ", key);

	if (bt_value_is_array(object) &&
			bt_value_array_is_empty(object)) {
		fprintf(print_map_value_data->fp, "[ ]\n");
		return true;
	}

	if (bt_value_is_map(object) &&
			bt_value_map_is_empty(object)) {
		fprintf(print_map_value_data->fp, "{ }\n");
		return true;
	}

	if (bt_value_is_array(object) ||
			bt_value_is_map(object)) {
		fprintf(print_map_value_data->fp, "\n");
	}

	print_value_rec(print_map_value_data->fp, object,
		print_map_value_data->indent + 2);
	return BT_TRUE;
}

static
void print_value_rec(FILE *fp, struct bt_value *value, size_t indent)
{
	bt_bool bool_val;
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
		fprintf(fp, "%snull%s\n", bt_common_color_bold(),
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_BOOL:
		bt_value_bool_get(value, &bool_val);
		fprintf(fp, "%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_cyan(), bool_val ? "yes" : "no",
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_INTEGER:
		bt_value_integer_get(value, &int_val);
		fprintf(fp, "%s%s%" PRId64 "%s\n", bt_common_color_bold(),
			bt_common_color_fg_red(), int_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_FLOAT:
		bt_value_float_get(value, &dbl_val);
		fprintf(fp, "%s%s%lf%s\n", bt_common_color_bold(),
			bt_common_color_fg_red(), dbl_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_STRING:
		bt_value_string_get(value, &str_val);
		fprintf(fp, "%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_green(), str_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_ARRAY:
		size = bt_value_array_size(value);
		assert(size >= 0);

		if (size == 0) {
			print_indent(fp, indent);
			fprintf(fp, "[ ]\n");
			break;
		}

		for (i = 0; i < size; i++) {
			struct bt_value *element =
					bt_value_array_get(value, i);

			assert(element);
			print_indent(fp, indent);
			fprintf(fp, "- ");

			if (bt_value_is_array(element) &&
					bt_value_array_is_empty(element)) {
				fprintf(fp, "[ ]\n");
				continue;
			}

			if (bt_value_is_map(element) &&
					bt_value_map_is_empty(element)) {
				fprintf(fp, "{ }\n");
				continue;
			}

			if (bt_value_is_array(element) ||
					bt_value_is_map(element)) {
				fprintf(fp, "\n");
			}

			print_value_rec(fp, element, indent + 2);
			BT_PUT(element);
		}
		break;
	case BT_VALUE_TYPE_MAP:
	{
		struct print_map_value_data data = {
			.indent = indent,
			.fp = fp,
		};

		if (bt_value_map_is_empty(value)) {
			print_indent(fp, indent);
			fprintf(fp, "{ }\n");
			break;
		}

		bt_value_map_foreach(value, print_map_value, &data);
		break;
	}
	default:
		assert(false);
	}
}

static
void print_value(FILE *fp, struct bt_value *value, size_t indent)
{
	if (!bt_value_is_array(value) && !bt_value_is_map(value)) {
		print_indent(fp, indent);
	}

	print_value_rec(fp, value, indent);
}

static
void print_bt_config_component(struct bt_config_component *bt_config_component)
{
	fprintf(stderr, "    ");
	print_plugin_comp_cls_opt(stderr, bt_config_component->plugin_name->str,
		bt_config_component->comp_cls_name->str,
		bt_config_component->type);
	fprintf(stderr, ":\n");

	if (bt_config_component->instance_name->len > 0) {
		fprintf(stderr, "      Name: %s\n",
			bt_config_component->instance_name->str);
	}

	fprintf(stderr, "      Parameters:\n");
	print_value(stderr, bt_config_component->params, 8);
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
	fprintf(stderr, "  Plugin paths:\n");
	print_value(stderr, plugin_paths, 4);
}

static
void print_cfg_run(struct bt_config *cfg)
{
	size_t i;

	print_plugin_paths(cfg->plugin_paths);
	fprintf(stderr, "  Source component instances:\n");
	print_bt_config_components(cfg->cmd_data.run.sources);

	if (cfg->cmd_data.run.filters->len > 0) {
		fprintf(stderr, "  Filter component instances:\n");
		print_bt_config_components(cfg->cmd_data.run.filters);
	}

	fprintf(stderr, "  Sink component instances:\n");
	print_bt_config_components(cfg->cmd_data.run.sinks);
	fprintf(stderr, "  Connections:\n");

	for (i = 0; i < cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *cfg_connection =
			g_ptr_array_index(cfg->cmd_data.run.connections,
				i);

		fprintf(stderr, "    %s%s%s -> %s%s%s\n",
			cfg_connection->upstream_comp_name->str,
			cfg_connection->upstream_port_glob->len > 0 ? "." : "",
			cfg_connection->upstream_port_glob->str,
			cfg_connection->downstream_comp_name->str,
			cfg_connection->downstream_port_glob->len > 0 ? "." : "",
			cfg_connection->downstream_port_glob->str);
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
	fprintf(stderr, "  Path: %s\n",
		cfg->cmd_data.print_ctf_metadata.path->str);
}

static
void print_cfg_print_lttng_live_sessions(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
	fprintf(stderr, "  URL: %s\n",
		cfg->cmd_data.print_lttng_live_sessions.url->str);
}

static
void print_cfg_query(struct bt_config *cfg)
{
	print_plugin_paths(cfg->plugin_paths);
	fprintf(stderr, "  Object: `%s`\n", cfg->cmd_data.query.object->str);
	fprintf(stderr, "  Component class:\n");
	print_bt_config_component(cfg->cmd_data.query.cfg_component);
}

static
void print_cfg(struct bt_config *cfg)
{
	if (!BT_LOG_ON_INFO) {
		return;
	}

	BT_LOGI_STR("Configuration:");
	fprintf(stderr, "  Debug mode: %s\n", cfg->debug ? "yes" : "no");
	fprintf(stderr, "  Verbose mode: %s\n", cfg->verbose ? "yes" : "no");

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
void add_to_loaded_plugins(struct bt_plugin_set *plugin_set)
{
	int64_t i;
	int64_t count;

	count = bt_plugin_set_get_plugin_count(plugin_set);
	assert(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_plugin *plugin =
			bt_plugin_set_get_plugin(plugin_set, i);
		struct bt_plugin *loaded_plugin =
				find_plugin(bt_plugin_get_name(plugin));

		assert(plugin);

		if (loaded_plugin) {
			BT_LOGI("Not using plugin: another one already exists with the same name: "
				"plugin-name=\"%s\", plugin-path=\"%s\", "
				"existing-plugin-path=\"%s\"",
				bt_plugin_get_name(plugin),
				bt_plugin_get_path(plugin),
				bt_plugin_get_path(loaded_plugin));
			bt_put(loaded_plugin);
		} else {
			/* Add to global array. */
			BT_LOGD("Adding plugin to loaded plugins: plugin-path=\"%s\"",
				bt_plugin_get_name(plugin));
			g_ptr_array_add(loaded_plugins, bt_get(plugin));
		}

		bt_put(plugin);
	}
}

static
int load_dynamic_plugins(struct bt_value *plugin_paths)
{
	int nr_paths, i, ret = 0;

	nr_paths = bt_value_array_size(plugin_paths);
	if (nr_paths < 0) {
		BT_LOGE_STR("Cannot load dynamic plugins: no plugin path.");
		ret = -1;
		goto end;
	}

	BT_LOGI("Loading dynamic plugins.");

	for (i = 0; i < nr_paths; i++) {
		struct bt_value *plugin_path_value = NULL;
		const char *plugin_path;
		struct bt_plugin_set *plugin_set;

		plugin_path_value = bt_value_array_get(plugin_paths, i);
		bt_value_string_get(plugin_path_value, &plugin_path);
		assert(plugin_path);
		plugin_set = bt_plugin_create_all_from_dir(plugin_path, false);
		if (!plugin_set) {
			BT_LOGD("Unable to load dynamic plugins: path=\"%s\"",
				plugin_path);
			BT_PUT(plugin_path_value);
			continue;
		}

		add_to_loaded_plugins(plugin_set);
		bt_put(plugin_set);
		BT_PUT(plugin_path_value);
	}
end:
	return ret;
}

static
int load_static_plugins(void)
{
	int ret = 0;
	struct bt_plugin_set *plugin_set;

	BT_LOGI("Loading static plugins.");
	plugin_set = bt_plugin_create_all_from_static();
	if (!plugin_set) {
		BT_LOGE("Unable to load static plugins.");
		ret = -1;
		goto end;
	}

	add_to_loaded_plugins(plugin_set);
	bt_put(plugin_set);
end:
	return ret;
}

static
int load_all_plugins(struct bt_value *plugin_paths)
{
	int ret = 0;

	if (load_dynamic_plugins(plugin_paths)) {
		ret = -1;
		goto end;
	}

	if (load_static_plugins()) {
		ret = -1;
		goto end;
	}

	BT_LOGI("Loaded all plugins: count=%u", loaded_plugins->len);

end:
	return ret;
}

static
void print_plugin_info(struct bt_plugin *plugin)
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

static
int cmd_query(struct bt_config *cfg)
{
	int ret = 0;
	struct bt_component_class *comp_cls = NULL;
	struct bt_value *results = NULL;

	comp_cls = find_component_class(cfg->cmd_data.query.cfg_component->plugin_name->str,
		cfg->cmd_data.query.cfg_component->comp_cls_name->str,
		cfg->cmd_data.query.cfg_component->type);
	if (!comp_cls) {
		BT_LOGE("Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			cfg->cmd_data.query.cfg_component->plugin_name->str,
			cfg->cmd_data.query.cfg_component->comp_cls_name->str,
			cfg->cmd_data.query.cfg_component->type);
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
		BT_LOGE("Failed to query component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d "
			"object=\"%s\"",
			cfg->cmd_data.query.cfg_component->plugin_name->str,
			cfg->cmd_data.query.cfg_component->comp_cls_name->str,
			cfg->cmd_data.query.cfg_component->type,
			cfg->cmd_data.query.object->str);
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

	print_value(stdout, results, 0);

end:
	bt_put(comp_cls);
	bt_put(results);
	return ret;
}

static
int cmd_help(struct bt_config *cfg)
{
	int ret = 0;
	struct bt_plugin *plugin = NULL;
	size_t i;

	plugin = find_plugin(cfg->cmd_data.help.cfg_component->plugin_name->str);
	if (!plugin) {
		BT_LOGE("Cannot find plugin: plugin-name=\"%s\"",
			cfg->cmd_data.help.cfg_component->plugin_name->str);
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
			(int) bt_plugin_get_component_class_count(plugin));


	if (cfg->cmd_data.help.cfg_component->type !=
			BT_COMPONENT_CLASS_TYPE_UNKNOWN) {
		struct bt_component_class *needed_comp_cls =
			find_component_class(
				cfg->cmd_data.help.cfg_component->plugin_name->str,
				cfg->cmd_data.help.cfg_component->comp_cls_name->str,
				cfg->cmd_data.help.cfg_component->type);

		if (!needed_comp_cls) {
			BT_LOGE("Cannot find component class: plugin-name=\"%s\", "
				"comp-cls-name=\"%s\", comp-cls-type=%d",
				cfg->cmd_data.help.cfg_component->plugin_name->str,
				cfg->cmd_data.help.cfg_component->comp_cls_name->str,
				cfg->cmd_data.help.cfg_component->type);
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
			bt_plugin_get_component_class_by_index(plugin, i);
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

static
int cmd_list_plugins(struct bt_config *cfg)
{
	int ret = 0;
	int plugins_count, component_classes_count = 0, i;

	printf("From the following plugin paths:\n\n");
	print_value(stdout, cfg->plugin_paths, 2);
	printf("\n");
	plugins_count = loaded_plugins->len;
	if (plugins_count == 0) {
		printf("No plugins found.\n");
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
			printf("  %sComponent classes%s: (none)\n",
				bt_common_color_bold(),
				bt_common_color_reset());
		} else {
			printf("  %sComponent classes%s:\n",
				bt_common_color_bold(),
				bt_common_color_reset());
		}

		for (j = 0; j < component_classes_count; j++) {
			struct bt_component_class *comp_class =
				bt_plugin_get_component_class_by_index(
					plugin, j);
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

static
int cmd_print_lttng_live_sessions(struct bt_config *cfg)
{
	printf("TODO\n");
	return -1;
}

static
int cmd_print_ctf_metadata(struct bt_config *cfg)
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
		BT_LOGE("Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			plugin_name, comp_cls_name,
			BT_COMPONENT_CLASS_TYPE_SOURCE);
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
		BT_LOGE_STR("Failed to query for metadata info.");
		fprintf(stderr, "%s%sFailed to request metadata info%s\n",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			bt_common_color_reset());
		goto end;
	}

	metadata_text_value = bt_value_map_get(results, "text");
	if (!metadata_text_value) {
		BT_LOGE_STR("Cannot find `text` string value in the resulting metadata info object.");
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

struct cmd_run_ctx {
	/* Owned by this */
	GHashTable *components;

	/* Owned by this */
	struct bt_graph *graph;

	/* Weak */
	struct bt_config *cfg;

	bool connect_ports;
};

static
int cmd_run_ctx_connect_upstream_port_to_downstream_component(
		struct cmd_run_ctx *ctx, struct bt_component *upstream_comp,
		struct bt_port *upstream_port,
		struct bt_config_connection *cfg_conn)
{
	int ret = 0;
	GQuark downstreamp_comp_name_quark;
	struct bt_component *downstream_comp;
	int64_t downstream_port_count;
	uint64_t i;
	int64_t (*port_count_fn)(struct bt_component *);
	struct bt_port *(*port_by_index_fn)(struct bt_component *, uint64_t);
	void *conn = NULL;

	BT_LOGI("Connecting upstream port to the next available downstream port: "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-comp-name=\"%s\", conn-arg=\"%s\"",
		upstream_port, bt_port_get_name(upstream_port),
		cfg_conn->downstream_comp_name->str,
		cfg_conn->arg->str);
	downstreamp_comp_name_quark = g_quark_from_string(
		cfg_conn->downstream_comp_name->str);
	assert(downstreamp_comp_name_quark > 0);
	downstream_comp = g_hash_table_lookup(ctx->components,
		(gpointer) (long) downstreamp_comp_name_quark);
	if (!downstream_comp) {
		BT_LOGE("Cannot find downstream component:  comp-name=\"%s\", "
			"conn-arg=\"%s\"", cfg_conn->downstream_comp_name->str,
			cfg_conn->arg->str);
		fprintf(stderr, "Cannot create connection: cannot find downstream component: %s\n",
			cfg_conn->arg->str);
		goto error;
	}

	if (bt_component_is_filter(downstream_comp)) {
		port_count_fn = bt_component_filter_get_input_port_count;
		port_by_index_fn = bt_component_filter_get_input_port_by_index;
	} else if (bt_component_is_sink(downstream_comp)) {
		port_count_fn = bt_component_sink_get_input_port_count;
		port_by_index_fn = bt_component_sink_get_input_port_by_index;
	} else {
		/*
		 * Should never happen because the connections are
		 * validated before we get here.
		 */
		BT_LOGF("Invalid connection: downstream component is a source: "
			"conn-arg=\"%s\"", cfg_conn->arg->str);
		assert(false);
	}

	downstream_port_count = port_count_fn(downstream_comp);
	assert(downstream_port_count >= 0);

	for (i = 0; i < downstream_port_count; i++) {
		struct bt_port *downstream_port =
			port_by_index_fn(downstream_comp, i);
		const char *downstream_port_name;

		assert(downstream_port);

		/* Skip port if it's already connected */
		if (bt_port_is_connected(downstream_port)) {
			bt_put(downstream_port);
			BT_LOGD("Skipping downstream port: already connected: "
				"port-addr=%p, port-name=\"%s\"",
				downstream_port,
				bt_port_get_name(downstream_port));
			continue;
		}

		downstream_port_name = bt_port_get_name(downstream_port);
		assert(downstream_port_name);

		if (bt_common_star_glob_match(
				cfg_conn->downstream_port_glob->str, -1ULL,
				downstream_port_name, -1ULL)) {
			/* We have a winner! */
			conn = bt_graph_connect_ports(ctx->graph,
				upstream_port, downstream_port);
			bt_put(downstream_port);
			if (!conn) {
				BT_LOGE("Cannot create connection: graph refuses to connect ports: "
					"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
					"upstream-port-addr=%p, upstream-port-name=\"%s\", "
					"downstream-comp-addr=%p, downstream-comp-name=\"%s\", "
					"downstream-port-addr=%p, downstream-port-name=\"%s\", "
					"conn-arg=\"%s\"",
					upstream_comp, bt_component_get_name(upstream_comp),
					upstream_port, bt_port_get_name(upstream_port),
					downstream_comp, cfg_conn->downstream_comp_name->str,
					downstream_port, downstream_port_name,
					cfg_conn->arg->str);
				fprintf(stderr,
					"Cannot create connection: graph refuses to connect ports (`%s` to `%s`): %s\n",
					bt_port_get_name(upstream_port),
					downstream_port_name,
					cfg_conn->arg->str);
				goto error;
			}

			BT_LOGI("Connected component ports: "
				"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
				"upstream-port-addr=%p, upstream-port-name=\"%s\", "
				"downstream-comp-addr=%p, downstream-comp-name=\"%s\", "
				"downstream-port-addr=%p, downstream-port-name=\"%s\", "
				"conn-arg=\"%s\"",
				upstream_comp, bt_component_get_name(upstream_comp),
				upstream_port, bt_port_get_name(upstream_port),
				downstream_comp, cfg_conn->downstream_comp_name->str,
				downstream_port, downstream_port_name,
				cfg_conn->arg->str);

			goto end;
		}

		bt_put(downstream_port);
	}

	if (!conn) {
		BT_LOGE("Cannot create connection: cannot find a matching downstream port for upstream port: "
			"upstream-port-addr=%p, upstream-port-name=\"%s\", "
			"downstream-comp-name=\"%s\", conn-arg=\"%s\"",
			upstream_port, bt_port_get_name(upstream_port),
			cfg_conn->downstream_comp_name->str,
			cfg_conn->arg->str);
		fprintf(stderr,
			"Cannot create connection: cannot find a matching downstream port for upstream port `%s`: %s\n",
			bt_port_get_name(upstream_port), cfg_conn->arg->str);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(conn);
	return ret;
}

static
int cmd_run_ctx_connect_upstream_port(struct cmd_run_ctx *ctx,
		struct bt_port *upstream_port)
{
	int ret = 0;
	const char *upstream_port_name;
	const char *upstream_comp_name;
	struct bt_component *upstream_comp = NULL;
	size_t i;

	assert(ctx);
	assert(upstream_port);
	upstream_port_name = bt_port_get_name(upstream_port);
	assert(upstream_port_name);
	upstream_comp = bt_port_get_component(upstream_port);
	if (!upstream_comp) {
		BT_LOGW("Upstream port to connect is not part of a component: "
			"port-addr=%p, port-name=\"%s\"",
			upstream_port, upstream_port_name);
		ret = -1;
		goto end;
	}

	upstream_comp_name = bt_component_get_name(upstream_comp);
	assert(upstream_comp_name);
	BT_LOGI("Connecting upstream port: comp-addr=%p, comp-name=\"%s\", "
		"port-addr=%p, port-name=\"%s\"",
		upstream_comp, upstream_comp_name,
		upstream_port, upstream_port_name);

	for (i = 0; i < ctx->cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *cfg_conn =
			g_ptr_array_index(
				ctx->cfg->cmd_data.run.connections, i);

		if (strcmp(cfg_conn->upstream_comp_name->str,
				upstream_comp_name) == 0) {
			if (bt_common_star_glob_match(
					cfg_conn->upstream_port_glob->str,
					-1ULL, upstream_port_name, -1ULL)) {
				ret = cmd_run_ctx_connect_upstream_port_to_downstream_component(
					ctx, upstream_comp, upstream_port,
					cfg_conn);
				if (ret) {
					BT_LOGE("Cannot connect upstream port: "
						"port-addr=%p, port-name=\"%s\"",
						upstream_port,
						upstream_port_name);
					fprintf(stderr,
						"Cannot connect port `%s` of component `%s` to a downstream port: %s\n",
						upstream_port_name,
						upstream_comp_name,
						cfg_conn->arg->str);
					goto error;
				}

				goto end;
			}
		}
	}

	BT_LOGE("Cannot connect upstream port: port does not match any connection argument: "
		"port-addr=%p, port-name=\"%s\"", upstream_port,
		upstream_port_name);
	fprintf(stderr,
		"Cannot create connection: upstream port `%s` does not match any connection\n",
		upstream_port_name);

error:
	ret = -1;

end:
	bt_put(upstream_comp);
	return ret;
}

static
void graph_port_added_listener(struct bt_port *port, void *data)
{
	struct bt_component *comp = NULL;
	struct cmd_run_ctx *ctx = data;

	comp = bt_port_get_component(port);
	BT_LOGI("Port added to a graph's component: comp-addr=%p, "
		"comp-name=\"%s\", port-addr=%p, port-name=\"%s\"",
		comp, comp ? bt_component_get_name(comp) : "",
		port, bt_port_get_name(port));
	if (!comp) {
		BT_LOGW_STR("Port has no component.");
		goto end;
	}

	if (bt_port_is_connected(port)) {
		BT_LOGW_STR("Port is already connected.");
		goto end;
	}

	if (!bt_port_is_output(port)) {
		BT_LOGI_STR("Skipping input port.");
		goto end;
	}

	if (cmd_run_ctx_connect_upstream_port(ctx, port)) {
		BT_LOGF_STR("Cannot connect upstream port.");
		fprintf(stderr, "Added port could not be connected: aborting\n");
		abort();
	}

end:
	bt_put(comp);
	return;
}

static
void graph_port_removed_listener(struct bt_component *component,
		struct bt_port *port, void *data)
{
	BT_LOGI("Port removed from a graph's component: comp-addr=%p, "
		"comp-name=\"%s\", port-addr=%p, port-name=\"%s\"",
		component, bt_component_get_name(component),
		port, bt_port_get_name(port));
}

static
void graph_ports_connected_listener(struct bt_port *upstream_port,
		struct bt_port *downstream_port, void *data)
{
	struct bt_component *upstream_comp = bt_port_get_component(upstream_port);
	struct bt_component *downstream_comp = bt_port_get_component(downstream_port);

	assert(upstream_comp);
	assert(downstream_comp);
	BT_LOGI("Graph's component ports connected: "
		"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-comp-addr=%p, downstream-comp-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		upstream_comp, bt_component_get_name(upstream_comp),
		upstream_port, bt_port_get_name(upstream_port),
		downstream_comp, bt_component_get_name(downstream_comp),
		downstream_port, bt_port_get_name(downstream_port));
	bt_put(upstream_comp);
	bt_put(downstream_comp);
}

static
void graph_ports_disconnected_listener(
		struct bt_component *upstream_component,
		struct bt_component *downstream_component,
		struct bt_port *upstream_port, struct bt_port *downstream_port,
		void *data)
{
	BT_LOGI("Graph's component ports disconnected: "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port));
}

static
void cmd_run_ctx_destroy(struct cmd_run_ctx *ctx)
{
	if (!ctx) {
		return;
	}

	if (ctx->components) {
		g_hash_table_destroy(ctx->components);
		ctx->components = NULL;
	}

	BT_PUT(ctx->graph);
	the_graph = NULL;
	ctx->cfg = NULL;
}

static
int cmd_run_ctx_init(struct cmd_run_ctx *ctx, struct bt_config *cfg)
{
	int ret = 0;

	ctx->cfg = cfg;
	ctx->connect_ports = false;
	ctx->components = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, bt_put);
	if (!ctx->components) {
		goto error;
	}

	ctx->graph = bt_graph_create();
	if (!ctx->graph) {
		goto error;
	}

	the_graph = ctx->graph;
	ret = bt_graph_add_port_added_listener(ctx->graph,
		graph_port_added_listener, ctx);
	if (ret) {
		goto error;
	}

	ret = bt_graph_add_port_removed_listener(ctx->graph,
		graph_port_removed_listener, ctx);
	if (ret) {
		goto error;
	}

	ret = bt_graph_add_ports_connected_listener(ctx->graph,
		graph_ports_connected_listener, ctx);
	if (ret) {
		goto error;
	}

	ret = bt_graph_add_ports_disconnected_listener(ctx->graph,
		graph_ports_disconnected_listener, ctx);
	if (ret) {
		goto error;
	}

	goto end;

error:
	cmd_run_ctx_destroy(ctx);
	ret = -1;

end:
	return ret;
}

static
int cmd_run_ctx_create_components_from_config_components(
		struct cmd_run_ctx *ctx, GPtrArray *cfg_components)
{
	size_t i;
	struct bt_component_class *comp_cls = NULL;
	struct bt_component *comp = NULL;
	int ret = 0;

	for (i = 0; i < cfg_components->len; i++) {
		struct bt_config_component *cfg_comp =
			g_ptr_array_index(cfg_components, i);
		GQuark quark;

		comp_cls = find_component_class(cfg_comp->plugin_name->str,
			cfg_comp->comp_cls_name->str, cfg_comp->type);
		if (!comp_cls) {
			BT_LOGE("Cannot find component class: plugin-name=\"%s\", "
				"comp-cls-name=\"%s\", comp-cls-type=%d",
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str,
				cfg_comp->type);
			fprintf(stderr, "%s%sCannot find component class %s",
				bt_common_color_bold(),
				bt_common_color_fg_red(),
				bt_common_color_reset());
			print_plugin_comp_cls_opt(stderr,
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str,
				cfg_comp->type);
			fprintf(stderr, "\n");
			goto error;
		}

		comp = bt_component_create(comp_cls,
			cfg_comp->instance_name->str, cfg_comp->params);
		if (!comp) {
			BT_LOGE("Cannot create component: plugin-name=\"%s\", "
				"comp-cls-name=\"%s\", comp-cls-type=%d",
				"comp-name=\"%s\"",
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str,
				cfg_comp->type, cfg_comp->instance_name->str);
			fprintf(stderr, "%s%sCannot create component `%s`%s\n",
				bt_common_color_bold(),
				bt_common_color_fg_red(),
				cfg_comp->instance_name->str,
				bt_common_color_reset());
			goto error;
		}

		BT_LOGI("Created and inserted component: comp-addr=%p, comp-name=\"%s\"",
			comp, cfg_comp->instance_name->str);
		quark = g_quark_from_string(cfg_comp->instance_name->str);
		assert(quark > 0);
		g_hash_table_insert(ctx->components,
			(gpointer) (long) quark, comp);
		comp = NULL;
		BT_PUT(comp_cls);
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(comp);
	bt_put(comp_cls);
	return ret;
}

static
int cmd_run_ctx_create_components(struct cmd_run_ctx *ctx)
{
	int ret = 0;

	/*
	 * Make sure that, during this phase, our graph's "port added"
	 * listener does not connect ports while we are creating the
	 * components because we have a special, initial phase for
	 * this.
	 */
	ctx->connect_ports = false;

	ret = cmd_run_ctx_create_components_from_config_components(
		ctx, ctx->cfg->cmd_data.run.sources);
	if (ret) {
		ret = -1;
		goto end;
	}

	ret = cmd_run_ctx_create_components_from_config_components(
		ctx, ctx->cfg->cmd_data.run.filters);
	if (ret) {
		ret = -1;
		goto end;
	}

	ret = cmd_run_ctx_create_components_from_config_components(
		ctx, ctx->cfg->cmd_data.run.sinks);
	if (ret) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int cmd_run_ctx_connect_comp_ports(struct cmd_run_ctx *ctx,
		struct bt_component *comp,
		int64_t (*port_count_fn)(struct bt_component *),
		struct bt_port *(*port_by_index_fn)(struct bt_component *, uint64_t))
{
	int ret = 0;
	int64_t count;
	uint64_t i;

	count = port_count_fn(comp);
	assert(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_port *upstream_port = port_by_index_fn(comp, i);

		assert(upstream_port);
		ret = cmd_run_ctx_connect_upstream_port(ctx, upstream_port);
		bt_put(upstream_port);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
int cmd_run_ctx_connect_ports(struct cmd_run_ctx *ctx)
{
	int ret = 0;
	GHashTableIter iter;
	gpointer g_name_quark, g_comp;

	ctx->connect_ports = true;
	g_hash_table_iter_init(&iter, ctx->components);

	while (g_hash_table_iter_next(&iter, &g_name_quark, &g_comp)) {
		if (bt_component_is_source(g_comp)) {
			ret = cmd_run_ctx_connect_comp_ports(ctx,
				g_comp, bt_component_source_get_output_port_count,
				bt_component_source_get_output_port_by_index);
		} else if (bt_component_is_filter(g_comp)) {
			ret = cmd_run_ctx_connect_comp_ports(ctx,
				g_comp, bt_component_filter_get_output_port_count,
				bt_component_filter_get_output_port_by_index);
		}

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static inline
const char *bt_graph_status_str(enum bt_graph_status status)
{
	switch (status) {
	case BT_GRAPH_STATUS_CANCELED:
		return "BT_GRAPH_STATUS_CANCELED";
	case BT_GRAPH_STATUS_AGAIN:
		return "BT_GRAPH_STATUS_AGAIN";
	case BT_GRAPH_STATUS_END:
		return "BT_GRAPH_STATUS_END";
	case BT_GRAPH_STATUS_OK:
		return "BT_GRAPH_STATUS_OK";
	case BT_GRAPH_STATUS_ALREADY_IN_A_GRAPH:
		return "BT_GRAPH_STATUS_ALREADY_IN_A_GRAPH";
	case BT_GRAPH_STATUS_INVALID:
		return "BT_GRAPH_STATUS_INVALID";
	case BT_GRAPH_STATUS_NO_SINK:
		return "BT_GRAPH_STATUS_NO_SINK";
	case BT_GRAPH_STATUS_ERROR:
		return "BT_GRAPH_STATUS_ERROR";
	default:
		return "(unknown)";
	}
}

static
int cmd_run(struct bt_config *cfg)
{
	int ret = 0;
	struct cmd_run_ctx ctx = { 0 };

	/* Initialize the command's context and the graph object */
	if (cmd_run_ctx_init(&ctx, cfg)) {
		BT_LOGE_STR("Cannot initialize the command's context.");
		fprintf(stderr, "Cannot initialize the command's context\n");
		goto error;
	}

	/* Create the requested component instances */
	if (cmd_run_ctx_create_components(&ctx)) {
		BT_LOGE_STR("Cannot create components.");
		fprintf(stderr, "Cannot create components\n");
		goto error;
	}

	/* Connect the initially visible component ports */
	if (cmd_run_ctx_connect_ports(&ctx)) {
		BT_LOGE_STR("Cannot connect initial component ports.");
		fprintf(stderr, "Cannot connect initial component ports\n");
		goto error;
	}

	if (canceled) {
		goto end;
	}

	BT_LOGI_STR("Running the graph.");

	/* Run the graph */
	while (true) {
		enum bt_graph_status graph_status = bt_graph_run(ctx.graph);

		BT_LOGV("bt_graph_run() returned: status=%s",
			bt_graph_status_str(graph_status));

		switch (graph_status) {
		case BT_GRAPH_STATUS_OK:
			break;
		case BT_GRAPH_STATUS_CANCELED:
			BT_LOGI_STR("Graph was canceled by user.");
			goto error;
		case BT_GRAPH_STATUS_AGAIN:
			if (bt_graph_is_canceled(ctx.graph)) {
				BT_LOGI_STR("Graph was canceled by user.");
				goto error;
			}

			if (cfg->cmd_data.run.retry_duration_us > 0) {
				BT_LOGV("Got BT_GRAPH_STATUS_AGAIN: sleeping: "
					"time-us=%" PRIu64,
					cfg->cmd_data.run.retry_duration_us);

				if (usleep(cfg->cmd_data.run.retry_duration_us)) {
					if (bt_graph_is_canceled(ctx.graph)) {
						BT_LOGI_STR("Graph was canceled by user.");
						goto error;
					}
				}
			}
			break;
		case BT_COMPONENT_STATUS_END:
			goto end;
		default:
			BT_LOGE_STR("Graph failed to complete successfully");
			fprintf(stderr, "Graph failed to complete successfully\n");
			goto error;
		}
	}

	goto end;

error:
	if (ret == 0) {
		ret = -1;
	}

end:
	cmd_run_ctx_destroy(&ctx);
	return ret;
}

static
void warn_command_name_and_directory_clash(struct bt_config *cfg)
{
	const char *env_clash;

	if (!cfg->command_name) {
		return;
	}

	env_clash = getenv(ENV_BABELTRACE_WARN_COMMAND_NAME_DIRECTORY_CLASH);
	if (env_clash && strcmp(env_clash, "0") == 0) {
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

static
void init_log_level(void)
{
	enum bt_logging_level log_level = BT_LOG_NONE;
	const char *log_level_env = getenv("BABELTRACE_CLI_LOG_LEVEL");

	if (!log_level_env) {
		goto set_level;
	}

	if (strcmp(log_level_env, "VERBOSE") == 0) {
		log_level = BT_LOGGING_LEVEL_VERBOSE;
	} else if (strcmp(log_level_env, "DEBUG") == 0) {
		log_level = BT_LOGGING_LEVEL_DEBUG;
	} else if (strcmp(log_level_env, "INFO") == 0) {
		log_level = BT_LOGGING_LEVEL_INFO;
	} else if (strcmp(log_level_env, "WARN") == 0) {
		log_level = BT_LOGGING_LEVEL_WARN;
	} else if (strcmp(log_level_env, "ERROR") == 0) {
		log_level = BT_LOGGING_LEVEL_ERROR;
	} else if (strcmp(log_level_env, "FATAL") == 0) {
		log_level = BT_LOGGING_LEVEL_FATAL;
	} else if (strcmp(log_level_env, "NONE") == 0) {
		log_level = BT_LOGGING_LEVEL_NONE;
	}

set_level:
	bt_cli_log_level = log_level;
}

void set_sigint_handler(void)
{
	struct sigaction new_action, old_action;

	new_action.sa_handler = sigint_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	sigaction(SIGINT, NULL, &old_action);

	if (old_action.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &new_action, NULL);
	}
}

int main(int argc, const char **argv)
{
	int ret;
	int retcode;
	struct bt_config *cfg;

	init_log_level();
	set_sigint_handler();
	init_static_data();
	cfg = bt_config_cli_args_create_with_default(argc, argv, &retcode);

	if (retcode < 0) {
		/* Quit without errors; typically usage/version */
		retcode = 0;
		BT_LOGI_STR("Quitting without errors.");
		goto end;
	}

	if (retcode > 0) {
		BT_LOGE("Command-line error: retcode=%d", retcode);
		goto end;
	}

	if (!cfg) {
		BT_LOGE_STR("Failed to create a valid Babeltrace configuration.");
		fprintf(stderr, "Failed to create Babeltrace configuration\n");
		retcode = 1;
		goto end;
	}

	if (cfg->verbose) {
		bt_cli_log_level = BT_LOGGING_LEVEL_VERBOSE;
		bt_logging_set_global_level(BT_LOGGING_LEVEL_VERBOSE);
		// TODO: for backward compat., set the log level
		//       environment variables of the known plugins
		//       to VERBOSE
	} else if (cfg->debug) {
		bt_cli_log_level = BT_LOGGING_LEVEL_DEBUG;
		bt_logging_set_global_level(BT_LOGGING_LEVEL_DEBUG);
		// TODO: for backward compat., set the log level
		//       environment variables of the known plugins
		//       to DEBUG
	}

	babeltrace_debug = cfg->debug;
	babeltrace_verbose = cfg->verbose;
	print_cfg(cfg);

	if (cfg->command_needs_plugins) {
		ret = load_all_plugins(cfg->plugin_paths);
		if (ret) {
			BT_LOGE("Failed to load plugins: ret=%d", ret);
			retcode = 1;
			goto end;
		}
	}

	BT_LOGI("Executing command: cmd=%d, command-name=\"%s\"",
		cfg->command, cfg->command_name);

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
		BT_LOGF("Invalid command: cmd=%d", cfg->command);
		assert(false);
	}

	BT_LOGI("Command completed: cmd=%d, command-name=\"%s\", ret=%d",
		cfg->command, cfg->command_name, ret);
	warn_command_name_and_directory_clash(cfg);
	retcode = ret ? 1 : 0;

end:
	BT_PUT(cfg);
	fini_static_data();
	return retcode;
}
