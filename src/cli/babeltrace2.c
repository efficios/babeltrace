/*
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

#define BT_LOG_TAG "CLI"
#include "logging.h"

#include <babeltrace2/babeltrace.h>
#include "common/common.h"
#include "string-format/format-error.h"
#include "string-format/format-plugin-comp-cls-name.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include "babeltrace2-cfg.h"
#include "babeltrace2-cfg-cli-args.h"
#include "babeltrace2-cfg-cli-args-default.h"
#include "babeltrace2-log-level.h"
#include "babeltrace2-plugins.h"
#include "babeltrace2-query.h"

#define ENV_BABELTRACE_WARN_COMMAND_NAME_DIRECTORY_CLASH "BABELTRACE_CLI_WARN_COMMAND_NAME_DIRECTORY_CLASH"
#define NSEC_PER_SEC	1000000000LL

enum bt_cmd_status {
	BT_CMD_STATUS_OK	    = 0,
	BT_CMD_STATUS_ERROR	    = -1,
	BT_CMD_STATUS_INTERRUPTED   = -2,
};

static
const char *bt_cmd_status_string(enum bt_cmd_status cmd_status)
{
	switch (cmd_status) {
	case BT_CMD_STATUS_OK:
		return "OK";
	case BT_CMD_STATUS_ERROR:
		return "ERROR";
	case BT_CMD_STATUS_INTERRUPTED:
		return "INTERRUPTED";
	default:
		bt_common_abort();
	}
}

/* Application's interrupter (owned by this) */
static bt_interrupter *the_interrupter;

#ifdef __MINGW32__

#include <windows.h>

static
BOOL WINAPI signal_handler(DWORD signal) {
	if (the_interrupter) {
		bt_interrupter_set(the_interrupter);
	}

	return TRUE;
}

static
void set_signal_handler(void)
{
	if (!SetConsoleCtrlHandler(signal_handler, TRUE)) {
		BT_LOGE("Failed to set the Ctrl+C handler.");
	}
}

#else /* __MINGW32__ */

static
void signal_handler(int signum)
{
	if (signum != SIGINT) {
		return;
	}

	if (the_interrupter) {
		bt_interrupter_set(the_interrupter);
	}
}

static
void set_signal_handler(void)
{
	struct sigaction new_action, old_action;

	new_action.sa_handler = signal_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	sigaction(SIGINT, NULL, &old_action);

	if (old_action.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &new_action, NULL);
	}
}

#endif /* __MINGW32__ */

static
int query(struct bt_config *cfg, const bt_component_class *comp_cls,
		const char *obj, const bt_value *params,
		const bt_value **user_result, const char **fail_reason)
{
	return cli_query(comp_cls, obj, params, cfg->log_level,
		the_interrupter, user_result, fail_reason);
}

typedef const void *(*plugin_borrow_comp_cls_func_t)(
		const bt_plugin *, const char *);

static
const void *find_component_class_from_plugin(const char *plugin_name,
		const char *comp_class_name,
		plugin_borrow_comp_cls_func_t plugin_borrow_comp_cls_func)
{
	const void *comp_class = NULL;
	const bt_plugin *plugin;

	BT_LOGI("Finding component class: plugin-name=\"%s\", "
		"comp-cls-name=\"%s\"", plugin_name, comp_class_name);

	plugin = borrow_loaded_plugin_by_name(plugin_name);
	if (!plugin) {
		goto end;
	}

	comp_class = plugin_borrow_comp_cls_func(plugin, comp_class_name);
	bt_object_get_ref(comp_class);

end:
	if (comp_class) {
		BT_LOGI("Found component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\"", plugin_name, comp_class_name);
	} else {
		BT_LOGI("Cannot find source component class: "
			"plugin-name=\"%s\", comp-cls-name=\"%s\"",
			plugin_name, comp_class_name);
	}

	return comp_class;
}

static
const bt_component_class_source *find_source_component_class(
		const char *plugin_name, const char *comp_class_name)
{
	return (const void *) find_component_class_from_plugin(
		plugin_name, comp_class_name,
		(plugin_borrow_comp_cls_func_t)
			bt_plugin_borrow_source_component_class_by_name_const);
}

static
const bt_component_class_filter *find_filter_component_class(
		const char *plugin_name, const char *comp_class_name)
{
	return (const void *) find_component_class_from_plugin(
		plugin_name, comp_class_name,
		(plugin_borrow_comp_cls_func_t)
			bt_plugin_borrow_filter_component_class_by_name_const);
}

static
const bt_component_class_sink *find_sink_component_class(
		const char *plugin_name, const char *comp_class_name)
{
	return (const void *) find_component_class_from_plugin(plugin_name,
		comp_class_name,
		(plugin_borrow_comp_cls_func_t)
			bt_plugin_borrow_sink_component_class_by_name_const);
}

static
const bt_component_class *find_component_class(const char *plugin_name,
		const char *comp_class_name,
		bt_component_class_type comp_class_type)
{
	const bt_component_class *comp_cls = NULL;

	switch (comp_class_type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		comp_cls = bt_component_class_source_as_component_class_const(find_source_component_class(plugin_name, comp_class_name));
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		comp_cls = bt_component_class_filter_as_component_class_const(find_filter_component_class(plugin_name, comp_class_name));
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		comp_cls = bt_component_class_sink_as_component_class_const(find_sink_component_class(plugin_name, comp_class_name));
		break;
	default:
		bt_common_abort();
	}

	return comp_cls;
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
void print_value(FILE *, const bt_value *, size_t);

static
void print_value_rec(FILE *, const bt_value *, size_t);

static
void print_map_value(const char *key, const bt_value *object, FILE *fp,
		size_t indent)
{
	print_indent(fp, indent);
	fprintf(fp, "%s: ", key);
	BT_ASSERT(object);

	if (bt_value_is_array(object) &&
			bt_value_array_is_empty(object)) {
		fprintf(fp, "[ ]\n");
		goto end;
	}

	if (bt_value_is_map(object) &&
			bt_value_map_is_empty(object)) {
		fprintf(fp, "{ }\n");
		goto end;
	}

	if (bt_value_is_array(object) ||
			bt_value_is_map(object)) {
		fprintf(fp, "\n");
	}

	print_value_rec(fp, object, indent + 2);

end:
	return;
}

static
bt_value_map_foreach_entry_const_func_status collect_map_keys(
		const char *key, const bt_value *object, void *data)
{
	GPtrArray *map_keys = data;

	g_ptr_array_add(map_keys, (gpointer *) key);

	return BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_OK;
}

static
gint g_ptr_array_sort_strings(gconstpointer a, gconstpointer b) {
	const char *s1 = *((const char **) a);
	const char *s2 = *((const char **) b);

	return g_strcmp0(s1, s2);
}

static
void print_value_rec(FILE *fp, const bt_value *value, size_t indent)
{
	bt_bool bool_val;
	int64_t int_val;
	uint64_t uint_val;
	double dbl_val;
	const char *str_val;
	GPtrArray *map_keys = NULL;

	BT_ASSERT(value);

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_NULL:
		fprintf(fp, "%snull%s\n", bt_common_color_bold(),
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_BOOL:
		bool_val = bt_value_bool_get(value);
		fprintf(fp, "%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_bright_cyan(), bool_val ? "yes" : "no",
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_UNSIGNED_INTEGER:
		uint_val = bt_value_integer_unsigned_get(value);
		fprintf(fp, "%s%s%" PRIu64 "%s\n", bt_common_color_bold(),
			bt_common_color_fg_bright_red(), uint_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_SIGNED_INTEGER:
		int_val = bt_value_integer_signed_get(value);
		fprintf(fp, "%s%s%" PRId64 "%s\n", bt_common_color_bold(),
			bt_common_color_fg_bright_red(), int_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_REAL:
		dbl_val = bt_value_real_get(value);
		fprintf(fp, "%s%s%lf%s\n", bt_common_color_bold(),
			bt_common_color_fg_bright_red(), dbl_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_STRING:
		str_val = bt_value_string_get(value);
		fprintf(fp, "%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_bright_green(), str_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_ARRAY:
	{
		uint64_t i, size;
		size = bt_value_array_get_length(value);
		if (size == 0) {
			print_indent(fp, indent);
			fprintf(fp, "[ ]\n");
			break;
		}

		for (i = 0; i < size; i++) {
			const bt_value *element =
				bt_value_array_borrow_element_by_index_const(
					value, i);

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
		}
		break;
	}
	case BT_VALUE_TYPE_MAP:
	{
		guint i;
		bt_value_map_foreach_entry_const_status foreach_status;

		if (bt_value_map_is_empty(value)) {
			print_indent(fp, indent);
			fprintf(fp, "{ }\n");
			break;
		}

		map_keys = g_ptr_array_new();
		if (!map_keys) {
			BT_CLI_LOGE_APPEND_CAUSE("Failed to allocated on GPtrArray.");
			goto end;
		}

		/*
		 * We want to print the map entries in a stable order.  Collect
		 * all the map's keys in a GPtrArray, sort it, then print the
		 * entries in that order.
		 */
		foreach_status = bt_value_map_foreach_entry_const(value,
			collect_map_keys, map_keys);
		if (foreach_status != BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_OK) {
			BT_CLI_LOGE_APPEND_CAUSE("Failed to iterator on map value.");
			goto end;
		}

		g_ptr_array_sort(map_keys, g_ptr_array_sort_strings);

		for (i = 0; i < map_keys->len; i++) {
			const char *map_key = g_ptr_array_index(map_keys, i);
			const bt_value *map_value;

			map_value = bt_value_map_borrow_entry_value_const(value, map_key);
			BT_ASSERT(map_value);

			print_map_value(map_key, map_value, fp, indent);
		}

		break;
	}
	default:
		bt_common_abort();
	}

	goto end;

end:
	if (map_keys) {
		g_ptr_array_free(map_keys, TRUE);
	}
}

static
void print_value(FILE *fp, const bt_value *value, size_t indent)
{
	if (!bt_value_is_array(value) && !bt_value_is_map(value)) {
		print_indent(fp, indent);
	}

	print_value_rec(fp, value, indent);
}

static
void print_bt_config_component(struct bt_config_component *bt_config_component)
{
	gchar *comp_cls_str;

	comp_cls_str = format_plugin_comp_cls_opt(
		bt_config_component->plugin_name->str,
		bt_config_component->comp_cls_name->str,
		bt_config_component->type,
		BT_COMMON_COLOR_WHEN_AUTO);
	BT_ASSERT(comp_cls_str);

	fprintf(stderr, "    %s:\n", comp_cls_str);

	if (bt_config_component->instance_name->len > 0) {
		fprintf(stderr, "      Name: %s\n",
			bt_config_component->instance_name->str);
	}

	fprintf(stderr, "      Parameters:\n");
	print_value(stderr, bt_config_component->params, 8);

	g_free(comp_cls_str);
}

static
void print_bt_config_components(GPtrArray *array)
{
	size_t i;

	for (i = 0; i < array->len; i++) {
		struct bt_config_component *cfg_component =
				bt_config_get_component(array, i);
		print_bt_config_component(cfg_component);
		BT_OBJECT_PUT_REF_AND_RESET(cfg_component);
	}
}

static
void print_plugin_paths(const bt_value *plugin_paths)
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

	BT_LOGI_STR("CLI configuration:");

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
		bt_common_abort();
	}
}

static
void print_plugin_info(const bt_plugin *plugin)
{
	unsigned int major, minor, patch;
	const char *extra;
	bt_property_availability version_avail;
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
	version_avail = bt_plugin_get_version(plugin, &major, &minor,
		&patch, &extra);
	printf("%s%s%s%s:\n", bt_common_color_bold(),
		bt_common_color_fg_bright_blue(), plugin_name,
		bt_common_color_reset());
	if (path) {
		printf("  %sPath%s: %s\n", bt_common_color_bold(),
			bt_common_color_reset(), path);
	} else {
		puts("  Built-in");
	}

	if (version_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
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
enum bt_cmd_status cmd_query(struct bt_config *cfg)
{
	int ret = 0;
	enum bt_cmd_status cmd_status;
	const bt_component_class *comp_cls = NULL;
	const bt_value *results = NULL;
	const char *fail_reason = NULL;

	comp_cls = find_component_class(
		cfg->cmd_data.query.cfg_component->plugin_name->str,
		cfg->cmd_data.query.cfg_component->comp_cls_name->str,
		cfg->cmd_data.query.cfg_component->type);
	if (!comp_cls) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			cfg->cmd_data.query.cfg_component->plugin_name->str,
			cfg->cmd_data.query.cfg_component->comp_cls_name->str,
			cfg->cmd_data.query.cfg_component->type);
		goto error;
	}

	ret = query(cfg, comp_cls, cfg->cmd_data.query.object->str,
		cfg->cmd_data.query.cfg_component->params,
		&results, &fail_reason);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Failed to query component class: %s: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d "
			"object=\"%s\"", fail_reason,
			cfg->cmd_data.query.cfg_component->plugin_name->str,
			cfg->cmd_data.query.cfg_component->comp_cls_name->str,
			cfg->cmd_data.query.cfg_component->type,
			cfg->cmd_data.query.object->str);
		goto error;
	}

	print_value(stdout, results, 0);
	cmd_status = BT_CMD_STATUS_OK;
	goto end;

error:
	cmd_status = BT_CMD_STATUS_ERROR;

end:
	bt_component_class_put_ref(comp_cls);
	bt_value_put_ref(results);
	return cmd_status;
}

static
void print_component_class_help(const char *plugin_name,
		const bt_component_class *comp_cls)
{
	const char *comp_class_name =
		bt_component_class_get_name(comp_cls);
	const char *comp_class_description =
		bt_component_class_get_description(comp_cls);
	const char *comp_class_help =
		bt_component_class_get_help(comp_cls);
	bt_component_class_type type =
		bt_component_class_get_type(comp_cls);
	gchar *comp_cls_str;

	comp_cls_str = format_plugin_comp_cls_opt(plugin_name, comp_class_name,
		type, BT_COMMON_COLOR_WHEN_AUTO);
	BT_ASSERT(comp_cls_str);

	printf("%s\n", comp_cls_str);
	printf("  %sDescription%s: %s\n", bt_common_color_bold(),
		bt_common_color_reset(),
		comp_class_description ? comp_class_description : "(None)");

	if (comp_class_help) {
		printf("\n%s\n", comp_class_help);
	}

	g_free(comp_cls_str);
}

static
enum bt_cmd_status cmd_help(struct bt_config *cfg)
{
	enum bt_cmd_status cmd_status;
	const bt_plugin *plugin = NULL;
	const bt_component_class *needed_comp_cls = NULL;

	plugin = borrow_loaded_plugin_by_name(cfg->cmd_data.help.cfg_component->plugin_name->str);
	if (!plugin) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find plugin: plugin-name=\"%s\"",
			cfg->cmd_data.help.cfg_component->plugin_name->str);
		goto error;
	}

	print_plugin_info(plugin);
	printf("  %sSource component classes%s: %d\n",
			bt_common_color_bold(),
			bt_common_color_reset(),
			(int) bt_plugin_get_source_component_class_count(plugin));
	printf("  %sFilter component classes%s: %d\n",
			bt_common_color_bold(),
			bt_common_color_reset(),
			(int) bt_plugin_get_filter_component_class_count(plugin));
	printf("  %sSink component classes%s: %d\n",
			bt_common_color_bold(),
			bt_common_color_reset(),
			(int) bt_plugin_get_sink_component_class_count(plugin));

	if (strlen(cfg->cmd_data.help.cfg_component->comp_cls_name->str) == 0) {
		/* Plugin help only */
		cmd_status = BT_CMD_STATUS_OK;
		goto end;
	}

	needed_comp_cls = find_component_class(
		cfg->cmd_data.help.cfg_component->plugin_name->str,
		cfg->cmd_data.help.cfg_component->comp_cls_name->str,
		cfg->cmd_data.help.cfg_component->type);
	if (!needed_comp_cls) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			cfg->cmd_data.help.cfg_component->plugin_name->str,
			cfg->cmd_data.help.cfg_component->comp_cls_name->str,
			cfg->cmd_data.help.cfg_component->type);
		goto error;
	}

	printf("\n");
	print_component_class_help(
		cfg->cmd_data.help.cfg_component->plugin_name->str,
		needed_comp_cls);
	cmd_status = BT_CMD_STATUS_OK;
	goto end;

error:
	cmd_status = BT_CMD_STATUS_ERROR;

end:
	bt_component_class_put_ref(needed_comp_cls);
	return cmd_status;
}

typedef void *(* plugin_borrow_comp_cls_by_index_func_t)(const bt_plugin *,
	uint64_t);
typedef const bt_component_class *(* spec_comp_cls_borrow_comp_cls_func_t)(
	void *);

static
void cmd_list_plugins_print_component_classes(const bt_plugin *plugin,
		const char *cc_type_name, uint64_t count,
		plugin_borrow_comp_cls_by_index_func_t borrow_comp_cls_by_index_func,
		spec_comp_cls_borrow_comp_cls_func_t spec_comp_cls_borrow_comp_cls_func)
{
	uint64_t i;
	gchar *comp_cls_str = NULL;

	if (count == 0) {
		printf("  %s%s component classes%s: (none)\n",
			bt_common_color_bold(),
			cc_type_name,
			bt_common_color_reset());
		goto end;
	} else {
		printf("  %s%s component classes%s:\n",
			bt_common_color_bold(),
			cc_type_name,
			bt_common_color_reset());
	}

	for (i = 0; i < count; i++) {
		const bt_component_class *comp_class =
			spec_comp_cls_borrow_comp_cls_func(
				borrow_comp_cls_by_index_func(plugin, i));
		const char *comp_class_name =
			bt_component_class_get_name(comp_class);
		const char *comp_class_description =
			bt_component_class_get_description(comp_class);
		bt_component_class_type type =
			bt_component_class_get_type(comp_class);

		g_free(comp_cls_str);
		comp_cls_str = format_plugin_comp_cls_opt(
			bt_plugin_get_name(plugin), comp_class_name, type,
			BT_COMMON_COLOR_WHEN_AUTO);
		printf("    %s", comp_cls_str);

		if (comp_class_description) {
			printf(": %s", comp_class_description);
		}

		printf("\n");
	}

end:
	g_free(comp_cls_str);
}

static
enum bt_cmd_status cmd_list_plugins(struct bt_config *cfg)
{
	int plugins_count, component_classes_count = 0, i;

	printf("From the following plugin paths:\n\n");
	print_value(stdout, cfg->plugin_paths, 2);
	printf("\n");
	plugins_count = get_loaded_plugins_count();
	if (plugins_count == 0) {
		printf("No plugins found.\n");
		goto end;
	}

	for (i = 0; i < plugins_count; i++) {
		const bt_plugin *plugin = borrow_loaded_plugin_by_index(i);

		component_classes_count +=
			bt_plugin_get_source_component_class_count(plugin) +
			bt_plugin_get_filter_component_class_count(plugin) +
			bt_plugin_get_sink_component_class_count(plugin);
	}

	printf("Found %s%d%s component classes in %s%d%s plugins.\n",
		bt_common_color_bold(),
		component_classes_count,
		bt_common_color_reset(),
		bt_common_color_bold(),
		plugins_count,
		bt_common_color_reset());

	for (i = 0; i < plugins_count; i++) {
		const bt_plugin *plugin = borrow_loaded_plugin_by_index(i);

		printf("\n");
		print_plugin_info(plugin);
		cmd_list_plugins_print_component_classes(plugin, "Source",
			bt_plugin_get_source_component_class_count(plugin),
			(plugin_borrow_comp_cls_by_index_func_t)
				bt_plugin_borrow_source_component_class_by_index_const,
			(spec_comp_cls_borrow_comp_cls_func_t)
				bt_component_class_source_as_component_class);
		cmd_list_plugins_print_component_classes(plugin, "Filter",
			bt_plugin_get_filter_component_class_count(plugin),
			(plugin_borrow_comp_cls_by_index_func_t)
				bt_plugin_borrow_filter_component_class_by_index_const,
			(spec_comp_cls_borrow_comp_cls_func_t)
				bt_component_class_filter_as_component_class);
		cmd_list_plugins_print_component_classes(plugin, "Sink",
			bt_plugin_get_sink_component_class_count(plugin),
			(plugin_borrow_comp_cls_by_index_func_t)
				bt_plugin_borrow_sink_component_class_by_index_const,
			(spec_comp_cls_borrow_comp_cls_func_t)
				bt_component_class_sink_as_component_class);
	}

end:
	return BT_CMD_STATUS_OK;
}

static
enum bt_cmd_status cmd_print_lttng_live_sessions(struct bt_config *cfg)
{
	int ret = 0;
	enum bt_cmd_status cmd_status;
	const bt_component_class *comp_cls = NULL;
	const bt_value *results = NULL;
	bt_value *params = NULL;
	const bt_value *map = NULL;
	const bt_value *v = NULL;
	static const char * const plugin_name = "ctf";
	static const char * const comp_cls_name = "lttng-live";
	static const bt_component_class_type comp_cls_type =
		BT_COMPONENT_CLASS_TYPE_SOURCE;
	uint64_t array_size, i;
	const char *fail_reason = NULL;
	FILE *out_stream = stdout;

	BT_ASSERT(cfg->cmd_data.print_lttng_live_sessions.url);
	comp_cls = find_component_class(plugin_name, comp_cls_name,
		comp_cls_type);
	if (!comp_cls) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			plugin_name, comp_cls_name,
			BT_COMPONENT_CLASS_TYPE_SOURCE);
		goto error;
	}

	params = bt_value_map_create();
	if (!params) {
		goto error;
	}

	ret = bt_value_map_insert_string_entry(params, "url",
		cfg->cmd_data.print_lttng_live_sessions.url->str);
	if (ret) {
		goto error;
	}

	ret = query(cfg, comp_cls, "sessions", params,
		    &results, &fail_reason);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to query `sessions` object: %s",
			fail_reason);
		goto error;
	}

	BT_ASSERT(results);

	if (!bt_value_is_array(results)) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Expecting an array for LTTng live `sessions` query.");
		goto error;
	}

	if (cfg->cmd_data.print_lttng_live_sessions.output_path->len > 0) {
		out_stream =
			fopen(cfg->cmd_data.print_lttng_live_sessions.output_path->str,
				"w");
		if (!out_stream) {
			BT_LOGE_ERRNO("Cannot open file for writing",
				": path=\"%s\"",
				cfg->cmd_data.print_lttng_live_sessions.output_path->str);
			(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
				"Babeltrace CLI",
				"Cannot open file for writing: path=\"%s\"",
				cfg->cmd_data.print_lttng_live_sessions.output_path->str);
			goto error;
		}
	}

	array_size = bt_value_array_get_length(results);
	for (i = 0; i < array_size; i++) {
		const char *url_text;
		int64_t timer_us, streams, clients;

		map = bt_value_array_borrow_element_by_index_const(results, i);
		if (!bt_value_is_map(map)) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected entry type.");
			goto error;
		}

		v = bt_value_map_borrow_entry_value_const(map, "url");
		if (!v) {
			BT_CLI_LOGE_APPEND_CAUSE("Missing `url` entry.");
			goto error;
		}
		url_text = bt_value_string_get(v);
		fprintf(out_stream, "%s", url_text);
		v = bt_value_map_borrow_entry_value_const(map, "timer-us");
		if (!v) {
			BT_CLI_LOGE_APPEND_CAUSE("Missing `timer-us` entry.");
			goto error;
		}
		timer_us = bt_value_integer_unsigned_get(v);
		fprintf(out_stream, " (timer = %" PRIu64 ", ", timer_us);
		v = bt_value_map_borrow_entry_value_const(map, "stream-count");
		if (!v) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Missing `stream-count` entry.");
			goto error;
		}
		streams = bt_value_integer_unsigned_get(v);
		fprintf(out_stream, "%" PRIu64 " stream(s), ", streams);
		v = bt_value_map_borrow_entry_value_const(map, "client-count");
		if (!v) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Missing `client-count` entry.");
			goto error;
		}
		clients = bt_value_integer_unsigned_get(v);
		fprintf(out_stream, "%" PRIu64 " client(s) connected)\n", clients);
	}

	cmd_status = BT_CMD_STATUS_OK;
	goto end;

error:
	cmd_status = BT_CMD_STATUS_ERROR;

end:
	bt_value_put_ref(results);
	bt_value_put_ref(params);
	bt_component_class_put_ref(comp_cls);

	if (out_stream && out_stream != stdout) {
		int fclose_ret = fclose(out_stream);

		if (fclose_ret) {
			BT_LOGE_ERRNO("Cannot close file stream",
				": path=\"%s\"",
				cfg->cmd_data.print_lttng_live_sessions.output_path->str);
		}
	}

	return cmd_status;
}

static
enum bt_cmd_status cmd_print_ctf_metadata(struct bt_config *cfg)
{
	int ret = 0;
	enum bt_cmd_status cmd_status;
	const bt_component_class *comp_cls = NULL;
	const bt_value *results = NULL;
	bt_value *params = NULL;
	const bt_value *metadata_text_value = NULL;
	const char *metadata_text = NULL;
	static const char * const plugin_name = "ctf";
	static const char * const comp_cls_name = "fs";
	static const bt_component_class_type comp_cls_type =
		BT_COMPONENT_CLASS_TYPE_SOURCE;
	const char *fail_reason = NULL;
	FILE *out_stream = stdout;

	BT_ASSERT(cfg->cmd_data.print_ctf_metadata.path);
	comp_cls = find_component_class(plugin_name, comp_cls_name,
		comp_cls_type);
	if (!comp_cls) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			plugin_name, comp_cls_name,
			BT_COMPONENT_CLASS_TYPE_SOURCE);
		goto error;
	}

	params = bt_value_map_create();
	if (!params) {
		goto error;
	}

	ret = bt_value_map_insert_string_entry(params, "path",
		cfg->cmd_data.print_ctf_metadata.path->str);
	if (ret) {
		goto error;
	}

	ret = query(cfg, comp_cls, "metadata-info",
		params, &results, &fail_reason);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Failed to query `metadata-info` object: %s", fail_reason);
		goto error;
	}

	metadata_text_value = bt_value_map_borrow_entry_value_const(results,
		"text");
	if (!metadata_text_value) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find `text` string value in the resulting metadata info object.");
		goto error;
	}

	metadata_text = bt_value_string_get(metadata_text_value);

	if (cfg->cmd_data.print_ctf_metadata.output_path->len > 0) {
		out_stream =
			fopen(cfg->cmd_data.print_ctf_metadata.output_path->str,
				"w");
		if (!out_stream) {
			BT_LOGE_ERRNO("Cannot open file for writing",
				": path=\"%s\"",
				cfg->cmd_data.print_ctf_metadata.output_path->str);
			(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
				"Babeltrace CLI",
				"Cannot open file for writing: path=\"%s\"",
				cfg->cmd_data.print_ctf_metadata.output_path->str);
			goto error;
		}
	}

	ret = fprintf(out_stream, "%s\n", metadata_text);
	if (ret < 0) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot write whole metadata text to output stream: "
			"ret=%d", ret);
		goto error;
	}

	cmd_status = BT_CMD_STATUS_OK;
	goto end;

error:
	cmd_status = BT_CMD_STATUS_ERROR;

end:
	bt_value_put_ref(results);
	bt_value_put_ref(params);
	bt_component_class_put_ref(comp_cls);

	if (out_stream && out_stream != stdout) {
		int fclose_ret = fclose(out_stream);

		if (fclose_ret) {
			BT_LOGE_ERRNO("Cannot close file stream",
				": path=\"%s\"",
				cfg->cmd_data.print_ctf_metadata.output_path->str);
		}
	}

	return cmd_status;
}

struct port_id {
	char *instance_name;
	char *port_name;
};

struct trace_range {
	uint64_t intersection_range_begin_ns;
	uint64_t intersection_range_end_ns;
};

static
guint port_id_hash(gconstpointer v)
{
	const struct port_id *id = v;

	BT_ASSERT(id->instance_name);
	BT_ASSERT(id->port_name);

	return g_str_hash(id->instance_name) ^ g_str_hash(id->port_name);
}

static
gboolean port_id_equal(gconstpointer v1, gconstpointer v2)
{
	const struct port_id *id1 = v1;
	const struct port_id *id2 = v2;

	return strcmp(id1->instance_name, id2->instance_name) == 0 &&
		strcmp(id1->port_name, id2->port_name) == 0;
}

static
void port_id_destroy(gpointer data)
{
	struct port_id *id = data;

	free(id->instance_name);
	free(id->port_name);
	free(id);
}

static
void trace_range_destroy(gpointer data)
{
	free(data);
}

struct cmd_run_ctx {
	/* Owned by this */
	GHashTable *src_components;

	/* Owned by this */
	GHashTable *flt_components;

	/* Owned by this */
	GHashTable *sink_components;

	/* Owned by this */
	bt_graph *graph;

	/* Weak */
	struct bt_config *cfg;

	bool connect_ports;

	bool stream_intersection_mode;

	/*
	 * Association of struct port_id -> struct trace_range.
	 */
	GHashTable *intersections;
};

/* Returns a timestamp of the form "(-)s.ns" */
static
char *s_from_ns(int64_t ns)
{
	int ret;
	char *s_ret = NULL;
	bool is_negative;
	int64_t ts_sec_abs, ts_nsec_abs;
	int64_t ts_sec = ns / NSEC_PER_SEC;
	int64_t ts_nsec = ns % NSEC_PER_SEC;

	if (ts_sec >= 0 && ts_nsec >= 0) {
		is_negative = false;
		ts_sec_abs = ts_sec;
		ts_nsec_abs = ts_nsec;
	} else if (ts_sec > 0 && ts_nsec < 0) {
		is_negative = false;
		ts_sec_abs = ts_sec - 1;
		ts_nsec_abs = NSEC_PER_SEC + ts_nsec;
	} else if (ts_sec == 0 && ts_nsec < 0) {
		is_negative = true;
		ts_sec_abs = ts_sec;
		ts_nsec_abs = -ts_nsec;
	} else if (ts_sec < 0 && ts_nsec > 0) {
		is_negative = true;
		ts_sec_abs = -(ts_sec + 1);
		ts_nsec_abs = NSEC_PER_SEC - ts_nsec;
	} else if (ts_sec < 0 && ts_nsec == 0) {
		is_negative = true;
		ts_sec_abs = -ts_sec;
		ts_nsec_abs = ts_nsec;
	} else {	/* (ts_sec < 0 && ts_nsec < 0) */
		is_negative = true;
		ts_sec_abs = -ts_sec;
		ts_nsec_abs = -ts_nsec;
	}

	ret = asprintf(&s_ret, "%s%" PRId64 ".%09" PRId64,
		is_negative ? "-" : "", ts_sec_abs, ts_nsec_abs);
	if (ret < 0) {
		s_ret = NULL;
	}
	return s_ret;
}

static
int cmd_run_ctx_connect_upstream_port_to_downstream_component(
		struct cmd_run_ctx *ctx,
		const bt_component *upstream_comp,
		const bt_port_output *out_upstream_port,
		struct bt_config_connection *cfg_conn)
{
	typedef uint64_t (*input_port_count_func_t)(void *);
	typedef const bt_port_input *(*borrow_input_port_by_index_func_t)(
		const void *, uint64_t);
	const bt_port *upstream_port =
		bt_port_output_as_port_const(out_upstream_port);

	int ret = 0;
	GQuark downstreamp_comp_name_quark;
	void *downstream_comp;
	uint64_t downstream_port_count;
	uint64_t i;
	input_port_count_func_t port_count_fn;
	borrow_input_port_by_index_func_t port_by_index_fn;
	bt_graph_connect_ports_status connect_ports_status =
		BT_GRAPH_CONNECT_PORTS_STATUS_OK;
	bool insert_trimmer = false;
	bt_value *trimmer_params = NULL;
	char *intersection_begin = NULL;
	char *intersection_end = NULL;
	const bt_component_filter *trimmer = NULL;
	const bt_component_class_filter *trimmer_class = NULL;
	const bt_port_input *trimmer_input = NULL;
	const bt_port_output *trimmer_output = NULL;

	if (ctx->intersections &&
		bt_component_get_class_type(upstream_comp) ==
			BT_COMPONENT_CLASS_TYPE_SOURCE) {
		struct trace_range *range;
		struct port_id port_id = {
			.instance_name = (char *) bt_component_get_name(upstream_comp),
			.port_name = (char *) bt_port_get_name(upstream_port)
		};

		if (!port_id.instance_name || !port_id.port_name) {
			goto error;
		}

		range = (struct trace_range *) g_hash_table_lookup(
			ctx->intersections, &port_id);
		if (range) {
			bt_value_map_insert_entry_status insert_status;

			intersection_begin = s_from_ns(
				range->intersection_range_begin_ns);
			intersection_end = s_from_ns(
				range->intersection_range_end_ns);
			if (!intersection_begin || !intersection_end) {
				BT_CLI_LOGE_APPEND_CAUSE(
					"Cannot create trimmer argument timestamp string.");
				goto error;
			}

			insert_trimmer = true;
			trimmer_params = bt_value_map_create();
			if (!trimmer_params) {
				goto error;
			}

			insert_status = bt_value_map_insert_string_entry(
				trimmer_params, "begin", intersection_begin);
			if (insert_status < 0) {
				goto error;
			}
			insert_status = bt_value_map_insert_string_entry(
				trimmer_params,
				"end", intersection_end);
			if (insert_status < 0) {
				goto error;
			}
		}

		trimmer_class = find_filter_component_class("utils", "trimmer");
		if (!trimmer_class) {
			goto error;
		}
	}

	BT_LOGI("Connecting upstream port to the next available downstream port: "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-comp-name=\"%s\", conn-arg=\"%s\"",
		upstream_port, bt_port_get_name(upstream_port),
		cfg_conn->downstream_comp_name->str,
		cfg_conn->arg->str);
	downstreamp_comp_name_quark = g_quark_from_string(
		cfg_conn->downstream_comp_name->str);
	BT_ASSERT(downstreamp_comp_name_quark > 0);
	downstream_comp = g_hash_table_lookup(ctx->flt_components,
		GUINT_TO_POINTER(downstreamp_comp_name_quark));
	port_count_fn = (input_port_count_func_t)
		bt_component_filter_get_input_port_count;
	port_by_index_fn = (borrow_input_port_by_index_func_t)
		bt_component_filter_borrow_input_port_by_index_const;

	if (!downstream_comp) {
		downstream_comp = g_hash_table_lookup(ctx->sink_components,
			GUINT_TO_POINTER(downstreamp_comp_name_quark));
		port_count_fn = (input_port_count_func_t)
			bt_component_sink_get_input_port_count;
		port_by_index_fn = (borrow_input_port_by_index_func_t)
			bt_component_sink_borrow_input_port_by_index_const;
	}

	if (!downstream_comp) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot find downstream component: "
			"comp-name=\"%s\", conn-arg=\"%s\"",
			cfg_conn->downstream_comp_name->str,
			cfg_conn->arg->str);
		goto error;
	}

	downstream_port_count = port_count_fn(downstream_comp);

	for (i = 0; i < downstream_port_count; i++) {
		const bt_port_input *in_downstream_port =
			port_by_index_fn(downstream_comp, i);
		const bt_port *downstream_port =
			bt_port_input_as_port_const(in_downstream_port);
		const char *upstream_port_name;
		const char *downstream_port_name;

		BT_ASSERT(downstream_port);

		/* Skip port if it's already connected. */
		if (bt_port_is_connected(downstream_port)) {
			BT_LOGI("Skipping downstream port: already connected: "
				"port-addr=%p, port-name=\"%s\"",
				downstream_port,
				bt_port_get_name(downstream_port));
			continue;
		}

		downstream_port_name = bt_port_get_name(downstream_port);
		BT_ASSERT(downstream_port_name);
		upstream_port_name = bt_port_get_name(upstream_port);
		BT_ASSERT(upstream_port_name);

		if (!bt_common_star_glob_match(
				cfg_conn->downstream_port_glob->str, SIZE_MAX,
				downstream_port_name, SIZE_MAX)) {
			continue;
		}

		if (insert_trimmer) {
			/*
			 * In order to insert the trimmer between the
			 * two components that were being connected, we
			 * create a connection configuration entry which
			 * describes a connection from the trimmer's
			 * output to the original input that was being
			 * connected.
			 *
			 * Hence, the creation of the trimmer will cause
			 * the graph "new port" listener to establish
			 * all downstream connections as its output port
			 * is connected. We will then establish the
			 * connection between the original upstream
			 * source and the trimmer.
			 */
			char *trimmer_name = NULL;
			bt_graph_add_component_status add_comp_status;

			ret = asprintf(&trimmer_name,
				"stream-intersection-trimmer-%s",
				upstream_port_name);
			if (ret < 0) {
				goto error;
			}
			ret = 0;

			ctx->connect_ports = false;
			add_comp_status = bt_graph_add_filter_component(
				ctx->graph, trimmer_class, trimmer_name,
				trimmer_params, ctx->cfg->log_level,
				&trimmer);
			bt_component_filter_get_ref(trimmer);
			free(trimmer_name);
			if (add_comp_status !=
					BT_GRAPH_ADD_COMPONENT_STATUS_OK) {
				goto error;
			}
			BT_ASSERT(trimmer);

			trimmer_input =
				bt_component_filter_borrow_input_port_by_index_const(
					trimmer, 0);
			if (!trimmer_input) {
				goto error;
			}
			trimmer_output =
				bt_component_filter_borrow_output_port_by_index_const(
					trimmer, 0);
			if (!trimmer_output) {
				goto error;
			}

			/*
			 * Replace the current downstream port by the trimmer's
			 * upstream port.
			 */
			in_downstream_port = trimmer_input;
			downstream_port =
				bt_port_input_as_port_const(in_downstream_port);
			downstream_port_name = bt_port_get_name(
				downstream_port);
			BT_ASSERT(downstream_port_name);
		}

		/* We have a winner! */
		connect_ports_status = bt_graph_connect_ports(ctx->graph,
			out_upstream_port, in_downstream_port, NULL);
		downstream_port = NULL;
		switch (connect_ports_status) {
		case BT_GRAPH_CONNECT_PORTS_STATUS_OK:
			break;
		default:
			BT_CLI_LOGE_APPEND_CAUSE(
				"Cannot create connection: graph refuses to connect ports: "
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

		if (insert_trimmer) {
			/*
			 * The first connection, from the source to the trimmer,
			 * has been done. We now connect the trimmer to the
			 * original downstream port.
			 */
			ret = cmd_run_ctx_connect_upstream_port_to_downstream_component(
				ctx,
				bt_component_filter_as_component_const(trimmer),
				trimmer_output, cfg_conn);
			if (ret) {
				goto error;
			}
			ctx->connect_ports = true;
		}

		/*
		 * We found a matching downstream port: the search is
		 * over.
		 */
		goto end;
	}

	/* No downstream port found */
	BT_CLI_LOGE_APPEND_CAUSE(
		"Cannot create connection: cannot find a matching downstream port for upstream port: "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-comp-name=\"%s\", conn-arg=\"%s\"",
		upstream_port, bt_port_get_name(upstream_port),
		cfg_conn->downstream_comp_name->str,
		cfg_conn->arg->str);

error:
	ret = -1;

end:
	free(intersection_begin);
	free(intersection_end);
	BT_VALUE_PUT_REF_AND_RESET(trimmer_params);
	BT_COMPONENT_CLASS_FILTER_PUT_REF_AND_RESET(trimmer_class);
	BT_COMPONENT_FILTER_PUT_REF_AND_RESET(trimmer);
	return ret;
}

static
int cmd_run_ctx_connect_upstream_port(struct cmd_run_ctx *ctx,
		const bt_port_output *upstream_port)
{
	int ret = 0;
	const char *upstream_port_name;
	const char *upstream_comp_name;
	const bt_component *upstream_comp = NULL;
	size_t i;

	BT_ASSERT(ctx);
	BT_ASSERT(upstream_port);
	upstream_port_name = bt_port_get_name(
		bt_port_output_as_port_const(upstream_port));
	BT_ASSERT(upstream_port_name);
	upstream_comp = bt_port_borrow_component_const(
		bt_port_output_as_port_const(upstream_port));
	BT_ASSERT(upstream_comp);
	upstream_comp_name = bt_component_get_name(upstream_comp);
	BT_ASSERT(upstream_comp_name);
	BT_LOGI("Connecting upstream port: comp-addr=%p, comp-name=\"%s\", "
		"port-addr=%p, port-name=\"%s\"",
		upstream_comp, upstream_comp_name,
		upstream_port, upstream_port_name);

	for (i = 0; i < ctx->cfg->cmd_data.run.connections->len; i++) {
		struct bt_config_connection *cfg_conn =
			g_ptr_array_index(
				ctx->cfg->cmd_data.run.connections, i);

		if (strcmp(cfg_conn->upstream_comp_name->str,
				upstream_comp_name)) {
			continue;
		}

		if (!bt_common_star_glob_match(
			    cfg_conn->upstream_port_glob->str,
			    SIZE_MAX, upstream_port_name, SIZE_MAX)) {
			continue;
		}

		ret = cmd_run_ctx_connect_upstream_port_to_downstream_component(
			ctx, upstream_comp, upstream_port, cfg_conn);
		if (ret) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Cannot connect upstream port: "
				"port-addr=%p, port-name=\"%s\"",
				upstream_port,
				upstream_port_name);
			goto error;
		}
		goto end;
	}

	BT_CLI_LOGE_APPEND_CAUSE(
		"Cannot connect upstream port: port does not match any connection argument: "
		"port-addr=%p, port-name=\"%s\"", upstream_port,
		upstream_port_name);

error:
	ret = -1;

end:
	return ret;
}

static
bt_graph_listener_func_status
graph_output_port_added_listener(struct cmd_run_ctx *ctx,
		const bt_port_output *out_port)
{
	const bt_component *comp;
	const bt_port *port = bt_port_output_as_port_const(out_port);
	bt_graph_listener_func_status ret =
		BT_GRAPH_LISTENER_FUNC_STATUS_OK;

	comp = bt_port_borrow_component_const(port);
	BT_LOGI("Port added to a graph's component: comp-addr=%p, "
		"comp-name=\"%s\", port-addr=%p, port-name=\"%s\"",
		comp, comp ? bt_component_get_name(comp) : "",
		port, bt_port_get_name(port));

	if (!ctx->connect_ports) {
		goto end;
	}

	BT_ASSERT(comp);

	if (bt_port_is_connected(port)) {
		BT_LOGW_STR("Port is already connected.");
		goto end;
	}

	if (cmd_run_ctx_connect_upstream_port(ctx, out_port)) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot connect upstream port.");
		ret = BT_GRAPH_LISTENER_FUNC_STATUS_ERROR;
		goto end;
	}

end:
	return ret;
}

static
bt_graph_listener_func_status graph_source_output_port_added_listener(
		const bt_component_source *component,
		const bt_port_output *port, void *data)
{
	return graph_output_port_added_listener(data, port);
}

static
bt_graph_listener_func_status graph_filter_output_port_added_listener(
		const bt_component_filter *component,
		const bt_port_output *port, void *data)
{
	return graph_output_port_added_listener(data, port);
}

static
void cmd_run_ctx_destroy(struct cmd_run_ctx *ctx)
{
	if (!ctx) {
		return;
	}

	if (ctx->src_components) {
		g_hash_table_destroy(ctx->src_components);
		ctx->src_components = NULL;
	}

	if (ctx->flt_components) {
		g_hash_table_destroy(ctx->flt_components);
		ctx->flt_components = NULL;
	}

	if (ctx->sink_components) {
		g_hash_table_destroy(ctx->sink_components);
		ctx->sink_components = NULL;
	}

	if (ctx->intersections) {
		g_hash_table_destroy(ctx->intersections);
		ctx->intersections = NULL;
	}

	BT_GRAPH_PUT_REF_AND_RESET(ctx->graph);
	ctx->cfg = NULL;
}

static
int add_descriptor_to_component_descriptor_set(
		bt_component_descriptor_set *comp_descr_set,
		const char *plugin_name, const char *comp_cls_name,
		bt_component_class_type comp_cls_type,
		const bt_value *params)
{
	const bt_component_class *comp_cls;
	int status = 0;

	comp_cls = find_component_class(plugin_name, comp_cls_name,
		comp_cls_type);
	if (!comp_cls) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find component class: plugin-name=\"%s\", "
			"comp-cls-name=\"%s\", comp-cls-type=%d",
			plugin_name, comp_cls_name, comp_cls_type);
		status = -1;
		goto end;
	}

	status = bt_component_descriptor_set_add_descriptor(
		comp_descr_set, comp_cls, params);
	if (status != BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot append descriptor to component descriptor set: "
			"status=%s", bt_common_func_status_string(status));
		goto end;
	}

end:
	bt_component_class_put_ref(comp_cls);
	return status;
}

static
int append_descriptors_from_bt_config_component_array(
		bt_component_descriptor_set *comp_descr_set,
		GPtrArray *component_configs)
{
	int ret = 0;
	uint64_t i;

	for (i = 0; i < component_configs->len; i++) {
		struct bt_config_component *cfg_comp =
			component_configs->pdata[i];

		ret = add_descriptor_to_component_descriptor_set(
			comp_descr_set,
			cfg_comp->plugin_name->str,
			cfg_comp->comp_cls_name->str,
			cfg_comp->type, cfg_comp->params);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
bt_get_greatest_operative_mip_version_status get_greatest_operative_mip_version(
		struct bt_config *cfg, uint64_t *mip_version)
{
	bt_get_greatest_operative_mip_version_status status =
		BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK;
	bt_component_descriptor_set *comp_descr_set = NULL;
	int ret;

	BT_ASSERT(cfg);
	BT_ASSERT(mip_version);
	comp_descr_set = bt_component_descriptor_set_create();
	if (!comp_descr_set) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Failed to create a component descriptor set object.");
		status = BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_MEMORY_ERROR;
		goto end;
	}

	ret = append_descriptors_from_bt_config_component_array(
		comp_descr_set, cfg->cmd_data.run.sources);
	if (ret) {
		status = BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR;
		goto end;
	}

	ret = append_descriptors_from_bt_config_component_array(
		comp_descr_set, cfg->cmd_data.run.filters);
	if (ret) {
		status = BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR;
		goto end;
	}

	ret = append_descriptors_from_bt_config_component_array(
		comp_descr_set, cfg->cmd_data.run.sinks);
	if (ret) {
		status = BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR;
		goto end;
	}

	if (cfg->cmd_data.run.stream_intersection_mode) {
		/*
		 * Stream intersection mode adds `flt.utils.trimmer`
		 * components; we need to include this type of component
		 * in the component descriptor set to get the real
		 * greatest operative MIP version.
		 */
		ret = add_descriptor_to_component_descriptor_set(
			comp_descr_set, "utils", "trimmer",
			BT_COMPONENT_CLASS_TYPE_FILTER, NULL);
		if (ret) {
			status = BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR;
			goto end;
		}
	}

	status = bt_get_greatest_operative_mip_version(comp_descr_set,
		bt_cli_log_level, mip_version);

end:
	bt_component_descriptor_set_put_ref(comp_descr_set);
	return status;
}

static
int cmd_run_ctx_init(struct cmd_run_ctx *ctx, struct bt_config *cfg)
{
	int ret = 0;
	bt_graph_add_listener_status add_listener_status;
	bt_get_greatest_operative_mip_version_status mip_version_status;
	uint64_t mip_version = UINT64_C(-1);

	ctx->cfg = cfg;
	ctx->connect_ports = false;
	ctx->src_components = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) bt_object_put_ref);
	if (!ctx->src_components) {
		goto error;
	}

	ctx->flt_components = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) bt_object_put_ref);
	if (!ctx->flt_components) {
		goto error;
	}

	ctx->sink_components = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) bt_object_put_ref);
	if (!ctx->sink_components) {
		goto error;
	}

	if (cfg->cmd_data.run.stream_intersection_mode) {
		ctx->stream_intersection_mode = true;
		ctx->intersections = g_hash_table_new_full(port_id_hash,
			port_id_equal, port_id_destroy, trace_range_destroy);
		if (!ctx->intersections) {
			goto error;
		}
	}

	/*
	 * Get the greatest operative MIP version to use to configure
	 * the graph to create.
	 */
	mip_version_status = get_greatest_operative_mip_version(
		cfg, &mip_version);
	if (mip_version_status == BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Failed to find an operative message interchange "
			"protocol version to use to create the `run` command's "
			"graph (components are not interoperable).");
		goto error;
	} else if (mip_version_status < 0) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot find an operative message interchange "
			"protocol version to use to create the `run` command's "
			"graph: status=%s",
			bt_common_func_status_string(mip_version_status));
		goto error;
	}

	BT_ASSERT(mip_version_status == BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK);
	BT_LOGI("Found operative message interchange protocol version to "
		"configure the `run` command's graph: mip-version=%" PRIu64,
		mip_version);
	ctx->graph = bt_graph_create(mip_version);
	if (!ctx->graph) {
		goto error;
	}

	bt_graph_add_interrupter(ctx->graph, the_interrupter);
	add_listener_status = bt_graph_add_source_component_output_port_added_listener(
		ctx->graph, graph_source_output_port_added_listener, ctx,
		NULL);
	if (add_listener_status != BT_GRAPH_ADD_LISTENER_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot add \"port added\" listener to graph.");
		goto error;
	}

	add_listener_status = bt_graph_add_filter_component_output_port_added_listener(
		ctx->graph, graph_filter_output_port_added_listener, ctx,
		NULL);
	if (add_listener_status != BT_GRAPH_ADD_LISTENER_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot add \"port added\" listener to graph.");
		goto error;
	}

	goto end;

error:
	cmd_run_ctx_destroy(ctx);
	ret = -1;

end:
	return ret;
}

/*
 * Compute the intersection of all streams in the array `streams`, write it
 * in `range`.
 */

static
int compute_stream_intersection(const bt_value *streams,
		struct trace_range *range)
{
	uint64_t i, stream_count;
	int ret;

	BT_ASSERT(bt_value_get_type(streams) == BT_VALUE_TYPE_ARRAY);

	stream_count = bt_value_array_get_length(streams);

	BT_ASSERT(stream_count > 0);

	range->intersection_range_begin_ns = 0;
	range->intersection_range_end_ns = UINT64_MAX;

	for (i = 0; i < stream_count; i++) {
		int64_t begin_ns, end_ns;
		uint64_t begin_ns_u, end_ns_u;
		const bt_value *stream_value;
		const bt_value *range_ns_value;
		const bt_value *begin_value;
		const bt_value *end_value;

		stream_value = bt_value_array_borrow_element_by_index_const(streams, i);
		if (bt_value_get_type(stream_value) != BT_VALUE_TYPE_MAP) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"expected streams array element to be a map, got %s.",
				bt_common_value_type_string(bt_value_get_type(stream_value)));
			goto error;
		}

		range_ns_value = bt_value_map_borrow_entry_value_const(
			stream_value, "range-ns");
		if (!range_ns_value) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"missing expected `range-ns` key in stream map.");
			goto error;
		}

		if (bt_value_get_type(range_ns_value) != BT_VALUE_TYPE_MAP) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"expected `range-ns` entry value of stream map to be a map, got %s.",
				bt_common_value_type_string(bt_value_get_type(range_ns_value)));
			goto error;
		}

		begin_value = bt_value_map_borrow_entry_value_const(range_ns_value, "begin");
		if (!begin_value) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"missing expected `begin` key in range-ns map.");
			goto error;
		}

		if (bt_value_get_type(begin_value) != BT_VALUE_TYPE_SIGNED_INTEGER) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"expected `begin` entry value of range-ns map to be a signed integer, got %s.",
				bt_common_value_type_string(bt_value_get_type(range_ns_value)));
			goto error;
		}

		end_value = bt_value_map_borrow_entry_value_const(range_ns_value, "end");
		if (!end_value) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"missing expected `end` key in range-ns map.");
			goto error;
		}

		if (bt_value_get_type(end_value) != BT_VALUE_TYPE_SIGNED_INTEGER) {
			BT_CLI_LOGE_APPEND_CAUSE("Unexpected format of `babeltrace.trace-infos` query result: "
				"expected `end` entry value of range-ns map to be a signed integer, got %s.",
				bt_common_value_type_string(bt_value_get_type(range_ns_value)));
			goto error;
		}

		begin_ns = bt_value_integer_signed_get(begin_value);
		end_ns = bt_value_integer_signed_get(end_value);

		if (begin_ns < 0 || end_ns < 0 || end_ns < begin_ns) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Invalid stream range values: "
				"range-ns:begin=%" PRId64 ", "
				"range-ns:end=%" PRId64,
				begin_ns, end_ns);
			goto error;
		}

		begin_ns_u = begin_ns;
		end_ns_u = end_ns;

		range->intersection_range_begin_ns =
			MAX(range->intersection_range_begin_ns, begin_ns_u);
		range->intersection_range_end_ns =
			MIN(range->intersection_range_end_ns, end_ns_u);
	}

	ret = 0;
	goto end;
error:
	ret = -1;

end:
	return ret;
}

static
int set_stream_intersections(struct cmd_run_ctx *ctx,
		struct bt_config_component *cfg_comp,
		const bt_component_class_source *src_comp_cls)
{
	int ret = 0;
	uint64_t trace_idx;
	uint64_t trace_count;
	const bt_value *query_result = NULL;
	const bt_value *trace_info = NULL;
	const bt_value *stream_infos = NULL;
	const bt_value *stream_info = NULL;
	struct port_id *port_id = NULL;
	struct trace_range *stream_intersection = NULL;
	const char *fail_reason = NULL;
	const bt_component_class *comp_cls =
		bt_component_class_source_as_component_class_const(src_comp_cls);

	ret = query(ctx->cfg, comp_cls, "babeltrace.trace-infos",
		cfg_comp->params, &query_result,
		&fail_reason);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to execute `babeltrace.trace-infos` query: %s: "
			"comp-class-name=\"%s\"", fail_reason,
			bt_component_class_get_name(comp_cls));
		ret = -1;
		goto end;
	}

	BT_ASSERT(query_result);

	if (!bt_value_is_array(query_result)) {
		BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: expecting result to be an array: "
			"component-class-name=%s, actual-type=%s",
			bt_component_class_get_name(comp_cls),
			bt_common_value_type_string(bt_value_get_type(query_result)));
		ret = -1;
		goto end;
	}

	trace_count = bt_value_array_get_length(query_result);
	if (trace_count == 0) {
		BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: result is empty: "
			"component-class-name=%s", bt_component_class_get_name(comp_cls));
		ret = -1;
		goto end;
	}

	for (trace_idx = 0; trace_idx < trace_count; trace_idx++) {
		uint64_t stream_idx;
		int64_t stream_count;
		struct trace_range trace_intersection;

		trace_info = bt_value_array_borrow_element_by_index_const(
			query_result, trace_idx);
		if (!bt_value_is_map(trace_info)) {
			ret = -1;
			BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: expecting element to be a map: "
				"component-class-name=%s, actual-type=%s",
				bt_component_class_get_name(comp_cls),
				bt_common_value_type_string(bt_value_get_type(trace_info)));
			goto end;
		}

		stream_infos = bt_value_map_borrow_entry_value_const(
			trace_info, "stream-infos");
		if (!stream_infos) {
			ret = -1;
			BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: missing `streams` key in trace info map: "
				"component-class-name=%s",
				bt_component_class_get_name(comp_cls));
			goto end;
		}

		if (!bt_value_is_array(stream_infos)) {
			ret = -1;
			BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: expecting `streams` entry of trace info map to be an array: "
				"component-class-name=%s, actual-type=%s",
				bt_component_class_get_name(comp_cls),
				bt_common_value_type_string(bt_value_get_type(stream_infos)));
			goto end;
		}

		stream_count = bt_value_array_get_length(stream_infos);
		if (stream_count == 0) {
			ret = -1;
			BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: list of streams is empty: "
				"component-class-name=%s",
				bt_component_class_get_name(comp_cls));
			goto end;
		}

		ret = compute_stream_intersection(stream_infos, &trace_intersection);
		if (ret != 0) {
			BT_CLI_LOGE_APPEND_CAUSE("Failed to compute trace streams intersection.");
			goto end;
		}

		for (stream_idx = 0; stream_idx < stream_count; stream_idx++) {
			const bt_value *port_name;

			port_id = g_new0(struct port_id, 1);
			if (!port_id) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE(
					"Cannot allocate memory for port_id structure.");
				goto end;
			}
			port_id->instance_name = strdup(cfg_comp->instance_name->str);
			if (!port_id->instance_name) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE(
					"Cannot allocate memory for port_id component instance name.");
				goto end;
			}

			stream_intersection = g_new0(struct trace_range, 1);
			if (!stream_intersection) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE(
					"Cannot allocate memory for trace_range structure.");
				goto end;
			}

			*stream_intersection = trace_intersection;

			stream_info = bt_value_array_borrow_element_by_index_const(
				stream_infos, stream_idx);
			if (!bt_value_is_map(stream_info)) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: "
					"expecting element of stream list to be a map: "
					"component-class-name=%s, actual-type=%s",
					bt_component_class_get_name(comp_cls),
					bt_common_value_type_string(bt_value_get_type(stream_info)));
				goto end;
			}

			port_name = bt_value_map_borrow_entry_value_const(stream_info, "port-name");
			if (!port_name) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: "
					"missing `port-name` key in stream info map: "
					"component-class-name=%s",
					bt_component_class_get_name(comp_cls));
				goto end;
			}

			if (!bt_value_is_string(port_name)) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE("`babeltrace.trace-infos` query: "
					"expecting `port-name` entry of stream info map to be a string: "
					"component-class-name=%s, actual-type=%s",
					bt_component_class_get_name(comp_cls),
					bt_common_value_type_string(bt_value_get_type(port_name)));
				goto end;
			}

			port_id->port_name = g_strdup(bt_value_string_get(port_name));
			if (!port_id->port_name) {
				ret = -1;
				BT_CLI_LOGE_APPEND_CAUSE(
					"Cannot allocate memory for port_id port_name.");
				goto end;
			}

			BT_LOGD("Inserting stream intersection ");

			g_hash_table_insert(ctx->intersections, port_id, stream_intersection);

			port_id = NULL;
			stream_intersection = NULL;
		}
	}

	ret = 0;

end:
	bt_value_put_ref(query_result);
	g_free(port_id);
	g_free(stream_intersection);
	return ret;
}

static
int cmd_run_ctx_create_components_from_config_components(
		struct cmd_run_ctx *ctx, GPtrArray *cfg_components)
{
	size_t i;
	const void *comp_cls = NULL;
	const void *comp = NULL;
	int ret = 0;

	for (i = 0; i < cfg_components->len; i++) {
		struct bt_config_component *cfg_comp =
			g_ptr_array_index(cfg_components, i);
		GQuark quark;

		switch (cfg_comp->type) {
		case BT_COMPONENT_CLASS_TYPE_SOURCE:
			comp_cls = find_source_component_class(
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str);
			break;
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			comp_cls = find_filter_component_class(
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str);
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			comp_cls = find_sink_component_class(
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str);
			break;
		default:
			bt_common_abort();
		}

		if (!comp_cls) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Cannot find component class: plugin-name=\"%s\", "
				"comp-cls-name=\"%s\", comp-cls-type=%d",
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str,
				cfg_comp->type);
			goto error;
		}

		BT_ASSERT(cfg_comp->log_level >= BT_LOG_TRACE);

		switch (cfg_comp->type) {
		case BT_COMPONENT_CLASS_TYPE_SOURCE:
			ret = bt_graph_add_source_component(ctx->graph,
				comp_cls, cfg_comp->instance_name->str,
				cfg_comp->params, cfg_comp->log_level,
				(void *) &comp);
			bt_component_source_get_ref(comp);
			break;
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			ret = bt_graph_add_filter_component(ctx->graph,
				comp_cls, cfg_comp->instance_name->str,
				cfg_comp->params, cfg_comp->log_level,
				(void *) &comp);
			bt_component_filter_get_ref(comp);
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			ret = bt_graph_add_sink_component(ctx->graph,
				comp_cls, cfg_comp->instance_name->str,
				cfg_comp->params, cfg_comp->log_level,
				(void *) &comp);
			bt_component_sink_get_ref(comp);
			break;
		default:
			bt_common_abort();
		}

		if (ret) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Cannot create component: plugin-name=\"%s\", "
				"comp-cls-name=\"%s\", comp-cls-type=%d, "
				"comp-name=\"%s\"",
				cfg_comp->plugin_name->str,
				cfg_comp->comp_cls_name->str,
				cfg_comp->type, cfg_comp->instance_name->str);
			goto error;
		}

		if (ctx->stream_intersection_mode &&
				cfg_comp->type == BT_COMPONENT_CLASS_TYPE_SOURCE) {
			ret = set_stream_intersections(ctx, cfg_comp, comp_cls);
			if (ret) {
				BT_CLI_LOGE_APPEND_CAUSE(
					"Cannot determine stream intersection of trace.");
				goto error;
			}
		}

		BT_LOGI("Created and inserted component: comp-addr=%p, comp-name=\"%s\"",
			comp, cfg_comp->instance_name->str);
		quark = g_quark_from_string(cfg_comp->instance_name->str);
		BT_ASSERT(quark > 0);

		switch (cfg_comp->type) {
		case BT_COMPONENT_CLASS_TYPE_SOURCE:
			g_hash_table_insert(ctx->src_components,
				GUINT_TO_POINTER(quark), (void *) comp);
			break;
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			g_hash_table_insert(ctx->flt_components,
				GUINT_TO_POINTER(quark), (void *) comp);
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			g_hash_table_insert(ctx->sink_components,
				GUINT_TO_POINTER(quark), (void *) comp);
			break;
		default:
			bt_common_abort();
		}

		comp = NULL;
		BT_OBJECT_PUT_REF_AND_RESET(comp_cls);
	}

	goto end;

error:
	ret = -1;

end:
	bt_object_put_ref(comp);
	bt_object_put_ref(comp_cls);
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

typedef uint64_t (*output_port_count_func_t)(const void *);
typedef const bt_port_output *(*borrow_output_port_by_index_func_t)(
	const void *, uint64_t);

static
int cmd_run_ctx_connect_comp_ports(struct cmd_run_ctx *ctx,
		void *comp, output_port_count_func_t port_count_fn,
		borrow_output_port_by_index_func_t port_by_index_fn)
{
	int ret = 0;
	uint64_t count;
	uint64_t i;

	count = port_count_fn(comp);

	for (i = 0; i < count; i++) {
		const bt_port_output *upstream_port = port_by_index_fn(comp, i);

		BT_ASSERT(upstream_port);
		ret = cmd_run_ctx_connect_upstream_port(ctx, upstream_port);
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
	g_hash_table_iter_init(&iter, ctx->src_components);

	while (g_hash_table_iter_next(&iter, &g_name_quark, &g_comp)) {
		ret = cmd_run_ctx_connect_comp_ports(ctx, g_comp,
			(output_port_count_func_t)
				bt_component_source_get_output_port_count,
			(borrow_output_port_by_index_func_t)
				bt_component_source_borrow_output_port_by_index_const);
		if (ret) {
			goto end;
		}
	}

	g_hash_table_iter_init(&iter, ctx->flt_components);

	while (g_hash_table_iter_next(&iter, &g_name_quark, &g_comp)) {
		ret = cmd_run_ctx_connect_comp_ports(ctx, g_comp,
			(output_port_count_func_t)
				bt_component_filter_get_output_port_count,
			(borrow_output_port_by_index_func_t)
				bt_component_filter_borrow_output_port_by_index_const);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
enum bt_cmd_status cmd_run(struct bt_config *cfg)
{
	enum bt_cmd_status cmd_status;
	struct cmd_run_ctx ctx = { 0 };

	/* Initialize the command's context and the graph object */
	if (cmd_run_ctx_init(&ctx, cfg)) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot initialize the command's context.");
		goto error;
	}

	if (bt_interrupter_is_set(the_interrupter)) {
		BT_CLI_LOGW_APPEND_CAUSE(
			"Interrupted by user before creating components.");
		goto error;
	}

	BT_LOGI_STR("Creating components.");

	/* Create the requested component instances */
	if (cmd_run_ctx_create_components(&ctx)) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot create components.");
		goto error;
	}

	if (bt_interrupter_is_set(the_interrupter)) {
		BT_CLI_LOGW_APPEND_CAUSE(
			"Interrupted by user before connecting components.");
		goto error;
	}

	BT_LOGI_STR("Connecting components.");

	/* Connect the initially visible component ports */
	if (cmd_run_ctx_connect_ports(&ctx)) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot connect initial component ports.");
		goto error;
	}

	BT_LOGI_STR("Running the graph.");

	/* Run the graph */
	while (true) {
		bt_graph_run_status run_status = bt_graph_run(ctx.graph);

		/*
		 * Reset console in case something messed with console
		 * codes during the graph's execution.
		 */
		printf("%s", bt_common_color_reset());
		fflush(stdout);
		fprintf(stderr, "%s", bt_common_color_reset());
		BT_LOGT("bt_graph_run() returned: status=%s",
			bt_common_func_status_string(run_status));

		switch (run_status) {
		case BT_GRAPH_RUN_STATUS_OK:
			cmd_status = BT_CMD_STATUS_OK;
			goto end;
		case BT_GRAPH_RUN_STATUS_AGAIN:
			if (bt_interrupter_is_set(the_interrupter)) {
				/*
				 * The graph was interrupted by a SIGINT.
				 */
				cmd_status = BT_CMD_STATUS_INTERRUPTED;
				goto end;
			}

			if (cfg->cmd_data.run.retry_duration_us > 0) {
				BT_LOGT("Got BT_GRAPH_RUN_STATUS_AGAIN: sleeping: "
					"time-us=%" PRIu64,
					cfg->cmd_data.run.retry_duration_us);

				if (usleep(cfg->cmd_data.run.retry_duration_us)) {
					if (bt_interrupter_is_set(the_interrupter)) {
						cmd_status = BT_CMD_STATUS_INTERRUPTED;
						goto end;
					}
				}
			}
			break;
		default:
			if (bt_interrupter_is_set(the_interrupter)) {
				cmd_status = BT_CMD_STATUS_INTERRUPTED;
				goto end;
			}

			BT_CLI_LOGE_APPEND_CAUSE(
				"Graph failed to complete successfully");
			goto error;
		}
	}
error:
	cmd_status = BT_CMD_STATUS_ERROR;

end:
	cmd_run_ctx_destroy(&ctx);
	return cmd_status;
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
		_bt_log_write_d(_BT_LOG_SRCLOC_FUNCTION, __FILE__, __LINE__,
				BT_LOG_WARNING, BT_LOG_TAG,
				"The `%s` command was executed. "
				"If you meant to convert a trace located in "
				"the local `%s` directory, please use:\n\n"
				"    babeltrace2 convert %s [OPTIONS]",
				cfg->command_name, cfg->command_name,
				cfg->command_name);
	}
}

static
void init_log_level(void)
{
	bt_cli_log_level = bt_log_get_level_from_env(ENV_BABELTRACE_CLI_LOG_LEVEL);
}

static
void print_error_causes(void)
{
	const bt_error *error = bt_current_thread_take_error();
	unsigned int columns;
	gchar *error_str = NULL;

	if (!error || bt_error_get_cause_count(error) == 0) {
		fprintf(stderr, "%s%sUnknown command-line error.%s\n",
			bt_common_color_bold(), bt_common_color_fg_bright_red(),
			bt_common_color_reset());
		goto end;
	}

	/* Try to get terminal width to fold the error cause messages */
	if (bt_common_get_term_size(&columns, NULL) < 0) {
		/* Width not found: default to 80 */
		columns = 80;
	}

	/*
	 * This helps visually separate the error causes from the last
	 * logging statement.
	 */
	fputc('\n',  stderr);

	error_str = format_bt_error(error, columns, bt_cli_log_level,
		BT_COMMON_COLOR_WHEN_AUTO);
	BT_ASSERT(error_str);

	fprintf(stderr, "%s\n", error_str);

end:
	if (error) {
		bt_error_release(error);
	}

	g_free(error_str);
}

int main(int argc, const char **argv)
{
	int ret, retcode;
	enum bt_cmd_status cmd_status;
	struct bt_config *cfg = NULL;

	init_log_level();
	set_signal_handler();
	init_loaded_plugins();

	BT_ASSERT(!the_interrupter);
	the_interrupter = bt_interrupter_create();
	if (!the_interrupter) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to create an interrupter object.");
		retcode = 1;
		goto end;
	}

	cfg = bt_config_cli_args_create_with_default(argc, argv, &retcode,
		the_interrupter);

	if (retcode < 0) {
		/* Quit without errors; typically usage/version */
		retcode = 0;
		BT_LOGI_STR("Quitting without errors.");
		goto end;
	}

	if (retcode > 0) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Command-line error: retcode=%d", retcode);
		goto end;
	}

	if (!cfg) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Failed to create a valid Babeltrace CLI configuration.");
		retcode = 1;
		goto end;
	}

	print_cfg(cfg);

	if (cfg->command_needs_plugins) {
		ret = require_loaded_plugins(cfg->plugin_paths);
		if (ret) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Failed to load plugins: ret=%d", ret);
			retcode = 1;
			goto end;
		}
	}

	BT_LOGI("Executing command: cmd=%d, command-name=\"%s\"",
		cfg->command, cfg->command_name);

	switch (cfg->command) {
	case BT_CONFIG_COMMAND_RUN:
		cmd_status = cmd_run(cfg);
		break;
	case BT_CONFIG_COMMAND_LIST_PLUGINS:
		cmd_status = cmd_list_plugins(cfg);
		break;
	case BT_CONFIG_COMMAND_HELP:
		cmd_status = cmd_help(cfg);
		break;
	case BT_CONFIG_COMMAND_QUERY:
		cmd_status = cmd_query(cfg);
		break;
	case BT_CONFIG_COMMAND_PRINT_CTF_METADATA:
		cmd_status = cmd_print_ctf_metadata(cfg);
		break;
	case BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS:
		cmd_status = cmd_print_lttng_live_sessions(cfg);
		break;
	default:
		BT_LOGF("Invalid/unknown command: cmd=%d", cfg->command);
		bt_common_abort();
	}

	BT_LOGI("Command completed: cmd=%d, command-name=\"%s\", command-status=\"%s\"",
		cfg->command, cfg->command_name, bt_cmd_status_string(cmd_status));
	warn_command_name_and_directory_clash(cfg);

	switch (cmd_status) {
	case BT_CMD_STATUS_OK:
		retcode = 0;
		break;
	case BT_CMD_STATUS_ERROR:
		retcode = 1;
		break;
	case BT_CMD_STATUS_INTERRUPTED:
		retcode = 2;
		break;
	default:
		BT_LOGF("Invalid command status: cmd-status=%d", cmd_status);
		bt_common_abort();
	}

end:
	if (retcode == 1) {
		print_error_causes();
	}

	BT_OBJECT_PUT_REF_AND_RESET(cfg);
	fini_loaded_plugins();
	bt_interrupter_put_ref(the_interrupter);

	/*
	 * Clear current thread's error in case there is one to avoid a
	 * memory leak.
	 */
	bt_current_thread_clear_error();
	return retcode;
}
