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

#define BT_LOG_TAG "CLI"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/values-internal.h>
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

#define ENV_BABELTRACE_WARN_COMMAND_NAME_DIRECTORY_CLASH "BABELTRACE_CLI_WARN_COMMAND_NAME_DIRECTORY_CLASH"
#define ENV_BABELTRACE_CLI_LOG_LEVEL "BABELTRACE_CLI_LOG_LEVEL"
#define NSEC_PER_SEC	1000000000LL

/*
 * Known environment variable names for the log levels of the project's
 * modules.
 */
static const char* log_level_env_var_names[] = {
	"BABELTRACE_COMMON_LOG_LEVEL",
	"BABELTRACE_COMPAT_LOG_LEVEL",
	"BABELTRACE_PLUGIN_CTF_BTR_LOG_LEVEL",
	"BABELTRACE_SINK_CTF_FS_LOG_LEVEL",
	"BABELTRACE_SRC_CTF_FS_LOG_LEVEL",
	"BABELTRACE_SRC_CTF_LTTNG_LIVE_LOG_LEVEL",
	"BABELTRACE_PLUGIN_CTF_METADATA_LOG_LEVEL",
	"BABELTRACE_PLUGIN_CTF_NOTIF_ITER_LOG_LEVEL",
	"BABELTRACE_PLUGIN_CTFCOPYTRACE_LIB_LOG_LEVEL",
	"BABELTRACE_FLT_LTTNG_UTILS_DEBUG_INFO_LOG_LEVEL",
	"BABELTRACE_SRC_TEXT_DMESG_LOG_LEVEL",
	"BABELTRACE_SINK_TEXT_PRETTY_LOG_LEVEL",
	"BABELTRACE_FLT_UTILS_MUXER_LOG_LEVEL",
	"BABELTRACE_FLT_UTILS_TRIMMER_LOG_LEVEL",
	"BABELTRACE_PYTHON_BT2_LOG_LEVEL",
	"BABELTRACE_PYTHON_PLUGIN_PROVIDER_LOG_LEVEL",
	NULL,
};

/* Application's processing graph (weak) */
static struct bt_graph *the_graph;
static struct bt_query_executor *the_query_executor;
static bool canceled = false;

GPtrArray *loaded_plugins;

#ifdef __MINGW32__
#include <windows.h>

static
BOOL WINAPI signal_handler(DWORD signal) {
	if (the_graph) {
		bt_graph_cancel(the_graph);
	}

	canceled = true;

	return TRUE;
}

static
void set_signal_handler(void)
{
	if (!SetConsoleCtrlHandler(signal_handler, TRUE)) {
		BT_LOGE("Failed to set the ctrl+c handler.");
	}
}
#else /* __MINGW32__ */
static
void signal_handler(int signum)
{
	if (signum != SIGINT) {
		return;
	}

	if (the_graph) {
		bt_graph_cancel(the_graph);
	}

	if (the_query_executor) {
		bt_query_executor_cancel(the_query_executor);
	}

	canceled = true;
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
int create_the_query_executor(void)
{
	int ret = 0;

	the_query_executor = bt_query_executor_create();
	if (!the_query_executor) {
		BT_LOGE_STR("Cannot create a query executor.");
		ret = -1;
	}

	return ret;
}

static
void destroy_the_query_executor(void)
{
	BT_PUT(the_query_executor);
}

static
int query(struct bt_component_class *comp_cls, const char *obj,
		struct bt_value *params, struct bt_value **user_result,
		const char **fail_reason)
{
	struct bt_value *result = NULL;
	enum bt_query_status status;
	*fail_reason = "unknown error";
	int ret = 0;

	BT_ASSERT(fail_reason);
	BT_ASSERT(user_result);
	ret = create_the_query_executor();
	if (ret) {
		/* create_the_query_executor() logs errors */
		goto end;
	}

	if (canceled) {
		BT_LOGI("Canceled by user before executing the query: "
			"comp-cls-addr=%p, comp-cls-name=\"%s\", "
			"query-obj=\"%s\"", comp_cls,
			bt_component_class_get_name(comp_cls), obj);
		*fail_reason = "canceled by user";
		goto error;
	}

	while (true) {
		status = bt_query_executor_query(the_query_executor, comp_cls,
			obj, params, &result);
		switch (status) {
		case BT_QUERY_STATUS_OK:
			goto ok;
		case BT_QUERY_STATUS_AGAIN:
		{
			const uint64_t sleep_time_us = 100000;

			/* Wait 100 ms and retry */
			BT_LOGV("Got BT_QUERY_STATUS_AGAIN: sleeping: "
				"time-us=%" PRIu64, sleep_time_us);

			if (usleep(sleep_time_us)) {
				if (bt_query_executor_is_canceled(the_query_executor)) {
					BT_LOGI("Query was canceled by user: "
						"comp-cls-addr=%p, comp-cls-name=\"%s\", "
						"query-obj=\"%s\"", comp_cls,
						bt_component_class_get_name(comp_cls),
						obj);
					*fail_reason = "canceled by user";
					goto error;
				}
			}

			continue;
		}
		case BT_QUERY_STATUS_EXECUTOR_CANCELED:
			*fail_reason = "canceled by user";
			goto error;
		case BT_QUERY_STATUS_ERROR:
		case BT_QUERY_STATUS_INVALID:
			goto error;
		case BT_QUERY_STATUS_INVALID_OBJECT:
			*fail_reason = "invalid or unknown query object";
			goto error;
		case BT_QUERY_STATUS_INVALID_PARAMS:
			*fail_reason = "invalid query parameters";
			goto error;
		case BT_QUERY_STATUS_NOMEM:
			*fail_reason = "not enough memory";
			goto error;
		default:
			BT_LOGF("Unknown query status: status=%d", status);
			abort();
		}
	}

ok:
	*user_result = result;
	result = NULL;
	goto end;

error:
	ret = -1;

end:
	destroy_the_query_executor();
	bt_put(result);
	return ret;
}

static
struct bt_plugin *find_plugin(const char *name)
{
	int i;
	struct bt_plugin *plugin = NULL;

	BT_ASSERT(name);
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
		return "(unknown)";
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

	fprintf(fh, "'%s%s%s%s.%s%s%s.%s%s%s'",
		bt_common_color_bold(),
		bt_common_color_fg_cyan(),
		component_type_str(type),
		bt_common_color_fg_default(),
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
	BT_ASSERT(object);

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
	enum bt_value_status status;

	if (!value) {
		return;
	}

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_NULL:
		fprintf(fp, "%snull%s\n", bt_common_color_bold(),
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_BOOL:
		status = bt_value_bool_get(value, &bool_val);
		if (status != BT_VALUE_STATUS_OK) {
			goto error;
		}
		fprintf(fp, "%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_cyan(), bool_val ? "yes" : "no",
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_INTEGER:
		status = bt_value_integer_get(value, &int_val);
		if (status != BT_VALUE_STATUS_OK) {
			goto error;
		}
		fprintf(fp, "%s%s%" PRId64 "%s\n", bt_common_color_bold(),
			bt_common_color_fg_red(), int_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_REAL:
		status = bt_value_real_get(value, &dbl_val);
		if (status != BT_VALUE_STATUS_OK) {
			goto error;
		}
		fprintf(fp, "%s%s%lf%s\n", bt_common_color_bold(),
			bt_common_color_fg_red(), dbl_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_STRING:
		status = bt_value_string_get(value, &str_val);
		if (status != BT_VALUE_STATUS_OK) {
			goto error;
		}
		fprintf(fp, "%s%s%s%s\n", bt_common_color_bold(),
			bt_common_color_fg_green(), str_val,
			bt_common_color_reset());
		break;
	case BT_VALUE_TYPE_ARRAY:
		size = bt_value_array_size(value);
		if (size < 0) {
			goto error;
		}

		if (size == 0) {
			print_indent(fp, indent);
			fprintf(fp, "[ ]\n");
			break;
		}

		for (i = 0; i < size; i++) {
			struct bt_value *element =
					bt_value_array_get(value, i);

			if (!element) {
				goto error;
			}
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
		abort();
	}
	return;

error:
	BT_LOGE("Error printing value of type %s.",
		bt_value_type_string(bt_value_get_type(value)));
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
		abort();
	}
}

static
void add_to_loaded_plugins(struct bt_plugin_set *plugin_set)
{
	int64_t i;
	int64_t count;

	count = bt_plugin_set_get_plugin_count(plugin_set);
	BT_ASSERT(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_plugin *plugin =
			bt_plugin_set_get_plugin(plugin_set, i);
		struct bt_plugin *loaded_plugin =
				find_plugin(bt_plugin_get_name(plugin));

		BT_ASSERT(plugin);

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
		enum bt_value_status status;

		plugin_path_value = bt_value_array_get(plugin_paths, i);
		status = bt_value_string_get(plugin_path_value, &plugin_path);
		if (status != BT_VALUE_STATUS_OK) {
			BT_LOGD_STR("Cannot get plugin path string.");
			BT_PUT(plugin_path_value);
			continue;
		}

		/*
		 * Skip this if the directory does not exist because
		 * bt_plugin_create_all_from_dir() expects an existing
		 * directory.
		 */
		if (!g_file_test(plugin_path, G_FILE_TEST_IS_DIR)) {
			BT_LOGV("Skipping nonexistent directory path: "
				"path=\"%s\"", plugin_path);
			BT_PUT(plugin_path_value);
			continue;
		}

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
	if (path) {
		printf("  %sPath%s: %s\n", bt_common_color_bold(),
			bt_common_color_reset(), path);
	} else {
		puts("  Built-in");
	}

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
	const char *fail_reason = NULL;

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

	ret = query(comp_cls, cfg->cmd_data.query.object->str,
		cfg->cmd_data.query.cfg_component->params, &results,
		&fail_reason);
	if (ret) {
		goto failed;
	}

	print_value(stdout, results, 0);
	goto end;

failed:
	BT_LOGE("Failed to query component class: %s: plugin-name=\"%s\", "
		"comp-cls-name=\"%s\", comp-cls-type=%d "
		"object=\"%s\"", fail_reason,
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
	fprintf(stderr, "%s%s with object `%s`: %s%s\n",
		bt_common_color_bold(),
		bt_common_color_fg_red(),
		cfg->cmd_data.query.object->str,
		fail_reason,
		bt_common_color_reset());
	ret = -1;

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

		BT_ASSERT(comp_cls);

		if (cfg->cmd_data.help.cfg_component->type !=
				BT_COMPONENT_CLASS_TYPE_UNKNOWN) {
			if (strcmp(cfg->cmd_data.help.cfg_component->comp_cls_name->str,
					comp_class_name) != 0 ||
					type != cfg->cmd_data.help.cfg_component->type) {
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
	int ret = 0;
	struct bt_component_class *comp_cls = NULL;
	struct bt_value *results = NULL;
	struct bt_value *params = NULL;
	struct bt_value *map = NULL;
	struct bt_value *v = NULL;
	static const char * const plugin_name = "ctf";
	static const char * const comp_cls_name = "lttng-live";
	static const enum bt_component_class_type comp_cls_type =
		BT_COMPONENT_CLASS_TYPE_SOURCE;
	int64_t array_size, i;
	const char *fail_reason = NULL;
	FILE *out_stream = stdout;

	BT_ASSERT(cfg->cmd_data.print_lttng_live_sessions.url);
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
		goto error;
	}

	params = bt_value_map_create();
	if (!params) {
		goto error;
	}

	ret = bt_value_map_insert_string(params, "url",
		cfg->cmd_data.print_lttng_live_sessions.url->str);
	if (ret) {
		goto error;
	}

	ret = query(comp_cls, "sessions", params, &results, &fail_reason);
	if (ret) {
		goto failed;
	}

	BT_ASSERT(results);

	if (!bt_value_is_array(results)) {
		BT_LOGE_STR("Expecting an array for sessions query.");
		fprintf(stderr, "%s%sUnexpected type returned by session query%s\n",
			bt_common_color_bold(),
			bt_common_color_fg_red(),
			bt_common_color_reset());
		goto error;
	}

	if (cfg->cmd_data.print_lttng_live_sessions.output_path->len > 0) {
		out_stream =
			fopen(cfg->cmd_data.print_lttng_live_sessions.output_path->str,
				"w");
		if (!out_stream) {
			ret = -1;
			BT_LOGE_ERRNO("Cannot open file for writing",
				": path=\"%s\"",
				cfg->cmd_data.print_lttng_live_sessions.output_path->str);
			goto end;
		}
	}

	array_size = bt_value_array_size(results);
	for (i = 0; i < array_size; i++) {
		const char *url_text;
		int64_t timer_us, streams, clients;

		map = bt_value_array_get(results, i);
		if (!map) {
			BT_LOGE_STR("Unexpected empty array entry.");
			goto error;
		}
		if (!bt_value_is_map(map)) {
			BT_LOGE_STR("Unexpected entry type.");
			goto error;
		}

		v = bt_value_map_get(map, "url");
		if (!v) {
			BT_LOGE_STR("Unexpected empty array \"url\" entry.");
			goto error;
		}
		ret = bt_value_string_get(v, &url_text);
		BT_ASSERT(ret == 0);
		fprintf(out_stream, "%s", url_text);
		BT_PUT(v);

		v = bt_value_map_get(map, "timer-us");
		if (!v) {
			BT_LOGE_STR("Unexpected empty array \"timer-us\" entry.");
			goto error;
		}
		ret = bt_value_integer_get(v, &timer_us);
		BT_ASSERT(ret == 0);
		fprintf(out_stream, " (timer = %" PRIu64 ", ", timer_us);
		BT_PUT(v);

		v = bt_value_map_get(map, "stream-count");
		if (!v) {
			BT_LOGE_STR("Unexpected empty array \"stream-count\" entry.");
			goto error;
		}
		ret = bt_value_integer_get(v, &streams);
		BT_ASSERT(ret == 0);
		fprintf(out_stream, "%" PRIu64 " stream(s), ", streams);
		BT_PUT(v);

		v = bt_value_map_get(map, "client-count");
		if (!v) {
			BT_LOGE_STR("Unexpected empty array \"client-count\" entry.");
			goto error;
		}
		ret = bt_value_integer_get(v, &clients);
		BT_ASSERT(ret == 0);
		fprintf(out_stream, "%" PRIu64 " client(s) connected)\n", clients);
		BT_PUT(v);

		BT_PUT(map);
	}

	goto end;

failed:
	BT_LOGE("Failed to query for sessions: %s", fail_reason);
	fprintf(stderr, "%s%sFailed to request sessions: %s%s\n",
		bt_common_color_bold(),
		bt_common_color_fg_red(),
		fail_reason,
		bt_common_color_reset());

error:
	ret = -1;

end:
	bt_put(v);
	bt_put(map);
	bt_put(results);
	bt_put(params);
	bt_put(comp_cls);

	if (out_stream && out_stream != stdout) {
		int fclose_ret = fclose(out_stream);

		if (fclose_ret) {
			BT_LOGE_ERRNO("Cannot close file stream",
				": path=\"%s\"",
				cfg->cmd_data.print_lttng_live_sessions.output_path->str);
		}
	}

	return 0;
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
	const char *fail_reason = NULL;
	FILE *out_stream = stdout;

	BT_ASSERT(cfg->cmd_data.print_ctf_metadata.path);
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

	ret = query(comp_cls, "metadata-info", params, &results, &fail_reason);
	if (ret) {
		goto failed;
	}

	metadata_text_value = bt_value_map_get(results, "text");
	if (!metadata_text_value) {
		BT_LOGE_STR("Cannot find `text` string value in the resulting metadata info object.");
		ret = -1;
		goto end;
	}

	ret = bt_value_string_get(metadata_text_value, &metadata_text);
	BT_ASSERT(ret == 0);

	if (cfg->cmd_data.print_ctf_metadata.output_path->len > 0) {
		out_stream =
			fopen(cfg->cmd_data.print_ctf_metadata.output_path->str,
				"w");
		if (!out_stream) {
			ret = -1;
			BT_LOGE_ERRNO("Cannot open file for writing",
				": path=\"%s\"",
				cfg->cmd_data.print_ctf_metadata.output_path->str);
			goto end;
		}
	}

	ret = fprintf(out_stream, "%s\n", metadata_text);
	if (ret < 0) {
		BT_LOGE("Cannot write whole metadata text to output stream: "
			"ret=%d", ret);
	}

	goto end;

failed:
	ret = -1;
	BT_LOGE("Failed to query for metadata info: %s", fail_reason);
	fprintf(stderr, "%s%sFailed to request metadata info: %s%s\n",
		bt_common_color_bold(),
		bt_common_color_fg_red(),
		fail_reason,
		bt_common_color_reset());

end:
	destroy_the_query_executor();
	bt_put(results);
	bt_put(params);
	bt_put(metadata_text_value);
	bt_put(comp_cls);

	if (out_stream && out_stream != stdout) {
		int fclose_ret = fclose(out_stream);

		if (fclose_ret) {
			BT_LOGE_ERRNO("Cannot close file stream",
				": path=\"%s\"",
				cfg->cmd_data.print_ctf_metadata.output_path->str);
		}
	}

	return 0;
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

	return !strcmp(id1->instance_name, id2->instance_name) &&
		!strcmp(id1->port_name, id2->port_name);
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
	GHashTable *components;

	/* Owned by this */
	struct bt_graph *graph;

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
	enum bt_graph_status status = BT_GRAPH_STATUS_ERROR;
	bool insert_trimmer = false;
	struct bt_value *trimmer_params = NULL;
	char *intersection_begin = NULL;
	char *intersection_end = NULL;
	struct bt_component *trimmer = NULL;
	struct bt_component_class *trimmer_class = NULL;
	struct bt_port *trimmer_input = NULL;
	struct bt_port *trimmer_output = NULL;

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
			enum bt_value_status status;

			intersection_begin = s_from_ns(
				range->intersection_range_begin_ns);
			intersection_end = s_from_ns(
				range->intersection_range_end_ns);
			if (!intersection_begin || !intersection_end) {
				BT_LOGE_STR("Cannot create trimmer argument timestamp string.");
				goto error;
			}

			insert_trimmer = true;
			trimmer_params = bt_value_map_create();
			if (!trimmer_params) {
				goto error;
			}

			status = bt_value_map_insert_string(trimmer_params,
				"begin", intersection_begin);
			if (status != BT_VALUE_STATUS_OK) {
				goto error;
			}
			status = bt_value_map_insert_string(trimmer_params,
				"end", intersection_end);
			if (status != BT_VALUE_STATUS_OK) {
				goto error;
			}
		}

		trimmer_class = find_component_class("utils", "trimmer",
			BT_COMPONENT_CLASS_TYPE_FILTER);
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
	downstream_comp = g_hash_table_lookup(ctx->components,
		GUINT_TO_POINTER(downstreamp_comp_name_quark));
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
		abort();
	}

	downstream_port_count = port_count_fn(downstream_comp);
	BT_ASSERT(downstream_port_count >= 0);

	for (i = 0; i < downstream_port_count; i++) {
		struct bt_port *downstream_port =
			port_by_index_fn(downstream_comp, i);
		const char *upstream_port_name;
		const char *downstream_port_name;

		BT_ASSERT(downstream_port);

		/* Skip port if it's already connected. */
		if (bt_port_is_connected(downstream_port)) {
			bt_put(downstream_port);
			BT_LOGD("Skipping downstream port: already connected: "
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
			bt_put(downstream_port);
			continue;
		}

		if (insert_trimmer) {
			/*
			 * In order to insert the trimmer between the two
			 * components that were being connected, we create
			 * a connection configuration entry which describes
			 * a connection from the trimmer's output to the
			 * original input that was being connected.
			 *
			 * Hence, the creation of the trimmer will cause the
			 * graph "new port" listener to establish all downstream
			 * connections as its output port is connected. We will
			 * then establish the connection between the original
			 * upstream source and the trimmer.
			 */
			char *trimmer_name = NULL;
			enum bt_graph_status graph_status;

			ret = asprintf(&trimmer_name, "%s-%s",
				"stream-intersection-trimmer",
				upstream_port_name);
			if (ret < 0) {
				goto error;
			}
			ret = 0;

			ctx->connect_ports = false;
			graph_status = bt_graph_add_component(ctx->graph,
				trimmer_class, trimmer_name, trimmer_params,
				&trimmer);
			free(trimmer_name);
			if (graph_status != BT_GRAPH_STATUS_OK) {
				goto error;
			}
			BT_ASSERT(trimmer);

			trimmer_input =
				bt_component_filter_get_input_port_by_index(
					trimmer, 0);
			if (!trimmer_input) {
				goto error;
			}
			trimmer_output =
				bt_component_filter_get_output_port_by_index(
					trimmer, 0);
			if (!trimmer_output) {
				goto error;
			}

			/*
			 * Replace the current downstream port by the trimmer's
			 * upstream port.
			 */
		        BT_MOVE(downstream_port, trimmer_input);
			downstream_port_name = bt_port_get_name(
				downstream_port);
			if (!downstream_port_name) {
				goto error;
			}
		}

		/* We have a winner! */
		status = bt_graph_connect_ports(ctx->graph,
			upstream_port, downstream_port, NULL);
		BT_PUT(downstream_port);
		switch (status) {
		case BT_GRAPH_STATUS_OK:
			break;
		case BT_GRAPH_STATUS_CANCELED:
			BT_LOGI_STR("Graph was canceled by user.");
			status = BT_GRAPH_STATUS_OK;
			break;
		case BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION:
			BT_LOGE("A component refused a connection to one of its ports: "
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
				"A component refused a connection to one of its ports (`%s` to `%s`): %s\n",
				bt_port_get_name(upstream_port),
				downstream_port_name,
				cfg_conn->arg->str);
			break;
		default:
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

		if (insert_trimmer) {
			/*
			 * The first connection, from the source to the trimmer,
			 * has been done. We now connect the trimmer to the
			 * original downstream port.
			 */
			ret = cmd_run_ctx_connect_upstream_port_to_downstream_component(
				ctx, trimmer, trimmer_output, cfg_conn);
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
	BT_LOGE("Cannot create connection: cannot find a matching downstream port for upstream port: "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-comp-name=\"%s\", conn-arg=\"%s\"",
		upstream_port, bt_port_get_name(upstream_port),
		cfg_conn->downstream_comp_name->str,
		cfg_conn->arg->str);
	fprintf(stderr,
		"Cannot create connection: cannot find a matching downstream port for upstream port `%s`: %s\n",
		bt_port_get_name(upstream_port), cfg_conn->arg->str);

error:
	ret = -1;

end:
	free(intersection_begin);
	free(intersection_end);
	BT_PUT(trimmer_params);
	BT_PUT(trimmer_class);
	BT_PUT(trimmer);
	BT_PUT(trimmer_input);
	BT_PUT(trimmer_output);
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

	BT_ASSERT(ctx);
	BT_ASSERT(upstream_port);
	upstream_port_name = bt_port_get_name(upstream_port);
	BT_ASSERT(upstream_port_name);
	upstream_comp = bt_port_get_component(upstream_port);
	if (!upstream_comp) {
		BT_LOGW("Upstream port to connect is not part of a component: "
			"port-addr=%p, port-name=\"%s\"",
			upstream_port, upstream_port_name);
		ret = -1;
		goto end;
	}

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

	if (!ctx->connect_ports) {
		goto end;
	}

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

	BT_ASSERT(upstream_comp);
	BT_ASSERT(downstream_comp);
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

	if (ctx->intersections) {
		g_hash_table_destroy(ctx->intersections);
		ctx->intersections = NULL;
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

	if (cfg->cmd_data.run.stream_intersection_mode) {
		ctx->stream_intersection_mode = true;
		ctx->intersections = g_hash_table_new_full(port_id_hash,
			port_id_equal, port_id_destroy, trace_range_destroy);
		if (!ctx->intersections) {
			goto error;
		}
	}

	ctx->graph = bt_graph_create();
	if (!ctx->graph) {
		goto error;
	}

	the_graph = ctx->graph;
	ret = bt_graph_add_port_added_listener(ctx->graph,
		graph_port_added_listener, NULL, ctx);
	if (ret < 0) {
		BT_LOGE_STR("Cannot add \"port added\" listener to graph.");
		goto error;
	}

	ret = bt_graph_add_port_removed_listener(ctx->graph,
		graph_port_removed_listener, NULL, ctx);
	if (ret < 0) {
		BT_LOGE_STR("Cannot add \"port removed\" listener to graph.");
		goto error;
	}

	ret = bt_graph_add_ports_connected_listener(ctx->graph,
		graph_ports_connected_listener, NULL, ctx);
	if (ret < 0) {
		BT_LOGE_STR("Cannot add \"ports connected\" listener to graph.");
		goto error;
	}

	ret = bt_graph_add_ports_disconnected_listener(ctx->graph,
		graph_ports_disconnected_listener, NULL, ctx);
	if (ret < 0) {
		BT_LOGE_STR("Cannot add \"ports disconnected\" listener to graph.");
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
int set_stream_intersections(struct cmd_run_ctx *ctx,
		struct bt_config_component *cfg_comp,
		struct bt_component_class *comp_cls)
{
	int ret = 0;
	uint64_t trace_idx;
	int64_t trace_count;
	enum bt_value_status value_status;
	const char *path = NULL;
	struct bt_value *component_path_value = NULL;
	struct bt_value *query_params = NULL;
	struct bt_value *query_result = NULL;
	struct bt_value *trace_info = NULL;
	struct bt_value *intersection_range = NULL;
	struct bt_value *intersection_begin = NULL;
	struct bt_value *intersection_end = NULL;
	struct bt_value *stream_path_value = NULL;
	struct bt_value *stream_paths = NULL;
	struct bt_value *stream_infos = NULL;
	struct bt_value *stream_info = NULL;
	struct port_id *port_id = NULL;
	struct trace_range *trace_range = NULL;
	const char *fail_reason = NULL;

	component_path_value = bt_value_map_get(cfg_comp->params, "path");
	if (component_path_value && !bt_value_is_string(component_path_value)) {
		BT_LOGD("Cannot get path parameter: component-name=%s",
			cfg_comp->instance_name->str);
		ret = -1;
		goto error;
	}

	value_status = bt_value_string_get(component_path_value, &path);
	if (value_status != BT_VALUE_STATUS_OK) {
		BT_LOGD("Cannot get path string value: component-name=%s",
			cfg_comp->instance_name->str);
		ret = -1;
		goto error;
	}

	query_params = bt_value_map_create();
	if (!query_params) {
		BT_LOGE_STR("Cannot create query parameters.");
		ret = -1;
		goto error;
	}

	value_status = bt_value_map_insert(query_params, "path", component_path_value);
	if (value_status != BT_VALUE_STATUS_OK) {
		BT_LOGE_STR("Cannot insert path parameter in query parameter map.");
		ret = -1;
		goto error;
	}

	ret = query(comp_cls, "trace-info", query_params, &query_result,
		&fail_reason);
	if (ret) {
		BT_LOGD("Component class does not support the `trace-info` query: %s: "
			"comp-class-name=\"%s\"", fail_reason,
			bt_component_class_get_name(comp_cls));
		ret = -1;
		goto error;
	}

	BT_ASSERT(query_result);

	if (!bt_value_is_array(query_result)) {
		BT_LOGD("Unexpected format of \'trace-info\' query result: "
			"component-class-name=%s",
			bt_component_class_get_name(comp_cls));
		ret = -1;
		goto error;
	}

	trace_count = bt_value_array_size(query_result);
	if (trace_count < 0) {
		ret = -1;
		goto error;
	}

	for (trace_idx = 0; trace_idx < trace_count; trace_idx++) {
		int64_t begin, end;
		uint64_t stream_idx;
		int64_t stream_count;

		trace_info = bt_value_array_get(query_result, trace_idx);
		if (!trace_info || !bt_value_is_map(trace_info)) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve trace from query result.");
			goto error;
		}

		intersection_range = bt_value_map_get(trace_info,
			"intersection-range-ns");
		if (!intersection_range) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve \'intersetion-range-ns\' field from query result.");
			goto error;
		}

		intersection_begin = bt_value_map_get(intersection_range,
			"begin");
		if (!intersection_begin) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve intersection-range-ns \'begin\' field from query result.");
			goto error;
		}

		intersection_end = bt_value_map_get(intersection_range,
			"end");
		if (!intersection_end) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve intersection-range-ns \'end\' field from query result.");
			goto error;
		}

		value_status = bt_value_integer_get(intersection_begin, &begin);
		if (value_status != BT_VALUE_STATUS_OK) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve value of intersection-range-ns \'begin\' field from query result.");
			goto error;
		}

		value_status = bt_value_integer_get(intersection_end, &end);
		if (value_status != BT_VALUE_STATUS_OK) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve value of intersection-range-ns \'end\' field from query result.");
			goto error;
		}

		if (begin < 0 || end < 0 || end < begin) {
			BT_LOGW("Invalid trace stream intersection values: "
				"intersection-range-ns:begin=%" PRId64
				", intersection-range-ns:end=%" PRId64,
				begin, end);
			ret = -1;
			goto error;
		}

		stream_infos = bt_value_map_get(trace_info, "streams");
		if (!stream_infos || !bt_value_is_array(stream_infos)) {
			ret = -1;
			BT_LOGD_STR("Cannot retrieve stream information from trace in query result.");
			goto error;
		}

		stream_count = bt_value_array_size(stream_infos);
		if (stream_count < 0) {
			ret = -1;
			goto error;
		}

		/*
		 * FIXME
		 *
		 * The first path of a stream's "paths" is currently used to
		 * associate streams/ports to a given trace intersection.
		 *
		 * This is a fragile hack as it relies on the port names
		 * being set to the various streams path.
		 *
		 * A stream name should be introduced as part of the trace-info
		 * query result.
		 */
		for (stream_idx = 0; stream_idx < stream_count; stream_idx++) {
			const char *stream_path;

			port_id = g_new0(struct port_id, 1);
			if (!port_id) {
				ret = -1;
				BT_LOGE_STR("Cannot allocate memory for port_id structure.");
				goto error;
			}
			port_id->instance_name = strdup(cfg_comp->instance_name->str);
			if (!port_id->instance_name) {
				ret = -1;
				BT_LOGE_STR("Cannot allocate memory for port_id component instance name.");
				goto error;
			}

			trace_range = g_new0(struct trace_range, 1);
			if (!trace_range) {
				ret = -1;
				BT_LOGE_STR("Cannot allocate memory for trace_range structure.");
				goto error;
			}
			trace_range->intersection_range_begin_ns = begin;
			trace_range->intersection_range_end_ns = end;

			stream_info = bt_value_array_get(stream_infos,
				stream_idx);
			if (!stream_info || !bt_value_is_map(stream_info)) {
				ret = -1;
				BT_LOGD_STR("Cannot retrieve stream informations from trace in query result.");
				goto error;
			}

			stream_paths = bt_value_map_get(stream_info, "paths");
			if (!stream_paths || !bt_value_is_array(stream_paths)) {
				ret = -1;
				BT_LOGD_STR("Cannot retrieve stream paths from trace in query result.");
				goto error;
			}

			stream_path_value = bt_value_array_get(stream_paths, 0);
			if (!stream_path_value ||
				!bt_value_is_string(stream_path_value)) {
				ret = -1;
				BT_LOGD_STR("Cannot retrieve stream path value from trace in query result.");
				goto error;
			}

			value_status = bt_value_string_get(stream_path_value,
				&stream_path);
			if (value_status != BT_VALUE_STATUS_OK) {
				ret = -1;
				goto error;
			}

			port_id->port_name = strdup(stream_path);
			if (!port_id->port_name) {
				ret = -1;
				BT_LOGE_STR("Cannot allocate memory for port_id port_name.");
				goto error;
			}

			BT_LOGD("Inserting stream intersection ");

			g_hash_table_insert(ctx->intersections, port_id, trace_range);

			port_id = NULL;
			trace_range = NULL;
			BT_PUT(stream_info);
			BT_PUT(stream_paths);
			BT_PUT(stream_path_value);
		}

		BT_PUT(trace_info);
		BT_PUT(stream_paths);
		BT_PUT(stream_path_value);
		BT_PUT(intersection_range);
		BT_PUT(intersection_begin);
		BT_PUT(intersection_end);
		BT_PUT(stream_paths);
		BT_PUT(stream_path_value);
	}

	goto end;

error:
	fprintf(stderr, "%s%sCannot determine stream intersection of trace at path \'%s\'.%s\n",
		bt_common_color_bold(),
		bt_common_color_fg_yellow(),
		path ? path : "(unknown)",
		bt_common_color_reset());
end:
	bt_put(component_path_value);
	bt_put(query_params);
	bt_put(query_result);
	bt_put(trace_info);
	bt_put(intersection_range);
	bt_put(intersection_begin);
	bt_put(intersection_end);
	bt_put(stream_infos);
	bt_put(stream_info);
	bt_put(stream_paths);
	bt_put(stream_path_value);
	g_free(port_id);
	g_free(trace_range);
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

		ret = bt_graph_add_component(ctx->graph, comp_cls,
			cfg_comp->instance_name->str, cfg_comp->params, &comp);
		if (ret) {
			BT_LOGE("Cannot create component: plugin-name=\"%s\", "
				"comp-cls-name=\"%s\", comp-cls-type=%d, "
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

		if (ctx->stream_intersection_mode &&
				cfg_comp->type == BT_COMPONENT_CLASS_TYPE_SOURCE) {
			ret = set_stream_intersections(ctx, cfg_comp, comp_cls);
			if (ret) {
				goto error;
			}
		}

		BT_LOGI("Created and inserted component: comp-addr=%p, comp-name=\"%s\"",
			comp, cfg_comp->instance_name->str);
		quark = g_quark_from_string(cfg_comp->instance_name->str);
		BT_ASSERT(quark > 0);
		g_hash_table_insert(ctx->components,
			GUINT_TO_POINTER(quark), comp);
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
	BT_ASSERT(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_port *upstream_port = port_by_index_fn(comp, i);

		BT_ASSERT(upstream_port);
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
		int64_t (*port_count_fn)(struct bt_component *);
		struct bt_port *(*port_by_index_fn)(struct bt_component *, uint64_t);

		if (bt_component_is_source(g_comp)) {
			port_count_fn =
				bt_component_source_get_output_port_count;
			port_by_index_fn =
				bt_component_source_get_output_port_by_index;
		} else if (bt_component_is_filter(g_comp)) {
			port_count_fn =
				bt_component_filter_get_output_port_count;
			port_by_index_fn =
				bt_component_filter_get_output_port_by_index;
		} else {
			continue;
		}

		ret = cmd_run_ctx_connect_comp_ports(ctx,
			g_comp, port_count_fn, port_by_index_fn);
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

	if (canceled) {
		BT_LOGI_STR("Canceled by user before creating components.");
		goto error;
	}

	BT_LOGI_STR("Creating components.");

	/* Create the requested component instances */
	if (cmd_run_ctx_create_components(&ctx)) {
		BT_LOGE_STR("Cannot create components.");
		fprintf(stderr, "Cannot create components\n");
		goto error;
	}

	if (canceled) {
		BT_LOGI_STR("Canceled by user before connecting components.");
		goto error;
	}

	BT_LOGI_STR("Connecting components.");

	/* Connect the initially visible component ports */
	if (cmd_run_ctx_connect_ports(&ctx)) {
		BT_LOGE_STR("Cannot connect initial component ports.");
		fprintf(stderr, "Cannot connect initial component ports\n");
		goto error;
	}

	if (canceled) {
		BT_LOGI_STR("Canceled by user before running the graph.");
		goto error;
	}

	BT_LOGI_STR("Running the graph.");

	/* Run the graph */
	while (true) {
		enum bt_graph_status graph_status = bt_graph_run(ctx.graph);

		/*
		 * Reset console in case something messed with console
		 * codes during the graph's execution.
		 */
		printf("%s", bt_common_color_reset());
		fflush(stdout);
		fprintf(stderr, "%s", bt_common_color_reset());
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
	bt_cli_log_level = bt_log_get_level_from_env(ENV_BABELTRACE_CLI_LOG_LEVEL);
}

static
void set_auto_log_levels(struct bt_config *cfg)
{
	const char **env_var_name;

	/*
	 * Override the configuration's default log level if
	 * BABELTRACE_VERBOSE or BABELTRACE_DEBUG environment variables
	 * are found for backward compatibility with legacy Babetrace 1.
	 */
	if (getenv("BABELTRACE_DEBUG") &&
			strcmp(getenv("BABELTRACE_DEBUG"), "1") == 0) {
		cfg->log_level = 'V';
	} else if (getenv("BABELTRACE_VERBOSE") &&
			strcmp(getenv("BABELTRACE_VERBOSE"), "1") == 0) {
		cfg->log_level = 'I';
	}

	/*
	 * Set log levels according to --debug or --verbose. For
	 * backward compatibility, --debug is more verbose than
	 * --verbose. So:
	 *
	 *     --verbose: INFO log level
	 *     --debug:   VERBOSE log level (includes DEBUG, which is
	 *                is less verbose than VERBOSE in the internal
	 *                logging framework)
	 */
	if (!getenv("BABELTRACE_LOGGING_GLOBAL_LEVEL")) {
		if (cfg->verbose) {
			bt_logging_set_global_level(BT_LOGGING_LEVEL_INFO);
		} else if (cfg->debug) {
			bt_logging_set_global_level(BT_LOGGING_LEVEL_VERBOSE);
		} else {
			/*
			 * Set library's default log level if not
			 * explicitly specified.
			 */
			switch (cfg->log_level) {
			case 'N':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_NONE);
				break;
			case 'V':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_VERBOSE);
				break;
			case 'D':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_DEBUG);
				break;
			case 'I':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_INFO);
				break;
			case 'W':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_WARN);
				break;
			case 'E':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_ERROR);
				break;
			case 'F':
				bt_logging_set_global_level(BT_LOGGING_LEVEL_FATAL);
				break;
			default:
				abort();
			}
		}
	}

	if (!getenv(ENV_BABELTRACE_CLI_LOG_LEVEL)) {
		if (cfg->verbose) {
			bt_cli_log_level = BT_LOG_INFO;
		} else if (cfg->debug) {
			bt_cli_log_level = BT_LOG_VERBOSE;
		} else {
			/*
			 * Set CLI's default log level if not explicitly
			 * specified.
			 */
			switch (cfg->log_level) {
			case 'N':
				bt_cli_log_level = BT_LOG_NONE;
				break;
			case 'V':
				bt_cli_log_level = BT_LOG_VERBOSE;
				break;
			case 'D':
				bt_cli_log_level = BT_LOG_DEBUG;
				break;
			case 'I':
				bt_cli_log_level = BT_LOG_INFO;
				break;
			case 'W':
				bt_cli_log_level = BT_LOG_WARN;
				break;
			case 'E':
				bt_cli_log_level = BT_LOG_ERROR;
				break;
			case 'F':
				bt_cli_log_level = BT_LOG_FATAL;
				break;
			default:
				abort();
			}
		}
	}

	env_var_name = log_level_env_var_names;

	while (*env_var_name) {
		if (!getenv(*env_var_name)) {
			if (cfg->verbose) {
				g_setenv(*env_var_name, "I", 1);
			} else if (cfg->debug) {
				g_setenv(*env_var_name, "V", 1);
			} else {
				char val[2] = { 0 };

				/*
				 * Set module's default log level if not
				 * explicitly specified.
				 */
				val[0] = cfg->log_level;
				g_setenv(*env_var_name, val, 1);
			}
		}

		env_var_name++;
	}
}

int main(int argc, const char **argv)
{
	int ret;
	int retcode;
	struct bt_config *cfg;

	init_log_level();
	set_signal_handler();
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

	set_auto_log_levels(cfg);
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
		BT_LOGF("Invalid/unknown command: cmd=%d", cfg->command);
		abort();
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
