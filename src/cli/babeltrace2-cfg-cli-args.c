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

#define BT_LOG_TAG "CLI/CFG-CLI-ARGS"
#include "logging.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "common/assert.h"
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <babeltrace2/babeltrace.h>
#include "common/common.h"
#include <glib.h>
#include <sys/types.h>
#include "argpar/argpar.h"
#include "babeltrace2-cfg.h"
#include "babeltrace2-cfg-cli-args.h"
#include "babeltrace2-cfg-cli-args-connect.h"
#include "param-parse/param-parse.h"
#include "babeltrace2-log-level.h"
#include "babeltrace2-plugins.h"
#include "babeltrace2-query.h"
#include "autodisc/autodisc.h"
#include "common/version.h"

#define BT_CLI_LOGE_APPEND_CAUSE_OOM() BT_CLI_LOGE_APPEND_CAUSE("Out of memory.")

/*
 * Returns the plugin name, component class name, component class type,
 * and component name from a command-line --component option's argument.
 * arg must have the following format:
 *
 *     [NAME:]TYPE.PLUGIN.CLS
 *
 * where NAME is the optional component name, TYPE is either `source`,
 * `filter`, or `sink`, PLUGIN is the plugin name, and CLS is the
 * component class name.
 *
 * On success, both *plugin and *component are not NULL. *plugin
 * and *comp_cls are owned by the caller. On success, *name can be NULL
 * if no component class name was found, and *comp_cls_type is set.
 */
static
void plugin_comp_cls_names(const char *arg, char **name, char **plugin,
		char **comp_cls, bt_component_class_type *comp_cls_type)
{
	const char *at = arg;
	GString *gs_name = NULL;
	GString *gs_comp_cls_type = NULL;
	GString *gs_plugin = NULL;
	GString *gs_comp_cls = NULL;
	size_t end_pos;

	BT_ASSERT(arg);
	BT_ASSERT(plugin);
	BT_ASSERT(comp_cls);
	BT_ASSERT(comp_cls_type);

	if (!bt_common_string_is_printable(arg)) {
		BT_CLI_LOGE_APPEND_CAUSE("Argument contains a non-printable character.");
		goto error;
	}

	/* Parse the component name */
	gs_name = bt_common_string_until(at, ".:\\", ":", &end_pos);
	if (!gs_name) {
		goto error;
	}

	if (arg[end_pos] == ':') {
		at += end_pos + 1;
	} else {
		/* No name */
		g_string_assign(gs_name, "");
	}

	/* Parse the component class type */
	gs_comp_cls_type = bt_common_string_until(at, ".:\\", ".", &end_pos);
	if (!gs_comp_cls_type || at[end_pos] == '\0') {
		BT_CLI_LOGE_APPEND_CAUSE("Missing component class type (`source`, `filter`, or `sink`).");
		goto error;
	}

	if (strcmp(gs_comp_cls_type->str, "source") == 0 ||
			strcmp(gs_comp_cls_type->str, "src") == 0) {
		*comp_cls_type = BT_COMPONENT_CLASS_TYPE_SOURCE;
	} else if (strcmp(gs_comp_cls_type->str, "filter") == 0 ||
			strcmp(gs_comp_cls_type->str, "flt") == 0) {
		*comp_cls_type = BT_COMPONENT_CLASS_TYPE_FILTER;
	} else if (strcmp(gs_comp_cls_type->str, "sink") == 0) {
		*comp_cls_type = BT_COMPONENT_CLASS_TYPE_SINK;
	} else {
		BT_CLI_LOGE_APPEND_CAUSE("Unknown component class type: `%s`.",
			gs_comp_cls_type->str);
		goto error;
	}

	at += end_pos + 1;

	/* Parse the plugin name */
	gs_plugin = bt_common_string_until(at, ".:\\", ".", &end_pos);
	if (!gs_plugin || gs_plugin->len == 0 || at[end_pos] == '\0') {
		BT_CLI_LOGE_APPEND_CAUSE("Missing plugin or component class name.");
		goto error;
	}

	at += end_pos + 1;

	/* Parse the component class name */
	gs_comp_cls = bt_common_string_until(at, ".:\\", ".", &end_pos);
	if (!gs_comp_cls || gs_comp_cls->len == 0) {
		BT_CLI_LOGE_APPEND_CAUSE("Missing component class name.");
		goto error;
	}

	if (at[end_pos] != '\0') {
		/* Found a non-escaped `.` */
		goto error;
	}

	if (name) {
		if (gs_name->len == 0) {
			*name = NULL;
			g_string_free(gs_name, TRUE);
		} else {
			*name = gs_name->str;
			g_string_free(gs_name, FALSE);
		}
	} else {
		g_string_free(gs_name, TRUE);
	}

	*plugin = gs_plugin->str;
	*comp_cls = gs_comp_cls->str;
	g_string_free(gs_plugin, FALSE);
	g_string_free(gs_comp_cls, FALSE);
	gs_name = NULL;
	gs_plugin = NULL;
	gs_comp_cls = NULL;
	goto end;

error:
	if (name) {
		*name = NULL;
	}

	*plugin = NULL;
	*comp_cls = NULL;

end:
	if (gs_name) {
		g_string_free(gs_name, TRUE);
	}

	if (gs_plugin) {
		g_string_free(gs_plugin, TRUE);
	}

	if (gs_comp_cls) {
		g_string_free(gs_comp_cls, TRUE);
	}

	if (gs_comp_cls_type) {
		g_string_free(gs_comp_cls_type, TRUE);
	}

	return;
}

static
void print_and_indent(const char *str)
{
	const char *ch = &str[0];

	for (; *ch != '\0'; ch++) {
		if (*ch == '\n') {
			if (ch[1] != '\0') {
				printf("\n  ");
			}
		} else {
			printf("%c", *ch);
		}
	}

	printf("\n");
}

/*
 * Prints the Babeltrace version.
 */
static
void print_version(void)
{
	bool has_extra_name = strlen(BT_VERSION_EXTRA_NAME) > 0;
	bool has_extra_description = strlen(BT_VERSION_EXTRA_DESCRIPTION) > 0;
	bool has_extra_patch_names = strlen(BT_VERSION_EXTRA_PATCHES) > 0;
	bool has_extra = has_extra_name || has_extra_description ||
		has_extra_patch_names;

	printf("%sBabeltrace %s%s",
		bt_common_color_bold(),
		VERSION,
		bt_common_color_reset());

	if (strlen(BT_VERSION_NAME) > 0) {
		printf(" \"%s%s%s%s\"",
			bt_common_color_fg_bright_blue(),
			bt_common_color_bold(),
			BT_VERSION_NAME,
			bt_common_color_reset());
	}

	if (strlen(BT_VERSION_GIT) > 0) {
		printf(" [%s%s%s]",
			bt_common_color_fg_yellow(),
			BT_VERSION_GIT,
			bt_common_color_reset());
	}

	printf("\n");

	if (strlen(BT_VERSION_DESCRIPTION) > 0) {
		unsigned int columns;
		GString *descr;

		if (bt_common_get_term_size(&columns, NULL) < 0) {
			/* Width not found: default to 80 */
			columns = 80;
		}

		descr = bt_common_fold(BT_VERSION_DESCRIPTION, columns, 0);
		BT_ASSERT(descr);
		printf("\n%s\n", descr->str);
		g_string_free(descr, TRUE);
	}

	if (has_extra) {
		printf("\n");

		if (has_extra_name) {
			printf("%sExtra name%s: %s\n",
				bt_common_color_fg_cyan(),
				bt_common_color_reset(),
				BT_VERSION_EXTRA_NAME);
		}

		if (has_extra_description) {
			printf("%sExtra description%s:\n  ",
				bt_common_color_fg_cyan(),
				bt_common_color_reset());
			print_and_indent(BT_VERSION_EXTRA_DESCRIPTION);
		}

		if (has_extra_patch_names) {
			printf("%sExtra patch names%s:\n  ",
				bt_common_color_fg_cyan(),
				bt_common_color_reset());
			print_and_indent(BT_VERSION_EXTRA_PATCHES);
		}
	}
}

/*
 * Destroys a component configuration.
 */
static
void bt_config_component_destroy(bt_object *obj)
{
	struct bt_config_component *bt_config_component =
		container_of(obj, struct bt_config_component, base);

	if (!obj) {
		goto end;
	}

	if (bt_config_component->plugin_name) {
		g_string_free(bt_config_component->plugin_name, TRUE);
	}

	if (bt_config_component->comp_cls_name) {
		g_string_free(bt_config_component->comp_cls_name, TRUE);
	}

	if (bt_config_component->instance_name) {
		g_string_free(bt_config_component->instance_name, TRUE);
	}

	BT_VALUE_PUT_REF_AND_RESET(bt_config_component->params);
	g_free(bt_config_component);

end:
	return;
}

/*
 * Creates a component configuration using the given plugin name and
 * component name. `plugin_name` and `comp_cls_name` are copied (belong
 * to the return value).
 *
 * Return value is owned by the caller.
 */
static
struct bt_config_component *bt_config_component_create(
		bt_component_class_type type,
		const char *plugin_name, const char *comp_cls_name,
		int init_log_level)
{
	struct bt_config_component *cfg_component = NULL;

	cfg_component = g_new0(struct bt_config_component, 1);
	if (!cfg_component) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	bt_object_init_shared(&cfg_component->base,
		bt_config_component_destroy);
	cfg_component->type = type;
	cfg_component->plugin_name = g_string_new(plugin_name);
	if (!cfg_component->plugin_name) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg_component->comp_cls_name = g_string_new(comp_cls_name);
	if (!cfg_component->comp_cls_name) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg_component->instance_name = g_string_new(NULL);
	if (!cfg_component->instance_name) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg_component->log_level = init_log_level;

	/* Start with empty parameters */
	cfg_component->params = bt_value_map_create();
	if (!cfg_component->params) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg_component);

end:
	return cfg_component;
}

/*
 * Creates a component configuration from a command-line --component
 * option's argument.
 */
static
struct bt_config_component *bt_config_component_from_arg(const char *arg,
		int init_log_level)
{
	struct bt_config_component *cfg_comp = NULL;
	char *name = NULL;
	char *plugin_name = NULL;
	char *comp_cls_name = NULL;
	bt_component_class_type type;

	plugin_comp_cls_names(arg, &name, &plugin_name, &comp_cls_name, &type);
	if (!plugin_name || !comp_cls_name) {
		goto error;
	}

	cfg_comp = bt_config_component_create(type, plugin_name, comp_cls_name,
		init_log_level);
	if (!cfg_comp) {
		goto error;
	}

	if (name) {
		g_string_assign(cfg_comp->instance_name, name);
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg_comp);

end:
	g_free(name);
	g_free(plugin_name);
	g_free(comp_cls_name);
	return cfg_comp;
}

/*
 * Destroys a configuration.
 */
static
void bt_config_destroy(bt_object *obj)
{
	struct bt_config *cfg =
		container_of(obj, struct bt_config, base);

	if (!obj) {
		goto end;
	}

	BT_VALUE_PUT_REF_AND_RESET(cfg->plugin_paths);

	switch (cfg->command) {
	case BT_CONFIG_COMMAND_RUN:
		if (cfg->cmd_data.run.sources) {
			g_ptr_array_free(cfg->cmd_data.run.sources, TRUE);
		}

		if (cfg->cmd_data.run.filters) {
			g_ptr_array_free(cfg->cmd_data.run.filters, TRUE);
		}

		if (cfg->cmd_data.run.sinks) {
			g_ptr_array_free(cfg->cmd_data.run.sinks, TRUE);
		}

		if (cfg->cmd_data.run.connections) {
			g_ptr_array_free(cfg->cmd_data.run.connections,
				TRUE);
		}
		break;
	case BT_CONFIG_COMMAND_LIST_PLUGINS:
		break;
	case BT_CONFIG_COMMAND_HELP:
		BT_OBJECT_PUT_REF_AND_RESET(cfg->cmd_data.help.cfg_component);
		break;
	case BT_CONFIG_COMMAND_QUERY:
		BT_OBJECT_PUT_REF_AND_RESET(cfg->cmd_data.query.cfg_component);

		if (cfg->cmd_data.query.object) {
			g_string_free(cfg->cmd_data.query.object, TRUE);
		}
		break;
	case BT_CONFIG_COMMAND_PRINT_CTF_METADATA:
		if (cfg->cmd_data.print_ctf_metadata.path) {
			g_string_free(cfg->cmd_data.print_ctf_metadata.path,
				TRUE);
			g_string_free(
				cfg->cmd_data.print_ctf_metadata.output_path,
				TRUE);
		}
		break;
	case BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS:
		if (cfg->cmd_data.print_lttng_live_sessions.url) {
			g_string_free(
				cfg->cmd_data.print_lttng_live_sessions.url,
				TRUE);
			g_string_free(
				cfg->cmd_data.print_lttng_live_sessions.output_path,
				TRUE);
		}
		break;
	default:
		bt_common_abort();
	}

	g_free(cfg);

end:
	return;
}

static
void destroy_glist_of_gstring(GList *list)
{
	GList *at;

	if (!list) {
		return;
	}

	for (at = list; at; at = g_list_next(at)) {
		g_string_free(at->data, TRUE);
	}

	g_list_free(list);
}

/*
 * Creates a simple lexical scanner for parsing comma-delimited names
 * and fields.
 *
 * Return value is owned by the caller.
 */
static
GScanner *create_csv_identifiers_scanner(void)
{
	GScanner *scanner;
	GScannerConfig scanner_config = {
		.cset_skip_characters = (gchar *) " \t\n",
		.cset_identifier_first = (gchar *) G_CSET_a_2_z G_CSET_A_2_Z "_",
		.cset_identifier_nth = (gchar *) G_CSET_a_2_z G_CSET_A_2_Z ":_-",
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

	scanner = g_scanner_new(&scanner_config);
	if (!scanner) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
	}

	return scanner;
}

/*
 * Converts a comma-delimited list of known names (--names option) to
 * an array value object containing those names as string value objects.
 *
 * Return value is owned by the caller.
 */
static
bt_value *names_from_arg(const char *arg)
{
	GScanner *scanner = NULL;
	bt_value *names = NULL;
	bool found_all = false, found_none = false, found_item = false;

	names = bt_value_array_create();
	if (!names) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	scanner = create_csv_identifiers_scanner();
	if (!scanner) {
		goto error;
	}

	g_scanner_input_text(scanner, arg, strlen(arg));

	while (true) {
		GTokenType token_type = g_scanner_get_next_token(scanner);

		switch (token_type) {
		case G_TOKEN_IDENTIFIER:
		{
			const char *identifier = scanner->value.v_identifier;

			if (strcmp(identifier, "payload") == 0 ||
					strcmp(identifier, "args") == 0 ||
					strcmp(identifier, "arg") == 0) {
				found_item = true;
				if (bt_value_array_append_string_element(names,
						"payload")) {
					goto error;
				}
			} else if (strcmp(identifier, "context") == 0 ||
					strcmp(identifier, "ctx") == 0) {
				found_item = true;
				if (bt_value_array_append_string_element(names,
						"context")) {
					goto error;
				}
			} else if (strcmp(identifier, "scope") == 0 ||
					strcmp(identifier, "header") == 0) {
				found_item = true;
				if (bt_value_array_append_string_element(names,
						identifier)) {
					goto error;
				}
			} else if (strcmp(identifier, "all") == 0) {
				found_all = true;
				if (bt_value_array_append_string_element(names,
						identifier)) {
					goto error;
				}
			} else if (strcmp(identifier, "none") == 0) {
				found_none = true;
				if (bt_value_array_append_string_element(names,
						identifier)) {
					goto error;
				}
			} else {
				BT_CLI_LOGE_APPEND_CAUSE("Unknown name: `%s`.",
					identifier);
				goto error;
			}
			break;
		}
		case G_TOKEN_COMMA:
			continue;
		case G_TOKEN_EOF:
			goto end;
		default:
			goto error;
		}
	}

end:
	if (found_none && found_all) {
		BT_CLI_LOGE_APPEND_CAUSE("Only either `all` or `none` can be specified in the list given to the --names option, but not both.");
		goto error;
	}
	/*
	 * Legacy behavior is to clear the defaults (show none) when at
	 * least one item is specified.
	 */
	if (found_item && !found_none && !found_all) {
		if (bt_value_array_append_string_element(names, "none")) {
			goto error;
		}
	}
	if (scanner) {
		g_scanner_destroy(scanner);
	}
	return names;

error:
	BT_VALUE_PUT_REF_AND_RESET(names);
	if (scanner) {
		g_scanner_destroy(scanner);
	}
	return names;
}

/*
 * Converts a comma-delimited list of known fields (--fields option) to
 * an array value object containing those fields as string
 * value objects.
 *
 * Return value is owned by the caller.
 */
static
bt_value *fields_from_arg(const char *arg)
{
	GScanner *scanner = NULL;
	bt_value *fields;

	fields = bt_value_array_create();
	if (!fields) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	scanner = create_csv_identifiers_scanner();
	if (!scanner) {
		goto error;
	}

	g_scanner_input_text(scanner, arg, strlen(arg));

	while (true) {
		GTokenType token_type = g_scanner_get_next_token(scanner);

		switch (token_type) {
		case G_TOKEN_IDENTIFIER:
		{
			const char *identifier = scanner->value.v_identifier;

			if (strcmp(identifier, "trace") == 0 ||
					strcmp(identifier, "trace:hostname") == 0 ||
					strcmp(identifier, "trace:domain") == 0 ||
					strcmp(identifier, "trace:procname") == 0 ||
					strcmp(identifier, "trace:vpid") == 0 ||
					strcmp(identifier, "loglevel") == 0 ||
					strcmp(identifier, "emf") == 0 ||
					strcmp(identifier, "callsite") == 0 ||
					strcmp(identifier, "all") == 0) {
				if (bt_value_array_append_string_element(fields,
						identifier)) {
					goto error;
				}
			} else {
				BT_CLI_LOGE_APPEND_CAUSE("Unknown field: `%s`.",
					identifier);
				goto error;
			}
			break;
		}
		case G_TOKEN_COMMA:
			continue;
		case G_TOKEN_EOF:
			goto end;
		default:
			goto error;
		}
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(fields);

end:
	if (scanner) {
		g_scanner_destroy(scanner);
	}
	return fields;
}

static
void append_param_arg(GString *params_arg, const char *key, const char *value)
{
	BT_ASSERT(params_arg);
	BT_ASSERT(key);
	BT_ASSERT(value);

	if (params_arg->len != 0) {
		g_string_append_c(params_arg, ',');
	}

	g_string_append(params_arg, key);
	g_string_append_c(params_arg, '=');
	g_string_append(params_arg, value);
}

/*
 * Inserts the equivalent "prefix-NAME=yes" strings into params_arg
 * where the names are in names_array.
 */
static
int insert_flat_params_from_array(GString *params_arg,
		const bt_value *names_array, const char *prefix)
{
	int ret = 0;
	uint64_t i;
	GString *tmpstr = NULL, *default_value = NULL;
	bool default_set = false, non_default_set = false;

	/*
	 * names_array may be NULL if no CLI options were specified to
	 * trigger its creation.
	 */
	if (!names_array) {
		goto end;
	}

	tmpstr = g_string_new(NULL);
	if (!tmpstr) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	default_value = g_string_new(NULL);
	if (!default_value) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	for (i = 0; i < bt_value_array_get_length(names_array); i++) {
		const bt_value *str_obj =
			bt_value_array_borrow_element_by_index_const(names_array,
				i);
		const char *suffix;
		bool is_default = false;

		suffix = bt_value_string_get(str_obj);

		g_string_assign(tmpstr, prefix);
		g_string_append(tmpstr, "-");

		/* Special-case for "all" and "none". */
		if (strcmp(suffix, "all") == 0) {
			is_default = true;
			g_string_assign(default_value, "show");
		} else if (strcmp(suffix, "none") == 0) {
			is_default = true;
			g_string_assign(default_value, "hide");
		}
		if (is_default) {
			default_set = true;
			g_string_append(tmpstr, "default");
			append_param_arg(params_arg, tmpstr->str,
				default_value->str);
		} else {
			non_default_set = true;
			g_string_append(tmpstr, suffix);
			append_param_arg(params_arg, tmpstr->str, "yes");
		}
	}

	/* Implicit field-default=hide if any non-default option is set. */
	if (non_default_set && !default_set) {
		g_string_assign(tmpstr, prefix);
		g_string_append(tmpstr, "-default");
		g_string_assign(default_value, "hide");
		append_param_arg(params_arg, tmpstr->str, default_value->str);
	}

end:
	if (default_value) {
		g_string_free(default_value, TRUE);
	}

	if (tmpstr) {
		g_string_free(tmpstr, TRUE);
	}

	return ret;
}

/* argpar options */
enum {
	OPT_NONE = 0,
	OPT_BASE_PARAMS,
	OPT_BEGIN,
	OPT_CLOCK_CYCLES,
	OPT_CLOCK_DATE,
	OPT_CLOCK_FORCE_CORRELATE,
	OPT_CLOCK_GMT,
	OPT_CLOCK_OFFSET,
	OPT_CLOCK_OFFSET_NS,
	OPT_CLOCK_SECONDS,
	OPT_COLOR,
	OPT_COMPONENT,
	OPT_CONNECT,
	OPT_DEBUG,
	OPT_DEBUG_INFO,
	OPT_DEBUG_INFO_DIR,
	OPT_DEBUG_INFO_FULL_PATH,
	OPT_DEBUG_INFO_TARGET_PREFIX,
	OPT_END,
	OPT_FIELDS,
	OPT_HELP,
	OPT_INPUT_FORMAT,
	OPT_LIST,
	OPT_LOG_LEVEL,
	OPT_NAMES,
	OPT_NO_DELTA,
	OPT_OMIT_HOME_PLUGIN_PATH,
	OPT_OMIT_SYSTEM_PLUGIN_PATH,
	OPT_OUTPUT,
	OPT_OUTPUT_FORMAT,
	OPT_PARAMS,
	OPT_PLUGIN_PATH,
	OPT_RESET_BASE_PARAMS,
	OPT_RETRY_DURATION,
	OPT_RUN_ARGS,
	OPT_RUN_ARGS_0,
	OPT_STREAM_INTERSECTION,
	OPT_TIMERANGE,
	OPT_VERBOSE,
	OPT_VERSION,
};

enum bt_config_component_dest {
	BT_CONFIG_COMPONENT_DEST_UNKNOWN = -1,
	BT_CONFIG_COMPONENT_DEST_SOURCE,
	BT_CONFIG_COMPONENT_DEST_FILTER,
	BT_CONFIG_COMPONENT_DEST_SINK,
};

/*
 * Adds a configuration component to the appropriate configuration
 * array depending on the destination.
 */
static
void add_run_cfg_comp(struct bt_config *cfg,
		struct bt_config_component *cfg_comp,
		enum bt_config_component_dest dest)
{
	bt_object_get_ref(cfg_comp);

	switch (dest) {
	case BT_CONFIG_COMPONENT_DEST_SOURCE:
		g_ptr_array_add(cfg->cmd_data.run.sources, cfg_comp);
		break;
	case BT_CONFIG_COMPONENT_DEST_FILTER:
		g_ptr_array_add(cfg->cmd_data.run.filters, cfg_comp);
		break;
	case BT_CONFIG_COMPONENT_DEST_SINK:
		g_ptr_array_add(cfg->cmd_data.run.sinks, cfg_comp);
		break;
	default:
		bt_common_abort();
	}
}

static
int add_run_cfg_comp_check_name(struct bt_config *cfg,
		struct bt_config_component *cfg_comp,
		enum bt_config_component_dest dest,
		bt_value *instance_names)
{
	int ret = 0;

	if (cfg_comp->instance_name->len == 0) {
		BT_CLI_LOGE_APPEND_CAUSE("Found an unnamed component.");
		ret = -1;
		goto end;
	}

	if (bt_value_map_has_entry(instance_names,
				   cfg_comp->instance_name->str)) {
		BT_CLI_LOGE_APPEND_CAUSE("Duplicate component instance name:\n    %s",
			cfg_comp->instance_name->str);
		ret = -1;
		goto end;
	}

	if (bt_value_map_insert_entry(instance_names,
			cfg_comp->instance_name->str, bt_value_null)) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	add_run_cfg_comp(cfg, cfg_comp, dest);

end:
	return ret;
}

static
int append_env_var_plugin_paths(bt_value *plugin_paths)
{
	int ret = 0;
	const char *envvar;

	if (bt_common_is_setuid_setgid()) {
		BT_LOGI_STR("Skipping non-system plugin paths for setuid/setgid binary.");
		goto end;
	}

	envvar = getenv("BABELTRACE_PLUGIN_PATH");
	if (!envvar) {
		goto end;
	}

	ret = bt_config_append_plugin_paths(plugin_paths, envvar);

end:
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot append plugin paths from BABELTRACE_PLUGIN_PATH.");
	}

	return ret;
}

static
int append_home_and_system_plugin_paths(bt_value *plugin_paths,
		bool omit_system_plugin_path, bool omit_home_plugin_path)
{
	int ret;

	if (!omit_home_plugin_path) {
		if (bt_common_is_setuid_setgid()) {
			BT_LOGI_STR("Skipping non-system plugin paths for setuid/setgid binary.");
		} else {
			char *home_plugin_dir = bt_common_get_home_plugin_path(
				BT_LOG_OUTPUT_LEVEL);

			if (home_plugin_dir) {
				ret = bt_config_append_plugin_paths(
					plugin_paths, home_plugin_dir);
				free(home_plugin_dir);

				if (ret) {
					BT_CLI_LOGE_APPEND_CAUSE("Invalid home plugin path.");
					goto error;
				}
			}
		}
	}

	if (!omit_system_plugin_path) {
		if (bt_config_append_plugin_paths(plugin_paths,
				bt_common_get_system_plugin_path())) {
			BT_CLI_LOGE_APPEND_CAUSE("Invalid system plugin path.");
			goto error;
		}
	}
	return 0;
error:
	BT_CLI_LOGE_APPEND_CAUSE("Cannot append home and system plugin paths.");
	return -1;
}

static
struct bt_config *bt_config_base_create(enum bt_config_command command,
		const bt_value *plugin_paths, bool needs_plugins)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = g_new0(struct bt_config, 1);
	if (!cfg) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	bt_object_init_shared(&cfg->base, bt_config_destroy);
	cfg->command = command;
	cfg->command_needs_plugins = needs_plugins;

	if (plugin_paths) {
		bt_value *plugin_paths_copy;

		(void) bt_value_copy(plugin_paths,
			&plugin_paths_copy);
		cfg->plugin_paths = plugin_paths_copy;
	} else {
		cfg->plugin_paths = bt_value_array_create();
		if (!cfg->plugin_paths) {
			BT_CLI_LOGE_APPEND_CAUSE_OOM();
			goto error;
		}
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_run_create(const bt_value *plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_RUN,
		plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.run.sources = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!cfg->cmd_data.run.sources) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg->cmd_data.run.filters = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!cfg->cmd_data.run.filters) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg->cmd_data.run.sinks = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!cfg->cmd_data.run.sinks) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg->cmd_data.run.connections = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_config_connection_destroy);
	if (!cfg->cmd_data.run.connections) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_list_plugins_create(const bt_value *plugin_paths)
{
	return bt_config_base_create(BT_CONFIG_COMMAND_LIST_PLUGINS,
		plugin_paths, true);
}

static
struct bt_config *bt_config_help_create(const bt_value *plugin_paths,
		int default_log_level)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_HELP,
		plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.help.cfg_component =
		bt_config_component_create(-1, NULL, NULL, default_log_level);
	if (!cfg->cmd_data.help.cfg_component) {
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_query_create(const bt_value *plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_QUERY,
		plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.query.object = g_string_new(NULL);
	if (!cfg->cmd_data.query.object) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_print_ctf_metadata_create(
		const bt_value *plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_PRINT_CTF_METADATA,
		plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.print_ctf_metadata.path = g_string_new(NULL);
	if (!cfg->cmd_data.print_ctf_metadata.path) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg->cmd_data.print_ctf_metadata.output_path = g_string_new(NULL);
	if (!cfg->cmd_data.print_ctf_metadata.output_path) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_print_lttng_live_sessions_create(
		const bt_value *plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS,
		plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.print_lttng_live_sessions.url = g_string_new(NULL);
	if (!cfg->cmd_data.print_lttng_live_sessions.url) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	cfg->cmd_data.print_lttng_live_sessions.output_path =
		g_string_new(NULL);
	if (!cfg->cmd_data.print_lttng_live_sessions.output_path) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
int bt_config_append_plugin_paths_check_setuid_setgid(
		bt_value *plugin_paths, const char *arg)
{
	int ret = 0;

	if (bt_common_is_setuid_setgid()) {
		BT_LOGI_STR("Skipping non-system plugin paths for setuid/setgid binary.");
		goto end;
	}

	if (bt_config_append_plugin_paths(plugin_paths, arg)) {
		BT_CLI_LOGE_APPEND_CAUSE("Invalid --plugin-path option's argument:\n    %s",
			arg);
		ret = -1;
		goto end;
	}

end:
	return ret;
}

/*
 * Prints the expected format for a --params option.
 */
static
void print_expected_params_format(FILE *fp)
{
	fprintf(fp, "Expected format of PARAMS\n");
	fprintf(fp, "-------------------------\n");
	fprintf(fp, "\n");
	fprintf(fp, "    PARAM=VALUE[,PARAM=VALUE]...\n");
	fprintf(fp, "\n");
	fprintf(fp, "The parameter string is a comma-separated list of PARAM=VALUE assignments,\n");
	fprintf(fp, "where PARAM is the parameter name (C identifier plus the [:.-] characters),\n");
	fprintf(fp, "and VALUE can be one of:\n");
	fprintf(fp, "\n");
	fprintf(fp, "* `null`, `nul`, `NULL`: null value (no backticks).\n");
	fprintf(fp, "* `true`, `TRUE`, `yes`, `YES`: true boolean value (no backticks).\n");
	fprintf(fp, "* `false`, `FALSE`, `no`, `NO`: false boolean value (no backticks).\n");
	fprintf(fp, "* Binary (`0b` prefix), octal (`0` prefix), decimal, or hexadecimal\n");
	fprintf(fp, "  (`0x` prefix) unsigned (with `+` prefix) or signed 64-bit integer.\n");
	fprintf(fp, "* Double precision floating point number (scientific notation is accepted).\n");
	fprintf(fp, "* Unquoted string with no special characters, and not matching any of\n");
	fprintf(fp, "  the null and boolean value symbols above.\n");
	fprintf(fp, "* Double-quoted string (accepts escape characters).\n");
	fprintf(fp, "* Array, formatted as an opening `[`, a list of comma-separated values\n");
	fprintf(fp, "  (as described by the current list) and a closing `]`.\n");
	fprintf(fp, "* Map, formatted as an opening `{`, a comma-separated list of PARAM=VALUE\n");
	fprintf(fp, "  assignments and a closing `}`.\n");
	fprintf(fp, "\n");
	fprintf(fp, "You can put whitespaces allowed around individual `=` and `,` symbols.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Example:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    many=null, fresh=yes, condition=false, squirrel=-782329,\n");
	fprintf(fp, "    play=+23, observe=3.14, simple=beef, needs-quotes=\"some string\",\n");
	fprintf(fp, "    escape.chars-are:allowed=\"this is a \\\" double quote\",\n");
	fprintf(fp, "    things=[1, \"2\", 3]\n");
	fprintf(fp, "\n");
	fprintf(fp, "IMPORTANT: Make sure to single-quote the whole argument when you run\n");
	fprintf(fp, "babeltrace2 from a shell.\n");
}

static
bool help_option_is_specified(
		const struct argpar_parse_ret *argpar_parse_ret)
{
	int i;
	bool specified = false;

	for (i = 0; i < argpar_parse_ret->items->n_items; i++) {
		struct argpar_item *argpar_item =
			argpar_parse_ret->items->items[i];
		struct argpar_item_opt *argpar_item_opt;

		if (argpar_item->type != ARGPAR_ITEM_TYPE_OPT) {
			continue;
		}

		argpar_item_opt = (struct argpar_item_opt *) argpar_item;
		if (argpar_item_opt->descr->id == OPT_HELP) {
			specified = true;
			break;
		}
	}

	return specified;
}

/*
 * Prints the help command usage.
 */
static
void print_help_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace2 [GENERAL OPTIONS] help [OPTIONS] PLUGIN\n");
	fprintf(fp, "       babeltrace2 [GENERAL OPTIONS] help [OPTIONS] TYPE.PLUGIN.CLS\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -h, --help  Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace2 --help` for the list of general options.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Use `babeltrace2 list-plugins` to show the list of available plugins.\n");
}

static
const struct argpar_opt_descr help_options[] = {
	/* id, short_name, long_name, with_arg */
	{ OPT_HELP, 'h', "help", false },
	ARGPAR_OPT_DESCR_SENTINEL
};

/*
 * Creates a Babeltrace config object from the arguments of a help
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_help_from_args(int argc, const char *argv[],
		int *retcode, const bt_value *plugin_paths,
		int default_log_level)
{
	struct bt_config *cfg = NULL;
	char *plugin_name = NULL, *comp_cls_name = NULL;
	struct argpar_parse_ret argpar_parse_ret = { 0 };
	struct argpar_item_non_opt *non_opt;
	GString *substring = NULL;
	size_t end_pos;

	*retcode = 0;
	cfg = bt_config_help_create(plugin_paths, default_log_level);
	if (!cfg) {
		goto error;
	}

	/* Parse options */
	argpar_parse_ret = argpar_parse(argc, argv, help_options, true);
	if (argpar_parse_ret.error) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"While parsing `help` command's command-line arguments: %s",
			argpar_parse_ret.error);
		goto error;
	}

	if (help_option_is_specified(&argpar_parse_ret)) {
		print_help_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	if (argpar_parse_ret.items->n_items == 0) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Missing plugin name or component class descriptor.");
		goto error;
	} else if (argpar_parse_ret.items->n_items > 1) {
		/*
		 * At this point we know there are least two non-option
		 * arguments because we don't reach here with `--help`,
		 * the only option.
		 */
		non_opt = (struct argpar_item_non_opt *) argpar_parse_ret.items->items[1];
		BT_CLI_LOGE_APPEND_CAUSE(
			"Extraneous command-line argument specified to `help` command: `%s`.",
			non_opt->arg);
		goto error;
	}

	non_opt = (struct argpar_item_non_opt *) argpar_parse_ret.items->items[0];

	/* Look for unescaped dots in the argument. */
	substring = bt_common_string_until(non_opt->arg, ".\\", ".", &end_pos);
	if (!substring) {
		BT_CLI_LOGE_APPEND_CAUSE("Could not consume argument: arg=%s",
			non_opt->arg);
		goto error;
	}

	if (end_pos == strlen(non_opt->arg)) {
		/* Didn't find an unescaped dot, treat it as a plugin name. */
		g_string_assign(cfg->cmd_data.help.cfg_component->plugin_name,
			non_opt->arg);
	} else {
		/*
		 * Found an unescaped dot, treat it as a component class name.
		 */
		plugin_comp_cls_names(non_opt->arg, NULL, &plugin_name, &comp_cls_name,
			&cfg->cmd_data.help.cfg_component->type);
		if (!plugin_name || !comp_cls_name) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Could not parse argument as a component class name: arg=%s",
				non_opt->arg);
			goto error;
		}

		g_string_assign(cfg->cmd_data.help.cfg_component->plugin_name,
			plugin_name);
		g_string_assign(cfg->cmd_data.help.cfg_component->comp_cls_name,
			comp_cls_name);
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	g_free(plugin_name);
	g_free(comp_cls_name);

	if (substring) {
		g_string_free(substring, TRUE);
	}

	argpar_parse_ret_fini(&argpar_parse_ret);

	return cfg;
}

/*
 * Prints the help command usage.
 */
static
void print_query_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace2 [GEN OPTS] query [OPTS] TYPE.PLUGIN.CLS OBJECT\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -p, --params=PARAMS  Set the query parameters to PARAMS (see the expected\n");
	fprintf(fp, "                       format of PARAMS below)\n");
	fprintf(fp, "  -h, --help           Show this help and quit\n");
	fprintf(fp, "\n\n");
	print_expected_params_format(fp);
}

static
const struct argpar_opt_descr query_options[] = {
	/* id, short_name, long_name, with_arg */
	{ OPT_HELP, 'h', "help", false },
	{ OPT_PARAMS, 'p', "params", true },
	ARGPAR_OPT_DESCR_SENTINEL
};

/*
 * Creates a Babeltrace config object from the arguments of a query
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_query_from_args(int argc, const char *argv[],
		int *retcode, const bt_value *plugin_paths,
		int default_log_level)
{
	int i;
	struct bt_config *cfg = NULL;
	const char *component_class_spec = NULL;
	const char *query_object = NULL;
	GString *error_str = NULL;
	struct argpar_parse_ret argpar_parse_ret = { 0 };

	bt_value *params = bt_value_map_create();
	if (!params) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	*retcode = 0;
	cfg = bt_config_query_create(plugin_paths);
	if (!cfg) {
		goto error;
	}

	error_str = g_string_new(NULL);
	if (!error_str) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	/* Parse options */
	argpar_parse_ret = argpar_parse(argc, argv, query_options, true);
	if (argpar_parse_ret.error) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"While parsing `query` command's command-line arguments: %s",
			argpar_parse_ret.error);
		goto error;
	}

	if (help_option_is_specified(&argpar_parse_ret)) {
		print_query_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	for (i = 0; i < argpar_parse_ret.items->n_items; i++) {
		struct argpar_item *argpar_item =
			argpar_parse_ret.items->items[i];

		if (argpar_item->type == ARGPAR_ITEM_TYPE_OPT) {
			struct argpar_item_opt *argpar_item_opt =
				(struct argpar_item_opt *) argpar_item;
			const char *arg = argpar_item_opt->arg;

			switch (argpar_item_opt->descr->id) {
			case OPT_PARAMS:
			{
				bt_value *parsed_params = bt_param_parse(arg, error_str);
				bt_value_map_extend_status extend_status;
				if (!parsed_params) {
					BT_CLI_LOGE_APPEND_CAUSE("Invalid format for --params option's argument:\n    %s",
						error_str->str);
					goto error;
				}

				extend_status = bt_value_map_extend(params, parsed_params);
				BT_VALUE_PUT_REF_AND_RESET(parsed_params);
				if (extend_status) {
					BT_CLI_LOGE_APPEND_CAUSE("Cannot extend current parameters with --params option's argument:\n    %s",
						arg);
					goto error;
				}
				break;
			}
			default:
				BT_CLI_LOGE_APPEND_CAUSE("Unknown command-line option specified (option code %d).",
					argpar_item_opt->descr->id);
				goto error;
			}
		} else {
			struct argpar_item_non_opt *argpar_item_non_opt
				= (struct argpar_item_non_opt *) argpar_item;

			/*
			 * We need exactly two non-option arguments
			 * which are the mandatory component class
			 * specification and query object.
			 */
			if (!component_class_spec) {
				component_class_spec = argpar_item_non_opt->arg;
			} else if (!query_object) {
				query_object = argpar_item_non_opt->arg;
			} else {
				BT_CLI_LOGE_APPEND_CAUSE("Extraneous command-line argument specified to `query` command: `%s`.",
					argpar_item_non_opt->arg);
				goto error;
			}
		}
	}

	if (!component_class_spec || !query_object) {
		print_query_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	cfg->cmd_data.query.cfg_component =
		bt_config_component_from_arg(component_class_spec,
			default_log_level);
	if (!cfg->cmd_data.query.cfg_component) {
		BT_CLI_LOGE_APPEND_CAUSE("Invalid format for component class specification:\n    %s",
			component_class_spec);
		goto error;
	}

	BT_ASSERT(params);
	BT_OBJECT_MOVE_REF(cfg->cmd_data.query.cfg_component->params, params);

	if (strlen(query_object) == 0) {
		BT_CLI_LOGE_APPEND_CAUSE("Invalid empty object.");
		goto error;
	}

	g_string_assign(cfg->cmd_data.query.object, query_object);
	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	argpar_parse_ret_fini(&argpar_parse_ret);

	if (error_str) {
		g_string_free(error_str, TRUE);
	}

	bt_value_put_ref(params);
	return cfg;
}

/*
 * Prints the list-plugins command usage.
 */
static
void print_list_plugins_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace2 [GENERAL OPTIONS] list-plugins [OPTIONS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace2 --help` for the list of general options.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Use `babeltrace2 help` to get help for a specific plugin or component class.\n");
}

static
const struct argpar_opt_descr list_plugins_options[] = {
	/* id, short_name, long_name, with_arg */
	{ OPT_HELP, 'h', "help", false },
	ARGPAR_OPT_DESCR_SENTINEL
};

/*
 * Creates a Babeltrace config object from the arguments of a
 * list-plugins command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_list_plugins_from_args(int argc, const char *argv[],
		int *retcode, const bt_value *plugin_paths)
{
	struct bt_config *cfg = NULL;
	struct argpar_parse_ret argpar_parse_ret = { 0 };

	*retcode = 0;
	cfg = bt_config_list_plugins_create(plugin_paths);
	if (!cfg) {
		goto error;
	}

	/* Parse options */
	argpar_parse_ret = argpar_parse(argc, argv, list_plugins_options, true);
	if (argpar_parse_ret.error) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"While parsing `list-plugins` command's command-line arguments: %s",
			argpar_parse_ret.error);
		goto error;
	}

	if (help_option_is_specified(&argpar_parse_ret)) {
		print_list_plugins_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	if (argpar_parse_ret.items->n_items > 0) {
		/*
		 * At this point we know there's at least one non-option
		 * argument because we don't reach here with `--help`,
		 * the only option.
		 */
		struct argpar_item_non_opt *non_opt =
			(struct argpar_item_non_opt *) argpar_parse_ret.items->items[0];

		BT_CLI_LOGE_APPEND_CAUSE(
			"Extraneous command-line argument specified to `list-plugins` command: `%s`.",
			non_opt->arg);
		goto error;
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	argpar_parse_ret_fini(&argpar_parse_ret);

	return cfg;
}

/*
 * Prints the run command usage.
 */
static
void print_run_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace2 [GENERAL OPTIONS] run [OPTIONS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -b, --base-params=PARAMS          Set PARAMS as the current base parameters\n");
	fprintf(fp, "                                    for all the following components until\n");
	fprintf(fp, "                                    --reset-base-params is encountered\n");
	fprintf(fp, "                                    (see the expected format of PARAMS below)\n");
	fprintf(fp, "  -c, --component=NAME:TYPE.PLUGIN.CLS\n");
	fprintf(fp, "                                    Instantiate the component class CLS of type\n");
	fprintf(fp, "                                    TYPE (`source`, `filter`, or `sink`) found\n");
	fprintf(fp, "                                    in the plugin PLUGIN, add it to the graph,\n");
	fprintf(fp, "                                    and name it NAME");
	fprintf(fp, "  -x, --connect=CONNECTION          Connect two created components (see the\n");
	fprintf(fp, "                                    expected format of CONNECTION below)\n");
	fprintf(fp, "  -l, --log-level=LVL               Set the log level of the current component to LVL\n");
	fprintf(fp, "                                    (`N`, `T`, `D`, `I`, `W`, `E`, or `F`)\n");
	fprintf(fp, "  -p, --params=PARAMS               Add initialization parameters PARAMS to the\n");
	fprintf(fp, "                                    current component (see the expected format\n");
	fprintf(fp, "                                    of PARAMS below)\n");
	fprintf(fp, "  -r, --reset-base-params           Reset the current base parameters to an\n");
	fprintf(fp, "                                    empty map\n");
	fprintf(fp, "      --retry-duration=DUR          When babeltrace2(1) needs to retry to run\n");
	fprintf(fp, "                                    the graph later, retry in DUR µs\n");
	fprintf(fp, "                                    (default: 100000)\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace2 --help` for the list of general options.\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Expected format of CONNECTION\n");
	fprintf(fp, "-----------------------------\n");
	fprintf(fp, "\n");
	fprintf(fp, "    UPSTREAM[.UPSTREAM-PORT]:DOWNSTREAM[.DOWNSTREAM-PORT]\n");
	fprintf(fp, "\n");
	fprintf(fp, "UPSTREAM and DOWNSTREAM are names of the upstream and downstream\n");
	fprintf(fp, "components to connect together. You must escape the following characters\n\n");
	fprintf(fp, "with `\\`: `\\`, `.`, and `:`. You must set the name of the current\n");
	fprintf(fp, "component using the NAME prefix of the --component option.\n");
	fprintf(fp, "\n");
	fprintf(fp, "UPSTREAM-PORT and DOWNSTREAM-PORT are optional globbing patterns to\n");
	fprintf(fp, "identify the upstream and downstream ports to use for the connection.\n");
	fprintf(fp, "When the port is not specified, `*` is used.\n");
	fprintf(fp, "\n");
	fprintf(fp, "When a component named UPSTREAM has an available port which matches the\n");
	fprintf(fp, "UPSTREAM-PORT globbing pattern, it is connected to the first port which\n");
	fprintf(fp, "matches the DOWNSTREAM-PORT globbing pattern of the component named\n");
	fprintf(fp, "DOWNSTREAM.\n");
	fprintf(fp, "\n");
	fprintf(fp, "The only special character in UPSTREAM-PORT and DOWNSTREAM-PORT is `*`\n");
	fprintf(fp, "which matches anything. You must escape the following characters\n");
	fprintf(fp, "with `\\`: `\\`, `*`, `?`, `[`, `.`, and `:`.\n");
	fprintf(fp, "\n");
	fprintf(fp, "You can connect a source component to a filter or sink component. You\n");
	fprintf(fp, "can connect a filter component to a sink component.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Examples:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    my-src:my-sink\n");
	fprintf(fp, "    ctf-fs.*stream*:utils-muxer:*\n");
	fprintf(fp, "\n");
	fprintf(fp, "IMPORTANT: Make sure to single-quote the whole argument when you run\n");
	fprintf(fp, "babeltrace2 from a shell.\n");
	fprintf(fp, "\n\n");
	print_expected_params_format(fp);
}

/*
 * Creates a Babeltrace config object from the arguments of a run
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_run_from_args(int argc, const char *argv[],
		int *retcode, const bt_value *plugin_paths,
		int default_log_level)
{
	struct bt_config_component *cur_cfg_comp = NULL;
	bt_value *cur_base_params = NULL;
	int ret = 0;
	struct bt_config *cfg = NULL;
	bt_value *instance_names = NULL;
	bt_value *connection_args = NULL;
	char error_buf[256] = { 0 };
	long retry_duration = -1;
	bt_value_map_extend_status extend_status;
	GString *error_str = NULL;
	struct argpar_parse_ret argpar_parse_ret = { 0 };
	int i;

	static const struct argpar_opt_descr run_options[] = {
		{ OPT_BASE_PARAMS, 'b', "base-params", true },
		{ OPT_COMPONENT, 'c', "component", true },
		{ OPT_CONNECT, 'x', "connect", true },
		{ OPT_HELP, 'h', "help", false },
		{ OPT_LOG_LEVEL, 'l', "log-level", true },
		{ OPT_PARAMS, 'p', "params", true },
		{ OPT_RESET_BASE_PARAMS, 'r', "reset-base-params", false },
		{ OPT_RETRY_DURATION, '\0', "retry-duration", true },
		ARGPAR_OPT_DESCR_SENTINEL
	};

	*retcode = 0;

	error_str = g_string_new(NULL);
	if (!error_str) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	if (argc < 1) {
		print_run_usage(stdout);
		*retcode = -1;
		goto end;
	}

	cfg = bt_config_run_create(plugin_paths);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.run.retry_duration_us = 100000;
	cur_base_params = bt_value_map_create();
	if (!cur_base_params) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	instance_names = bt_value_map_create();
	if (!instance_names) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	connection_args = bt_value_array_create();
	if (!connection_args) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	/* Parse options */
	argpar_parse_ret = argpar_parse(argc, argv, run_options, true);
	if (argpar_parse_ret.error) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"While parsing `run` command's command-line arguments: %s",
			argpar_parse_ret.error);
		goto error;
	}

	if (help_option_is_specified(&argpar_parse_ret)) {
		print_run_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	for (i = 0; i < argpar_parse_ret.items->n_items; i++) {
		struct argpar_item *argpar_item =
			argpar_parse_ret.items->items[i];
		struct argpar_item_opt *argpar_item_opt;
		const char *arg;

		/* This command does not accept non-option arguments.*/
		if (argpar_item->type == ARGPAR_ITEM_TYPE_NON_OPT) {
			struct argpar_item_non_opt *argpar_nonopt_item =
				(struct argpar_item_non_opt *) argpar_item;

			BT_CLI_LOGE_APPEND_CAUSE("Unexpected argument: `%s`",
				argpar_nonopt_item->arg);
			goto error;
		}

		argpar_item_opt = (struct argpar_item_opt *) argpar_item;
		arg = argpar_item_opt->arg;

		switch (argpar_item_opt->descr->id) {
		case OPT_COMPONENT:
		{
			enum bt_config_component_dest dest;

			BT_OBJECT_PUT_REF_AND_RESET(cur_cfg_comp);
			cur_cfg_comp = bt_config_component_from_arg(arg,
				default_log_level);
			if (!cur_cfg_comp) {
				BT_CLI_LOGE_APPEND_CAUSE("Invalid format for --component option's argument:\n    %s",
					arg);
				goto error;
			}

			switch (cur_cfg_comp->type) {
			case BT_COMPONENT_CLASS_TYPE_SOURCE:
				dest = BT_CONFIG_COMPONENT_DEST_SOURCE;
				break;
			case BT_COMPONENT_CLASS_TYPE_FILTER:
				dest = BT_CONFIG_COMPONENT_DEST_FILTER;
				break;
			case BT_COMPONENT_CLASS_TYPE_SINK:
				dest = BT_CONFIG_COMPONENT_DEST_SINK;
				break;
			default:
				bt_common_abort();
			}

			BT_ASSERT(cur_base_params);
			bt_value_put_ref(cur_cfg_comp->params);
			if (bt_value_copy(cur_base_params,
					&cur_cfg_comp->params) < 0) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}

			ret = add_run_cfg_comp_check_name(cfg,
				cur_cfg_comp, dest,
				instance_names);
			if (ret) {
				goto error;
			}

			break;
		}
		case OPT_PARAMS:
		{
			bt_value *params;

			if (!cur_cfg_comp) {
				BT_CLI_LOGE_APPEND_CAUSE("Cannot add parameters to unavailable component:\n    %s",
					arg);
				goto error;
			}

			params = bt_param_parse(arg, error_str);
			if (!params) {
				BT_CLI_LOGE_APPEND_CAUSE("Invalid format for --params option's argument:\n    %s",
					error_str->str);
				goto error;
			}

			extend_status = bt_value_map_extend(cur_cfg_comp->params,
				params);
			BT_VALUE_PUT_REF_AND_RESET(params);
			if (extend_status != BT_VALUE_MAP_EXTEND_STATUS_OK) {
				BT_CLI_LOGE_APPEND_CAUSE("Cannot extend current component parameters with --params option's argument:\n    %s",
					arg);
				goto error;
			}

			break;
		}
		case OPT_LOG_LEVEL:
			if (!cur_cfg_comp) {
				BT_CLI_LOGE_APPEND_CAUSE("Cannot set the log level of unavailable component:\n    %s",
					arg);
				goto error;
			}

			cur_cfg_comp->log_level =
				bt_log_get_level_from_string(arg);
			if (cur_cfg_comp->log_level < 0) {
				BT_CLI_LOGE_APPEND_CAUSE("Invalid argument for --log-level option:\n    %s",
					arg);
				goto error;
			}
			break;
		case OPT_BASE_PARAMS:
		{
			bt_value *params = bt_param_parse(arg, error_str);

			if (!params) {
				BT_CLI_LOGE_APPEND_CAUSE("Invalid format for --base-params option's argument:\n    %s",
					error_str->str);
				goto error;
			}

			BT_OBJECT_MOVE_REF(cur_base_params, params);
			break;
		}
		case OPT_RESET_BASE_PARAMS:
			BT_VALUE_PUT_REF_AND_RESET(cur_base_params);
			cur_base_params = bt_value_map_create();
			if (!cur_base_params) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}
			break;
		case OPT_CONNECT:
			if (bt_value_array_append_string_element(
					connection_args, arg)) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}
			break;
		case OPT_RETRY_DURATION: {
			gchar *end;
			size_t arg_len = strlen(argpar_item_opt->arg);

			retry_duration = g_ascii_strtoll(argpar_item_opt->arg, &end, 10);

			if (arg_len == 0 || end != (argpar_item_opt->arg + arg_len)) {
				BT_CLI_LOGE_APPEND_CAUSE(
					"Could not parse --retry-duration option's argument as an unsigned integer: `%s`",
					argpar_item_opt->arg);
				goto error;
			}

			if (retry_duration < 0) {
				BT_CLI_LOGE_APPEND_CAUSE("--retry-duration option's argument must be positive or 0: %ld",
					retry_duration);
				goto error;
			}

			cfg->cmd_data.run.retry_duration_us =
				(uint64_t) retry_duration;
			break;
		}
		default:
			BT_CLI_LOGE_APPEND_CAUSE("Unknown command-line option specified (option code %d).",
				argpar_item_opt->descr->id);
			goto error;
		}
	}

	BT_OBJECT_PUT_REF_AND_RESET(cur_cfg_comp);

	if (cfg->cmd_data.run.sources->len == 0) {
		BT_CLI_LOGE_APPEND_CAUSE("Incomplete graph: no source component.");
		goto error;
	}

	if (cfg->cmd_data.run.sinks->len == 0) {
		BT_CLI_LOGE_APPEND_CAUSE("Incomplete graph: no sink component.");
		goto error;
	}

	ret = bt_config_cli_args_create_connections(cfg,
		connection_args,
		error_buf, 256);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot creation connections:\n%s", error_buf);
		goto error;
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	if (error_str) {
		g_string_free(error_str, TRUE);
	}

	argpar_parse_ret_fini(&argpar_parse_ret);
	BT_OBJECT_PUT_REF_AND_RESET(cur_cfg_comp);
	BT_VALUE_PUT_REF_AND_RESET(cur_base_params);
	BT_VALUE_PUT_REF_AND_RESET(instance_names);
	BT_VALUE_PUT_REF_AND_RESET(connection_args);
	return cfg;
}

static
struct bt_config *bt_config_run_from_args_array(const bt_value *run_args,
		int *retcode, const bt_value *plugin_paths,
		int default_log_level)
{
	struct bt_config *cfg = NULL;
	const char **argv;
	uint64_t i, len = bt_value_array_get_length(run_args);

	BT_ASSERT(len <= SIZE_MAX);
	argv = calloc((size_t) len, sizeof(*argv));
	if (!argv) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

	for (i = 0; i < len; i++) {
		const bt_value *arg_value =
			bt_value_array_borrow_element_by_index_const(run_args,
				i);
		const char *arg;

		arg = bt_value_string_get(arg_value);
		BT_ASSERT(arg);
		argv[i] = arg;
	}

	cfg = bt_config_run_from_args((int) len, argv, retcode,
		plugin_paths, default_log_level);

end:
	free(argv);
	return cfg;
}

/*
 * Prints the convert command usage.
 */
static
void print_convert_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace2 [GENERAL OPTIONS] [convert] [OPTIONS] [PATH/URL]\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -c, --component=[NAME:]TYPE.PLUGIN.CLS\n");
	fprintf(fp, "                                    Instantiate the component class CLS of type\n");
	fprintf(fp, "                                    TYPE (`source`, `filter`, or `sink`) found\n");
	fprintf(fp, "                                    in the plugin PLUGIN, add it to the\n");
	fprintf(fp, "                                    conversion graph, and optionally name it\n");
	fprintf(fp, "                                    NAME\n");
	fprintf(fp, "  -l, --log-level=LVL               Set the log level of the current component to LVL\n");
	fprintf(fp, "                                    (`N`, `T`, `D`, `I`, `W`, `E`, or `F`)\n");
	fprintf(fp, "  -p, --params=PARAMS               Add initialization parameters PARAMS to the\n");
	fprintf(fp, "                                    current component (see the expected format\n");
	fprintf(fp, "                                    of PARAMS below)\n");
	fprintf(fp, "      --retry-duration=DUR          When babeltrace2(1) needs to retry to run\n");
	fprintf(fp, "                                    the graph later, retry in DUR µs\n");
	fprintf(fp, "                                    (default: 100000)\n");
	fprintf(fp, "                                    dynamic plugins can be loaded\n");
	fprintf(fp, "      --run-args                    Print the equivalent arguments for the\n");
	fprintf(fp, "                                    `run` command to the standard output,\n");
	fprintf(fp, "                                    formatted for a shell, and quit\n");
	fprintf(fp, "      --run-args-0                  Print the equivalent arguments for the\n");
	fprintf(fp, "                                    `run` command to the standard output,\n");
	fprintf(fp, "                                    formatted for `xargs -0`, and quit\n");
	fprintf(fp, "      --stream-intersection         Only process events when all streams\n");
	fprintf(fp, "                                    are active\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "Implicit `source.ctf.fs` component options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --clock-force-correlate       Force the origin of all clocks\n");
	fprintf(fp, "                                    to the Unix epoch\n");
	fprintf(fp, "      --clock-offset=SEC            Set clock offset to SEC seconds\n");
	fprintf(fp, "      --clock-offset-ns=NS          Set clock offset to NS ns\n");
	fprintf(fp, "\n");
	fprintf(fp, "Implicit `sink.text.pretty` component options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --clock-cycles                Print timestamps in clock cycles\n");
	fprintf(fp, "      --clock-date                  Print timestamp dates\n");
	fprintf(fp, "      --clock-gmt                   Print and parse timestamps in the GMT\n");
	fprintf(fp, "                                    time zone instead of the local time zone\n");
	fprintf(fp, "      --clock-seconds               Print the timestamps as `SEC.NS` instead\n");
	fprintf(fp, "                                    of `hh:mm:ss.nnnnnnnnn`\n");
	fprintf(fp, "      --color=(never | auto | always)\n");
	fprintf(fp, "                                    Never, automatically, or always emit\n");
	fprintf(fp, "                                    console color codes\n");
	fprintf(fp, "  -f, --fields=FIELD[,FIELD]...     Print additional fields; FIELD can be:\n");
	fprintf(fp, "                                      `all`, `trace`, `trace:hostname`,\n");
	fprintf(fp, "                                      `trace:domain`, `trace:procname`,\n");
	fprintf(fp, "                                      `trace:vpid`, `loglevel`, `emf`\n");
	fprintf(fp, "  -n, --names=NAME[,NAME]...        Print field names; NAME can be:\n");
	fprintf(fp, "                                      `payload` (or `arg` or `args`), `none`,\n");
	fprintf(fp, "                                      `all`, `scope`, `header`, `context`\n");
	fprintf(fp, "                                      (or `ctx`)\n");
	fprintf(fp, "      --no-delta                    Do not print time delta between\n");
	fprintf(fp, "                                    consecutive events\n");
	fprintf(fp, "  -w, --output=PATH                 Write output text to PATH instead of\n");
	fprintf(fp, "                                    the standard output\n");
	fprintf(fp, "\n");
	fprintf(fp, "Implicit `filter.utils.trimmer` component options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -b, --begin=BEGIN                 Set the beginning time of the conversion\n");
	fprintf(fp, "                                    time range to BEGIN (see the format of\n");
	fprintf(fp, "                                    BEGIN below)\n");
	fprintf(fp, "  -e, --end=END                     Set the end time of the conversion time\n");
	fprintf(fp, "                                    range to END (see the format of END below)\n");
	fprintf(fp, "  -t, --timerange=TIMERANGE         Set conversion time range to TIMERANGE:\n");
	fprintf(fp, "                                    BEGIN,END or [BEGIN,END] (literally `[` and\n");
	fprintf(fp, "                                    `]`) (see the format of BEGIN/END below)\n");
	fprintf(fp, "\n");
	fprintf(fp, "Implicit `filter.lttng-utils.debug-info` component options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --debug-info                  Create an implicit\n");
	fprintf(fp, "                                    `filter.lttng-utils.debug-info` component\n");
	fprintf(fp, "      --debug-info-dir=DIR          Search for debug info in directory DIR\n");
	fprintf(fp, "                                    instead of `/usr/lib/debug`\n");
	fprintf(fp, "      --debug-info-full-path        Show full debug info source and\n");
	fprintf(fp, "                                    binary paths instead of just names\n");
	fprintf(fp, "      --debug-info-target-prefix=DIR\n");
	fprintf(fp, "                                    Use directory DIR as a prefix when\n");
	fprintf(fp, "                                    looking up executables during debug\n");
	fprintf(fp, "                                    info analysis\n");
	fprintf(fp, "\n");
	fprintf(fp, "Legacy options that still work:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -i, --input-format=(ctf | lttng-live)\n");
	fprintf(fp, "                                    `ctf`:\n");
	fprintf(fp, "                                      Create an implicit `source.ctf.fs`\n");
	fprintf(fp, "                                      component\n");
	fprintf(fp, "                                    `lttng-live`:\n");
	fprintf(fp, "                                      Create an implicit `source.ctf.lttng-live`\n");
	fprintf(fp, "                                      component\n");
	fprintf(fp, "  -o, --output-format=(text | ctf | dummy | ctf-metadata)\n");
	fprintf(fp, "                                    `text`:\n");
	fprintf(fp, "                                      Create an implicit `sink.text.pretty`\n");
	fprintf(fp, "                                      component\n");
	fprintf(fp, "                                    `ctf`:\n");
	fprintf(fp, "                                      Create an implicit `sink.ctf.fs`\n");
	fprintf(fp, "                                      component\n");
	fprintf(fp, "                                    `dummy`:\n");
	fprintf(fp, "                                      Create an implicit `sink.utils.dummy`\n");
	fprintf(fp, "                                      component\n");
	fprintf(fp, "                                    `ctf-metadata`:\n");
	fprintf(fp, "                                      Query the `source.ctf.fs` component class\n");
	fprintf(fp, "                                      for metadata text and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace2 --help` for the list of general options.\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Format of BEGIN and END\n");
	fprintf(fp, "-----------------------\n");
	fprintf(fp, "\n");
	fprintf(fp, "    [YYYY-MM-DD [hh:mm:]]ss[.nnnnnnnnn]\n");
	fprintf(fp, "\n\n");
	print_expected_params_format(fp);
}

static
const struct argpar_opt_descr convert_options[] = {
	/* id, short_name, long_name, with_arg */
	{ OPT_BEGIN, 'b', "begin", true },
	{ OPT_CLOCK_CYCLES, '\0', "clock-cycles", false },
	{ OPT_CLOCK_DATE, '\0', "clock-date", false },
	{ OPT_CLOCK_FORCE_CORRELATE, '\0', "clock-force-correlate", false },
	{ OPT_CLOCK_GMT, '\0', "clock-gmt", false },
	{ OPT_CLOCK_OFFSET, '\0', "clock-offset", true },
	{ OPT_CLOCK_OFFSET_NS, '\0', "clock-offset-ns", true },
	{ OPT_CLOCK_SECONDS, '\0', "clock-seconds", false },
	{ OPT_COLOR, '\0', "color", true },
	{ OPT_COMPONENT, 'c', "component", true },
	{ OPT_DEBUG, 'd', "debug", false },
	{ OPT_DEBUG_INFO_DIR, '\0', "debug-info-dir", true },
	{ OPT_DEBUG_INFO_FULL_PATH, '\0', "debug-info-full-path", false },
	{ OPT_DEBUG_INFO_TARGET_PREFIX, '\0', "debug-info-target-prefix", true },
	{ OPT_END, 'e', "end", true },
	{ OPT_FIELDS, 'f', "fields", true },
	{ OPT_HELP, 'h', "help", false },
	{ OPT_INPUT_FORMAT, 'i', "input-format", true },
	{ OPT_LOG_LEVEL, 'l', "log-level", true },
	{ OPT_NAMES, 'n', "names", true },
	{ OPT_DEBUG_INFO, '\0', "debug-info", false },
	{ OPT_NO_DELTA, '\0', "no-delta", false },
	{ OPT_OMIT_HOME_PLUGIN_PATH, '\0', "omit-home-plugin-path", false },
	{ OPT_OMIT_SYSTEM_PLUGIN_PATH, '\0', "omit-system-plugin-path", false },
	{ OPT_OUTPUT, 'w', "output", true },
	{ OPT_OUTPUT_FORMAT, 'o', "output-format", true },
	{ OPT_PARAMS, 'p', "params", true },
	{ OPT_PLUGIN_PATH, '\0', "plugin-path", true },
	{ OPT_RETRY_DURATION, '\0', "retry-duration", true },
	{ OPT_RUN_ARGS, '\0', "run-args", false },
	{ OPT_RUN_ARGS_0, '\0', "run-args-0", false },
	{ OPT_STREAM_INTERSECTION, '\0', "stream-intersection", false },
	{ OPT_TIMERANGE, '\0', "timerange", true },
	{ OPT_VERBOSE, 'v', "verbose", false },
	ARGPAR_OPT_DESCR_SENTINEL
};

static
GString *get_component_auto_name(const char *prefix,
		const bt_value *existing_names)
{
	unsigned int i = 0;
	GString *auto_name = g_string_new(NULL);

	if (!auto_name) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

	if (!bt_value_map_has_entry(existing_names, prefix)) {
		g_string_assign(auto_name, prefix);
		goto end;
	}

	do {
		g_string_printf(auto_name, "%s-%d", prefix, i);
		i++;
	} while (bt_value_map_has_entry(existing_names, auto_name->str));

end:
	return auto_name;
}

struct implicit_component_args {
	bool exists;

	/* The component class name (e.g. src.ctf.fs). */
	GString *comp_arg;

	/* The component instance name. */
	GString *name_arg;

	GString *params_arg;
	bt_value *extra_params;
};

static
int assign_name_to_implicit_component(struct implicit_component_args *args,
		const char *prefix, bt_value *existing_names,
		GList **comp_names, bool append_to_comp_names)
{
	int ret = 0;
	GString *name = NULL;

	if (!args->exists) {
		goto end;
	}

	name = get_component_auto_name(prefix,
		existing_names);

	if (!name) {
		ret = -1;
		goto end;
	}

	g_string_assign(args->name_arg, name->str);

	if (bt_value_map_insert_entry(existing_names, name->str,
			bt_value_null)) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	if (append_to_comp_names) {
		*comp_names = g_list_append(*comp_names, name);
		name = NULL;
	}

end:
	if (name) {
		g_string_free(name, TRUE);
	}

	return ret;
}

static
int append_run_args_for_implicit_component(
		struct implicit_component_args *impl_args,
		bt_value *run_args)
{
	int ret = 0;
	uint64_t i;
	GString *component_arg_for_run = NULL;

	if (!impl_args->exists) {
		goto end;
	}

	component_arg_for_run = g_string_new(NULL);
	if (!component_arg_for_run) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	/* Build the full `name:type.plugin.cls`. */
	BT_ASSERT(!strchr(impl_args->name_arg->str, '\\'));
	BT_ASSERT(!strchr(impl_args->name_arg->str, ':'));
	g_string_printf(component_arg_for_run, "%s:%s",
		impl_args->name_arg->str, impl_args->comp_arg->str);

	if (bt_value_array_append_string_element(run_args, "--component")) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	if (bt_value_array_append_string_element(run_args,
			component_arg_for_run->str)) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	if (impl_args->params_arg->len > 0) {
		if (bt_value_array_append_string_element(run_args, "--params")) {
			BT_CLI_LOGE_APPEND_CAUSE_OOM();
			goto error;
		}

		if (bt_value_array_append_string_element(run_args,
				impl_args->params_arg->str)) {
			BT_CLI_LOGE_APPEND_CAUSE_OOM();
			goto error;
		}
	}

	for (i = 0; i < bt_value_array_get_length(impl_args->extra_params); i++) {
		const bt_value *elem;
		const char *arg;

		elem = bt_value_array_borrow_element_by_index(
			impl_args->extra_params, i);

		BT_ASSERT(bt_value_is_string(elem));
		arg = bt_value_string_get(elem);
		ret = bt_value_array_append_string_element(run_args, arg);
		if (ret) {
			BT_CLI_LOGE_APPEND_CAUSE_OOM();
			goto error;
		}
	}

	goto end;

error:
	ret = -1;

end:
	if (component_arg_for_run) {
		g_string_free(component_arg_for_run, TRUE);
	}

	return ret;
}

/* Free the fields of a `struct implicit_component_args`. */

static
void finalize_implicit_component_args(struct implicit_component_args *args)
{
	BT_ASSERT(args);

	if (args->comp_arg) {
		g_string_free(args->comp_arg, TRUE);
	}

	if (args->name_arg) {
		g_string_free(args->name_arg, TRUE);
	}

	if (args->params_arg) {
		g_string_free(args->params_arg, TRUE);
	}

	bt_value_put_ref(args->extra_params);
}

/* Destroy a dynamically-allocated `struct implicit_component_args`. */

static
void destroy_implicit_component_args(struct implicit_component_args *args)
{
	finalize_implicit_component_args(args);
	g_free(args);
}

/* Initialize the fields of an already allocated `struct implicit_component_args`. */

static
int init_implicit_component_args(struct implicit_component_args *args,
		const char *comp_arg, bool exists)
{
	int ret = 0;

	args->exists = exists;
	args->comp_arg = g_string_new(comp_arg);
	args->name_arg = g_string_new(NULL);
	args->params_arg = g_string_new(NULL);
	args->extra_params = bt_value_array_create();

	if (!args->comp_arg || !args->name_arg ||
			!args->params_arg || !args->extra_params) {
		ret = -1;
		finalize_implicit_component_args(args);
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

end:
	return ret;
}

/* Dynamically allocate and initialize a `struct implicit_component_args`. */

static
struct implicit_component_args *create_implicit_component_args(
		const char *comp_arg)
{
	struct implicit_component_args *args;
	int status;

	args = g_new(struct implicit_component_args, 1);
	if (!args) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

	status = init_implicit_component_args(args, comp_arg, true);
	if (status != 0) {
		g_free(args);
		args = NULL;
	}

end:
	return args;
}

static
void append_implicit_component_param(struct implicit_component_args *args,
	const char *key, const char *value)
{
	BT_ASSERT(args);
	BT_ASSERT(key);
	BT_ASSERT(value);
	append_param_arg(args->params_arg, key, value);
}

/*
 * Append the given parameter (`key=value`) to all component specifications
 * in `implicit_comp_args` (an array of `struct implicit_component_args *`)
 * which match `comp_arg`.
 *
 * Return the number of matching components.
 */

static
int append_multiple_implicit_components_param(GPtrArray *implicit_comp_args,
		const char *comp_arg, const char *key, const char *value)
{
	int i;
	int n = 0;

	for (i = 0; i < implicit_comp_args->len; i++) {
		struct implicit_component_args *args = implicit_comp_args->pdata[i];

		if (strcmp(args->comp_arg->str, comp_arg) == 0) {
			append_implicit_component_param(args, key, value);
			n++;
		}
	}

	return n;
}

/* Escape value to make it suitable to use as a string parameter value. */
static
gchar *escape_string_value(const char *value)
{
	GString *ret;
	const char *in;

	ret = g_string_new(NULL);
	if (!ret) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

	in = value;
	while (*in) {
		switch (*in) {
		case '"':
		case '\\':
			g_string_append_c(ret, '\\');
			break;
		}

		g_string_append_c(ret, *in);

		in++;
	}

end:
	return g_string_free(ret, FALSE);
}

static
int bt_value_to_cli_param_value_append(const bt_value *value, GString *buf)
{
	BT_ASSERT(buf);

	int ret = -1;

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_STRING:
	{
		const char *str_value = bt_value_string_get(value);
		gchar *escaped_str_value;

		escaped_str_value = escape_string_value(str_value);
		if (!escaped_str_value) {
			goto end;
		}

		g_string_append_printf(buf, "\"%s\"", escaped_str_value);

		g_free(escaped_str_value);
		break;
	}
	case BT_VALUE_TYPE_ARRAY: {
		g_string_append_c(buf, '[');
		uint64_t sz = bt_value_array_get_length(value);
		for (uint64_t i = 0; i < sz; i++) {
			const bt_value *item;

			if (i > 0) {
				g_string_append(buf, ", ");
			}

			item = bt_value_array_borrow_element_by_index_const(
				value, i);
			ret = bt_value_to_cli_param_value_append(item, buf);

			if (ret) {
				goto end;
			}
		}
		g_string_append_c(buf, ']');
		break;
	}
	default:
		bt_common_abort();
	}

	ret = 0;

end:
	return ret;
}

/*
 * Convert `value` to its equivalent representation as a command line parameter
 * value.
 */

static
gchar *bt_value_to_cli_param_value(bt_value *value)
{
	GString *buf;
	gchar *result = NULL;

	buf = g_string_new(NULL);
	if (!buf) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	if (bt_value_to_cli_param_value_append(value, buf)) {
		goto error;
	}

	result = g_string_free(buf, FALSE);
	buf = NULL;

	goto end;

error:
	if (buf) {
		g_string_free(buf, TRUE);
	}

end:
	return result;
}

static
int append_parameter_to_args(bt_value *args, const char *key, bt_value *value)
{
	BT_ASSERT(args);
	BT_ASSERT(bt_value_get_type(args) == BT_VALUE_TYPE_ARRAY);
	BT_ASSERT(key);
	BT_ASSERT(value);

	int ret = 0;
	gchar *str_value = NULL;
	GString *parameter = NULL;

	if (bt_value_array_append_string_element(args, "--params")) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	str_value = bt_value_to_cli_param_value(value);
	if (!str_value) {
		ret = -1;
		goto end;
	}

	parameter = g_string_new(NULL);
	if (!parameter) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	g_string_printf(parameter, "%s=%s", key, str_value);

	if (bt_value_array_append_string_element(args, parameter->str)) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

end:
	if (parameter) {
		g_string_free(parameter, TRUE);
		parameter = NULL;
	}

	if (str_value) {
		g_free(str_value);
		str_value = NULL;
	}

	return ret;
}

static
int append_string_parameter_to_args(bt_value *args, const char *key, const char *value)
{
	bt_value *str_value;
	int ret;

	str_value = bt_value_string_create_init(value);

	if (!str_value) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	ret = append_parameter_to_args(args, key, str_value);

end:
	BT_VALUE_PUT_REF_AND_RESET(str_value);
	return ret;
}

static
int append_implicit_component_extra_param(struct implicit_component_args *args,
		const char *key, const char *value)
{
	return append_string_parameter_to_args(args->extra_params, key, value);
}

/*
 * Escapes `.`, `:`, and `\` of `input` with `\`.
 */
static
GString *escape_dot_colon(const char *input)
{
	GString *output = g_string_new(NULL);
	const char *ch;

	if (!output) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

	for (ch = input; *ch != '\0'; ch++) {
		if (*ch == '\\' || *ch == '.' || *ch == ':') {
			g_string_append_c(output, '\\');
		}

		g_string_append_c(output, *ch);
	}

end:
	return output;
}

/*
 * Appends a --connect option to a list of arguments. `upstream_name`
 * and `downstream_name` are escaped with escape_dot_colon() in this
 * function.
 */
static
int append_connect_arg(bt_value *run_args,
		const char *upstream_name, const char *downstream_name)
{
	int ret = 0;
	GString *e_upstream_name = escape_dot_colon(upstream_name);
	GString *e_downstream_name = escape_dot_colon(downstream_name);
	GString *arg = g_string_new(NULL);

	if (!e_upstream_name || !e_downstream_name || !arg) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	ret = bt_value_array_append_string_element(run_args, "--connect");
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

	g_string_append(arg, e_upstream_name->str);
	g_string_append_c(arg, ':');
	g_string_append(arg, e_downstream_name->str);
	ret = bt_value_array_append_string_element(run_args, arg->str);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		ret = -1;
		goto end;
	}

end:
	if (arg) {
		g_string_free(arg, TRUE);
	}

	if (e_upstream_name) {
		g_string_free(e_upstream_name, TRUE);
	}

	if (e_downstream_name) {
		g_string_free(e_downstream_name, TRUE);
	}

	return ret;
}

/*
 * Appends the run command's --connect options for the convert command.
 */
static
int convert_auto_connect(bt_value *run_args,
		GList *source_names, GList *filter_names,
		GList *sink_names)
{
	int ret = 0;
	GList *source_at = source_names;
	GList *filter_at = filter_names;
	GList *filter_prev;
	GList *sink_at = sink_names;

	BT_ASSERT(source_names);
	BT_ASSERT(filter_names);
	BT_ASSERT(sink_names);

	/* Connect all sources to the first filter */
	for (source_at = source_names; source_at; source_at = g_list_next(source_at)) {
		GString *source_name = source_at->data;
		GString *filter_name = filter_at->data;

		ret = append_connect_arg(run_args, source_name->str,
			filter_name->str);
		if (ret) {
			goto error;
		}
	}

	filter_prev = filter_at;
	filter_at = g_list_next(filter_at);

	/* Connect remaining filters */
	for (; filter_at; filter_prev = filter_at, filter_at = g_list_next(filter_at)) {
		GString *filter_name = filter_at->data;
		GString *filter_prev_name = filter_prev->data;

		ret = append_connect_arg(run_args, filter_prev_name->str,
			filter_name->str);
		if (ret) {
			goto error;
		}
	}

	/* Connect last filter to all sinks */
	for (sink_at = sink_names; sink_at; sink_at = g_list_next(sink_at)) {
		GString *filter_name = filter_prev->data;
		GString *sink_name = sink_at->data;

		ret = append_connect_arg(run_args, filter_name->str,
			sink_name->str);
		if (ret) {
			goto error;
		}
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int split_timerange(const char *arg, char **begin, char **end)
{
	int ret = 0;
	const char *ch = arg;
	size_t end_pos;
	GString *g_begin = NULL;
	GString *g_end = NULL;

	BT_ASSERT(arg);

	if (*ch == '[') {
		ch++;
	}

	g_begin = bt_common_string_until(ch, "", ",", &end_pos);
	if (!g_begin || ch[end_pos] != ',' || g_begin->len == 0) {
		goto error;
	}

	ch += end_pos + 1;

	g_end = bt_common_string_until(ch, "", "]", &end_pos);
	if (!g_end || g_end->len == 0) {
		goto error;
	}

	BT_ASSERT(begin);
	BT_ASSERT(end);
	*begin = g_begin->str;
	*end = g_end->str;
	g_string_free(g_begin, FALSE);
	g_string_free(g_end, FALSE);
	g_begin = NULL;
	g_end = NULL;
	goto end;

error:
	ret = -1;

end:
	if (g_begin) {
		g_string_free(g_begin, TRUE);
	}

	if (g_end) {
		g_string_free(g_end, TRUE);
	}

	return ret;
}

static
int g_list_prepend_gstring(GList **list, const char *string)
{
	int ret = 0;
	GString *gs = g_string_new(string);

	BT_ASSERT(list);

	if (!gs) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto end;
	}

	*list = g_list_prepend(*list, gs);

end:
	return ret;
}

/*
 * Create `struct implicit_component_args` structures for each of the
 * source components we identified.  Add them to `component_args`.
 *
 * `non_opts` is an array of the non-option arguments passed on the command
 * line.
 *
 * `non_opt_params` is an array where each element is an array of
 * strings containing all the arguments to `--params` that apply to the
 * non-option argument at the same index.  For example, if, for a
 * non-option argument, the following `--params` options applied:
 *
 *     --params=a=2 --params=b=3,c=4
 *
 * its entry in `non_opt_params` would contain
 *
 *     ["a=2", "b=3,c=4"]
 */

static
int create_implicit_component_args_from_auto_discovered_sources(
		const struct auto_source_discovery *auto_disc,
		const bt_value *non_opts,
		const bt_value *non_opt_params,
		const bt_value *non_opt_loglevels,
		GPtrArray *component_args)
{
	gchar *cc_name = NULL;
	struct implicit_component_args *comp = NULL;
	int status;
	guint i, len;

	len = auto_disc->results->len;

	for (i = 0; i < len; i++) {
		struct auto_source_discovery_result *res =
			g_ptr_array_index(auto_disc->results, i);
		uint64_t orig_indices_i, orig_indices_count;

		g_free(cc_name);
		cc_name = g_strdup_printf("source.%s.%s", res->plugin_name, res->source_cc_name);
		if (!cc_name) {
			BT_CLI_LOGE_APPEND_CAUSE_OOM();
			goto error;
		}

		comp = create_implicit_component_args(cc_name);
		if (!comp) {
			goto error;
		}

		/*
		 * Append parameters and log levels of all the
		 * non-option arguments that contributed to this
		 * component instance coming into existence.
		 */
		orig_indices_count = bt_value_array_get_length(res->original_input_indices);
		for (orig_indices_i = 0; orig_indices_i < orig_indices_count; orig_indices_i++) {
			const bt_value *orig_idx_value =
				bt_value_array_borrow_element_by_index(
					res->original_input_indices, orig_indices_i);
			uint64_t orig_idx = bt_value_integer_unsigned_get(orig_idx_value);
			const bt_value *params_array =
				bt_value_array_borrow_element_by_index_const(
					non_opt_params, orig_idx);
			uint64_t params_i, params_count;
			const bt_value *loglevel_value;

			params_count = bt_value_array_get_length(params_array);
			for (params_i = 0; params_i < params_count; params_i++) {
				const bt_value *params_value =
					bt_value_array_borrow_element_by_index_const(
						params_array, params_i);
				const char *params = bt_value_string_get(params_value);
				bt_value_array_append_element_status append_status;

				append_status = bt_value_array_append_string_element(
					comp->extra_params, "--params");
				if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
					BT_CLI_LOGE_APPEND_CAUSE("Failed to append array element.");
					goto error;
				}

				append_status = bt_value_array_append_string_element(
					comp->extra_params, params);
				if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
					BT_CLI_LOGE_APPEND_CAUSE("Failed to append array element.");
					goto error;
				}
			}

			loglevel_value = bt_value_array_borrow_element_by_index_const(
				non_opt_loglevels, orig_idx);
			if (bt_value_get_type(loglevel_value) == BT_VALUE_TYPE_STRING) {
				const char *loglevel = bt_value_string_get(loglevel_value);
				bt_value_array_append_element_status append_status;

				append_status = bt_value_array_append_string_element(
					comp->extra_params, "--log-level");
				if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
					BT_CLI_LOGE_APPEND_CAUSE("Failed to append array element.");
					goto error;
				}

				append_status = bt_value_array_append_string_element(
					comp->extra_params, loglevel);
				if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
					BT_CLI_LOGE_APPEND_CAUSE("Failed to append array element.");
					goto error;
				}
			}
		}

		/*
		 * If single input and a src.ctf.fs component, provide the
		 * relative path from the path passed on the command line to the
		 * found trace.
		 */
		if (bt_value_array_get_length(res->inputs) == 1 &&
				strcmp(res->plugin_name, "ctf") == 0 &&
				strcmp(res->source_cc_name, "fs") == 0) {
			const bt_value *orig_idx_value =
				bt_value_array_borrow_element_by_index(
					res->original_input_indices, 0);
			uint64_t orig_idx = bt_value_integer_unsigned_get(orig_idx_value);
			const bt_value *non_opt_value =
				bt_value_array_borrow_element_by_index_const(
					non_opts, orig_idx);
			const char *non_opt = bt_value_string_get(non_opt_value);
			const bt_value *input_value =
				bt_value_array_borrow_element_by_index_const(
					res->inputs, 0);
			const char *input = bt_value_string_get(input_value);

			BT_ASSERT(orig_indices_count == 1);
			BT_ASSERT(g_str_has_prefix(input, non_opt));

			input += strlen(non_opt);

			while (G_IS_DIR_SEPARATOR(*input)) {
				input++;
			}

			if (strlen(input) > 0) {
				append_string_parameter_to_args(comp->extra_params,
					"trace-name", input);
			}
		}

		status = append_parameter_to_args(comp->extra_params, "inputs", res->inputs);
		if (status != 0) {
			goto error;
		}

		g_ptr_array_add(component_args, comp);
		comp = NULL;
	}

	status = 0;
	goto end;

error:
	status = -1;

end:
	g_free(cc_name);

	if (comp) {
		destroy_implicit_component_args(comp);
	}

	return status;
}

/*
 * As we iterate the arguments to the convert command, this tracks what is the
 * type of the current item, to which some contextual options (e.g. --params)
 * apply to.
 */
enum convert_current_item_type {
	/* There is no current item. */
	CONVERT_CURRENT_ITEM_TYPE_NONE,

	/* Current item is a component. */
	CONVERT_CURRENT_ITEM_TYPE_COMPONENT,

	/* Current item is a non-option argument. */
	CONVERT_CURRENT_ITEM_TYPE_NON_OPT,
};

/*
 * Creates a Babeltrace config object from the arguments of a convert
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_convert_from_args(int argc, const char *argv[],
		int *retcode, const bt_value *plugin_paths,
		int *default_log_level, const bt_interrupter *interrupter)
{
	enum convert_current_item_type current_item_type =
		CONVERT_CURRENT_ITEM_TYPE_NONE;
	int ret = 0;
	struct bt_config *cfg = NULL;
	bool got_input_format_opt = false;
	bool got_output_format_opt = false;
	bool trimmer_has_begin = false;
	bool trimmer_has_end = false;
	bool stream_intersection_mode = false;
	bool print_run_args = false;
	bool print_run_args_0 = false;
	bool print_ctf_metadata = false;
	bt_value *run_args = NULL;
	bt_value *all_names = NULL;
	GList *source_names = NULL;
	GList *filter_names = NULL;
	GList *sink_names = NULL;
	bt_value *non_opts = NULL;
	bt_value *non_opt_params = NULL;
	bt_value *non_opt_loglevels = NULL;
	struct implicit_component_args implicit_ctf_output_args = { 0 };
	struct implicit_component_args implicit_lttng_live_args = { 0 };
	struct implicit_component_args implicit_dummy_args = { 0 };
	struct implicit_component_args implicit_text_args = { 0 };
	struct implicit_component_args implicit_debug_info_args = { 0 };
	struct implicit_component_args implicit_muxer_args = { 0 };
	struct implicit_component_args implicit_trimmer_args = { 0 };
	char error_buf[256] = { 0 };
	size_t i;
	struct bt_common_lttng_live_url_parts lttng_live_url_parts = { 0 };
	char *output = NULL;
	struct auto_source_discovery auto_disc = { NULL };
	GString *auto_disc_comp_name = NULL;
	struct argpar_parse_ret argpar_parse_ret = { 0 };
	GString *name_gstr = NULL;
	GString *component_arg_for_run = NULL;
	bt_value *live_inputs_array_val = NULL;

	/*
	 * Array of `struct implicit_component_args *` created for the sources
	 * we have auto-discovered.
	 */
	GPtrArray *discovered_source_args = NULL;

	/*
	 * If set, restrict automatic source discovery to this component class
	 * of this plugin.
	 */
	const char *auto_source_discovery_restrict_plugin_name = NULL;
	const char *auto_source_discovery_restrict_component_class_name = NULL;

	bool ctf_fs_source_force_clock_class_unix_epoch_origin = false;
	gchar *ctf_fs_source_clock_class_offset_arg = NULL;
	gchar *ctf_fs_source_clock_class_offset_ns_arg = NULL;
	*retcode = 0;

	if (argc < 1) {
		print_convert_usage(stdout);
		*retcode = -1;
		goto end;
	}

	if (init_implicit_component_args(&implicit_ctf_output_args,
			"sink.ctf.fs", false)) {
		goto error;
	}

	if (init_implicit_component_args(&implicit_lttng_live_args,
			"source.ctf.lttng-live", false)) {
		goto error;
	}

	if (init_implicit_component_args(&implicit_text_args,
			"sink.text.pretty", false)) {
		goto error;
	}

	if (init_implicit_component_args(&implicit_dummy_args,
			"sink.utils.dummy", false)) {
		goto error;
	}

	if (init_implicit_component_args(&implicit_debug_info_args,
			"filter.lttng-utils.debug-info", false)) {
		goto error;
	}

	if (init_implicit_component_args(&implicit_muxer_args,
			"filter.utils.muxer", true)) {
		goto error;
	}

	if (init_implicit_component_args(&implicit_trimmer_args,
			"filter.utils.trimmer", false)) {
		goto error;
	}

	all_names = bt_value_map_create();
	if (!all_names) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	run_args = bt_value_array_create();
	if (!run_args) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	component_arg_for_run = g_string_new(NULL);
	if (!component_arg_for_run) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	non_opts = bt_value_array_create();
	if (!non_opts) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	non_opt_params = bt_value_array_create();
	if (!non_opt_params) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	non_opt_loglevels = bt_value_array_create();
	if (!non_opt_loglevels) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	if (auto_source_discovery_init(&auto_disc) != 0) {
		goto error;
	}

	discovered_source_args =
		g_ptr_array_new_with_free_func((GDestroyNotify) destroy_implicit_component_args);
	if (!discovered_source_args) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	auto_disc_comp_name = g_string_new(NULL);
	if (!auto_disc_comp_name) {
		BT_CLI_LOGE_APPEND_CAUSE_OOM();
		goto error;
	}

	/*
	 * First pass: collect all arguments which need to be passed
	 * as is to the run command. This pass can also add --name
	 * arguments if needed to automatically name unnamed component
	 * instances.
	 */
	argpar_parse_ret = argpar_parse(argc, argv, convert_options, true);
	if (argpar_parse_ret.error) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"While parsing `convert` command's command-line arguments: %s",
			argpar_parse_ret.error);
		goto error;
	}

	if (help_option_is_specified(&argpar_parse_ret)) {
		print_convert_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	for (i = 0; i < argpar_parse_ret.items->n_items; i++) {
		struct argpar_item *argpar_item =
			argpar_parse_ret.items->items[i];
		struct argpar_item_opt *argpar_item_opt;
		char *name = NULL;
		char *plugin_name = NULL;
		char *comp_cls_name = NULL;
		const char *arg;

		if (argpar_item->type == ARGPAR_ITEM_TYPE_OPT) {
			argpar_item_opt = (struct argpar_item_opt *) argpar_item;
			arg = argpar_item_opt->arg;

			switch (argpar_item_opt->descr->id) {
			case OPT_COMPONENT:
			{
				bt_component_class_type type;

				current_item_type = CONVERT_CURRENT_ITEM_TYPE_COMPONENT;

				/* Parse the argument */
				plugin_comp_cls_names(arg, &name, &plugin_name,
					&comp_cls_name, &type);
				if (!plugin_name || !comp_cls_name) {
					BT_CLI_LOGE_APPEND_CAUSE(
						"Invalid format for --component option's argument:\n    %s",
						arg);
					goto error;
				}

				if (name) {
					/*
					 * Name was given by the user, verify it isn't
					 * taken.
					 */
					if (bt_value_map_has_entry(all_names, name)) {
						BT_CLI_LOGE_APPEND_CAUSE(
							"Duplicate component instance name:\n    %s",
							name);
						goto error;
					}

					name_gstr = g_string_new(name);
					if (!name_gstr) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}

					g_string_assign(component_arg_for_run, arg);
				} else {
					/* Name not given by user, generate one. */
					name_gstr = get_component_auto_name(arg, all_names);
					if (!name_gstr) {
						goto error;
					}

					g_string_printf(component_arg_for_run, "%s:%s",
						name_gstr->str, arg);
				}

				if (bt_value_array_append_string_element(run_args,
						"--component")) {
					BT_CLI_LOGE_APPEND_CAUSE_OOM();
					goto error;
				}

				if (bt_value_array_append_string_element(run_args,
						component_arg_for_run->str)) {
					BT_CLI_LOGE_APPEND_CAUSE_OOM();
					goto error;
				}

				/*
				 * Remember this name globally, for the uniqueness of
				 * all component names.
				 */
				if (bt_value_map_insert_entry(all_names,
						name_gstr->str, bt_value_null)) {
					BT_CLI_LOGE_APPEND_CAUSE_OOM();
					goto error;
				}

				/*
				 * Remember this name specifically for the type of the
				 * component. This is to create connection arguments.
				 *
				 * The list takes ownership of `name_gstr`.
				 */
				switch (type) {
				case BT_COMPONENT_CLASS_TYPE_SOURCE:
					source_names = g_list_append(source_names, name_gstr);
					break;
				case BT_COMPONENT_CLASS_TYPE_FILTER:
					filter_names = g_list_append(filter_names, name_gstr);
					break;
				case BT_COMPONENT_CLASS_TYPE_SINK:
					sink_names = g_list_append(sink_names, name_gstr);
					break;
				default:
					bt_common_abort();
				}
				name_gstr = NULL;

				free(name);
				free(plugin_name);
				free(comp_cls_name);
				name = NULL;
				plugin_name = NULL;
				comp_cls_name = NULL;
				break;
			}
			case OPT_PARAMS:
				if (current_item_type == CONVERT_CURRENT_ITEM_TYPE_COMPONENT) {
					/*
					 * The current item is a component (--component option),
					 * pass it directly to the run args.
					 */
					if (bt_value_array_append_string_element(run_args,
							"--params")) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}

					if (bt_value_array_append_string_element(run_args, arg)) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}
				} else if (current_item_type == CONVERT_CURRENT_ITEM_TYPE_NON_OPT) {
					/*
					 * The current item is a
					 * non-option argument, record
					 * it in `non_opt_params`.
					 */
					bt_value *array;
					bt_value_array_append_element_status append_element_status;
					uint64_t idx = bt_value_array_get_length(non_opt_params) - 1;

					array = bt_value_array_borrow_element_by_index(non_opt_params, idx);

					append_element_status = bt_value_array_append_string_element(array, arg);
					if (append_element_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}
				} else {
					BT_CLI_LOGE_APPEND_CAUSE(
						"No current component (--component option) or non-option argument of which to set parameters:\n    %s",
						arg);
					goto error;
				}
				break;
			case OPT_LOG_LEVEL:
				if (current_item_type == CONVERT_CURRENT_ITEM_TYPE_COMPONENT) {
					if (bt_value_array_append_string_element(run_args, "--log-level")) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}

					if (bt_value_array_append_string_element(run_args, arg)) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}
				} else if (current_item_type == CONVERT_CURRENT_ITEM_TYPE_NON_OPT) {
					uint64_t idx = bt_value_array_get_length(non_opt_loglevels) - 1;
					enum bt_value_array_set_element_by_index_status set_element_status;
					bt_value *log_level_str_value;

					log_level_str_value = bt_value_string_create_init(arg);
					if (!log_level_str_value) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}

					set_element_status =
						bt_value_array_set_element_by_index(non_opt_loglevels,
							idx, log_level_str_value);
					bt_value_put_ref(log_level_str_value);
					if (set_element_status != BT_VALUE_ARRAY_SET_ELEMENT_BY_INDEX_STATUS_OK) {
						BT_CLI_LOGE_APPEND_CAUSE_OOM();
						goto error;
					}
				} else {
					BT_CLI_LOGE_APPEND_CAUSE(
						"No current component (--component option) or non-option argument to assign a log level to:\n    %s",
						arg);
					goto error;
				}

				break;
			case OPT_RETRY_DURATION:
				if (bt_value_array_append_string_element(run_args,
						"--retry-duration")) {
					BT_CLI_LOGE_APPEND_CAUSE_OOM();
					goto error;
				}

				if (bt_value_array_append_string_element(run_args, arg)) {
					BT_CLI_LOGE_APPEND_CAUSE_OOM();
					goto error;
				}
				break;
			case OPT_BEGIN:
			case OPT_CLOCK_CYCLES:
			case OPT_CLOCK_DATE:
			case OPT_CLOCK_FORCE_CORRELATE:
			case OPT_CLOCK_GMT:
			case OPT_CLOCK_OFFSET:
			case OPT_CLOCK_OFFSET_NS:
			case OPT_CLOCK_SECONDS:
			case OPT_COLOR:
			case OPT_DEBUG:
			case OPT_DEBUG_INFO:
			case OPT_DEBUG_INFO_DIR:
			case OPT_DEBUG_INFO_FULL_PATH:
			case OPT_DEBUG_INFO_TARGET_PREFIX:
			case OPT_END:
			case OPT_FIELDS:
			case OPT_INPUT_FORMAT:
			case OPT_NAMES:
			case OPT_NO_DELTA:
			case OPT_OUTPUT_FORMAT:
			case OPT_OUTPUT:
			case OPT_RUN_ARGS:
			case OPT_RUN_ARGS_0:
			case OPT_STREAM_INTERSECTION:
			case OPT_TIMERANGE:
			case OPT_VERBOSE:
				/* Ignore in this pass */
				break;
			default:
				BT_CLI_LOGE_APPEND_CAUSE("Unknown command-line option specified (option code %d).",
					argpar_item_opt->descr->id);
				goto error;
			}
		} else if (argpar_item->type == ARGPAR_ITEM_TYPE_NON_OPT) {
			struct argpar_item_non_opt *argpar_item_non_opt;
			bt_value_array_append_element_status append_status;

			current_item_type = CONVERT_CURRENT_ITEM_TYPE_NON_OPT;

			argpar_item_non_opt = (struct argpar_item_non_opt *) argpar_item;

			append_status = bt_value_array_append_string_element(non_opts,
				argpar_item_non_opt->arg);
			if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}

			append_status = bt_value_array_append_empty_array_element(
				non_opt_params, NULL);
			if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}

			append_status = bt_value_array_append_element(non_opt_loglevels, bt_value_null);
			if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}
		} else {
			bt_common_abort();
		}
	}

	/*
	 * Second pass: transform the convert-specific options and
	 * arguments into implicit component instances for the run
	 * command.
	 */
	for (i = 0; i < argpar_parse_ret.items->n_items; i++) {
		struct argpar_item *argpar_item =
			argpar_parse_ret.items->items[i];
		struct argpar_item_opt *argpar_item_opt;
		const char *arg;

		if (argpar_item->type != ARGPAR_ITEM_TYPE_OPT) {
			continue;
		}

		argpar_item_opt = (struct argpar_item_opt *) argpar_item;
		arg = argpar_item_opt->arg;

		switch (argpar_item_opt->descr->id) {
		case OPT_BEGIN:
			if (trimmer_has_begin) {
				BT_CLI_LOGE_APPEND_CAUSE("At --begin option: --begin or --timerange option already specified\n    %s\n",
					arg);
				goto error;
			}

			trimmer_has_begin = true;
			ret = append_implicit_component_extra_param(
				&implicit_trimmer_args, "begin", arg);
			implicit_trimmer_args.exists = true;
			if (ret) {
				goto error;
			}
			break;
		case OPT_END:
			if (trimmer_has_end) {
				BT_CLI_LOGE_APPEND_CAUSE("At --end option: --end or --timerange option already specified\n    %s\n",
					arg);
				goto error;
			}

			trimmer_has_end = true;
			ret = append_implicit_component_extra_param(
				&implicit_trimmer_args, "end", arg);
			implicit_trimmer_args.exists = true;
			if (ret) {
				goto error;
			}
			break;
		case OPT_TIMERANGE:
		{
			char *begin;
			char *end;

			if (trimmer_has_begin || trimmer_has_end) {
				BT_CLI_LOGE_APPEND_CAUSE("At --timerange option: --begin, --end, or --timerange option already specified\n    %s\n",
					arg);
				goto error;
			}

			ret = split_timerange(arg, &begin, &end);
			if (ret) {
				BT_CLI_LOGE_APPEND_CAUSE("Invalid --timerange option's argument: expecting BEGIN,END or [BEGIN,END]:\n    %s",
					arg);
				goto error;
			}

			ret = append_implicit_component_extra_param(
				&implicit_trimmer_args, "begin", begin);
			ret |= append_implicit_component_extra_param(
				&implicit_trimmer_args, "end", end);
			implicit_trimmer_args.exists = true;
			free(begin);
			free(end);
			if (ret) {
				goto error;
			}
			break;
		}
		case OPT_CLOCK_CYCLES:
			append_implicit_component_param(
				&implicit_text_args, "clock-cycles", "yes");
			implicit_text_args.exists = true;
			break;
		case OPT_CLOCK_DATE:
			append_implicit_component_param(
				&implicit_text_args, "clock-date", "yes");
			implicit_text_args.exists = true;
			break;
		case OPT_CLOCK_FORCE_CORRELATE:
			ctf_fs_source_force_clock_class_unix_epoch_origin = true;
			break;
		case OPT_CLOCK_GMT:
			append_implicit_component_param(
				&implicit_text_args, "clock-gmt", "yes");
			append_implicit_component_param(
				&implicit_trimmer_args, "gmt", "yes");
			implicit_text_args.exists = true;
			break;
		case OPT_CLOCK_OFFSET:
			if (ctf_fs_source_clock_class_offset_arg) {
				BT_CLI_LOGE_APPEND_CAUSE("Duplicate --clock-offset option\n");
				goto error;
			}

			ctf_fs_source_clock_class_offset_arg = g_strdup(arg);
			if (!ctf_fs_source_clock_class_offset_arg) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}
			break;
		case OPT_CLOCK_OFFSET_NS:
			if (ctf_fs_source_clock_class_offset_ns_arg) {
				BT_CLI_LOGE_APPEND_CAUSE("Duplicate --clock-offset-ns option\n");
				goto error;
			}

			ctf_fs_source_clock_class_offset_ns_arg = g_strdup(arg);
			if (!ctf_fs_source_clock_class_offset_ns_arg) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}
			break;
		case OPT_CLOCK_SECONDS:
			append_implicit_component_param(
				&implicit_text_args, "clock-seconds", "yes");
			implicit_text_args.exists = true;
			break;
		case OPT_COLOR:
			implicit_text_args.exists = true;
			ret = append_implicit_component_extra_param(
				&implicit_text_args, "color", arg);
			if (ret) {
				goto error;
			}
			break;
		case OPT_DEBUG_INFO:
			implicit_debug_info_args.exists = true;
			break;
		case OPT_DEBUG_INFO_DIR:
			implicit_debug_info_args.exists = true;
			ret = append_implicit_component_extra_param(
				&implicit_debug_info_args, "debug-info-dir", arg);
			if (ret) {
				goto error;
			}
			break;
		case OPT_DEBUG_INFO_FULL_PATH:
			implicit_debug_info_args.exists = true;
			append_implicit_component_param(
				&implicit_debug_info_args, "full-path", "yes");
			break;
		case OPT_DEBUG_INFO_TARGET_PREFIX:
			implicit_debug_info_args.exists = true;
			ret = append_implicit_component_extra_param(
				&implicit_debug_info_args,
				"target-prefix", arg);
			if (ret) {
				goto error;
			}
			break;
		case OPT_FIELDS:
		{
			bt_value *fields = fields_from_arg(arg);

			if (!fields) {
				goto error;
			}

			implicit_text_args.exists = true;
			ret = insert_flat_params_from_array(
				implicit_text_args.params_arg,
				fields, "field");
			bt_value_put_ref(fields);
			if (ret) {
				goto error;
			}
			break;
		}
		case OPT_NAMES:
		{
			bt_value *names = names_from_arg(arg);

			if (!names) {
				goto error;
			}

			implicit_text_args.exists = true;
			ret = insert_flat_params_from_array(
				implicit_text_args.params_arg,
				names, "name");
			bt_value_put_ref(names);
			if (ret) {
				goto error;
			}
			break;
		}
		case OPT_NO_DELTA:
			append_implicit_component_param(
				&implicit_text_args, "no-delta", "yes");
			implicit_text_args.exists = true;
			break;
		case OPT_INPUT_FORMAT:
			if (got_input_format_opt) {
				BT_CLI_LOGE_APPEND_CAUSE("Duplicate --input-format option.");
				goto error;
			}

			got_input_format_opt = true;

			if (strcmp(arg, "ctf") == 0) {
				auto_source_discovery_restrict_plugin_name = "ctf";
				auto_source_discovery_restrict_component_class_name = "fs";
			} else if (strcmp(arg, "lttng-live") == 0) {
				auto_source_discovery_restrict_plugin_name = "ctf";
				auto_source_discovery_restrict_component_class_name = "lttng-live";
				implicit_lttng_live_args.exists = true;
			} else {
				BT_CLI_LOGE_APPEND_CAUSE("Unknown legacy input format:\n    %s",
					arg);
				goto error;
			}
			break;
		case OPT_OUTPUT_FORMAT:
			if (got_output_format_opt) {
				BT_CLI_LOGE_APPEND_CAUSE("Duplicate --output-format option.");
				goto error;
			}

			got_output_format_opt = true;

			if (strcmp(arg, "text") == 0) {
				implicit_text_args.exists = true;
			} else if (strcmp(arg, "ctf") == 0) {
				implicit_ctf_output_args.exists = true;
			} else if (strcmp(arg, "dummy") == 0) {
				implicit_dummy_args.exists = true;
			} else if (strcmp(arg, "ctf-metadata") == 0) {
				print_ctf_metadata = true;
			} else {
				BT_CLI_LOGE_APPEND_CAUSE("Unknown legacy output format:\n    %s",
					arg);
				goto error;
			}
			break;
		case OPT_OUTPUT:
			if (output) {
				BT_CLI_LOGE_APPEND_CAUSE("Duplicate --output option");
				goto error;
			}

			output = strdup(arg);
			if (!output) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}
			break;
		case OPT_RUN_ARGS:
			if (print_run_args_0) {
				BT_CLI_LOGE_APPEND_CAUSE("Cannot specify --run-args and --run-args-0.");
				goto error;
			}

			print_run_args = true;
			break;
		case OPT_RUN_ARGS_0:
			if (print_run_args) {
				BT_CLI_LOGE_APPEND_CAUSE("Cannot specify --run-args and --run-args-0.");
				goto error;
			}

			print_run_args_0 = true;
			break;
		case OPT_STREAM_INTERSECTION:
			/*
			 * Applies to all traces implementing the
			 * babeltrace.trace-infos query.
			 */
			stream_intersection_mode = true;
			break;
		case OPT_VERBOSE:
			*default_log_level =
				logging_level_min(*default_log_level, BT_LOG_INFO);
			break;
		case OPT_DEBUG:
			*default_log_level =
				logging_level_min(*default_log_level, BT_LOG_TRACE);
			break;
		default:
			break;
		}
	}

	set_auto_log_levels(default_log_level);

	/*
	 * Legacy behaviour: --verbose used to make the `text` output
	 * format print more information. --verbose is now equivalent to
	 * the INFO log level, which is why we compare to `BT_LOG_INFO`
	 * here.
	 */
	if (*default_log_level == BT_LOG_INFO) {
		append_implicit_component_param(&implicit_text_args,
			"verbose", "yes");
	}

	/* Print CTF metadata or print LTTng live sessions */
	if (print_ctf_metadata) {
		const bt_value *bt_val_non_opt;

		if (bt_value_array_is_empty(non_opts)) {
			BT_CLI_LOGE_APPEND_CAUSE("--output-format=ctf-metadata specified without a path.");
			goto error;
		}

		if (bt_value_array_get_length(non_opts) > 1) {
			BT_CLI_LOGE_APPEND_CAUSE("Too many paths specified for --output-format=ctf-metadata.");
			goto error;
		}

		cfg = bt_config_print_ctf_metadata_create(plugin_paths);
		if (!cfg) {
			goto error;
		}

		bt_val_non_opt = bt_value_array_borrow_element_by_index_const(non_opts, 0);
		g_string_assign(cfg->cmd_data.print_ctf_metadata.path,
				bt_value_string_get(bt_val_non_opt));

		if (output) {
			g_string_assign(
				cfg->cmd_data.print_ctf_metadata.output_path,
				output);
		}

		goto end;
	}

	/*
	 * If -o ctf was specified, make sure an output path (--output)
	 * was also specified. --output does not imply -o ctf because
	 * it's also used for the default, implicit -o text if -o ctf
	 * is not specified.
	 */
	if (implicit_ctf_output_args.exists) {
		if (!output) {
			BT_CLI_LOGE_APPEND_CAUSE("--output-format=ctf specified without --output (trace output path).");
			goto error;
		}

		/*
		 * At this point we know that -o ctf AND --output were
		 * specified. Make sure that no options were specified
		 * which would imply -o text because --output would be
		 * ambiguous in this case. For example, this is wrong:
		 *
		 *     babeltrace2 --names=all -o ctf --output=/tmp/path my-trace
		 *
		 * because --names=all implies -o text, and --output
		 * could apply to both the sink.text.pretty and
		 * sink.ctf.fs implicit components.
		 */
		if (implicit_text_args.exists) {
			BT_CLI_LOGE_APPEND_CAUSE("Ambiguous --output option: --output-format=ctf specified but another option implies --output-format=text.");
			goto error;
		}
	}

	/*
	 * If -o dummy and -o ctf were not specified, and if there are
	 * no explicit sink components, then use an implicit
	 * `sink.text.pretty` component.
	 */
	if (!implicit_dummy_args.exists && !implicit_ctf_output_args.exists &&
			!sink_names) {
		implicit_text_args.exists = true;
	}

	/*
	 * Set implicit `sink.text.pretty` or `sink.ctf.fs` component's
	 * `path` parameter if --output was specified.
	 */
	if (output) {
		if (implicit_text_args.exists) {
			append_implicit_component_extra_param(&implicit_text_args,
				"path", output);
		} else if (implicit_ctf_output_args.exists) {
			append_implicit_component_extra_param(&implicit_ctf_output_args,
				"path", output);
		}
	}

	/* Decide where the non-option argument(s) go */
	if (bt_value_array_get_length(non_opts) > 0) {
		if (implicit_lttng_live_args.exists) {
			const bt_value *bt_val_non_opt;

			if (bt_value_array_get_length(non_opts) > 1) {
				BT_CLI_LOGE_APPEND_CAUSE("Too many URLs specified for --input-format=lttng-live.");
				goto error;
			}

			bt_val_non_opt = bt_value_array_borrow_element_by_index_const(non_opts, 0);
			lttng_live_url_parts =
				bt_common_parse_lttng_live_url(bt_value_string_get(bt_val_non_opt),
					error_buf, sizeof(error_buf));
			if (!lttng_live_url_parts.proto) {
				BT_CLI_LOGE_APPEND_CAUSE("Invalid LTTng live URL format: %s.",
					error_buf);
				goto error;
			}

			if (!lttng_live_url_parts.session_name) {
				/* Print LTTng live sessions */
				cfg = bt_config_print_lttng_live_sessions_create(
					plugin_paths);
				if (!cfg) {
					goto error;
				}

				g_string_assign(cfg->cmd_data.print_lttng_live_sessions.url,
					bt_value_string_get(bt_val_non_opt));

				if (output) {
					g_string_assign(
						cfg->cmd_data.print_lttng_live_sessions.output_path,
						output);
				}

				goto end;
			}

			live_inputs_array_val = bt_value_array_create();
			if (!live_inputs_array_val) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}

			if (bt_value_array_append_string_element(
					live_inputs_array_val,
					bt_value_string_get(bt_val_non_opt))) {
				BT_CLI_LOGE_APPEND_CAUSE_OOM();
				goto error;
			}

			ret = append_parameter_to_args(
				implicit_lttng_live_args.extra_params,
				"inputs", live_inputs_array_val);
			if (ret) {
				goto error;
			}

			ret = append_implicit_component_extra_param(
				&implicit_lttng_live_args,
				"session-not-found-action", "end");
			if (ret) {
				goto error;
			}
		} else {
			int status;
			size_t plugin_count;
			const bt_plugin **plugins;
			const bt_plugin *plugin;

			status = require_loaded_plugins(plugin_paths);
			if (status != 0) {
				goto error;
			}

			if (auto_source_discovery_restrict_plugin_name) {
				plugin_count = 1;
				plugin = borrow_loaded_plugin_by_name(auto_source_discovery_restrict_plugin_name);
				plugins = &plugin;
			} else {
				plugin_count = get_loaded_plugins_count();
				plugins = borrow_loaded_plugins();
			}

			status = auto_discover_source_components(non_opts, plugins, plugin_count,
				auto_source_discovery_restrict_component_class_name,
				*default_log_level, &auto_disc, interrupter);

			if (status != 0) {
				if (status == AUTO_SOURCE_DISCOVERY_STATUS_INTERRUPTED) {
					BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
						"Babeltrace CLI", "Automatic source discovery interrupted by the user");
				}
				goto error;
			}

			status = create_implicit_component_args_from_auto_discovered_sources(
				&auto_disc, non_opts, non_opt_params, non_opt_loglevels,
				discovered_source_args);
			if (status != 0) {
				goto error;
			}
		}
	}


	/*
	 * If --clock-force-correlated was given, apply it to any src.ctf.fs
	 * component.
	 */
	if (ctf_fs_source_force_clock_class_unix_epoch_origin) {
		int n;

		n = append_multiple_implicit_components_param(
			discovered_source_args, "source.ctf.fs", "force-clock-class-origin-unix-epoch",
			"yes");
		if (n == 0) {
			BT_CLI_LOGE_APPEND_CAUSE("--clock-force-correlate specified, but no source.ctf.fs component instantiated.");
			goto error;
		}
	}

	/* If --clock-offset was given, apply it to any src.ctf.fs component. */
	if (ctf_fs_source_clock_class_offset_arg) {
		int n;

		n = append_multiple_implicit_components_param(
			discovered_source_args, "source.ctf.fs", "clock-class-offset-s",
			ctf_fs_source_clock_class_offset_arg);

		if (n == 0) {
			BT_CLI_LOGE_APPEND_CAUSE("--clock-offset specified, but no source.ctf.fs component instantiated.");
			goto error;
		}
	}

	/* If --clock-offset-ns was given, apply it to any src.ctf.fs component. */
	if (ctf_fs_source_clock_class_offset_ns_arg) {
		int n;

		n = append_multiple_implicit_components_param(
			discovered_source_args, "source.ctf.fs", "clock-class-offset-ns",
			ctf_fs_source_clock_class_offset_ns_arg);

		if (n == 0) {
			BT_CLI_LOGE_APPEND_CAUSE("--clock-offset-ns specified, but no source.ctf.fs component instantiated.");
			goto error;
		}
	}

	/*
	 * If the implicit `source.ctf.lttng-live` component exists,
	 * make sure there's at least one non-option argument (which is
	 * the URL).
	 */
	if (implicit_lttng_live_args.exists && bt_value_array_is_empty(non_opts)) {
		BT_CLI_LOGE_APPEND_CAUSE("Missing URL for implicit `%s` component.",
			implicit_lttng_live_args.comp_arg->str);
		goto error;
	}

	/* Assign names to implicit components */
	for (i = 0; i < discovered_source_args->len; i++) {
		struct implicit_component_args *args;
		int j;

		args = discovered_source_args->pdata[i];

		g_string_printf(auto_disc_comp_name, "auto-disc-%s", args->comp_arg->str);

		/* Give it a name like `auto-disc-src-ctf-fs`. */
		for (j = 0; j < auto_disc_comp_name->len; j++) {
			if (auto_disc_comp_name->str[j] == '.') {
				auto_disc_comp_name->str[j] = '-';
			}
		}

		ret = assign_name_to_implicit_component(args,
			auto_disc_comp_name->str, all_names, &source_names, true);
		if (ret) {
			goto error;
		}
	}

	ret = assign_name_to_implicit_component(&implicit_lttng_live_args,
		"lttng-live", all_names, &source_names, true);
	if (ret) {
		goto error;
	}

	ret = assign_name_to_implicit_component(&implicit_text_args,
		"pretty", all_names, &sink_names, true);
	if (ret) {
		goto error;
	}

	ret = assign_name_to_implicit_component(&implicit_ctf_output_args,
		"sink-ctf-fs", all_names, &sink_names, true);
	if (ret) {
		goto error;
	}

	ret = assign_name_to_implicit_component(&implicit_dummy_args,
		"dummy", all_names, &sink_names, true);
	if (ret) {
		goto error;
	}

	ret = assign_name_to_implicit_component(&implicit_muxer_args,
		"muxer", all_names, NULL, false);
	if (ret) {
		goto error;
	}

	ret = assign_name_to_implicit_component(&implicit_trimmer_args,
		"trimmer", all_names, NULL, false);
	if (ret) {
		goto error;
	}

	ret = assign_name_to_implicit_component(&implicit_debug_info_args,
		"debug-info", all_names, NULL, false);
	if (ret) {
		goto error;
	}

	/* Make sure there's at least one source and one sink */
	if (!source_names) {
		BT_CLI_LOGE_APPEND_CAUSE("No source component.");
		goto error;
	}

	if (!sink_names) {
		BT_CLI_LOGE_APPEND_CAUSE("No sink component.");
		goto error;
	}

	/* Make sure there's a single sink component */
	if (g_list_length(sink_names) != 1) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"More than one sink component specified.");
		goto error;
	}

	/*
	 * Prepend the muxer, the trimmer, and the debug info to the
	 * filter chain so that we have:
	 *
	 *     sources -> muxer -> [trimmer] -> [debug info] ->
	 *                [user filters] -> sinks
	 */
	if (implicit_debug_info_args.exists) {
		if (g_list_prepend_gstring(&filter_names,
				implicit_debug_info_args.name_arg->str)) {
			goto error;
		}
	}

	if (implicit_trimmer_args.exists) {
		if (g_list_prepend_gstring(&filter_names,
				implicit_trimmer_args.name_arg->str)) {
			goto error;
		}
	}

	if (g_list_prepend_gstring(&filter_names,
			implicit_muxer_args.name_arg->str)) {
		goto error;
	}

	/*
	 * Append the equivalent run arguments for the implicit
	 * components.
	 */
	for (i = 0; i < discovered_source_args->len; i++) {
		struct implicit_component_args *args =
			discovered_source_args->pdata[i];

		ret = append_run_args_for_implicit_component(args, run_args);
		if (ret) {
			goto error;
		}
	}

	ret = append_run_args_for_implicit_component(&implicit_lttng_live_args,
		run_args);
	if (ret) {
		goto error;
	}

	ret = append_run_args_for_implicit_component(&implicit_text_args,
		run_args);
	if (ret) {
		goto error;
	}

	ret = append_run_args_for_implicit_component(&implicit_ctf_output_args,
		run_args);
	if (ret) {
		goto error;
	}

	ret = append_run_args_for_implicit_component(&implicit_dummy_args,
		run_args);
	if (ret) {
		goto error;
	}

	ret = append_run_args_for_implicit_component(&implicit_muxer_args,
		run_args);
	if (ret) {
		goto error;
	}

	ret = append_run_args_for_implicit_component(&implicit_trimmer_args,
		run_args);
	if (ret) {
		goto error;
	}

	ret = append_run_args_for_implicit_component(&implicit_debug_info_args,
		run_args);
	if (ret) {
		goto error;
	}

	/* Auto-connect components */
	ret = convert_auto_connect(run_args, source_names, filter_names,
			sink_names);
	if (ret) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot auto-connect components.");
		goto error;
	}

	/*
	 * We have all the run command arguments now. Depending on
	 * --run-args, we pass this to the run command or print them
	 * here.
	 */
	if (print_run_args || print_run_args_0) {
		uint64_t args_idx, args_len;
		if (stream_intersection_mode) {
			BT_CLI_LOGE_APPEND_CAUSE("Cannot specify --stream-intersection with --run-args or --run-args-0.");
			goto error;
		}

		args_len = bt_value_array_get_length(run_args);
		for (args_idx = 0; args_idx < args_len; args_idx++) {
			const bt_value *arg_value =
				bt_value_array_borrow_element_by_index(run_args,
					args_idx);
			const char *arg;
			GString *quoted = NULL;
			const char *arg_to_print;

			arg = bt_value_string_get(arg_value);

			if (print_run_args) {
				quoted = bt_common_shell_quote(arg, true);
				if (!quoted) {
					goto error;
				}

				arg_to_print = quoted->str;
			} else {
				arg_to_print = arg;
			}

			printf("%s", arg_to_print);

			if (quoted) {
				g_string_free(quoted, TRUE);
			}

			if (args_idx < args_len - 1) {
				if (print_run_args) {
					putchar(' ');
				} else {
					putchar('\0');
				}
			}
		}

		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	cfg = bt_config_run_from_args_array(run_args, retcode,
		plugin_paths, *default_log_level);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.run.stream_intersection_mode = stream_intersection_mode;
	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	argpar_parse_ret_fini(&argpar_parse_ret);

	free(output);

	if (component_arg_for_run) {
		g_string_free(component_arg_for_run, TRUE);
	}

	if (name_gstr) {
		g_string_free(name_gstr, TRUE);
	}

	bt_value_put_ref(live_inputs_array_val);
	bt_value_put_ref(run_args);
	bt_value_put_ref(all_names);
	destroy_glist_of_gstring(source_names);
	destroy_glist_of_gstring(filter_names);
	destroy_glist_of_gstring(sink_names);
	bt_value_put_ref(non_opt_params);
	bt_value_put_ref(non_opt_loglevels);
	bt_value_put_ref(non_opts);
	finalize_implicit_component_args(&implicit_ctf_output_args);
	finalize_implicit_component_args(&implicit_lttng_live_args);
	finalize_implicit_component_args(&implicit_dummy_args);
	finalize_implicit_component_args(&implicit_text_args);
	finalize_implicit_component_args(&implicit_debug_info_args);
	finalize_implicit_component_args(&implicit_muxer_args);
	finalize_implicit_component_args(&implicit_trimmer_args);
	bt_common_destroy_lttng_live_url_parts(&lttng_live_url_parts);
	auto_source_discovery_fini(&auto_disc);

	if (discovered_source_args) {
		g_ptr_array_free(discovered_source_args, TRUE);
	}

	g_free(ctf_fs_source_clock_class_offset_arg);
	g_free(ctf_fs_source_clock_class_offset_ns_arg);

	if (auto_disc_comp_name) {
		g_string_free(auto_disc_comp_name, TRUE);
	}

	return cfg;
}

/*
 * Prints the Babeltrace 2.x general usage.
 */
static
void print_gen_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace2 [GENERAL OPTIONS] [COMMAND] [COMMAND ARGUMENTS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "General options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -d, --debug          		 Enable debug mode (same as --log-level=T)\n");
	fprintf(fp, "  -h, --help           		 Show this help and quit\n");
	fprintf(fp, "  -l, --log-level=LVL  		 Set the default log level to LVL (`N`, `T`, `D`,\n");
	fprintf(fp, "                       		 `I`, `W` (default), `E`, or `F`)\n");
	fprintf(fp, "      --omit-home-plugin-path       Omit home plugins from plugin search path\n");
	fprintf(fp, "                                    (~/.local/lib/babeltrace2/plugins)\n");
	fprintf(fp, "      --omit-system-plugin-path     Omit system plugins from plugin search path\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Add PATH to the list of paths from which\n");
	fprintf(fp, "                                    dynamic plugins can be loaded\n");
	fprintf(fp, "  -v, --verbose        		 Enable verbose mode (same as --log-level=I)\n");
	fprintf(fp, "  -V, --version        		 Show version and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "Available commands:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    convert       Convert and trim traces (default)\n");
	fprintf(fp, "    help          Get help for a plugin or a component class\n");
	fprintf(fp, "    list-plugins  List available plugins and their content\n");
	fprintf(fp, "    query         Query objects from a component class\n");
	fprintf(fp, "    run           Build a processing graph and run it\n");
	fprintf(fp, "\n");
	fprintf(fp, "Use `babeltrace2 COMMAND --help` to show the help of COMMAND.\n");
}

struct bt_config *bt_config_cli_args_create(int argc, const char *argv[],
		int *retcode, bool omit_system_plugin_path,
		bool omit_home_plugin_path,
		const bt_value *initial_plugin_paths,
		const bt_interrupter *interrupter)
{
	struct bt_config *config = NULL;
	int i;
	int top_level_argc;
	const char **top_level_argv;
	int command_argc = -1;
	const char **command_argv = NULL;
	const char *command_name = NULL;
	int default_log_level = -1;
	struct argpar_parse_ret argpar_parse_ret = { 0 };
	bt_value *plugin_paths = NULL;

	/* Top-level option descriptions. */
	static const struct argpar_opt_descr descrs[] = {
		{ OPT_DEBUG, 'd', "debug", false },
		{ OPT_HELP, 'h', "help", false },
		{ OPT_LOG_LEVEL, 'l', "log-level", true },
		{ OPT_VERBOSE, 'v', "verbose", false },
		{ OPT_VERSION, 'V', "version", false},
		{ OPT_OMIT_HOME_PLUGIN_PATH, '\0', "omit-home-plugin-path", false },
		{ OPT_OMIT_SYSTEM_PLUGIN_PATH, '\0', "omit-system-plugin-path", false },
		{ OPT_PLUGIN_PATH, '\0', "plugin-path", true },
		ARGPAR_OPT_DESCR_SENTINEL
	};

	enum command_type {
		COMMAND_TYPE_NONE = -1,
		COMMAND_TYPE_RUN = 0,
		COMMAND_TYPE_CONVERT,
		COMMAND_TYPE_LIST_PLUGINS,
		COMMAND_TYPE_HELP,
		COMMAND_TYPE_QUERY,
	} command_type = COMMAND_TYPE_NONE;

	*retcode = -1;

	if (!initial_plugin_paths) {
		plugin_paths = bt_value_array_create();
		if (!plugin_paths) {
			goto error;
		}
	} else {
		bt_value_copy_status copy_status = bt_value_copy(
			initial_plugin_paths, &plugin_paths);
		if (copy_status) {
			goto error;
		}
	}

	BT_ASSERT(plugin_paths);

	/*
	 * The `BABELTRACE_PLUGIN_PATH` paths take precedence over the
	 * `--plugin-path` option's paths, so append it now before
	 * parsing the general options.
	 */
	if (append_env_var_plugin_paths(plugin_paths)) {
		goto error;
	}

	if (argc <= 1) {
		print_version();
		puts("");
		print_gen_usage(stdout);
		goto end;
	}

	/* Skip first argument, the name of the program. */
	top_level_argc = argc - 1;
	top_level_argv = argv + 1;
	argpar_parse_ret = argpar_parse(top_level_argc, top_level_argv,
		descrs, false);

	if (argpar_parse_ret.error) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"While parsing command-line arguments: %s",
			argpar_parse_ret.error);
		goto error;
	}

	for (i = 0; i < argpar_parse_ret.items->n_items; i++) {
		struct argpar_item *item;

		item = argpar_parse_ret.items->items[i];

		if (item->type == ARGPAR_ITEM_TYPE_OPT) {
			struct argpar_item_opt *item_opt =
				(struct argpar_item_opt *) item;

			switch (item_opt->descr->id) {
				case OPT_DEBUG:
					default_log_level =
						logging_level_min(default_log_level, BT_LOG_TRACE);
					break;
				case OPT_VERBOSE:
					default_log_level =
						logging_level_min(default_log_level, BT_LOG_INFO);
					break;
				case OPT_LOG_LEVEL:
				{
					int level = bt_log_get_level_from_string(item_opt->arg);

					if (level < 0) {
						BT_CLI_LOGE_APPEND_CAUSE(
							"Invalid argument for --log-level option:\n    %s",
							item_opt->arg);
						goto error;
					}

					default_log_level =
						logging_level_min(default_log_level, level);
					break;
				}
				case OPT_PLUGIN_PATH:
					if (bt_config_append_plugin_paths_check_setuid_setgid(
							plugin_paths, item_opt->arg)) {
						goto error;
					}
					break;
				case OPT_OMIT_SYSTEM_PLUGIN_PATH:
					omit_system_plugin_path = true;
					break;
				case OPT_OMIT_HOME_PLUGIN_PATH:
					omit_home_plugin_path = true;
					break;
				case OPT_VERSION:
					print_version();
					goto end;
				case OPT_HELP:
					print_gen_usage(stdout);
					goto end;
			}
		} else if (item->type == ARGPAR_ITEM_TYPE_NON_OPT) {
			struct argpar_item_non_opt *item_non_opt =
				(struct argpar_item_non_opt *) item;
			/*
			 * First unknown argument: is it a known command
			 * name?
			 */
			command_argc =
				top_level_argc - item_non_opt->orig_index - 1;
			command_argv =
				&top_level_argv[item_non_opt->orig_index + 1];

			if (strcmp(item_non_opt->arg, "convert") == 0) {
				command_type = COMMAND_TYPE_CONVERT;
				command_name = "convert";
			} else if (strcmp(item_non_opt->arg, "list-plugins") == 0) {
				command_type = COMMAND_TYPE_LIST_PLUGINS;
				command_name = "list-plugins";
			} else if (strcmp(item_non_opt->arg, "help") == 0) {
				command_type = COMMAND_TYPE_HELP;
				command_name = "help";
			} else if (strcmp(item_non_opt->arg, "query") == 0) {
				command_type = COMMAND_TYPE_QUERY;
				command_name = "query";
			} else if (strcmp(item_non_opt->arg, "run") == 0) {
				command_type = COMMAND_TYPE_RUN;
				command_name = "run";
			} else {
				/*
				 * Non-option argument, but not a known
				 * command name: assume the default
				 * `convert` command.
				 */
				command_type = COMMAND_TYPE_CONVERT;
				command_name = "convert";
				command_argc++;
				command_argv--;
			}
			break;
		}
	}

	if (command_type == COMMAND_TYPE_NONE) {
		if (argpar_parse_ret.ingested_orig_args == top_level_argc) {
			/*
			 * We only got non-help, non-version general options
			 * like --verbose and --debug, without any other
			 * arguments, so we can't do anything useful: print the
			 * usage and quit.
			 */
			print_gen_usage(stdout);
			goto end;
		}

		/*
		 * We stopped on an unknown option argument (and therefore
		 * didn't see a command name). Assume `convert` command.
		 */
		command_type = COMMAND_TYPE_CONVERT;
		command_name = "convert";
		command_argc =
			top_level_argc - argpar_parse_ret.ingested_orig_args;
		command_argv =
			&top_level_argv[argpar_parse_ret.ingested_orig_args];
	}

	BT_ASSERT(command_argv);
	BT_ASSERT(command_argc >= 0);

	/*
	 * For all commands other than `convert`, we now know the log level to
	 * use, so we can apply it with `set_auto_log_levels`.
	 *
	 * The convert command has `--debug` and `--verbose` arguments that are
	 * equivalent to the top-level arguments of the same name.  So after it
	 * has parsed its arguments, `bt_config_convert_from_args` calls
	 * `set_auto_log_levels` itself.
	 */
	if (command_type != COMMAND_TYPE_CONVERT) {
		set_auto_log_levels(&default_log_level);
	}

	/*
	 * At this point, `plugin_paths` contains the initial plugin
	 * paths, the paths from the `BABELTRACE_PLUGIN_PATH` paths, and
	 * the paths from the `--plugin-path` option.
	 *
	 * Now append the user and system plugin paths.
	 */
	if (append_home_and_system_plugin_paths(plugin_paths,
			omit_system_plugin_path, omit_home_plugin_path)) {
		goto error;
	}

	switch (command_type) {
	case COMMAND_TYPE_RUN:
		config = bt_config_run_from_args(command_argc, command_argv,
			retcode, plugin_paths,
			default_log_level);
		break;
	case COMMAND_TYPE_CONVERT:
		config = bt_config_convert_from_args(command_argc, command_argv,
			retcode, plugin_paths, &default_log_level, interrupter);
		break;
	case COMMAND_TYPE_LIST_PLUGINS:
		config = bt_config_list_plugins_from_args(command_argc,
			command_argv, retcode, plugin_paths);
		break;
	case COMMAND_TYPE_HELP:
		config = bt_config_help_from_args(command_argc,
			command_argv, retcode, plugin_paths,
			default_log_level);
		break;
	case COMMAND_TYPE_QUERY:
		config = bt_config_query_from_args(command_argc,
			command_argv, retcode, plugin_paths,
			default_log_level);
		break;
	default:
		bt_common_abort();
	}

	if (config) {
		BT_ASSERT(default_log_level >= BT_LOG_TRACE);
		config->log_level = default_log_level;
		config->command_name = command_name;
	}

	goto end;

error:
	*retcode = 1;

end:
	argpar_parse_ret_fini(&argpar_parse_ret);
	bt_value_put_ref(plugin_paths);
	return config;
}
