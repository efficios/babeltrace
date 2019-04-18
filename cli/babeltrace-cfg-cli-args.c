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

#define BT_LOG_TAG "CLI-CFG-CLI-ARGS"
#include "logging.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <babeltrace/assert-internal.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include <popt.h>
#include <glib.h>
#include <sys/types.h>
#include "babeltrace-cfg.h"
#include "babeltrace-cfg-cli-args.h"
#include "babeltrace-cfg-cli-args-connect.h"

/*
 * Error printf() macro which prepends "Error: " the first time it's
 * called. This gives a nicer feel than having a bunch of error prefixes
 * (since the following lines usually describe the error and possible
 * solutions), or the error prefix just at the end.
 */
#define printf_err(fmt, args...)					\
	do {								\
		if (is_first_error) {					\
			fprintf(stderr, "Command line error: ");	\
			is_first_error = false;				\
		}							\
		fprintf(stderr, fmt, ##args);				\
	} while (0)

static bool is_first_error = true;

/* INI-style parsing FSM states */
enum ini_parsing_fsm_state {
	/* Expect a map key (identifier) */
	INI_EXPECT_MAP_KEY,

	/* Expect an equal character ('=') */
	INI_EXPECT_EQUAL,

	/* Expect a value */
	INI_EXPECT_VALUE,

	/* Expect a negative number value */
	INI_EXPECT_VALUE_NUMBER_NEG,

	/* Expect a comma character (',') */
	INI_EXPECT_COMMA,
};

/* INI-style parsing state variables */
struct ini_parsing_state {
	/* Lexical scanner (owned by this) */
	GScanner *scanner;

	/* Output map value object being filled (owned by this) */
	bt_value *params;

	/* Next expected FSM state */
	enum ini_parsing_fsm_state expecting;

	/* Last decoded map key (owned by this) */
	char *last_map_key;

	/* Complete INI-style string to parse (not owned by this) */
	const char *arg;

	/* Error buffer (not owned by this) */
	GString *ini_error;
};

/* Offset option with "is set" boolean */
struct offset_opt {
	int64_t value;
	bool is_set;
};

/* Legacy "ctf"/"lttng-live" format options */
struct ctf_legacy_opts {
	struct offset_opt offset_s;
	struct offset_opt offset_ns;
	bool stream_intersection;
};

/* Legacy "text" format options */
struct text_legacy_opts {
	/*
	 * output, dbg_info_dir, dbg_info_target_prefix, names,
	 * and fields are owned by this.
	 */
	GString *output;
	GString *dbg_info_dir;
	GString *dbg_info_target_prefix;
	const bt_value *names;
	const bt_value *fields;

	/* Flags */
	bool no_delta;
	bool clock_cycles;
	bool clock_seconds;
	bool clock_date;
	bool clock_gmt;
	bool dbg_info_full_path;
	bool verbose;
};

/* Legacy input format format */
enum legacy_input_format {
	LEGACY_INPUT_FORMAT_NONE = 0,
	LEGACY_INPUT_FORMAT_CTF,
	LEGACY_INPUT_FORMAT_LTTNG_LIVE,
};

/* Legacy output format format */
enum legacy_output_format {
	LEGACY_OUTPUT_FORMAT_NONE = 0,
	LEGACY_OUTPUT_FORMAT_TEXT,
	LEGACY_OUTPUT_FORMAT_DUMMY,
};

/*
 * Prints the "out of memory" error.
 */
static
void print_err_oom(void)
{
	printf_err("Out of memory\n");
}

/*
 * Appends an "expecting token" error to the INI-style parsing state's
 * error buffer.
 */
static
void ini_append_error_expecting(struct ini_parsing_state *state,
		GScanner *scanner, const char *expecting)
{
	size_t i;
	size_t pos;

	g_string_append_printf(state->ini_error, "Expecting %s:\n", expecting);

	/* Only print error if there's one line */
	if (strchr(state->arg, '\n') != NULL || strlen(state->arg) == 0) {
		return;
	}

	g_string_append_printf(state->ini_error, "\n    %s\n", state->arg);
	pos = g_scanner_cur_position(scanner) + 4;

	if (!g_scanner_eof(scanner)) {
		pos--;
	}

	for (i = 0; i < pos; ++i) {
		g_string_append_printf(state->ini_error, " ");
	}

	g_string_append_printf(state->ini_error, "^\n\n");
}

static
int ini_handle_state(struct ini_parsing_state *state)
{
	int ret = 0;
	GTokenType token_type;
	bt_value *value = NULL;

	token_type = g_scanner_get_next_token(state->scanner);
	if (token_type == G_TOKEN_EOF) {
		if (state->expecting != INI_EXPECT_COMMA) {
			switch (state->expecting) {
			case INI_EXPECT_EQUAL:
				ini_append_error_expecting(state,
					state->scanner, "'='");
				break;
			case INI_EXPECT_VALUE:
			case INI_EXPECT_VALUE_NUMBER_NEG:
				ini_append_error_expecting(state,
					state->scanner, "value");
				break;
			case INI_EXPECT_MAP_KEY:
				ini_append_error_expecting(state,
					state->scanner, "unquoted map key");
				break;
			default:
				break;
			}
			goto error;
		}

		/* We're done! */
		ret = 1;
		goto success;
	}

	switch (state->expecting) {
	case INI_EXPECT_MAP_KEY:
		if (token_type != G_TOKEN_IDENTIFIER) {
			ini_append_error_expecting(state, state->scanner,
				"unquoted map key");
			goto error;
		}

		free(state->last_map_key);
		state->last_map_key =
			strdup(state->scanner->value.v_identifier);
		if (!state->last_map_key) {
			g_string_append(state->ini_error,
				"Out of memory\n");
			goto error;
		}

		if (bt_value_map_has_entry(state->params,
					   state->last_map_key)) {
			g_string_append_printf(state->ini_error,
				"Duplicate parameter key: `%s`\n",
				state->last_map_key);
			goto error;
		}

		state->expecting = INI_EXPECT_EQUAL;
		goto success;
	case INI_EXPECT_EQUAL:
		if (token_type != G_TOKEN_CHAR) {
			ini_append_error_expecting(state,
				state->scanner, "'='");
			goto error;
		}

		if (state->scanner->value.v_char != '=') {
			ini_append_error_expecting(state,
				state->scanner, "'='");
			goto error;
		}

		state->expecting = INI_EXPECT_VALUE;
		goto success;
	case INI_EXPECT_VALUE:
	{
		switch (token_type) {
		case G_TOKEN_CHAR:
			if (state->scanner->value.v_char == '-') {
				/* Negative number */
				state->expecting =
					INI_EXPECT_VALUE_NUMBER_NEG;
				goto success;
			} else {
				ini_append_error_expecting(state,
					state->scanner, "value");
				goto error;
			}
			break;
		case G_TOKEN_INT:
		{
			/* Positive integer */
			uint64_t int_val = state->scanner->value.v_int64;

			if (int_val > (1ULL << 63) - 1) {
				g_string_append_printf(state->ini_error,
					"Integer value %" PRIu64 " is outside the range of a 64-bit signed integer\n",
					int_val);
				goto error;
			}

			value = bt_value_integer_create_init((int64_t)int_val);
			break;
		}
		case G_TOKEN_FLOAT:
			/* Positive floating point number */
			value = bt_value_real_create_init(state->scanner->value.v_float);
			break;
		case G_TOKEN_STRING:
			/* Quoted string */
			value = bt_value_string_create_init(state->scanner->value.v_string);
			break;
		case G_TOKEN_IDENTIFIER:
		{
			/*
			 * Using symbols would be appropriate here,
			 * but said symbols are allowed as map key,
			 * so it's easier to consider everything an
			 * identifier.
			 *
			 * If one of the known symbols is not
			 * recognized here, then fall back to creating
			 * a string value.
			 */
			const char *id = state->scanner->value.v_identifier;

			if (!strcmp(id, "null") || !strcmp(id, "NULL") ||
					!strcmp(id, "nul")) {
				value = bt_value_null;
			} else if (!strcmp(id, "true") || !strcmp(id, "TRUE") ||
					!strcmp(id, "yes") ||
					!strcmp(id, "YES")) {
				value = bt_value_bool_create_init(true);
			} else if (!strcmp(id, "false") ||
					!strcmp(id, "FALSE") ||
					!strcmp(id, "no") ||
					!strcmp(id, "NO")) {
				value = bt_value_bool_create_init(false);
			} else {
				value = bt_value_string_create_init(id);
			}
			break;
		}
		default:
			/* Unset value variable will trigger the error */
			break;
		}

		if (!value) {
			ini_append_error_expecting(state,
				state->scanner, "value");
			goto error;
		}

		state->expecting = INI_EXPECT_COMMA;
		goto success;
	}
	case INI_EXPECT_VALUE_NUMBER_NEG:
	{
		switch (token_type) {
		case G_TOKEN_INT:
		{
			/* Negative integer */
			uint64_t int_val = state->scanner->value.v_int64;

			if (int_val > (1ULL << 63) - 1) {
				g_string_append_printf(state->ini_error,
					"Integer value -%" PRIu64 " is outside the range of a 64-bit signed integer\n",
					int_val);
				goto error;
			}

			value = bt_value_integer_create_init(-((int64_t)int_val));
			break;
		}
		case G_TOKEN_FLOAT:
			/* Negative floating point number */
			value = bt_value_real_create_init(-state->scanner->value.v_float);
			break;
		default:
			/* Unset value variable will trigger the error */
			break;
		}

		if (!value) {
			ini_append_error_expecting(state,
				state->scanner, "value");
			goto error;
		}

		state->expecting = INI_EXPECT_COMMA;
		goto success;
	}
	case INI_EXPECT_COMMA:
		if (token_type != G_TOKEN_CHAR) {
			ini_append_error_expecting(state,
				state->scanner, "','");
			goto error;
		}

		if (state->scanner->value.v_char != ',') {
			ini_append_error_expecting(state,
				state->scanner, "','");
			goto error;
		}

		state->expecting = INI_EXPECT_MAP_KEY;
		goto success;
	default:
		abort();
	}

error:
	ret = -1;
	goto end;

success:
	if (value) {
		if (bt_value_map_insert_entry(state->params,
				state->last_map_key, value)) {
			/* Only override return value on error */
			ret = -1;
		}
	}

end:
	BT_VALUE_PUT_REF_AND_RESET(value);
	return ret;
}

/*
 * Converts an INI-style argument to an equivalent map value object.
 *
 * Return value is owned by the caller.
 */
static
bt_value *bt_value_from_ini(const char *arg,
		GString *ini_error)
{
	/* Lexical scanner configuration */
	GScannerConfig scanner_config = {
		/* Skip whitespaces */
		.cset_skip_characters = " \t\n",

		/* Identifier syntax is: [a-zA-Z_][a-zA-Z0-9_.:-]* */
		.cset_identifier_first =
			G_CSET_a_2_z
			"_"
			G_CSET_A_2_Z,
		.cset_identifier_nth =
			G_CSET_a_2_z
			"_0123456789-.:"
			G_CSET_A_2_Z,

		/* "hello" and "Hello" two different keys */
		.case_sensitive = TRUE,

		/* No comments */
		.cpair_comment_single = NULL,
		.skip_comment_multi = TRUE,
		.skip_comment_single = TRUE,
		.scan_comment_multi = FALSE,

		/*
		 * Do scan identifiers, including 1-char identifiers,
		 * but NULL is a normal identifier.
		 */
		.scan_identifier = TRUE,
		.scan_identifier_1char = TRUE,
		.scan_identifier_NULL = FALSE,

		/*
		 * No specific symbols: null and boolean "symbols" are
		 * scanned as plain identifiers.
		 */
		.scan_symbols = FALSE,
		.symbol_2_token = FALSE,
		.scope_0_fallback = FALSE,

		/*
		 * Scan "0b"-, "0"-, and "0x"-prefixed integers, but not
		 * integers prefixed with "$".
		 */
		.scan_binary = TRUE,
		.scan_octal = TRUE,
		.scan_float = TRUE,
		.scan_hex = TRUE,
		.scan_hex_dollar = FALSE,

		/* Convert scanned numbers to integer tokens */
		.numbers_2_int = TRUE,

		/* Support both integers and floating-point numbers */
		.int_2_float = FALSE,

		/* Scan integers as 64-bit signed integers */
		.store_int64 = TRUE,

		/* Only scan double-quoted strings */
		.scan_string_sq = FALSE,
		.scan_string_dq = TRUE,

		/* Do not converter identifiers to string tokens */
		.identifier_2_string = FALSE,

		/* Scan characters as G_TOKEN_CHAR token */
		.char_2_token = FALSE,
	};
	struct ini_parsing_state state = {
		.scanner = NULL,
		.params = NULL,
		.expecting = INI_EXPECT_MAP_KEY,
		.arg = arg,
		.ini_error = ini_error,
	};

	state.params = bt_value_map_create();
	if (!state.params) {
		goto error;
	}

	state.scanner = g_scanner_new(&scanner_config);
	if (!state.scanner) {
		goto error;
	}

	/* Let the scan begin */
	g_scanner_input_text(state.scanner, arg, strlen(arg));

	while (true) {
		int ret = ini_handle_state(&state);

		if (ret < 0) {
			/* Error */
			goto error;
		} else if (ret > 0) {
			/* Done */
			break;
		}
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(state.params);

end:
	if (state.scanner) {
		g_scanner_destroy(state.scanner);
	}

	free(state.last_map_key);
	return state.params;
}

/*
 * Returns the parameters map value object from a command-line
 * parameter option's argument.
 *
 * Return value is owned by the caller.
 */
static
bt_value *bt_value_from_arg(const char *arg)
{
	bt_value *params = NULL;
	GString *ini_error = NULL;

	ini_error = g_string_new(NULL);
	if (!ini_error) {
		print_err_oom();
		goto end;
	}

	/* Try INI-style parsing */
	params = bt_value_from_ini(arg, ini_error);
	if (!params) {
		printf_err("%s", ini_error->str);
		goto end;
	}

end:
	if (ini_error) {
		g_string_free(ini_error, TRUE);
	}

	return params;
}

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
		printf_err("Argument contains a non-printable character\n");
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
		printf_err("Missing component class type (`source`, `filter`, or `sink`)\n");
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
		printf_err("Unknown component class type: `%s`\n",
			gs_comp_cls_type->str);
		goto error;
	}

	at += end_pos + 1;

	/* Parse the plugin name */
	gs_plugin = bt_common_string_until(at, ".:\\", ".", &end_pos);
	if (!gs_plugin || gs_plugin->len == 0 || at[end_pos] == '\0') {
		printf_err("Missing plugin or component class name\n");
		goto error;
	}

	at += end_pos + 1;

	/* Parse the component class name */
	gs_comp_cls = bt_common_string_until(at, ".:\\", ".", &end_pos);
	if (!gs_comp_cls || gs_comp_cls->len == 0) {
		printf_err("Missing component class name\n");
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

/*
 * Prints the Babeltrace version.
 */
static
void print_version(void)
{
	puts("Babeltrace " VERSION);
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
		const char *plugin_name, const char *comp_cls_name)
{
	struct bt_config_component *cfg_component = NULL;

	cfg_component = g_new0(struct bt_config_component, 1);
	if (!cfg_component) {
		print_err_oom();
		goto error;
	}

	bt_object_init_shared(&cfg_component->base,
		bt_config_component_destroy);
	cfg_component->type = type;
	cfg_component->plugin_name = g_string_new(plugin_name);
	if (!cfg_component->plugin_name) {
		print_err_oom();
		goto error;
	}

	cfg_component->comp_cls_name = g_string_new(comp_cls_name);
	if (!cfg_component->comp_cls_name) {
		print_err_oom();
		goto error;
	}

	cfg_component->instance_name = g_string_new(NULL);
	if (!cfg_component->instance_name) {
		print_err_oom();
		goto error;
	}

	/* Start with empty parameters */
	cfg_component->params = bt_value_map_create();
	if (!cfg_component->params) {
		print_err_oom();
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
struct bt_config_component *bt_config_component_from_arg(const char *arg)
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

	cfg_comp = bt_config_component_create(type, plugin_name, comp_cls_name);
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
		abort();
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

	for (at = list; at != NULL; at = g_list_next(at)) {
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
		.cset_skip_characters = " \t\n",
		.cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z "_",
		.cset_identifier_nth = G_CSET_a_2_z G_CSET_A_2_Z ":_-",
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
		print_err_oom();
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
		print_err_oom();
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

			if (!strcmp(identifier, "payload") ||
					!strcmp(identifier, "args") ||
					!strcmp(identifier, "arg")) {
				found_item = true;
				if (bt_value_array_append_string_element(names,
						"payload")) {
					goto error;
				}
			} else if (!strcmp(identifier, "context") ||
					!strcmp(identifier, "ctx")) {
				found_item = true;
				if (bt_value_array_append_string_element(names,
						"context")) {
					goto error;
				}
			} else if (!strcmp(identifier, "scope") ||
					!strcmp(identifier, "header")) {
				found_item = true;
				if (bt_value_array_append_string_element(names,
						identifier)) {
					goto error;
				}
			} else if (!strcmp(identifier, "all")) {
				found_all = true;
				if (bt_value_array_append_string_element(names,
						identifier)) {
					goto error;
				}
			} else if (!strcmp(identifier, "none")) {
				found_none = true;
				if (bt_value_array_append_string_element(names,
						identifier)) {
					goto error;
				}
			} else {
				printf_err("Unknown name: `%s`\n",
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
		printf_err("Only either `all` or `none` can be specified in the list given to the --names option, but not both.\n");
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
		print_err_oom();
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

			if (!strcmp(identifier, "trace") ||
					!strcmp(identifier, "trace:hostname") ||
					!strcmp(identifier, "trace:domain") ||
					!strcmp(identifier, "trace:procname") ||
					!strcmp(identifier, "trace:vpid") ||
					!strcmp(identifier, "loglevel") ||
					!strcmp(identifier, "emf") ||
					!strcmp(identifier, "callsite") ||
					!strcmp(identifier, "all")) {
				if (bt_value_array_append_string_element(fields,
						identifier)) {
					goto error;
				}
			} else {
				printf_err("Unknown field: `%s`\n",
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
	int i;
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
		print_err_oom();
		ret = -1;
		goto end;
	}

	default_value = g_string_new(NULL);
	if (!default_value) {
		print_err_oom();
		ret = -1;
		goto end;
	}

	for (i = 0; i < bt_value_array_get_size(names_array); i++) {
		const bt_value *str_obj =
			bt_value_array_borrow_element_by_index_const(names_array,
								     i);
		const char *suffix;
		bool is_default = false;

		if (!str_obj) {
			printf_err("Unexpected error\n");
			ret = -1;
			goto end;
		}

		suffix = bt_value_string_get(str_obj);

		g_string_assign(tmpstr, prefix);
		g_string_append(tmpstr, "-");

		/* Special-case for "all" and "none". */
		if (!strcmp(suffix, "all")) {
			is_default = true;
			g_string_assign(default_value, "show");
		} else if (!strcmp(suffix, "none")) {
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

/* popt options */
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
	OPT_KEY,
	OPT_LIST,
	OPT_NAME,
	OPT_NAMES,
	OPT_NO_DELTA,
	OPT_OMIT_HOME_PLUGIN_PATH,
	OPT_OMIT_SYSTEM_PLUGIN_PATH,
	OPT_OUTPUT,
	OPT_OUTPUT_FORMAT,
	OPT_PARAMS,
	OPT_PATH,
	OPT_PLUGIN_PATH,
	OPT_RESET_BASE_PARAMS,
	OPT_RETRY_DURATION,
	OPT_RUN_ARGS,
	OPT_RUN_ARGS_0,
	OPT_STREAM_INTERSECTION,
	OPT_TIMERANGE,
	OPT_URL,
	OPT_VALUE,
	OPT_VERBOSE,
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
		abort();
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
		printf_err("Found an unnamed component\n");
		ret = -1;
		goto end;
	}

	if (bt_value_map_has_entry(instance_names,
				   cfg_comp->instance_name->str)) {
		printf_err("Duplicate component instance name:\n    %s\n",
			cfg_comp->instance_name->str);
		ret = -1;
		goto end;
	}

	if (bt_value_map_insert_entry(instance_names,
			cfg_comp->instance_name->str, bt_value_null)) {
		print_err_oom();
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
		printf_err("Cannot append plugin paths from BABELTRACE_PLUGIN_PATH\n");
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
			char *home_plugin_dir =
				bt_common_get_home_plugin_path();

			if (home_plugin_dir) {
				ret = bt_config_append_plugin_paths(
					plugin_paths, home_plugin_dir);
				free(home_plugin_dir);

				if (ret) {
					printf_err("Invalid home plugin path\n");
					goto error;
				}
			}
		}
	}

	if (!omit_system_plugin_path) {
		if (bt_config_append_plugin_paths(plugin_paths,
				bt_common_get_system_plugin_path())) {
			printf_err("Invalid system plugin path\n");
			goto error;
		}
	}
	return 0;
error:
	printf_err("Cannot append home and system plugin paths\n");
	return -1;
}

static
int append_home_and_system_plugin_paths_cfg(struct bt_config *cfg)
{
	return append_home_and_system_plugin_paths(cfg->plugin_paths,
		cfg->omit_system_plugin_path, cfg->omit_home_plugin_path);
}

static
struct bt_config *bt_config_base_create(enum bt_config_command command,
		const bt_value *initial_plugin_paths,
		bool needs_plugins)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = g_new0(struct bt_config, 1);
	if (!cfg) {
		print_err_oom();
		goto error;
	}

	bt_object_init_shared(&cfg->base, bt_config_destroy);
	cfg->command = command;
	cfg->command_needs_plugins = needs_plugins;

	if (initial_plugin_paths) {
		bt_value *initial_plugin_paths_copy;

		(void) bt_value_copy(initial_plugin_paths,
			&initial_plugin_paths_copy);
		cfg->plugin_paths = initial_plugin_paths_copy;
	} else {
		cfg->plugin_paths = bt_value_array_create();
		if (!cfg->plugin_paths) {
			print_err_oom();
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
struct bt_config *bt_config_run_create(
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_RUN,
		initial_plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.run.sources = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!cfg->cmd_data.run.sources) {
		print_err_oom();
		goto error;
	}

	cfg->cmd_data.run.filters = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!cfg->cmd_data.run.filters) {
		print_err_oom();
		goto error;
	}

	cfg->cmd_data.run.sinks = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!cfg->cmd_data.run.sinks) {
		print_err_oom();
		goto error;
	}

	cfg->cmd_data.run.connections = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_config_connection_destroy);
	if (!cfg->cmd_data.run.connections) {
		print_err_oom();
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_list_plugins_create(
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_LIST_PLUGINS,
		initial_plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	return cfg;
}

static
struct bt_config *bt_config_help_create(
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_HELP,
		initial_plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.help.cfg_component =
		bt_config_component_create(-1, NULL, NULL);
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
struct bt_config *bt_config_query_create(
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_QUERY,
		initial_plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.query.object = g_string_new(NULL);
	if (!cfg->cmd_data.query.object) {
		print_err_oom();
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
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_PRINT_CTF_METADATA,
		initial_plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.print_ctf_metadata.path = g_string_new(NULL);
	if (!cfg->cmd_data.print_ctf_metadata.path) {
		print_err_oom();
		goto error;
	}

	cfg->cmd_data.print_ctf_metadata.output_path = g_string_new(NULL);
	if (!cfg->cmd_data.print_ctf_metadata.output_path) {
		print_err_oom();
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
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg;

	/* Create config */
	cfg = bt_config_base_create(BT_CONFIG_COMMAND_PRINT_LTTNG_LIVE_SESSIONS,
		initial_plugin_paths, true);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.print_lttng_live_sessions.url = g_string_new(NULL);
	if (!cfg->cmd_data.print_lttng_live_sessions.url) {
		print_err_oom();
		goto error;
	}

	cfg->cmd_data.print_lttng_live_sessions.output_path =
		g_string_new(NULL);
	if (!cfg->cmd_data.print_lttng_live_sessions.output_path) {
		print_err_oom();
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
		printf_err("Invalid --plugin-path option's argument:\n    %s\n",
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
	fprintf(fp, "  (`0x` prefix) signed 64-bit integer.\n");
	fprintf(fp, "* Double precision floating point number (scientific notation is accepted).\n");
	fprintf(fp, "* Unquoted string with no special characters, and not matching any of\n");
	fprintf(fp, "  the null and boolean value symbols above.\n");
	fprintf(fp, "* Double-quoted string (accepts escape characters).\n");
	fprintf(fp, "\n");
	fprintf(fp, "You can put whitespaces allowed around individual `=` and `,` symbols.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Example:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    many=null, fresh=yes, condition=false, squirrel=-782329,\n");
	fprintf(fp, "    observe=3.14, simple=beef, needs-quotes=\"some string\",\n");
	fprintf(fp, "    escape.chars-are:allowed=\"this is a \\\" double quote\"\n");
	fprintf(fp, "\n");
	fprintf(fp, "IMPORTANT: Make sure to single-quote the whole argument when you run\n");
	fprintf(fp, "babeltrace from a shell.\n");
}


/*
 * Prints the help command usage.
 */
static
void print_help_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [GENERAL OPTIONS] help [OPTIONS] PLUGIN\n");
	fprintf(fp, "       babeltrace [GENERAL OPTIONS] help [OPTIONS] TYPE.PLUGIN.CLS\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --omit-home-plugin-path       Omit home plugins from plugin search path\n");
	fprintf(fp, "                                    (~/.local/lib/babeltrace/plugins)\n");
	fprintf(fp, "      --omit-system-plugin-path     Omit system plugins from plugin search path\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Add PATH to the list of paths from which\n");
	fprintf(fp, "                                    dynamic plugins can be loaded\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace --help` for the list of general options.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Use `babeltrace list-plugins` to show the list of available plugins.\n");
}

static
struct poptOption help_long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "omit-home-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_HOME_PLUGIN_PATH, NULL, NULL },
	{ "omit-system-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_SYSTEM_PLUGIN_PATH, NULL, NULL },
	{ "plugin-path", '\0', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ NULL, 0, '\0', NULL, 0, NULL, NULL },
};

/*
 * Creates a Babeltrace config object from the arguments of a help
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_help_from_args(int argc, const char *argv[],
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths)
{
	poptContext pc = NULL;
	char *arg = NULL;
	int opt;
	int ret;
	struct bt_config *cfg = NULL;
	const char *leftover;
	char *plugin_name = NULL, *comp_cls_name = NULL;

	*retcode = 0;
	cfg = bt_config_help_create(initial_plugin_paths);
	if (!cfg) {
		goto error;
	}

	cfg->omit_system_plugin_path = force_omit_system_plugin_path;
	cfg->omit_home_plugin_path = force_omit_home_plugin_path;
	ret = append_env_var_plugin_paths(cfg->plugin_paths);
	if (ret) {
		goto error;
	}

	/* Parse options */
	pc = poptGetContext(NULL, argc, (const char **) argv,
		help_long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_PLUGIN_PATH:
			if (bt_config_append_plugin_paths_check_setuid_setgid(
					cfg->plugin_paths, arg)) {
				goto error;
			}
			break;
		case OPT_OMIT_SYSTEM_PLUGIN_PATH:
			cfg->omit_system_plugin_path = true;
			break;
		case OPT_OMIT_HOME_PLUGIN_PATH:
			cfg->omit_home_plugin_path = true;
			break;
		case OPT_HELP:
			print_help_usage(stdout);
			*retcode = -1;
			BT_OBJECT_PUT_REF_AND_RESET(cfg);
			goto end;
		default:
			printf_err("Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	leftover = poptGetArg(pc);
	if (leftover) {
		plugin_comp_cls_names(leftover, NULL,
			&plugin_name, &comp_cls_name,
			&cfg->cmd_data.help.cfg_component->type);
		if (plugin_name && comp_cls_name) {
			/* Component class help */
			g_string_assign(
				cfg->cmd_data.help.cfg_component->plugin_name,
				plugin_name);
			g_string_assign(
				cfg->cmd_data.help.cfg_component->comp_cls_name,
				comp_cls_name);
		} else {
			/* Fall back to plugin help */
			g_string_assign(
				cfg->cmd_data.help.cfg_component->plugin_name,
				leftover);
		}
	} else {
		print_help_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	if (append_home_and_system_plugin_paths_cfg(cfg)) {
		goto error;
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	g_free(plugin_name);
	g_free(comp_cls_name);

	if (pc) {
		poptFreeContext(pc);
	}

	free(arg);
	return cfg;
}

/*
 * Prints the help command usage.
 */
static
void print_query_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [GEN OPTS] query [OPTS] TYPE.PLUGIN.CLS OBJECT\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --omit-home-plugin-path       Omit home plugins from plugin search path\n");
	fprintf(fp, "                                    (~/.local/lib/babeltrace/plugins)\n");
	fprintf(fp, "      --omit-system-plugin-path     Omit system plugins from plugin search path\n");
	fprintf(fp, "  -p, --params=PARAMS               Set the query parameters to PARAMS\n");
	fprintf(fp, "                                    (see the expected format of PARAMS below)\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Add PATH to the list of paths from which\n");
	fprintf(fp, "                                    dynamic plugins can be loaded\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n\n");
	print_expected_params_format(fp);
}

static
struct poptOption query_long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "omit-home-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_HOME_PLUGIN_PATH, NULL, NULL },
	{ "omit-system-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_SYSTEM_PLUGIN_PATH, NULL, NULL },
	{ "params", 'p', POPT_ARG_STRING, NULL, OPT_PARAMS, NULL, NULL },
	{ "plugin-path", '\0', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ NULL, 0, '\0', NULL, 0, NULL, NULL },
};

/*
 * Creates a Babeltrace config object from the arguments of a query
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_query_from_args(int argc, const char *argv[],
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths)
{
	poptContext pc = NULL;
	char *arg = NULL;
	int opt;
	int ret;
	struct bt_config *cfg = NULL;
	const char *leftover;
	bt_value *params;

	params = bt_value_null;
	bt_value_get_ref(bt_value_null);

	*retcode = 0;
	cfg = bt_config_query_create(initial_plugin_paths);
	if (!cfg) {
		goto error;
	}

	cfg->omit_system_plugin_path = force_omit_system_plugin_path;
	cfg->omit_home_plugin_path = force_omit_home_plugin_path;
	ret = append_env_var_plugin_paths(cfg->plugin_paths);
	if (ret) {
		goto error;
	}

	/* Parse options */
	pc = poptGetContext(NULL, argc, (const char **) argv,
		query_long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_PLUGIN_PATH:
			if (bt_config_append_plugin_paths_check_setuid_setgid(
					cfg->plugin_paths, arg)) {
				goto error;
			}
			break;
		case OPT_OMIT_SYSTEM_PLUGIN_PATH:
			cfg->omit_system_plugin_path = true;
			break;
		case OPT_OMIT_HOME_PLUGIN_PATH:
			cfg->omit_home_plugin_path = true;
			break;
		case OPT_PARAMS:
		{
			bt_value_put_ref(params);
			params = bt_value_from_arg(arg);
			if (!params) {
				printf_err("Invalid format for --params option's argument:\n    %s\n",
					arg);
				goto error;
			}
			break;
		}
		case OPT_HELP:
			print_query_usage(stdout);
			*retcode = -1;
			BT_OBJECT_PUT_REF_AND_RESET(cfg);
			goto end;
		default:
			printf_err("Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	/*
	 * We need exactly two leftover arguments which are the
	 * mandatory component class specification and query object.
	 */
	leftover = poptGetArg(pc);
	if (leftover) {
		cfg->cmd_data.query.cfg_component =
			bt_config_component_from_arg(leftover);
		if (!cfg->cmd_data.query.cfg_component) {
			printf_err("Invalid format for component class specification:\n    %s\n",
				leftover);
			goto error;
		}

		BT_ASSERT(params);
		BT_OBJECT_MOVE_REF(cfg->cmd_data.query.cfg_component->params,
			params);
	} else {
		print_query_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	leftover = poptGetArg(pc);
	if (leftover) {
		if (strlen(leftover) == 0) {
			printf_err("Invalid empty object\n");
			goto error;
		}

		g_string_assign(cfg->cmd_data.query.object, leftover);
	} else {
		print_query_usage(stdout);
		*retcode = -1;
		BT_OBJECT_PUT_REF_AND_RESET(cfg);
		goto end;
	}

	leftover = poptGetArg(pc);
	if (leftover) {
		printf_err("Unexpected argument: %s\n", leftover);
		goto error;
	}

	if (append_home_and_system_plugin_paths_cfg(cfg)) {
		goto error;
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	if (pc) {
		poptFreeContext(pc);
	}

	bt_value_put_ref(params);
	free(arg);
	return cfg;
}

/*
 * Prints the list-plugins command usage.
 */
static
void print_list_plugins_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [GENERAL OPTIONS] list-plugins [OPTIONS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --omit-home-plugin-path       Omit home plugins from plugin search path\n");
	fprintf(fp, "                                    (~/.local/lib/babeltrace/plugins)\n");
	fprintf(fp, "      --omit-system-plugin-path     Omit system plugins from plugin search path\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Add PATH to the list of paths from which\n");
	fprintf(fp, "                                    dynamic plugins can be loaded\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace --help` for the list of general options.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Use `babeltrace help` to get help for a specific plugin or component class.\n");
}

static
struct poptOption list_plugins_long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "omit-home-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_HOME_PLUGIN_PATH, NULL, NULL },
	{ "omit-system-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_SYSTEM_PLUGIN_PATH, NULL, NULL },
	{ "plugin-path", '\0', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ NULL, 0, '\0', NULL, 0, NULL, NULL },
};

/*
 * Creates a Babeltrace config object from the arguments of a
 * list-plugins command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_list_plugins_from_args(int argc, const char *argv[],
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths)
{
	poptContext pc = NULL;
	char *arg = NULL;
	int opt;
	int ret;
	struct bt_config *cfg = NULL;
	const char *leftover;

	*retcode = 0;
	cfg = bt_config_list_plugins_create(initial_plugin_paths);
	if (!cfg) {
		goto error;
	}

	cfg->omit_system_plugin_path = force_omit_system_plugin_path;
	cfg->omit_home_plugin_path = force_omit_home_plugin_path;
	ret = append_env_var_plugin_paths(cfg->plugin_paths);
	if (ret) {
		goto error;
	}

	/* Parse options */
	pc = poptGetContext(NULL, argc, (const char **) argv,
		list_plugins_long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_PLUGIN_PATH:
			if (bt_config_append_plugin_paths_check_setuid_setgid(
					cfg->plugin_paths, arg)) {
				goto error;
			}
			break;
		case OPT_OMIT_SYSTEM_PLUGIN_PATH:
			cfg->omit_system_plugin_path = true;
			break;
		case OPT_OMIT_HOME_PLUGIN_PATH:
			cfg->omit_home_plugin_path = true;
			break;
		case OPT_HELP:
			print_list_plugins_usage(stdout);
			*retcode = -1;
			BT_OBJECT_PUT_REF_AND_RESET(cfg);
			goto end;
		default:
			printf_err("Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	leftover = poptGetArg(pc);
	if (leftover) {
		printf_err("Unexpected argument: %s\n", leftover);
		goto error;
	}

	if (append_home_and_system_plugin_paths_cfg(cfg)) {
		goto error;
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	if (pc) {
		poptFreeContext(pc);
	}

	free(arg);
	return cfg;
}

/*
 * Prints the run command usage.
 */
static
void print_run_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [GENERAL OPTIONS] run [OPTIONS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -b, --base-params=PARAMS          Set PARAMS as the current base parameters\n");
	fprintf(fp, "                                    for all the following components until\n");
	fprintf(fp, "                                    --reset-base-params is encountered\n");
	fprintf(fp, "                                    (see the expected format of PARAMS below)\n");
	fprintf(fp, "  -c, --component=[NAME:]TYPE.PLUGIN.CLS\n");
	fprintf(fp, "                                    Instantiate the component class CLS of type\n");
	fprintf(fp, "                                    TYPE (`source`, `filter`, or `sink`) found\n");
	fprintf(fp, "                                    in the plugin PLUGIN, add it to the graph,\n");
	fprintf(fp, "                                    and optionally name it NAME (you can also\n");
	fprintf(fp, "                                    specify the name with --name)\n");
	fprintf(fp, "  -x, --connect=CONNECTION          Connect two created components (see the\n");
	fprintf(fp, "                                    expected format of CONNECTION below)\n");
	fprintf(fp, "      --key=KEY                     Set the current initialization string\n");
	fprintf(fp, "                                    parameter key to KEY (see --value)\n");
	fprintf(fp, "  -n, --name=NAME                   Set the name of the current component\n");
	fprintf(fp, "                                    to NAME (must be unique amongst all the\n");
	fprintf(fp, "                                    names of the created components)\n");
	fprintf(fp, "      --omit-home-plugin-path       Omit home plugins from plugin search path\n");
	fprintf(fp, "                                    (~/.local/lib/babeltrace/plugins)\n");
	fprintf(fp, "      --omit-system-plugin-path     Omit system plugins from plugin search path\n");
	fprintf(fp, "  -p, --params=PARAMS               Add initialization parameters PARAMS to the\n");
	fprintf(fp, "                                    current component (see the expected format\n");
	fprintf(fp, "                                    of PARAMS below)\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Add PATH to the list of paths from which\n");
	fprintf(fp, "                                    dynamic plugins can be loaded\n");
	fprintf(fp, "  -r, --reset-base-params           Reset the current base parameters to an\n");
	fprintf(fp, "                                    empty map\n");
	fprintf(fp, "      --retry-duration=DUR          When babeltrace(1) needs to retry to run\n");
	fprintf(fp, "                                    the graph later, retry in DUR µs\n");
	fprintf(fp, "                                    (default: 100000)\n");
	fprintf(fp, "      --value=VAL                   Add a string initialization parameter to\n");
	fprintf(fp, "                                    the current component with a name given by\n");
	fprintf(fp, "                                    the last argument of the --key option and a\n");
	fprintf(fp, "                                    value set to VAL\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "See `babeltrace --help` for the list of general options.\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Expected format of CONNECTION\n");
	fprintf(fp, "-----------------------------\n");
	fprintf(fp, "\n");
	fprintf(fp, "    UPSTREAM[.UPSTREAM-PORT]:DOWNSTREAM[.DOWNSTREAM-PORT]\n");
	fprintf(fp, "\n");
	fprintf(fp, "UPSTREAM and DOWNSTREAM are names of the upstream and downstream\n");
	fprintf(fp, "components to connect together. You must escape the following characters\n\n");
	fprintf(fp, "with `\\`: `\\`, `.`, and `:`. You can set the name of the current\n");
	fprintf(fp, "component with the --name option.\n");
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
	fprintf(fp, "babeltrace from a shell.\n");
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
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths)
{
	poptContext pc = NULL;
	char *arg = NULL;
	struct bt_config_component *cur_cfg_comp = NULL;
	enum bt_config_component_dest cur_cfg_comp_dest =
			BT_CONFIG_COMPONENT_DEST_UNKNOWN;
	bt_value *cur_base_params = NULL;
	int opt, ret = 0;
	struct bt_config *cfg = NULL;
	bt_value *instance_names = NULL;
	bt_value *connection_args = NULL;
	GString *cur_param_key = NULL;
	char error_buf[256] = { 0 };
	long retry_duration = -1;
	bt_value_status status;
	struct poptOption run_long_options[] = {
		{ "base-params", 'b', POPT_ARG_STRING, NULL, OPT_BASE_PARAMS, NULL, NULL },
		{ "component", 'c', POPT_ARG_STRING, NULL, OPT_COMPONENT, NULL, NULL },
		{ "connect", 'x', POPT_ARG_STRING, NULL, OPT_CONNECT, NULL, NULL },
		{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
		{ "key", '\0', POPT_ARG_STRING, NULL, OPT_KEY, NULL, NULL },
		{ "name", 'n', POPT_ARG_STRING, NULL, OPT_NAME, NULL, NULL },
		{ "omit-home-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_HOME_PLUGIN_PATH, NULL, NULL },
		{ "omit-system-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_SYSTEM_PLUGIN_PATH, NULL, NULL },
		{ "params", 'p', POPT_ARG_STRING, NULL, OPT_PARAMS, NULL, NULL },
		{ "plugin-path", '\0', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
		{ "reset-base-params", 'r', POPT_ARG_NONE, NULL, OPT_RESET_BASE_PARAMS, NULL, NULL },
		{ "retry-duration", '\0', POPT_ARG_LONG, &retry_duration, OPT_RETRY_DURATION, NULL, NULL },
		{ "value", '\0', POPT_ARG_STRING, NULL, OPT_VALUE, NULL, NULL },
		{ NULL, 0, '\0', NULL, 0, NULL, NULL },
	};

	*retcode = 0;
	cur_param_key = g_string_new(NULL);
	if (!cur_param_key) {
		print_err_oom();
		goto error;
	}

	if (argc <= 1) {
		print_run_usage(stdout);
		*retcode = -1;
		goto end;
	}

	cfg = bt_config_run_create(initial_plugin_paths);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.run.retry_duration_us = 100000;
	cfg->omit_system_plugin_path = force_omit_system_plugin_path;
	cfg->omit_home_plugin_path = force_omit_home_plugin_path;
	cur_base_params = bt_value_map_create();
	if (!cur_base_params) {
		print_err_oom();
		goto error;
	}

	instance_names = bt_value_map_create();
	if (!instance_names) {
		print_err_oom();
		goto error;
	}

	connection_args = bt_value_array_create();
	if (!connection_args) {
		print_err_oom();
		goto error;
	}

	ret = append_env_var_plugin_paths(cfg->plugin_paths);
	if (ret) {
		goto error;
	}

	/* Parse options */
	pc = poptGetContext(NULL, argc, (const char **) argv,
		run_long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_PLUGIN_PATH:
			if (bt_config_append_plugin_paths_check_setuid_setgid(
					cfg->plugin_paths, arg)) {
				goto error;
			}
			break;
		case OPT_OMIT_SYSTEM_PLUGIN_PATH:
			cfg->omit_system_plugin_path = true;
			break;
		case OPT_OMIT_HOME_PLUGIN_PATH:
			cfg->omit_home_plugin_path = true;
			break;
		case OPT_COMPONENT:
		{
			enum bt_config_component_dest new_dest;

			if (cur_cfg_comp) {
				ret = add_run_cfg_comp_check_name(cfg,
					cur_cfg_comp, cur_cfg_comp_dest,
					instance_names);
				BT_OBJECT_PUT_REF_AND_RESET(cur_cfg_comp);
				if (ret) {
					goto error;
				}
			}

			cur_cfg_comp = bt_config_component_from_arg(arg);
			if (!cur_cfg_comp) {
				printf_err("Invalid format for --component option's argument:\n    %s\n",
					arg);
				goto error;
			}

			switch (cur_cfg_comp->type) {
			case BT_COMPONENT_CLASS_TYPE_SOURCE:
				new_dest = BT_CONFIG_COMPONENT_DEST_SOURCE;
				break;
			case BT_COMPONENT_CLASS_TYPE_FILTER:
				new_dest = BT_CONFIG_COMPONENT_DEST_FILTER;
				break;
			case BT_COMPONENT_CLASS_TYPE_SINK:
				new_dest = BT_CONFIG_COMPONENT_DEST_SINK;
				break;
			default:
				abort();
			}

			BT_ASSERT(cur_base_params);
			bt_value_put_ref(cur_cfg_comp->params);
			status = bt_value_copy(cur_base_params,
				&cur_cfg_comp->params);
			if (status != BT_VALUE_STATUS_OK) {
				print_err_oom();
				goto error;
			}

			cur_cfg_comp_dest = new_dest;
			break;
		}
		case OPT_PARAMS:
		{
			bt_value *params;
			bt_value *params_to_set;

			if (!cur_cfg_comp) {
				printf_err("Cannot add parameters to unavailable component:\n    %s\n",
					arg);
				goto error;
			}

			params = bt_value_from_arg(arg);
			if (!params) {
				printf_err("Invalid format for --params option's argument:\n    %s\n",
					arg);
				goto error;
			}

			status = bt_value_map_extend(cur_cfg_comp->params,
				params, &params_to_set);
			BT_VALUE_PUT_REF_AND_RESET(params);
			if (status != BT_VALUE_STATUS_OK) {
				printf_err("Cannot extend current component parameters with --params option's argument:\n    %s\n",
					arg);
				goto error;
			}

			BT_OBJECT_MOVE_REF(cur_cfg_comp->params, params_to_set);
			break;
		}
		case OPT_KEY:
			if (strlen(arg) == 0) {
				printf_err("Cannot set an empty string as the current parameter key\n");
				goto error;
			}

			g_string_assign(cur_param_key, arg);
			break;
		case OPT_VALUE:
			if (!cur_cfg_comp) {
				printf_err("Cannot set a parameter's value of unavailable component:\n    %s\n",
					arg);
				goto error;
			}

			if (cur_param_key->len == 0) {
				printf_err("--value option specified without preceding --key option:\n    %s\n",
					arg);
				goto error;
			}

			if (bt_value_map_insert_string_entry(cur_cfg_comp->params,
					cur_param_key->str, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_NAME:
			if (!cur_cfg_comp) {
				printf_err("Cannot set the name of unavailable component:\n    %s\n",
					arg);
				goto error;
			}

			g_string_assign(cur_cfg_comp->instance_name, arg);
			break;
		case OPT_BASE_PARAMS:
		{
			bt_value *params =
				bt_value_from_arg(arg);

			if (!params) {
				printf_err("Invalid format for --base-params option's argument:\n    %s\n",
					arg);
				goto error;
			}

			BT_OBJECT_MOVE_REF(cur_base_params, params);
			break;
		}
		case OPT_RESET_BASE_PARAMS:
			BT_VALUE_PUT_REF_AND_RESET(cur_base_params);
			cur_base_params = bt_value_map_create();
			if (!cur_base_params) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_CONNECT:
			if (bt_value_array_append_string_element(
					connection_args, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_RETRY_DURATION:
			if (retry_duration < 0) {
				printf_err("--retry-duration option's argument must be positive or 0: %ld\n",
					retry_duration);
				goto error;
			}

			cfg->cmd_data.run.retry_duration_us =
				(uint64_t) retry_duration;
			break;
		case OPT_HELP:
			print_run_usage(stdout);
			*retcode = -1;
			BT_OBJECT_PUT_REF_AND_RESET(cfg);
			goto end;
		default:
			printf_err("Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	/* This command does not accept leftover arguments */
	if (poptPeekArg(pc)) {
		printf_err("Unexpected argument: %s\n", poptPeekArg(pc));
		goto error;
	}

	/* Add current component */
	if (cur_cfg_comp) {
		ret = add_run_cfg_comp_check_name(cfg, cur_cfg_comp,
			cur_cfg_comp_dest, instance_names);
		BT_OBJECT_PUT_REF_AND_RESET(cur_cfg_comp);
		if (ret) {
			goto error;
		}
	}

	if (cfg->cmd_data.run.sources->len == 0) {
		printf_err("Incomplete graph: no source component\n");
		goto error;
	}

	if (cfg->cmd_data.run.sinks->len == 0) {
		printf_err("Incomplete graph: no sink component\n");
		goto error;
	}

	if (append_home_and_system_plugin_paths_cfg(cfg)) {
		goto error;
	}

	ret = bt_config_cli_args_create_connections(cfg,
		connection_args,
		error_buf, 256);
	if (ret) {
		printf_err("Cannot creation connections:\n%s", error_buf);
		goto error;
	}

	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	if (pc) {
		poptFreeContext(pc);
	}

	if (cur_param_key) {
		g_string_free(cur_param_key, TRUE);
	}

	free(arg);
	BT_OBJECT_PUT_REF_AND_RESET(cur_cfg_comp);
	BT_VALUE_PUT_REF_AND_RESET(cur_base_params);
	BT_VALUE_PUT_REF_AND_RESET(instance_names);
	BT_VALUE_PUT_REF_AND_RESET(connection_args);
	return cfg;
}

static
struct bt_config *bt_config_run_from_args_array(const bt_value *run_args,
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths)
{
	struct bt_config *cfg = NULL;
	const char **argv;
	int64_t i, len;
	const size_t argc = bt_value_array_get_size(run_args) + 1;

	argv = calloc(argc, sizeof(*argv));
	if (!argv) {
		print_err_oom();
		goto end;
	}

	argv[0] = "run";

	len = bt_value_array_get_size(run_args);
	if (len < 0) {
		printf_err("Invalid executable arguments\n");
		goto end;
	}
	for (i = 0; i < len; i++) {
		const bt_value *arg_value =
			bt_value_array_borrow_element_by_index_const(run_args,
								     i);
		const char *arg;

		BT_ASSERT(arg_value);
		arg = bt_value_string_get(arg_value);
		BT_ASSERT(arg);
		argv[i + 1] = arg;
	}

	cfg = bt_config_run_from_args(argc, argv, retcode,
		force_omit_system_plugin_path, force_omit_home_plugin_path,
		initial_plugin_paths);

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
	fprintf(fp, "Usage: babeltrace [GENERAL OPTIONS] [convert] [OPTIONS] [PATH/URL]\n");
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -c, --component=[NAME:]TYPE.PLUGIN.CLS\n");
	fprintf(fp, "                                    Instantiate the component class CLS of type\n");
	fprintf(fp, "                                    TYPE (`source`, `filter`, or `sink`) found\n");
	fprintf(fp, "                                    in the plugin PLUGIN, add it to the\n");
	fprintf(fp, "                                    conversion graph, and optionally name it\n");
	fprintf(fp, "                                    NAME (you can also specify the name with\n");
	fprintf(fp, "                                    --name)\n");
	fprintf(fp, "      --name=NAME                   Set the name of the current component\n");
	fprintf(fp, "                                    to NAME (must be unique amongst all the\n");
	fprintf(fp, "                                    names of the created components)\n");
	fprintf(fp, "      --omit-home-plugin-path       Omit home plugins from plugin search path\n");
	fprintf(fp, "                                    (~/.local/lib/babeltrace/plugins)\n");
	fprintf(fp, "      --omit-system-plugin-path     Omit system plugins from plugin search path\n");
	fprintf(fp, "  -p, --params=PARAMS               Add initialization parameters PARAMS to the\n");
	fprintf(fp, "                                    current component (see the expected format\n");
	fprintf(fp, "                                    of PARAMS below)\n");
	fprintf(fp, "  -P, --path=PATH                   Set the `path` string parameter of the\n");
	fprintf(fp, "                                    current component to PATH\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Add PATH to the list of paths from which\n");
	fprintf(fp, "      --retry-duration=DUR          When babeltrace(1) needs to retry to run\n");
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
	fprintf(fp, "  -u, --url=URL                     Set the `url` string parameter of the\n");
	fprintf(fp, "                                    current component to URL\n");
	fprintf(fp, "  -h, --help                        Show this help and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "Implicit `source.ctf.fs` component options:\n");
	fprintf(fp, "\n");
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
	fprintf(fp, "Implicit `filter.utils.muxer` component options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --clock-force-correlate       Assume that clocks are inherently\n");
	fprintf(fp, "                                    correlated across traces\n");
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
	fprintf(fp, "See `babeltrace --help` for the list of general options.\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Format of BEGIN and END\n");
	fprintf(fp, "-----------------------\n");
	fprintf(fp, "\n");
	fprintf(fp, "    [YYYY-MM-DD [hh:mm:]]ss[.nnnnnnnnn]\n");
	fprintf(fp, "\n\n");
	print_expected_params_format(fp);
}

static
struct poptOption convert_long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "begin", 'b', POPT_ARG_STRING, NULL, OPT_BEGIN, NULL, NULL },
	{ "clock-cycles", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_CYCLES, NULL, NULL },
	{ "clock-date", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_DATE, NULL, NULL },
	{ "clock-force-correlate", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_FORCE_CORRELATE, NULL, NULL },
	{ "clock-gmt", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_GMT, NULL, NULL },
	{ "clock-offset", '\0', POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET, NULL, NULL },
	{ "clock-offset-ns", '\0', POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET_NS, NULL, NULL },
	{ "clock-seconds", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_SECONDS, NULL, NULL },
	{ "color", '\0', POPT_ARG_STRING, NULL, OPT_COLOR, NULL, NULL },
	{ "component", 'c', POPT_ARG_STRING, NULL, OPT_COMPONENT, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ "debug-info-dir", 0, POPT_ARG_STRING, NULL, OPT_DEBUG_INFO_DIR, NULL, NULL },
	{ "debug-info-full-path", 0, POPT_ARG_NONE, NULL, OPT_DEBUG_INFO_FULL_PATH, NULL, NULL },
	{ "debug-info-target-prefix", 0, POPT_ARG_STRING, NULL, OPT_DEBUG_INFO_TARGET_PREFIX, NULL, NULL },
	{ "end", 'e', POPT_ARG_STRING, NULL, OPT_END, NULL, NULL },
	{ "fields", 'f', POPT_ARG_STRING, NULL, OPT_FIELDS, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "input-format", 'i', POPT_ARG_STRING, NULL, OPT_INPUT_FORMAT, NULL, NULL },
	{ "name", '\0', POPT_ARG_STRING, NULL, OPT_NAME, NULL, NULL },
	{ "names", 'n', POPT_ARG_STRING, NULL, OPT_NAMES, NULL, NULL },
	{ "debug-info", '\0', POPT_ARG_NONE, NULL, OPT_DEBUG_INFO, NULL, NULL },
	{ "no-delta", '\0', POPT_ARG_NONE, NULL, OPT_NO_DELTA, NULL, NULL },
	{ "omit-home-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_HOME_PLUGIN_PATH, NULL, NULL },
	{ "omit-system-plugin-path", '\0', POPT_ARG_NONE, NULL, OPT_OMIT_SYSTEM_PLUGIN_PATH, NULL, NULL },
	{ "output", 'w', POPT_ARG_STRING, NULL, OPT_OUTPUT, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, NULL, OPT_OUTPUT_FORMAT, NULL, NULL },
	{ "params", 'p', POPT_ARG_STRING, NULL, OPT_PARAMS, NULL, NULL },
	{ "path", 'P', POPT_ARG_STRING, NULL, OPT_PATH, NULL, NULL },
	{ "plugin-path", '\0', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ "retry-duration", '\0', POPT_ARG_STRING, NULL, OPT_RETRY_DURATION, NULL, NULL },
	{ "run-args", '\0', POPT_ARG_NONE, NULL, OPT_RUN_ARGS, NULL, NULL },
	{ "run-args-0", '\0', POPT_ARG_NONE, NULL, OPT_RUN_ARGS_0, NULL, NULL },
	{ "stream-intersection", '\0', POPT_ARG_NONE, NULL, OPT_STREAM_INTERSECTION, NULL, NULL },
	{ "timerange", '\0', POPT_ARG_STRING, NULL, OPT_TIMERANGE, NULL, NULL },
	{ "url", 'u', POPT_ARG_STRING, NULL, OPT_URL, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ NULL, 0, '\0', NULL, 0, NULL, NULL },
};

static
GString *get_component_auto_name(const char *prefix,
		const bt_value *existing_names)
{
	unsigned int i = 0;
	GString *auto_name = g_string_new(NULL);

	if (!auto_name) {
		print_err_oom();
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
	GString *comp_arg;
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
		print_err_oom();
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
	size_t i;

	if (!impl_args->exists) {
		goto end;
	}

	if (bt_value_array_append_string_element(run_args, "--component")) {
		print_err_oom();
		goto error;
	}

	if (bt_value_array_append_string_element(run_args, impl_args->comp_arg->str)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_array_append_string_element(run_args, "--name")) {
		print_err_oom();
		goto error;
	}

	if (bt_value_array_append_string_element(run_args, impl_args->name_arg->str)) {
		print_err_oom();
		goto error;
	}

	if (impl_args->params_arg->len > 0) {
		if (bt_value_array_append_string_element(run_args, "--params")) {
			print_err_oom();
			goto error;
		}

		if (bt_value_array_append_string_element(run_args,
				impl_args->params_arg->str)) {
			print_err_oom();
			goto error;
		}
	}

	for (i = 0; i < bt_value_array_get_size(impl_args->extra_params);
			i++) {
		const bt_value *elem;
		const char *arg;

		elem = bt_value_array_borrow_element_by_index(impl_args->extra_params,
							      i);
		if (!elem) {
			goto error;
		}

		BT_ASSERT(bt_value_is_string(elem));
		arg = bt_value_string_get(elem);
		ret = bt_value_array_append_string_element(run_args, arg);
		if (ret) {
			print_err_oom();
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

static
void destroy_implicit_component_args(void *args)
{
	if (!args) {
		return;
	}

	finalize_implicit_component_args(args);
	g_free(args);
}

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
		print_err_oom();
		goto end;
	}

end:
	return ret;
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

static
int append_implicit_component_extra_param(struct implicit_component_args *args,
		const char *key, const char *value)
{
	int ret = 0;

	BT_ASSERT(args);
	BT_ASSERT(key);
	BT_ASSERT(value);

	if (bt_value_array_append_string_element(args->extra_params, "--key")) {
		print_err_oom();
		ret = -1;
		goto end;
	}

	if (bt_value_array_append_string_element(args->extra_params, key)) {
		print_err_oom();
		ret = -1;
		goto end;
	}

	if (bt_value_array_append_string_element(args->extra_params, "--value")) {
		print_err_oom();
		ret = -1;
		goto end;
	}

	if (bt_value_array_append_string_element(args->extra_params, value)) {
		print_err_oom();
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int convert_append_name_param(enum bt_config_component_dest dest,
		GString *cur_name, GString *cur_name_prefix,
		bt_value *run_args,
		bt_value *all_names,
		GList **source_names, GList **filter_names,
		GList **sink_names)
{
	int ret = 0;

	if (cur_name_prefix->len > 0) {
		/* We're after a --component option */
		GString *name = NULL;
		bool append_name_opt = false;

		if (cur_name->len == 0) {
			/*
			 * No explicit name was provided for the user
			 * component.
			 */
			name = get_component_auto_name(cur_name_prefix->str,
				all_names);
			append_name_opt = true;
		} else {
			/*
			 * An explicit name was provided for the user
			 * component.
			 */
			if (bt_value_map_has_entry(all_names,
						   cur_name->str)) {
				printf_err("Duplicate component instance name:\n    %s\n",
					cur_name->str);
				goto error;
			}

			name = g_string_new(cur_name->str);
		}

		if (!name) {
			print_err_oom();
			goto error;
		}

		/*
		 * Remember this name globally, for the uniqueness of
		 * all component names.
		 */
		if (bt_value_map_insert_entry(all_names, name->str, bt_value_null)) {
			print_err_oom();
			goto error;
		}

		/*
		 * Append the --name option if necessary.
		 */
		if (append_name_opt) {
			if (bt_value_array_append_string_element(run_args, "--name")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, name->str)) {
				print_err_oom();
				goto error;
			}
		}

		/*
		 * Remember this name specifically for the type of the
		 * component. This is to create connection arguments.
		 */
		switch (dest) {
		case BT_CONFIG_COMPONENT_DEST_SOURCE:
			*source_names = g_list_append(*source_names, name);
			break;
		case BT_CONFIG_COMPONENT_DEST_FILTER:
			*filter_names = g_list_append(*filter_names, name);
			break;
		case BT_CONFIG_COMPONENT_DEST_SINK:
			*sink_names = g_list_append(*sink_names, name);
			break;
		default:
			abort();
		}

		g_string_assign(cur_name_prefix, "");
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
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
		print_err_oom();
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
		print_err_oom();
		ret = -1;
		goto end;
	}

	ret = bt_value_array_append_string_element(run_args, "--connect");
	if (ret) {
		print_err_oom();
		ret = -1;
		goto end;
	}

	g_string_append(arg, e_upstream_name->str);
	g_string_append_c(arg, ':');
	g_string_append(arg, e_downstream_name->str);
	ret = bt_value_array_append_string_element(run_args, arg->str);
	if (ret) {
		print_err_oom();
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
	for (source_at = source_names; source_at != NULL; source_at = g_list_next(source_at)) {
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
	for (; filter_at != NULL; filter_prev = filter_at, filter_at = g_list_next(filter_at)) {
		GString *filter_name = filter_at->data;
		GString *filter_prev_name = filter_prev->data;

		ret = append_connect_arg(run_args, filter_prev_name->str,
			filter_name->str);
		if (ret) {
			goto error;
		}
	}

	/* Connect last filter to all sinks */
	for (sink_at = sink_names; sink_at != NULL; sink_at = g_list_next(sink_at)) {
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
		print_err_oom();
		goto end;
	}

	*list = g_list_prepend(*list, gs);

end:
	return ret;
}

static
struct implicit_component_args *create_implicit_component_args(void)
{
	struct implicit_component_args *impl_args =
		g_new0(struct implicit_component_args, 1);

	if (!impl_args) {
		goto end;
	}

	if (init_implicit_component_args(impl_args, NULL, true)) {
		destroy_implicit_component_args(impl_args);
		impl_args = NULL;
		goto end;
	}

end:
	return impl_args;
}

static
int fill_implicit_ctf_inputs_args(GPtrArray *implicit_ctf_inputs_args,
		struct implicit_component_args *base_implicit_ctf_input_args,
		GList *leftovers)
{
	int ret = 0;
	GList *leftover;
	bt_value_status status;

	for (leftover = leftovers; leftover != NULL;
			leftover = g_list_next(leftover)) {
		GString *gs_leftover = leftover->data;
		struct implicit_component_args *impl_args =
			create_implicit_component_args();

		if (!impl_args) {
			print_err_oom();
			goto error;
		}

		impl_args->exists = true;
		g_string_assign(impl_args->comp_arg,
			base_implicit_ctf_input_args->comp_arg->str);
		g_string_assign(impl_args->params_arg,
			base_implicit_ctf_input_args->params_arg->str);

		/*
		 * We need our own copy of the extra parameters because
		 * this is where the unique path goes.
		 */
		BT_VALUE_PUT_REF_AND_RESET(impl_args->extra_params);
		status = bt_value_copy(base_implicit_ctf_input_args->extra_params,
			&impl_args->extra_params);
		if (status != BT_VALUE_STATUS_OK) {
			print_err_oom();
			destroy_implicit_component_args(impl_args);
			goto error;
		}

		/* Append unique path parameter */
		ret = append_implicit_component_extra_param(impl_args,
			"path", gs_leftover->str);
		if (ret) {
			destroy_implicit_component_args(impl_args);
			goto error;
		}

		g_ptr_array_add(implicit_ctf_inputs_args, impl_args);
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

/*
 * Creates a Babeltrace config object from the arguments of a convert
 * command.
 *
 * *retcode is set to the appropriate exit code to use.
 */
static
struct bt_config *bt_config_convert_from_args(int argc, const char *argv[],
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths, char *log_level)
{
	poptContext pc = NULL;
	char *arg = NULL;
	enum bt_config_component_dest cur_comp_dest =
			BT_CONFIG_COMPONENT_DEST_UNKNOWN;
	int opt, ret = 0;
	struct bt_config *cfg = NULL;
	bool got_input_format_opt = false;
	bool got_output_format_opt = false;
	bool trimmer_has_begin = false;
	bool trimmer_has_end = false;
	bool stream_intersection_mode = false;
	GString *cur_name = NULL;
	GString *cur_name_prefix = NULL;
	const char *leftover = NULL;
	bool print_run_args = false;
	bool print_run_args_0 = false;
	bool print_ctf_metadata = false;
	bt_value *run_args = NULL;
	bt_value *all_names = NULL;
	GList *source_names = NULL;
	GList *filter_names = NULL;
	GList *sink_names = NULL;
	GList *leftovers = NULL;
	GPtrArray *implicit_ctf_inputs_args = NULL;
	struct implicit_component_args base_implicit_ctf_input_args = { 0 };
	struct implicit_component_args implicit_ctf_output_args = { 0 };
	struct implicit_component_args implicit_lttng_live_args = { 0 };
	struct implicit_component_args implicit_dummy_args = { 0 };
	struct implicit_component_args implicit_text_args = { 0 };
	struct implicit_component_args implicit_debug_info_args = { 0 };
	struct implicit_component_args implicit_muxer_args = { 0 };
	struct implicit_component_args implicit_trimmer_args = { 0 };
	bt_value *plugin_paths;
	char error_buf[256] = { 0 };
	size_t i;
	struct bt_common_lttng_live_url_parts lttng_live_url_parts = { 0 };
	char *output = NULL;

	(void) bt_value_copy(initial_plugin_paths, &plugin_paths);

	*retcode = 0;

	if (argc <= 1) {
		print_convert_usage(stdout);
		*retcode = -1;
		goto end;
	}

	if (init_implicit_component_args(&base_implicit_ctf_input_args,
			"source.ctf.fs", false)) {
		goto error;
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

	implicit_ctf_inputs_args = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_implicit_component_args);
	if (!implicit_ctf_inputs_args) {
		print_err_oom();
		goto error;
	}

	all_names = bt_value_map_create();
	if (!all_names) {
		print_err_oom();
		goto error;
	}

	run_args = bt_value_array_create();
	if (!run_args) {
		print_err_oom();
		goto error;
	}

	cur_name = g_string_new(NULL);
	if (!cur_name) {
		print_err_oom();
		goto error;
	}

	cur_name_prefix = g_string_new(NULL);
	if (!cur_name_prefix) {
		print_err_oom();
		goto error;
	}

	ret = append_env_var_plugin_paths(plugin_paths);
	if (ret) {
		goto error;
	}

	/*
	 * First pass: collect all arguments which need to be passed
	 * as is to the run command. This pass can also add --name
	 * arguments if needed to automatically name unnamed component
	 * instances. Also it does the following transformations:
	 *
	 *     --path=PATH -> --key path --value PATH
	 *     --url=URL   -> --key url --value URL
	 *
	 * Also it appends the plugin paths of --plugin-path to
	 * `plugin_paths`.
	 */
	pc = poptGetContext(NULL, argc, (const char **) argv,
		convert_long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		char *name = NULL;
		char *plugin_name = NULL;
		char *comp_cls_name = NULL;

		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_COMPONENT:
		{
			bt_component_class_type type;
			const char *type_prefix;

			/* Append current component's name if needed */
			ret = convert_append_name_param(cur_comp_dest, cur_name,
				cur_name_prefix, run_args, all_names,
				&source_names, &filter_names, &sink_names);
			if (ret) {
				goto error;
			}

			/* Parse the argument */
			plugin_comp_cls_names(arg, &name, &plugin_name,
				&comp_cls_name, &type);
			if (!plugin_name || !comp_cls_name) {
				printf_err("Invalid format for --component option's argument:\n    %s\n",
					arg);
				goto error;
			}

			if (name) {
				g_string_assign(cur_name, name);
			} else {
				g_string_assign(cur_name, "");
			}

			switch (type) {
			case BT_COMPONENT_CLASS_TYPE_SOURCE:
				cur_comp_dest = BT_CONFIG_COMPONENT_DEST_SOURCE;
				type_prefix = "source";
				break;
			case BT_COMPONENT_CLASS_TYPE_FILTER:
				cur_comp_dest = BT_CONFIG_COMPONENT_DEST_FILTER;
				type_prefix = "filter";
				break;
			case BT_COMPONENT_CLASS_TYPE_SINK:
				cur_comp_dest = BT_CONFIG_COMPONENT_DEST_SINK;
				type_prefix = "sink";
				break;
			default:
				abort();
			}

			if (bt_value_array_append_string_element(run_args,
					"--component")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}

			g_string_assign(cur_name_prefix, "");
			g_string_append_printf(cur_name_prefix, "%s.%s.%s",
				type_prefix, plugin_name, comp_cls_name);
			free(name);
			free(plugin_name);
			free(comp_cls_name);
			name = NULL;
			plugin_name = NULL;
			comp_cls_name = NULL;
			break;
		}
		case OPT_PARAMS:
			if (cur_name_prefix->len == 0) {
				printf_err("No current component of which to set parameters:\n    %s\n",
					arg);
				goto error;
			}

			if (bt_value_array_append_string_element(run_args,
					"--params")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_PATH:
			if (cur_name_prefix->len == 0) {
				printf_err("No current component of which to set `path` parameter:\n    %s\n",
					arg);
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "--key")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "path")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "--value")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_URL:
			if (cur_name_prefix->len == 0) {
				printf_err("No current component of which to set `url` parameter:\n    %s\n",
					arg);
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "--key")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "url")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "--value")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_NAME:
			if (cur_name_prefix->len == 0) {
				printf_err("No current component to name:\n    %s\n",
					arg);
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, "--name")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}

			g_string_assign(cur_name, arg);
			break;
		case OPT_OMIT_HOME_PLUGIN_PATH:
			force_omit_home_plugin_path = true;

			if (bt_value_array_append_string_element(run_args,
					"--omit-home-plugin-path")) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_RETRY_DURATION:
			if (bt_value_array_append_string_element(run_args,
					"--retry-duration")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_OMIT_SYSTEM_PLUGIN_PATH:
			force_omit_system_plugin_path = true;

			if (bt_value_array_append_string_element(run_args,
					"--omit-system-plugin-path")) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_PLUGIN_PATH:
			if (bt_config_append_plugin_paths_check_setuid_setgid(
					plugin_paths, arg)) {
				goto error;
			}

			if (bt_value_array_append_string_element(run_args,
					"--plugin-path")) {
				print_err_oom();
				goto error;
			}

			if (bt_value_array_append_string_element(run_args, arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_HELP:
			print_convert_usage(stdout);
			*retcode = -1;
			BT_OBJECT_PUT_REF_AND_RESET(cfg);
			goto end;
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
			printf_err("Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Append current component's name if needed */
	ret = convert_append_name_param(cur_comp_dest, cur_name,
		cur_name_prefix, run_args, all_names, &source_names,
		&filter_names, &sink_names);
	if (ret) {
		goto error;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	poptFreeContext(pc);
	free(arg);
	arg = NULL;

	/*
	 * Second pass: transform the convert-specific options and
	 * arguments into implicit component instances for the run
	 * command.
	 */
	pc = poptGetContext(NULL, argc, (const char **) argv,
		convert_long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_BEGIN:
			if (trimmer_has_begin) {
				printf("At --begin option: --begin or --timerange option already specified\n    %s\n",
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
				printf("At --end option: --end or --timerange option already specified\n    %s\n",
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
				printf("At --timerange option: --begin, --end, or --timerange option already specified\n    %s\n",
					arg);
				goto error;
			}

			ret = split_timerange(arg, &begin, &end);
			if (ret) {
				printf_err("Invalid --timerange option's argument: expecting BEGIN,END or [BEGIN,END]:\n    %s\n",
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
			append_implicit_component_param(
				&implicit_muxer_args,
				"assume-absolute-clock-classes", "yes");
			break;
		case OPT_CLOCK_GMT:
			append_implicit_component_param(
				&implicit_text_args, "clock-gmt", "yes");
			append_implicit_component_param(
				&implicit_trimmer_args, "clock-gmt", "yes");
			implicit_text_args.exists = true;
			break;
		case OPT_CLOCK_OFFSET:
			base_implicit_ctf_input_args.exists = true;
			append_implicit_component_param(
					&base_implicit_ctf_input_args,
					"clock-class-offset-s", arg);
			break;
		case OPT_CLOCK_OFFSET_NS:
			base_implicit_ctf_input_args.exists = true;
			append_implicit_component_param(
					&base_implicit_ctf_input_args,
					"clock-class-offset-ns", arg);
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
				printf_err("Duplicate --input-format option\n");
				goto error;
			}

			got_input_format_opt = true;

			if (strcmp(arg, "ctf") == 0) {
				base_implicit_ctf_input_args.exists = true;
			} else if (strcmp(arg, "lttng-live") == 0) {
				implicit_lttng_live_args.exists = true;
			} else {
				printf_err("Unknown legacy input format:\n    %s\n",
					arg);
				goto error;
			}
			break;
		case OPT_OUTPUT_FORMAT:
			if (got_output_format_opt) {
				printf_err("Duplicate --output-format option\n");
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
				printf_err("Unknown legacy output format:\n    %s\n",
					arg);
				goto error;
			}
			break;
		case OPT_OUTPUT:
			if (output) {
				printf_err("Duplicate --output option\n");
				goto error;
			}

			output = strdup(arg);
			if (!output) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_RUN_ARGS:
			if (print_run_args_0) {
				printf_err("Cannot specify --run-args and --run-args-0\n");
				goto error;
			}

			print_run_args = true;
			break;
		case OPT_RUN_ARGS_0:
			if (print_run_args) {
				printf_err("Cannot specify --run-args and --run-args-0\n");
				goto error;
			}

			print_run_args_0 = true;
			break;
		case OPT_STREAM_INTERSECTION:
			/*
			 * Applies to all traces implementing the trace-info
			 * query.
			 */
			stream_intersection_mode = true;
			break;
		case OPT_VERBOSE:
			if (*log_level != 'V' && *log_level != 'D') {
				*log_level = 'I';
			}
			break;
		case OPT_DEBUG:
			*log_level = 'V';
			break;
		}

		free(arg);
		arg = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	/*
	 * Legacy behaviour: --verbose used to make the `text` output
	 * format print more information. --verbose is now equivalent to
	 * the INFO log level, which is why we compare to 'I' here.
	 */
	if (*log_level == 'I') {
		append_implicit_component_param(&implicit_text_args,
			"verbose", "yes");
	}

	/*
	 * Append home and system plugin paths now that we possibly got
	 * --plugin-path.
	 */
	if (append_home_and_system_plugin_paths(plugin_paths,
			force_omit_system_plugin_path,
			force_omit_home_plugin_path)) {
		goto error;
	}

	/* Consume and keep leftover arguments */
	while ((leftover = poptGetArg(pc))) {
		GString *gs_leftover = g_string_new(leftover);

		if (!gs_leftover) {
			print_err_oom();
			goto error;
		}

		leftovers = g_list_append(leftovers, gs_leftover);
		if (!leftovers) {
			g_string_free(gs_leftover, TRUE);
			print_err_oom();
			goto error;
		}
	}

	/* Print CTF metadata or print LTTng live sessions */
	if (print_ctf_metadata) {
		GString *gs_leftover;

		if (g_list_length(leftovers) == 0) {
			printf_err("--output-format=ctf-metadata specified without a path\n");
			goto error;
		}

		if (g_list_length(leftovers) > 1) {
			printf_err("Too many paths specified for --output-format=ctf-metadata\n");
			goto error;
		}

		cfg = bt_config_print_ctf_metadata_create(plugin_paths);
		if (!cfg) {
			goto error;
		}

		gs_leftover = leftovers->data;
		g_string_assign(cfg->cmd_data.print_ctf_metadata.path,
			gs_leftover->str);

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
			printf_err("--output-format=ctf specified without --output (trace output path)\n");
			goto error;
		}

		/*
		 * At this point we know that -o ctf AND --output were
		 * specified. Make sure that no options were specified
		 * which would imply -o text because --output would be
		 * ambiguous in this case. For example, this is wrong:
		 *
		 *     babeltrace --names=all -o ctf --output=/tmp/path my-trace
		 *
		 * because --names=all implies -o text, and --output
		 * could apply to both the sink.text.pretty and
		 * sink.ctf.fs implicit components.
		 */
		if (implicit_text_args.exists) {
			printf_err("Ambiguous --output option: --output-format=ctf specified but another option implies --output-format=text\n");
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

	/* Decide where the leftover argument(s) go */
	if (g_list_length(leftovers) > 0) {
		if (implicit_lttng_live_args.exists) {
			GString *gs_leftover;

			if (g_list_length(leftovers) > 1) {
				printf_err("Too many URLs specified for --output-format=lttng-live\n");
				goto error;
			}

			gs_leftover = leftovers->data;
			lttng_live_url_parts =
				bt_common_parse_lttng_live_url(gs_leftover->str,
					error_buf, sizeof(error_buf));
			if (!lttng_live_url_parts.proto) {
				printf_err("Invalid LTTng live URL format: %s\n",
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
					gs_leftover->str);

				if (output) {
					g_string_assign(
						cfg->cmd_data.print_lttng_live_sessions.output_path,
						output);
				}

				goto end;
			}

			ret = append_implicit_component_extra_param(
				&implicit_lttng_live_args, "url",
				gs_leftover->str);
			if (ret) {
				goto error;
			}
		} else {
			/*
			 * Append one implicit component argument set
			 * for each leftover (souce.ctf.fs paths). Copy
			 * the base implicit component arguments.
			 * Note that they still have to be named later.
			 */
			ret = fill_implicit_ctf_inputs_args(
				implicit_ctf_inputs_args,
				&base_implicit_ctf_input_args, leftovers);
			if (ret) {
				goto error;
			}
		}
	}

	/*
	 * Ensure mutual exclusion between implicit `source.ctf.fs` and
	 * `source.ctf.lttng-live` components.
	 */
	if (base_implicit_ctf_input_args.exists &&
			implicit_lttng_live_args.exists) {
		printf_err("Cannot create both implicit `%s` and `%s` components\n",
			base_implicit_ctf_input_args.comp_arg->str,
			implicit_lttng_live_args.comp_arg->str);
		goto error;
	}

	/*
	 * If the implicit `source.ctf.fs` or `source.ctf.lttng-live`
	 * components exists, make sure there's at least one leftover
	 * (which is the path or URL).
	 */
	if (base_implicit_ctf_input_args.exists &&
			g_list_length(leftovers) == 0) {
		printf_err("Missing path for implicit `%s` component\n",
			base_implicit_ctf_input_args.comp_arg->str);
		goto error;
	}

	if (implicit_lttng_live_args.exists && g_list_length(leftovers) == 0) {
		printf_err("Missing URL for implicit `%s` component\n",
			implicit_lttng_live_args.comp_arg->str);
		goto error;
	}

	/* Assign names to implicit components */
	for (i = 0; i < implicit_ctf_inputs_args->len; i++) {
		struct implicit_component_args *impl_args =
			g_ptr_array_index(implicit_ctf_inputs_args, i);

		ret = assign_name_to_implicit_component(impl_args,
			"source-ctf-fs", all_names, &source_names, true);
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
		printf_err("No source component\n");
		goto error;
	}

	if (!sink_names) {
		printf_err("No sink component\n");
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
	for (i = 0; i < implicit_ctf_inputs_args->len; i++) {
		struct implicit_component_args *impl_args =
			g_ptr_array_index(implicit_ctf_inputs_args, i);

		ret = append_run_args_for_implicit_component(impl_args,
			run_args);
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
		printf_err("Cannot auto-connect components\n");
		goto error;
	}

	/*
	 * We have all the run command arguments now. Depending on
	 * --run-args, we pass this to the run command or print them
	 * here.
	 */
	if (print_run_args || print_run_args_0) {
		if (stream_intersection_mode) {
			printf_err("Cannot specify --stream-intersection with --run-args or --run-args-0\n");
			goto error;
		}

		for (i = 0; i < bt_value_array_get_size(run_args); i++) {
			const bt_value *arg_value =
				bt_value_array_borrow_element_by_index(run_args,
								       i);
			const char *arg;
			GString *quoted = NULL;
			const char *arg_to_print;

			BT_ASSERT(arg_value);
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

			if (i < bt_value_array_get_size(run_args) - 1) {
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
					    force_omit_system_plugin_path,
					    force_omit_home_plugin_path,
					    initial_plugin_paths);
	if (!cfg) {
		goto error;
	}

	cfg->cmd_data.run.stream_intersection_mode = stream_intersection_mode;
	goto end;

error:
	*retcode = 1;
	BT_OBJECT_PUT_REF_AND_RESET(cfg);

end:
	if (pc) {
		poptFreeContext(pc);
	}

	free(arg);
	free(output);

	if (cur_name) {
		g_string_free(cur_name, TRUE);
	}

	if (cur_name_prefix) {
		g_string_free(cur_name_prefix, TRUE);
	}

	if (implicit_ctf_inputs_args) {
		g_ptr_array_free(implicit_ctf_inputs_args, TRUE);
	}

	bt_value_put_ref(run_args);
	bt_value_put_ref(all_names);
	destroy_glist_of_gstring(source_names);
	destroy_glist_of_gstring(filter_names);
	destroy_glist_of_gstring(sink_names);
	destroy_glist_of_gstring(leftovers);
	finalize_implicit_component_args(&base_implicit_ctf_input_args);
	finalize_implicit_component_args(&implicit_ctf_output_args);
	finalize_implicit_component_args(&implicit_lttng_live_args);
	finalize_implicit_component_args(&implicit_dummy_args);
	finalize_implicit_component_args(&implicit_text_args);
	finalize_implicit_component_args(&implicit_debug_info_args);
	finalize_implicit_component_args(&implicit_muxer_args);
	finalize_implicit_component_args(&implicit_trimmer_args);
	bt_value_put_ref(plugin_paths);
	bt_common_destroy_lttng_live_url_parts(&lttng_live_url_parts);
	return cfg;
}

/*
 * Prints the Babeltrace 2.x general usage.
 */
static
void print_gen_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [GENERAL OPTIONS] [COMMAND] [COMMAND ARGUMENTS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "General options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -d, --debug          Enable debug mode (same as --log-level=V)\n");
	fprintf(fp, "  -h, --help           Show this help and quit\n");
	fprintf(fp, "  -l, --log-level=LVL  Set all log levels to LVL (`N`, `V`, `D`,\n");
	fprintf(fp, "                       `I`, `W` (default), `E`, or `F`)\n");
	fprintf(fp, "  -v, --verbose        Enable verbose mode (same as --log-level=I)\n");
	fprintf(fp, "  -V, --version        Show version and quit\n");
	fprintf(fp, "\n");
	fprintf(fp, "Available commands:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    convert       Convert and trim traces (default)\n");
	fprintf(fp, "    help          Get help for a plugin or a component class\n");
	fprintf(fp, "    list-plugins  List available plugins and their content\n");
	fprintf(fp, "    query         Query objects from a component class\n");
	fprintf(fp, "    run           Build a processing graph and run it\n");
	fprintf(fp, "\n");
	fprintf(fp, "Use `babeltrace COMMAND --help` to show the help of COMMAND.\n");
}

static
char log_level_from_arg(const char *arg)
{
	char level = 'U';

	if (strcmp(arg, "VERBOSE") == 0 ||
			strcmp(arg, "V") == 0) {
		level = 'V';
	} else if (strcmp(arg, "DEBUG") == 0 ||
			strcmp(arg, "D") == 0) {
		level = 'D';
	} else if (strcmp(arg, "INFO") == 0 ||
			strcmp(arg, "I") == 0) {
		level = 'I';
	} else if (strcmp(arg, "WARN") == 0 ||
			strcmp(arg, "WARNING") == 0 ||
			strcmp(arg, "W") == 0) {
		level = 'W';
	} else if (strcmp(arg, "ERROR") == 0 ||
			strcmp(arg, "E") == 0) {
		level = 'E';
	} else if (strcmp(arg, "FATAL") == 0 ||
			strcmp(arg, "F") == 0) {
		level = 'F';
	} else if (strcmp(arg, "NONE") == 0 ||
			strcmp(arg, "N") == 0) {
		level = 'N';
	}

	return level;
}

struct bt_config *bt_config_cli_args_create(int argc, const char *argv[],
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths)
{
	struct bt_config *config = NULL;
	int i;
	const char **command_argv = NULL;
	int command_argc = -1;
	const char *command_name = NULL;
	char log_level = 'U';

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
		initial_plugin_paths = bt_value_array_create();
		if (!initial_plugin_paths) {
			*retcode = 1;
			goto end;
		}
	} else {
		bt_value_get_ref(initial_plugin_paths);
	}

	if (argc <= 1) {
		print_version();
		puts("");
		print_gen_usage(stdout);
		goto end;
	}

	for (i = 1; i < argc; i++) {
		const char *cur_arg = argv[i];
		const char *next_arg = i == (argc - 1) ? NULL : argv[i + 1];

		if (strcmp(cur_arg, "-d") == 0 ||
				strcmp(cur_arg, "--debug") == 0) {
			log_level = 'V';
		} else if (strcmp(cur_arg, "-v") == 0 ||
				strcmp(cur_arg, "--verbose") == 0) {
			if (log_level != 'V' && log_level != 'D') {
				/*
				 * Legacy: do not override a previous
				 * --debug because --verbose and --debug
				 * can be specified together (in this
				 * case we want the lowest log level to
				 * apply, VERBOSE).
				 */
				log_level = 'I';
			}
		} else if (strcmp(cur_arg, "--log-level") == 0 ||
				strcmp(cur_arg, "-l") == 0) {
			if (!next_arg) {
				printf_err("Missing log level value for --log-level option\n");
				*retcode = 1;
				goto end;
			}

			log_level = log_level_from_arg(next_arg);
			if (log_level == 'U') {
				printf_err("Invalid argument for --log-level option:\n    %s\n",
					next_arg);
				*retcode = 1;
				goto end;
			}

			i++;
		} else if (strncmp(cur_arg, "--log-level=", 12) == 0) {
			const char *arg = &cur_arg[12];

			log_level = log_level_from_arg(arg);
			if (log_level == 'U') {
				printf_err("Invalid argument for --log-level option:\n    %s\n",
					arg);
				*retcode = 1;
				goto end;
			}
		} else if (strncmp(cur_arg, "-l", 2) == 0) {
			const char *arg = &cur_arg[2];

			log_level = log_level_from_arg(arg);
			if (log_level == 'U') {
				printf_err("Invalid argument for --log-level option:\n    %s\n",
					arg);
				*retcode = 1;
				goto end;
			}
		} else if (strcmp(cur_arg, "-V") == 0 ||
				strcmp(cur_arg, "--version") == 0) {
			print_version();
			goto end;
		} else if (strcmp(cur_arg, "-h") == 0 ||
				strcmp(cur_arg, "--help") == 0) {
			print_gen_usage(stdout);
			goto end;
		} else {
			/*
			 * First unknown argument: is it a known command
			 * name?
			 */
			command_argv = &argv[i];
			command_argc = argc - i;

			if (strcmp(cur_arg, "convert") == 0) {
				command_type = COMMAND_TYPE_CONVERT;
			} else if (strcmp(cur_arg, "list-plugins") == 0) {
				command_type = COMMAND_TYPE_LIST_PLUGINS;
			} else if (strcmp(cur_arg, "help") == 0) {
				command_type = COMMAND_TYPE_HELP;
			} else if (strcmp(cur_arg, "query") == 0) {
				command_type = COMMAND_TYPE_QUERY;
			} else if (strcmp(cur_arg, "run") == 0) {
				command_type = COMMAND_TYPE_RUN;
			} else {
				/*
				 * Unknown argument, but not a known
				 * command name: assume the default
				 * `convert` command.
				 */
				command_type = COMMAND_TYPE_CONVERT;
				command_name = "convert";
				command_argv = &argv[i - 1];
				command_argc = argc - i + 1;
			}
			break;
		}
	}

	if (command_type == COMMAND_TYPE_NONE) {
		/*
		 * We only got non-help, non-version general options
		 * like --verbose and --debug, without any other
		 * arguments, so we can't do anything useful: print the
		 * usage and quit.
		 */
		print_gen_usage(stdout);
		goto end;
	}

	BT_ASSERT(command_argv);
	BT_ASSERT(command_argc >= 0);

	switch (command_type) {
	case COMMAND_TYPE_RUN:
		config = bt_config_run_from_args(command_argc, command_argv,
			retcode, force_omit_system_plugin_path,
			force_omit_home_plugin_path, initial_plugin_paths);
		break;
	case COMMAND_TYPE_CONVERT:
		config = bt_config_convert_from_args(command_argc, command_argv,
			retcode, force_omit_system_plugin_path,
			force_omit_home_plugin_path,
			initial_plugin_paths, &log_level);
		break;
	case COMMAND_TYPE_LIST_PLUGINS:
		config = bt_config_list_plugins_from_args(command_argc,
			command_argv, retcode, force_omit_system_plugin_path,
			force_omit_home_plugin_path, initial_plugin_paths);
		break;
	case COMMAND_TYPE_HELP:
		config = bt_config_help_from_args(command_argc,
			command_argv, retcode, force_omit_system_plugin_path,
			force_omit_home_plugin_path, initial_plugin_paths);
		break;
	case COMMAND_TYPE_QUERY:
		config = bt_config_query_from_args(command_argc,
			command_argv, retcode, force_omit_system_plugin_path,
			force_omit_home_plugin_path, initial_plugin_paths);
		break;
	default:
		abort();
	}

	if (config) {
		if (log_level == 'U') {
			log_level = 'W';
		}

		config->log_level = log_level;
		config->command_name = command_name;
	}

end:
	bt_value_put_ref(initial_plugin_paths);
	return config;
}
