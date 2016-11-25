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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/values.h>
#include <popt.h>
#include <glib.h>
#include "babeltrace-cfg.h"

/*
 * Error printf() macro which prepends "Error: " the first time it's
 * called. This gives a nicer feel than having a bunch of error prefixes
 * (since the following lines usually describe the error and possible
 * solutions), or the error prefix just at the end.
 */
#define printf_err(fmt, args...)			\
	do {						\
		if (is_first_error) {			\
			fprintf(stderr, "Error: ");	\
			is_first_error = false;		\
		}					\
		fprintf(stderr, fmt, ##args);		\
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
	struct bt_value *params;

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
	struct bt_value *names;
	struct bt_value *fields;

	/* Flags */
	bool no_delta;
	bool clock_cycles;
	bool clock_seconds;
	bool clock_date;
	bool clock_gmt;
	bool dbg_info_full_path;
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
	LEGACY_OUTPUT_FORMAT_CTF_METADATA,
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
 * Prints duplicate legacy output format error.
 */
static
void print_err_dup_legacy_output(void)
{
	printf_err("More than one legacy output format specified\n");
}

/*
 * Prints duplicate legacy input format error.
 */
static
void print_err_dup_legacy_input(void)
{
	printf_err("More than one legacy input format specified\n");
}

/*
 * Checks if any of the "text" legacy options is set.
 */
static
bool text_legacy_opts_is_any_set(struct text_legacy_opts *opts)
{
	return (opts->output && opts->output->len > 0) ||
		(opts->dbg_info_dir && opts->dbg_info_dir->len > 0) ||
		(opts->dbg_info_target_prefix &&
			opts->dbg_info_target_prefix->len > 0) ||
		bt_value_array_size(opts->names) > 0 ||
		bt_value_array_size(opts->fields) > 0 ||
		opts->no_delta || opts->clock_cycles || opts->clock_seconds ||
		opts->clock_date || opts->clock_gmt ||
		opts->dbg_info_full_path;
}

/*
 * Checks if any of the "ctf" legacy options is set.
 */
static
bool ctf_legacy_opts_is_any_set(struct ctf_legacy_opts *opts)
{
	return opts->offset_s.is_set || opts->offset_ns.is_set ||
		opts->stream_intersection;
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
	struct bt_value *value = NULL;

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

		if (bt_value_map_has_key(state->params, state->last_map_key)) {
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

			value = bt_value_integer_create_init(
				(int64_t) int_val);
			break;
		}
		case G_TOKEN_FLOAT:
			/* Positive floating point number */
			value = bt_value_float_create_init(
				state->scanner->value.v_float);
			break;
		case G_TOKEN_STRING:
			/* Quoted string */
			value = bt_value_string_create_init(
				state->scanner->value.v_string);
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

			value = bt_value_integer_create_init(
				-((int64_t) int_val));
			break;
		}
		case G_TOKEN_FLOAT:
			/* Negative floating point number */
			value = bt_value_float_create_init(
				-state->scanner->value.v_float);
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
		assert(false);
	}

error:
	ret = -1;
	goto end;

success:
	if (value) {
		if (bt_value_map_insert(state->params,
				state->last_map_key, value)) {
			/* Only override return value on error */
			ret = -1;
		}
	}

end:
	BT_PUT(value);
	return ret;
}

/*
 * Converts an INI-style argument to an equivalent map value object.
 *
 * Return value is owned by the caller.
 */
static
struct bt_value *bt_value_from_ini(const char *arg, GString *ini_error)
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
	BT_PUT(state.params);

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
struct bt_value *bt_value_from_arg(const char *arg)
{
	struct bt_value *params = NULL;
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
 * Returns the plugin and component names from a command-line
 * source/sink option's argument. arg must have the following format:
 *
 *     PLUGIN.COMPONENT
 *
 * where PLUGIN is the plugin name, and COMPONENT is the component
 * name.
 *
 * On success, both *plugin and *component are not NULL. *plugin
 * and *component are owned by the caller.
 */
static
void plugin_component_names_from_arg(const char *arg, char **plugin,
		char **component)
{
	const char *dot;
	const char *end;
	size_t plugin_len;
	size_t component_len;

	/* Initialize both return values to NULL: not found */
	*plugin = NULL;
	*component = NULL;

	dot = strchr(arg, '.');
	if (!dot) {
		/* No dot */
		goto end;
	}

	end = arg + strlen(arg);
	plugin_len = dot - arg;
	component_len = end - dot - 1;
	if (plugin_len == 0 || component_len == 0) {
		goto end;
	}

	*plugin = g_malloc0(plugin_len + 1);
	if (!*plugin) {
		print_err_oom();
		goto end;
	}

	g_strlcpy(*plugin, arg, plugin_len + 1);
	*component = g_malloc0(component_len + 1);
	if (!*component) {
		print_err_oom();
		goto end;
	}

	g_strlcpy(*component, dot + 1, component_len + 1);

end:
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
 * Prints the legacy, Babeltrace 1.x command usage. Those options are
 * still compatible in Babeltrace 2.x, but it is recommended to use
 * the more generic plugin/component parameters instead of those
 * hard-coded option names.
 */
static
void print_legacy_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [OPTIONS] INPUT...\n");
	fprintf(fp, "\n");
	fprintf(fp, "The following options are compatible with the Babeltrace 1.x options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --help-legacy            Show this help\n");
	fprintf(fp, "  -V, --version                Show version\n");
	fprintf(fp, "      --clock-force-correlate  Assume that clocks are inherently correlated\n");
	fprintf(fp, "                               across traces\n");
	fprintf(fp, "  -d, --debug                  Enable debug mode\n");
	fprintf(fp, "  -i, --input-format=FORMAT    Input trace format (default: ctf)\n");
	fprintf(fp, "  -l, --list                   List available formats\n");
	fprintf(fp, "  -o, --output-format=FORMAT   Output trace format (default: text)\n");
	fprintf(fp, "  -v, --verbose                Enable verbose output\n");
	fprintf(fp, "\n");
	fprintf(fp, "  Available input formats:  ctf, lttng-live, ctf-metadata\n");
	fprintf(fp, "  Available output formats: text, dummy\n");
	fprintf(fp, "\n");
	fprintf(fp, "Input formats specific options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  INPUT...                     Input trace file(s), directory(ies), or URLs\n");
	fprintf(fp, "      --clock-offset=SEC       Set clock offset to SEC seconds\n");
	fprintf(fp, "      --clock-offset-ns=NS     Set clock offset to NS nanoseconds\n");
	fprintf(fp, "      --stream-intersection    Only process events when all streams are active\n");
	fprintf(fp, "\n");
	fprintf(fp, "text output format specific options:\n");
	fprintf(fp, "  \n");
	fprintf(fp, "      --clock-cycles           Print timestamps in clock cycles\n");
	fprintf(fp, "      --clock-date             Print timestamp dates\n");
	fprintf(fp, "      --clock-gmt              Print timestamps in GMT time zone\n");
	fprintf(fp, "                               (default: local time zone)\n");
	fprintf(fp, "      --clock-seconds          Print the timestamps as [SEC.NS]\n");
	fprintf(fp, "                               (default format: [HH:MM:SS.NS])\n");
	fprintf(fp, "      --debug-info-dir=DIR     Search for debug info in directory DIR\n");
	fprintf(fp, "                               (default: `/usr/lib/debug`)\n");
	fprintf(fp, "      --debug-info-full-path   Show full debug info source and binary paths\n");
	fprintf(fp, "      --debug-info-target-prefix=DIR  Use directory DIR as a prefix when looking\n");
	fprintf(fp, "                                      up executables during debug info analysis\n");
	fprintf(fp, "                               (default: `/usr/lib/debug`)\n");
	fprintf(fp, "  -f, --fields=NAME[,NAME]...  Print additional fields:\n");
	fprintf(fp, "                                 all, trace, trace:hostname, trace:domain,\n");
	fprintf(fp, "                                 trace:procname, trace:vpid, loglevel, emf\n");
	fprintf(fp, "                                 (default: trace:hostname, trace:procname,\n");
	fprintf(fp, "                                           trace:vpid)\n");
	fprintf(fp, "  -n, --names=NAME[,NAME]...   Print field names:\n");
	fprintf(fp, "                                 payload (or arg or args)\n");
	fprintf(fp, "                                 none, all, scope, header, context (or ctx)\n");
	fprintf(fp, "                                 (default: payload, context)\n");
	fprintf(fp, "      --no-delta               Do not print time delta between consecutive\n");
	fprintf(fp, "                               events\n");
	fprintf(fp, "  -w, --output=PATH            Write output to PATH (default: standard output)\n");
}

/*
 * Prints the Babeltrace 2.x usage.
 */
static
void print_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [OPTIONS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -B, --base-begin-ns=NS            Set NS as the current base beginning\n");
	fprintf(fp, "                                    timestamp of the following source component\n");
	fprintf(fp, "                                    instances\n");
	fprintf(fp, "  -E, --base-end-ns=NS              Set NS as the current base end timestamp\n");
	fprintf(fp, "                                    of the following source component instances\n");
	fprintf(fp, "  -b, --base-params=PARAMS          Set PARAMS as the current base parameters\n");
	fprintf(fp, "                                    of the following source and sink component\n");
	fprintf(fp, "                                    instances (see the exact format of PARAMS\n");
	fprintf(fp, "                                    below)\n");
	fprintf(fp, "      --begin-ns=NS                 Set the beginning timestamp of the latest\n");
	fprintf(fp, "                                    source component instance to NS\n");
	fprintf(fp, "  -d, --debug                       Enable debug mode\n");
	fprintf(fp, "      --end-ns=NS                   Set the end timestamp of the latest source\n");
	fprintf(fp, "                                    component instance to NS\n");
	fprintf(fp, "  -l, --list                        List available plugins and their components\n");
	fprintf(fp, "  -P, --path=PATH                   Set the `path` parameter of the latest source\n");
	fprintf(fp, "                                    or sink component to PATH\n");
	fprintf(fp, "  -p, --params=PARAMS               Set the parameters of the latest source or\n");
	fprintf(fp, "                                    sink component instance (in command-line \n");
	fprintf(fp, "                                    order) to PARAMS (see the exact format of\n");
	fprintf(fp, "                                    PARAMS below)\n");
	fprintf(fp, "      --plugin-path=PATH[:PATH]...  Set paths from which dynamic plugins can be\n");
	fprintf(fp, "                                    loaded to PATH\n");
	fprintf(fp, "      --reset-base-begin-ns         Reset the current base beginning timestamp\n");
	fprintf(fp, "                                    of the following source component instances\n");
	fprintf(fp, "      --reset-base-end-ns           Reset the current base end timestamp of the\n");
	fprintf(fp, "                                    following source component instances\n");
	fprintf(fp, "  -r, --reset-base-params           Reset the current base parameters of the\n");
	fprintf(fp, "                                    following source and sink component\n");
	fprintf(fp, "                                    instances to an empty map\n");
	fprintf(fp, "  -o, --sink=PLUGIN.COMPCLS         Instantiate a sink component from plugin\n");
	fprintf(fp, "                                    PLUGIN and component class COMPCLS (may be\n");
	fprintf(fp, "                                    repeated)\n");
	fprintf(fp, "  -i, --source=PLUGIN.COMPCLS       Instantiate a source component from plugin\n");
	fprintf(fp, "                                    PLUGIN and component class COMPCLS (may be\n");
	fprintf(fp, "                                    repeated)\n");
	fprintf(fp, "  -h  --help                        Show this help\n");
	fprintf(fp, "      --help-legacy                 Show Babeltrace 1.x legacy options\n");
	fprintf(fp, "  -v, --verbose                     Enable verbose output\n");
	fprintf(fp, "  -V, --version                     Show version\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Format of PARAMS\n");
	fprintf(fp, "----------------\n");
	fprintf(fp, "\n");
	fprintf(fp, "    PARAM=VALUE[,PARAM=VALUE]...\n");
	fprintf(fp, "\n");
	fprintf(fp, "The parameter string is a comma-separated list of PARAM=VALUE assignments,\n");
	fprintf(fp, "where PARAM is the parameter name (C identifier plus [:.-] characters), and\n");
	fprintf(fp, "VALUE can be one of:\n");
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
	fprintf(fp, "Whitespaces are allowed around individual `=` and `,` tokens.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Example:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    many=null, fresh=yes, condition=false, squirrel=-782329,\n");
	fprintf(fp, "    observe=3.14, simple=beef, needs-quotes=\"some string\",\n");
	fprintf(fp, "    escape.chars-are:allowed=\"this is a \\\" double quote\"\n");
	fprintf(fp, "\n");
	fprintf(fp, "IMPORTANT: Make sure to single-quote the whole argument when you run babeltrace\n");
	fprintf(fp, "from a shell.\n");
}

/*
 * Destroys a component configuration.
 */
static
void bt_config_component_destroy(struct bt_object *obj)
{
	struct bt_config_component *bt_config_component =
		container_of(obj, struct bt_config_component, base);

	if (!obj) {
		goto end;
	}

	if (bt_config_component->plugin_name) {
		g_string_free(bt_config_component->plugin_name, TRUE);
	}

	if (bt_config_component->component_name) {
		g_string_free(bt_config_component->component_name, TRUE);
	}

	BT_PUT(bt_config_component->params);
	g_free(bt_config_component);

end:
	return;
}

/*
 * Creates a component configuration using the given plugin name and
 * component name. plugin_name and component_name are copied (belong to
 * the return value).
 *
 * Return value is owned by the caller.
 */
static
struct bt_config_component *bt_config_component_create(const char *plugin_name,
		const char *component_name)
{
	struct bt_config_component *cfg_component = NULL;

	cfg_component = g_new0(struct bt_config_component, 1);
	if (!cfg_component) {
		print_err_oom();
		goto error;
	}

	bt_object_init(cfg_component, bt_config_component_destroy);
	cfg_component->plugin_name = g_string_new(plugin_name);
	if (!cfg_component->plugin_name) {
		print_err_oom();
		goto error;
	}

	cfg_component->component_name = g_string_new(component_name);
	if (!cfg_component->component_name) {
		print_err_oom();
		goto error;
	}

	/* Start with empty parameters */
	cfg_component->params = bt_value_map_create();
	if (!cfg_component->params) {
		print_err_oom();
		goto error;
	}

	/* Begin/end timestamp: not set */
	cfg_component->begin_ns = -1ULL;
	cfg_component->end_ns = -1ULL;

	goto end;

error:
	BT_PUT(cfg_component);

end:
	return cfg_component;
}

/*
 * Creates a component configuration from a command-line source/sink
 * option's argument.
 */
static
struct bt_config_component *bt_config_component_from_arg(const char *arg)
{
	struct bt_config_component *bt_config_component = NULL;
	char *plugin_name;
	char *component_name;

	plugin_component_names_from_arg(arg, &plugin_name, &component_name);
	if (!plugin_name || !component_name) {
		printf_err("Cannot get plugin or component class name\n");
		goto error;
	}

	bt_config_component = bt_config_component_create(plugin_name,
		component_name);
	if (!bt_config_component) {
		goto error;
	}

	goto end;

error:
	BT_PUT(bt_config_component);

end:
	g_free(plugin_name);
	g_free(component_name);
	return bt_config_component;
}

/*
 * Destroys a configuration.
 */
static
void bt_config_destroy(struct bt_object *obj)
{
	struct bt_config *bt_config =
		container_of(obj, struct bt_config, base);

	if (!obj) {
		goto end;
	}

	if (bt_config->sources) {
		g_ptr_array_free(bt_config->sources, TRUE);
	}

	if (bt_config->sinks) {
		g_ptr_array_free(bt_config->sinks, TRUE);
	}

	BT_PUT(bt_config->plugin_paths);
	g_free(bt_config);

end:
	return;
}

/*
 * Extracts the various paths from the string arg, delimited by ':',
 * and converts them to an array value object.
 *
 * Returned array value object is empty if arg is empty.
 *
 * Return value is owned by the caller.
 */
static
struct bt_value *plugin_paths_from_arg(const char *arg)
{
	struct bt_value *plugin_paths;
	const char *at = arg;
	const char *end = arg + strlen(arg);

	plugin_paths = bt_value_array_create();
	if (!plugin_paths) {
		print_err_oom();
		goto error;
	}

	while (at < end) {
		int ret;
		GString *path;
		const char *next_colon;

		next_colon = strchr(at, ':');
		if (next_colon == at) {
			/*
			 * Empty path: try next character (supported
			 * to conform to the typical parsing of $PATH).
			 */
			at++;
			continue;
		} else if (!next_colon) {
			/* No more colon: use the remaining */
			next_colon = arg + strlen(arg);
		}

		path = g_string_new(NULL);
		if (!path) {
			print_err_oom();
			goto error;
		}

		g_string_append_len(path, at, next_colon - at);
		at = next_colon + 1;
		ret = bt_value_array_append_string(plugin_paths, path->str);
		g_string_free(path, TRUE);
		if (ret) {
			print_err_oom();
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(plugin_paths);

end:
	return plugin_paths;
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

	return g_scanner_new(&scanner_config);
}

/*
 * Inserts a string (if exists and not empty) or null to a map value
 * object.
 */
static
enum bt_value_status map_insert_string_or_null(struct bt_value *map,
		const char *key, GString *string)
{
	enum bt_value_status ret;

	if (string && string->len > 0) {
		ret = bt_value_map_insert_string(map, key, string->str);
	} else {
		ret = bt_value_map_insert(map, key, bt_value_null);
	}
	return ret;
}

/*
 * Converts a comma-delimited list of known names (--names option) to
 * an array value object containing those names as string value objects.
 *
 * Return value is owned by the caller.
 */
static
struct bt_value *names_from_arg(const char *arg)
{
	GScanner *scanner = NULL;
	struct bt_value *names = NULL;
	bool found_all = false, found_none = false, found_item = false;

	names = bt_value_array_create();
	if (!names) {
		print_err_oom();
		goto error;
	}

	scanner = create_csv_identifiers_scanner();
	if (!scanner) {
		print_err_oom();
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
				if (bt_value_array_append_string(names,
						"payload")) {
					goto error;
				}
			} else if (!strcmp(identifier, "context") ||
					!strcmp(identifier, "ctx")) {
				found_item = true;
				if (bt_value_array_append_string(names,
						"context")) {
					goto error;
				}
			} else if (!strcmp(identifier, "scope") ||
					!strcmp(identifier, "header")) {
				found_item = true;
				if (bt_value_array_append_string(names,
						identifier)) {
					goto error;
				}
			} else if (!strcmp(identifier, "all")) {
				found_all = true;
				if (bt_value_array_append_string(names,
						identifier)) {
					goto error;
				}
			} else if (!strcmp(identifier, "none")) {
				found_none = true;
				if (bt_value_array_append_string(names,
						identifier)) {
					goto error;
				}
			} else {
				printf_err("Unknown field name: `%s`\n",
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
		if (bt_value_array_append_string(names, "none")) {
			goto error;
		}
	}
	if (scanner) {
		g_scanner_destroy(scanner);
	}
	return names;

error:
	BT_PUT(names);
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
struct bt_value *fields_from_arg(const char *arg)
{
	GScanner *scanner = NULL;
	struct bt_value *fields;

	fields = bt_value_array_create();
	if (!fields) {
		print_err_oom();
		goto error;
	}

	scanner = create_csv_identifiers_scanner();
	if (!scanner) {
		print_err_oom();
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
				if (bt_value_array_append_string(fields,
						identifier)) {
					goto error;
				}
			} else {
				printf_err("Unknown field name: `%s`\n",
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
	BT_PUT(fields);

end:
	if (scanner) {
		g_scanner_destroy(scanner);
	}
	return fields;
}

/*
 * Inserts the equivalent "prefix-name" true boolean value objects into
 * map_obj where the names are in array_obj.
 */
static
int insert_flat_names_fields_from_array(struct bt_value *map_obj,
		struct bt_value *array_obj, const char *prefix)
{
	int ret = 0;
	int i;
	GString *tmpstr = NULL, *default_value = NULL;

	/*
	 * array_obj may be NULL if no CLI options were specified to
	 * trigger its creation.
	 */
	if (!array_obj) {
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

	for (i = 0; i < bt_value_array_size(array_obj); i++) {
		struct bt_value *str_obj = bt_value_array_get(array_obj, i);
		const char *suffix;
		bool is_default = false;

		if (!str_obj) {
			printf_err("Unexpected error\n");
			ret = -1;
			goto end;
		}

		ret = bt_value_string_get(str_obj, &suffix);
		BT_PUT(str_obj);
		if (ret) {
			printf_err("Unexpected error\n");
			goto end;
		}

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
			g_string_append(tmpstr, "default");
			ret = map_insert_string_or_null(map_obj,
					tmpstr->str,
					default_value);
			if (ret) {
				print_err_oom();
				goto end;
			}
		} else {
			g_string_append(tmpstr, suffix);
			ret = bt_value_map_insert_bool(map_obj, tmpstr->str,
					true);
			if (ret) {
				print_err_oom();
				goto end;
			}
		}
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

/*
 * Returns the parameters (map value object) corresponding to the
 * legacy text format options.
 *
 * Return value is owned by the caller.
 */
static
struct bt_value *params_from_text_legacy_opts(
		struct text_legacy_opts *text_legacy_opts)
{
	struct bt_value *params;

	params = bt_value_map_create();
	if (!params) {
		print_err_oom();
		goto error;
	}

	if (map_insert_string_or_null(params, "output-path",
			text_legacy_opts->output)) {
		print_err_oom();
		goto error;
	}

	if (map_insert_string_or_null(params, "debug-info-dir",
			text_legacy_opts->dbg_info_dir)) {
		print_err_oom();
		goto error;
	}

	if (map_insert_string_or_null(params, "debug-info-target-prefix",
			text_legacy_opts->dbg_info_target_prefix)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "debug-info-full-path",
			text_legacy_opts->dbg_info_full_path)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "no-delta",
			text_legacy_opts->no_delta)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "clock-cycles",
			text_legacy_opts->clock_cycles)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "clock-seconds",
			text_legacy_opts->clock_seconds)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "clock-date",
			text_legacy_opts->clock_date)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "clock-gmt",
			text_legacy_opts->clock_gmt)) {
		print_err_oom();
		goto error;
	}

	if (insert_flat_names_fields_from_array(params,
			text_legacy_opts->names, "name")) {
		goto error;
	}

	if (insert_flat_names_fields_from_array(params,
			text_legacy_opts->fields, "field")) {
		goto error;
	}

	goto end;

error:
	BT_PUT(params);

end:
	return params;
}

static
int append_sinks_from_legacy_opts(GPtrArray *sinks,
		enum legacy_output_format legacy_output_format,
		struct text_legacy_opts *text_legacy_opts)
{
	int ret = 0;
	struct bt_value *params = NULL;
	const char *plugin_name;
	const char *component_name;
	struct bt_config_component *bt_config_component = NULL;

	switch (legacy_output_format) {
	case LEGACY_OUTPUT_FORMAT_TEXT:
		plugin_name = "text";
		component_name = "text";
		break;
	case LEGACY_OUTPUT_FORMAT_CTF_METADATA:
		plugin_name = "ctf";
		component_name = "metadata-text";
		break;
	case LEGACY_OUTPUT_FORMAT_DUMMY:
		plugin_name = "dummy";
		component_name = "dummy";
		break;
	default:
		assert(false);
		break;
	}

	if (legacy_output_format == LEGACY_OUTPUT_FORMAT_TEXT) {
		/* Legacy "text" output format has parameters */
		params = params_from_text_legacy_opts(text_legacy_opts);
		if (!params) {
			goto error;
		}
	} else {
		/*
		 * Legacy "dummy" and "ctf-metadata" output formats do
		 * not have parameters.
		 */
		params = bt_value_map_create();
		if (!params) {
			print_err_oom();
			goto error;
		}
	}

	/* Create a component configuration */
	bt_config_component = bt_config_component_create(plugin_name,
		component_name);
	if (!bt_config_component) {
		goto error;
	}

	BT_MOVE(bt_config_component->params, params);

	/* Move created component configuration to the array */
	g_ptr_array_add(sinks, bt_config_component);

	goto end;

error:
	ret = -1;

end:
	BT_PUT(params);

	return ret;
}

/*
 * Returns the parameters (map value object) corresponding to the
 * given legacy CTF format options.
 *
 * Return value is owned by the caller.
 */
static
struct bt_value *params_from_ctf_legacy_opts(
		struct ctf_legacy_opts *ctf_legacy_opts)
{
	struct bt_value *params;

	params = bt_value_map_create();
	if (!params) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_integer(params, "offset-s",
			ctf_legacy_opts->offset_s.value)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_integer(params, "offset-ns",
			ctf_legacy_opts->offset_ns.value)) {
		print_err_oom();
		goto error;
	}

	if (bt_value_map_insert_bool(params, "stream-intersection",
			ctf_legacy_opts->stream_intersection)) {
		print_err_oom();
		goto error;
	}

	goto end;

error:
	BT_PUT(params);

end:
	return params;
}

static
int append_sources_from_legacy_opts(GPtrArray *sources,
		enum legacy_input_format legacy_input_format,
		struct ctf_legacy_opts *ctf_legacy_opts,
		struct bt_value *legacy_input_paths,
		uint64_t begin_ns, uint64_t end_ns)
{
	int ret = 0;
	int i;
	struct bt_value *base_params;
	struct bt_value *params = NULL;
	struct bt_value *input_path = NULL;
	struct bt_value *input_path_copy = NULL;
	const char *input_key;
	const char *component_name;

	switch (legacy_input_format) {
	case LEGACY_INPUT_FORMAT_CTF:
		input_key = "path";
		component_name = "fs";
		break;
	case LEGACY_INPUT_FORMAT_LTTNG_LIVE:
		input_key = "url";
		component_name = "lttng-live";
		break;
	default:
		assert(false);
		break;
	}

	base_params = params_from_ctf_legacy_opts(ctf_legacy_opts);
	if (!base_params) {
		goto error;
	}

	for (i = 0; i < bt_value_array_size(legacy_input_paths); i++) {
		struct bt_config_component *bt_config_component = NULL;

		/* Copy base parameters as current parameters */
		params = bt_value_copy(base_params);
		if (!params) {
			goto error;
		}

		/* Get current input path string value object */
		input_path = bt_value_array_get(legacy_input_paths, i);
		if (!input_path) {
			goto error;
		}

		/* Copy current input path value object */
		input_path_copy = bt_value_copy(input_path);
		if (!input_path_copy) {
			goto error;
		}

		/* Insert input path value object into current parameters */
		ret = bt_value_map_insert(params, input_key, input_path_copy);
		if (ret) {
			goto error;
		}

		/* Create a component configuration */
		bt_config_component = bt_config_component_create("ctf",
			component_name);
		if (!bt_config_component) {
			goto error;
		}

		BT_MOVE(bt_config_component->params, params);
		bt_config_component->begin_ns = begin_ns;
		bt_config_component->end_ns = end_ns;

		/* Move created component configuration to the array */
		g_ptr_array_add(sources, bt_config_component);

		/* Put current stuff */
		BT_PUT(input_path);
		BT_PUT(input_path_copy);
	}

	goto end;

error:
	ret = -1;

end:
	BT_PUT(base_params);
	BT_PUT(params);
	BT_PUT(input_path);
	BT_PUT(input_path_copy);
	return ret;
}

/*
 * Escapes a string for the shell. The string is escaped knowing that
 * it's a parameter string value (double-quoted), and that it will be
 * entered between single quotes in the shell.
 *
 * Return value is owned by the caller.
 */
static
char *str_shell_escape(const char *input)
{
	char *ret = NULL;
	const char *at = input;
	GString *str = g_string_new(NULL);

	if (!str) {
		goto end;
	}

	while (*at != '\0') {
		switch (*at) {
		case '\\':
			g_string_append(str, "\\\\");
			break;
		case '"':
			g_string_append(str, "\\\"");
			break;
		case '\'':
			g_string_append(str, "'\"'\"'");
			break;
		case '\n':
			g_string_append(str, "\\n");
			break;
		case '\t':
			g_string_append(str, "\\t");
			break;
		default:
			g_string_append_c(str, *at);
			break;
		}

		at++;
	}

end:
	if (str) {
		ret = str->str;
		g_string_free(str, FALSE);
	}

	return ret;
}

static
int append_prefixed_flag_params(GString *str, struct bt_value *flags,
		const char *prefix)
{
	int ret = 0;
	int i;

	if (!flags) {
		goto end;
	}

	for (i = 0; i < bt_value_array_size(flags); i++) {
		struct bt_value *value = bt_value_array_get(flags, i);
		const char *flag;

		if (!value) {
			ret = -1;
			goto end;
		}

		if (bt_value_string_get(value, &flag)) {
			BT_PUT(value);
			ret = -1;
			goto end;
		}

		g_string_append_printf(str, ",%s-%s=true", prefix, flag);
		BT_PUT(value);
	}

end:
	return ret;
}

/*
 * Appends a boolean parameter string.
 */
static
void g_string_append_bool_param(GString *str, const char *name, bool value)
{
	g_string_append_printf(str, ",%s=%s", name, value ? "true" : "false");
}

/*
 * Appends a path parameter string, or null if it's empty.
 */
static
int g_string_append_string_path_param(GString *str, const char *name,
		GString *path)
{
	int ret = 0;

	if (path->len > 0) {
		char *escaped_path = str_shell_escape(path->str);

		if (!escaped_path) {
			print_err_oom();
			goto error;
		}

		g_string_append_printf(str, "%s=\"%s\"", name, escaped_path);
		free(escaped_path);
	} else {
		g_string_append_printf(str, "%s=null", name);
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

/*
 * Prints the non-legacy sink options equivalent to the specified
 * legacy output format options.
 */
static
void print_output_legacy_to_sinks(
		enum legacy_output_format legacy_output_format,
		struct text_legacy_opts *text_legacy_opts)
{
	const char *input_format;
	GString *str = NULL;

	str = g_string_new("    ");
	if (!str) {
		print_err_oom();
		goto end;
	}

	switch (legacy_output_format) {
	case LEGACY_OUTPUT_FORMAT_TEXT:
		input_format = "text";
		break;
	case LEGACY_OUTPUT_FORMAT_CTF_METADATA:
		input_format = "ctf-metadata";
		break;
	case LEGACY_OUTPUT_FORMAT_DUMMY:
		input_format = "dummy";
		break;
	default:
		assert(false);
	}

	printf_err("Both `%s` legacy output format and non-legacy sink component\ninstances(s) specified.\n\n",
		input_format);
	printf_err("Specify the following non-legacy sink component instance instead of the\nlegacy `%s` output format options:\n\n",
		input_format);
	g_string_append(str, "-o ");

	switch (legacy_output_format) {
	case LEGACY_OUTPUT_FORMAT_TEXT:
		g_string_append(str, "text.text");
		break;
	case LEGACY_OUTPUT_FORMAT_CTF_METADATA:
		g_string_append(str, "ctf.metadata-text");
		break;
	case LEGACY_OUTPUT_FORMAT_DUMMY:
		g_string_append(str, "dummy.dummy");
		break;
	default:
		assert(false);
	}

	if (legacy_output_format == LEGACY_OUTPUT_FORMAT_TEXT &&
			text_legacy_opts_is_any_set(text_legacy_opts)) {
		int ret;

		g_string_append(str, " -p '");

		if (g_string_append_string_path_param(str, "output-path",
				text_legacy_opts->output)) {
			goto end;
		}

		g_string_append(str, ",");

		if (g_string_append_string_path_param(str, "debug-info-dir",
				text_legacy_opts->dbg_info_dir)) {
			goto end;
		}

		g_string_append(str, ",");

		if (g_string_append_string_path_param(str,
				"debug-info-target-prefix",
				text_legacy_opts->dbg_info_target_prefix)) {
			goto end;
		}

		g_string_append_bool_param(str, "no-delta",
			text_legacy_opts->no_delta);
		g_string_append_bool_param(str, "clock-cycles",
			text_legacy_opts->clock_cycles);
		g_string_append_bool_param(str, "clock-seconds",
			text_legacy_opts->clock_seconds);
		g_string_append_bool_param(str, "clock-date",
			text_legacy_opts->clock_date);
		g_string_append_bool_param(str, "clock-gmt",
			text_legacy_opts->clock_gmt);
		ret = append_prefixed_flag_params(str, text_legacy_opts->names,
			"name");
		if (ret) {
			goto end;
		}

		ret = append_prefixed_flag_params(str, text_legacy_opts->fields,
			"field");
		if (ret) {
			goto end;
		}

		/* Remove last comma and close single quote */
		g_string_append(str, "'");
	}

	printf_err("%s\n\n", str->str);

end:
	if (str) {
		g_string_free(str, TRUE);
	}
	return;
}

/*
 * Prints the non-legacy source options equivalent to the specified
 * legacy input format options.
 */
static
void print_input_legacy_to_sources(enum legacy_input_format legacy_input_format,
		struct bt_value *legacy_input_paths,
		struct ctf_legacy_opts *ctf_legacy_opts)
{
	const char *input_format;
	GString *str = NULL;
	int i;

	str = g_string_new("    ");
	if (!str) {
		print_err_oom();
		goto end;
	}

	switch (legacy_input_format) {
	case LEGACY_INPUT_FORMAT_CTF:
		input_format = "ctf";
		break;
	case LEGACY_INPUT_FORMAT_LTTNG_LIVE:
		input_format = "lttng-live";
		break;
	default:
		assert(false);
	}

	printf_err("Both `%s` legacy input format and non-legacy source component\ninstance(s) specified.\n\n",
		input_format);
	printf_err("Specify the following non-legacy source component instance(s) instead of the\nlegacy `%s` input format options and positional arguments:\n\n",
		input_format);

	for (i = 0; i < bt_value_array_size(legacy_input_paths); i++) {
		struct bt_value *input_value =
			bt_value_array_get(legacy_input_paths, i);
		const char *input = NULL;
		char *escaped_input;
		int ret;

		assert(input_value);
		ret = bt_value_string_get(input_value, &input);
		BT_PUT(input_value);
		assert(!ret && input);
		escaped_input = str_shell_escape(input);
		if (!escaped_input) {
			print_err_oom();
			goto end;
		}

		g_string_append(str, "-i ctf.");

		switch (legacy_input_format) {
		case LEGACY_INPUT_FORMAT_CTF:
			g_string_append(str, "fs -p 'path=\"");
			break;
		case LEGACY_INPUT_FORMAT_LTTNG_LIVE:
			g_string_append(str, "lttng-live -p 'url=\"");
			break;
		default:
			assert(false);
		}

		g_string_append(str, escaped_input);
		g_string_append(str, "\"");
		g_string_append_printf(str, ",offset-s=%" PRId64,
			ctf_legacy_opts->offset_s.value);
		g_string_append_printf(str, ",offset-ns=%" PRId64,
			ctf_legacy_opts->offset_ns.value);
		g_string_append_bool_param(str, "stream-intersection",
			ctf_legacy_opts->stream_intersection);
		g_string_append(str, "' ");
		g_free(escaped_input);
	}

	printf_err("%s\n\n", str->str);

end:
	if (str) {
		g_string_free(str, TRUE);
	}
	return;
}

/*
 * Validates a given source component configuration.
 */
static
bool validate_source_config_component(struct bt_config_component *cfg_comp)
{
	bool valid = false;

	if (cfg_comp->begin_ns != -1ULL && cfg_comp->end_ns != -1ULL) {
		if (cfg_comp->begin_ns > cfg_comp->end_ns) {
			printf_err("Beginning timestamp (%" PRIu64 ") is greater than end timestamp (%" PRIu64 ")\n",
				cfg_comp->begin_ns, cfg_comp->end_ns);
			goto end;
		}
	}

	valid = true;

end:
	return valid;
}

/*
 * Validates a given configuration, with optional legacy input and
 * output formats options. Prints useful error messages if anything
 * is wrong.
 *
 * Returns true when the configuration is valid.
 */
static
bool validate_cfg(struct bt_config *cfg,
		enum legacy_input_format *legacy_input_format,
		enum legacy_output_format *legacy_output_format,
		struct bt_value *legacy_input_paths,
		struct ctf_legacy_opts *ctf_legacy_opts,
		struct text_legacy_opts *text_legacy_opts)
{
	bool legacy_input = false;
	bool legacy_output = false;

	/* Determine if the input and output should be legacy-style */
	if (*legacy_input_format != LEGACY_INPUT_FORMAT_NONE ||
			cfg->sources->len == 0 ||
			!bt_value_array_is_empty(legacy_input_paths) ||
			ctf_legacy_opts_is_any_set(ctf_legacy_opts)) {
		legacy_input = true;
	}

	if (*legacy_output_format != LEGACY_OUTPUT_FORMAT_NONE ||
			cfg->sinks->len == 0 ||
			text_legacy_opts_is_any_set(text_legacy_opts)) {
		legacy_output = true;
	}

	if (legacy_input) {
		/* If no legacy input format was specified, default to CTF */
		if (*legacy_input_format == LEGACY_INPUT_FORMAT_NONE) {
			*legacy_input_format = LEGACY_INPUT_FORMAT_CTF;
		}

		/* Make sure at least one input path exists */
		if (bt_value_array_is_empty(legacy_input_paths)) {
			switch (*legacy_input_format) {
			case LEGACY_INPUT_FORMAT_CTF:
				printf_err("No input path specified for legacy `ctf` input format\n");
				break;
			case LEGACY_INPUT_FORMAT_LTTNG_LIVE:
				printf_err("No URL specified for legacy `lttng-live` input format\n");
				break;
			default:
				assert(false);
			}
			goto error;
		}

		/* Make sure no non-legacy sources are specified */
		if (cfg->sources->len != 0) {
			print_input_legacy_to_sources(*legacy_input_format,
				legacy_input_paths, ctf_legacy_opts);
			goto error;
		}
	}

	if (legacy_output) {
		/*
		 * If no legacy output format was specified, default to
		 * "text".
		 */
		if (*legacy_output_format == LEGACY_OUTPUT_FORMAT_NONE) {
			*legacy_output_format = LEGACY_OUTPUT_FORMAT_TEXT;
		}

		/*
		 * If any "text" option was specified, the output must be
		 * legacy "text".
		 */
		if (text_legacy_opts_is_any_set(text_legacy_opts) &&
				*legacy_output_format !=
				LEGACY_OUTPUT_FORMAT_TEXT) {
			printf_err("Options for legacy `text` output format specified with a different legacy output format\n");
			goto error;
		}

		/* Make sure no non-legacy sinks are specified */
		if (cfg->sinks->len != 0) {
			print_output_legacy_to_sinks(*legacy_output_format,
				text_legacy_opts);
			goto error;
		}
	}

	/*
	 * If the output is the legacy "ctf-metadata" format, then the
	 * input should be the legacy "ctf" input format.
	 */
	if (*legacy_output_format == LEGACY_OUTPUT_FORMAT_CTF_METADATA &&
			*legacy_input_format != LEGACY_INPUT_FORMAT_CTF) {
		printf_err("Legacy `ctf-metadata` output format requires using legacy `ctf` input format\n");
		goto error;
	}

	return true;

error:
	return false;
}

/*
 * Parses a 64-bit signed integer.
 *
 * Returns a negative value if anything goes wrong.
 */
static
int parse_int64(const char *arg, int64_t *val)
{
	char *endptr;

	errno = 0;
	*val = strtoll(arg, &endptr, 0);
	if (*endptr != '\0' || arg == endptr || errno != 0) {
		return -1;
	}

	return 0;
}

/*
 * Parses a time in nanoseconds.
 *
 * Returns a negative value if anything goes wrong.
 */
static
int ns_from_arg(const char *arg, uint64_t *ns)
{
	int ret = 0;
	int64_t value;

	if (parse_int64(arg, &value)) {
		ret = -1;
		goto end;
	}

	if (value < 0) {
		ret = -1;
		goto end;
	}

	*ns = (uint64_t) value;

end:
	return ret;
}

/* popt options */
enum {
	OPT_NONE = 0,
	OPT_BASE_BEGIN_NS,
	OPT_BASE_END_NS,
	OPT_BASE_PARAMS,
	OPT_BEGIN_NS,
	OPT_CLOCK_CYCLES,
	OPT_CLOCK_DATE,
	OPT_CLOCK_FORCE_CORRELATE,
	OPT_CLOCK_GMT,
	OPT_CLOCK_OFFSET,
	OPT_CLOCK_OFFSET_NS,
	OPT_CLOCK_SECONDS,
	OPT_DEBUG,
	OPT_DEBUG_INFO_DIR,
	OPT_DEBUG_INFO_FULL_PATH,
	OPT_DEBUG_INFO_TARGET_PREFIX,
	OPT_END_NS,
	OPT_FIELDS,
	OPT_HELP,
	OPT_HELP_LEGACY,
	OPT_INPUT_FORMAT,
	OPT_LIST,
	OPT_NAMES,
	OPT_NO_DELTA,
	OPT_OUTPUT_FORMAT,
	OPT_OUTPUT_PATH,
	OPT_PATH,
	OPT_PARAMS,
	OPT_PLUGIN_PATH,
	OPT_RESET_BASE_BEGIN_NS,
	OPT_RESET_BASE_END_NS,
	OPT_RESET_BASE_PARAMS,
	OPT_SINK,
	OPT_SOURCE,
	OPT_STREAM_INTERSECTION,
	OPT_VERBOSE,
	OPT_VERSION,
};

/* popt long option descriptions */
static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "base-begin-ns", 'B', POPT_ARG_STRING, NULL, OPT_BASE_BEGIN_NS, NULL, NULL },
	{ "base-end-ns", 'E', POPT_ARG_STRING, NULL, OPT_BASE_END_NS, NULL, NULL },
	{ "base-params", 'b', POPT_ARG_STRING, NULL, OPT_BASE_PARAMS, NULL, NULL },
	{ "begin-ns", '\0', POPT_ARG_STRING, NULL, OPT_BEGIN_NS, NULL, NULL },
	{ "clock-cycles", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_CYCLES, NULL, NULL },
	{ "clock-date", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_DATE, NULL, NULL },
	{ "clock-force-correlate", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_FORCE_CORRELATE, NULL, NULL },
	{ "clock-gmt", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_GMT, NULL, NULL },
	{ "clock-offset", '\0', POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET, NULL, NULL },
	{ "clock-offset-ns", '\0', POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET_NS, NULL, NULL },
	{ "clock-seconds", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_SECONDS, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ "debug-info-dir", 0, POPT_ARG_STRING, NULL, OPT_DEBUG_INFO_DIR, NULL, NULL },
	{ "debug-info-full-path", 0, POPT_ARG_NONE, NULL, OPT_DEBUG_INFO_FULL_PATH, NULL, NULL },
	{ "debug-info-target-prefix", 0, POPT_ARG_STRING, NULL, OPT_DEBUG_INFO_TARGET_PREFIX, NULL, NULL },
	{ "end-ns", '\0', POPT_ARG_STRING, NULL, OPT_END_NS, NULL, NULL },
	{ "fields", 'f', POPT_ARG_STRING, NULL, OPT_FIELDS, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "help-legacy", '\0', POPT_ARG_NONE, NULL, OPT_HELP_LEGACY, NULL, NULL },
	{ "input-format", 'i', POPT_ARG_STRING, NULL, OPT_INPUT_FORMAT, NULL, NULL },
	{ "list", 'l', POPT_ARG_NONE, NULL, OPT_LIST, NULL, NULL },
	{ "names", 'n', POPT_ARG_STRING, NULL, OPT_NAMES, NULL, NULL },
	{ "no-delta", '\0', POPT_ARG_NONE, NULL, OPT_NO_DELTA, NULL, NULL },
	{ "output", 'w', POPT_ARG_STRING, NULL, OPT_OUTPUT_PATH, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, NULL, OPT_OUTPUT_FORMAT, NULL, NULL },
	{ "path", 'P', POPT_ARG_STRING, NULL, OPT_PATH, NULL, NULL },
	{ "params", 'p', POPT_ARG_STRING, NULL, OPT_PARAMS, NULL, NULL },
	{ "plugin-path", '\0', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ "reset-base-begin-ns", '\0', POPT_ARG_NONE, NULL, OPT_RESET_BASE_BEGIN_NS, NULL, NULL },
	{ "reset-base-end-ns", '\0', POPT_ARG_NONE, NULL, OPT_RESET_BASE_END_NS, NULL, NULL },
	{ "reset-base-params", 'r', POPT_ARG_NONE, NULL, OPT_RESET_BASE_PARAMS, NULL, NULL },
	{ "sink", '\0', POPT_ARG_STRING, NULL, OPT_SINK, NULL, NULL },
	{ "source", '\0', POPT_ARG_STRING, NULL, OPT_SOURCE, NULL, NULL },
	{ "stream-intersection", '\0', POPT_ARG_NONE, NULL, OPT_STREAM_INTERSECTION, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "version", 'V', POPT_ARG_NONE, NULL, OPT_VERSION, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL },
};

/*
 * Sets the value of a given legacy offset option and marks it as set.
 */
static void set_offset_value(struct offset_opt *offset_opt, int64_t value)
{
	offset_opt->value = value;
	offset_opt->is_set = true;
}

enum bt_config_component_dest {
	BT_CONFIG_COMPONENT_DEST_SOURCE,
	BT_CONFIG_COMPONENT_DEST_SINK,
};

/*
 * Adds a configuration component to the appropriate configuration
 * array depending on the destination.
 */
static void add_cfg_comp(struct bt_config *cfg,
		struct bt_config_component *cfg_comp,
		enum bt_config_component_dest dest)
{
	if (dest == BT_CONFIG_COMPONENT_DEST_SOURCE) {
		g_ptr_array_add(cfg->sources, cfg_comp);
	} else {
		g_ptr_array_add(cfg->sinks, cfg_comp);
	}
}

/*
 * Returns a Babeltrace configuration, out of command-line arguments,
 * containing everything that is needed to instanciate specific
 * components with given parameters.
 *
 * *exit_code is set to the appropriate exit code to use as far as this
 * function goes.
 *
 * Return value is NULL on error, otherwise it's owned by the caller.
 */
struct bt_config *bt_config_from_args(int argc, char *argv[], int *exit_code)
{
	struct bt_config *cfg = NULL;
	poptContext pc = NULL;
	char *arg = NULL;
	struct ctf_legacy_opts ctf_legacy_opts;
	struct text_legacy_opts text_legacy_opts;
	enum legacy_input_format legacy_input_format = LEGACY_INPUT_FORMAT_NONE;
	enum legacy_output_format legacy_output_format =
		LEGACY_OUTPUT_FORMAT_NONE;
	struct bt_value *legacy_input_paths = NULL;
	struct bt_config_component *cur_cfg_comp = NULL;
	enum bt_config_component_dest cur_cfg_comp_dest =
		BT_CONFIG_COMPONENT_DEST_SOURCE;
	struct bt_value *cur_base_params = NULL;
	uint64_t cur_base_begin_ns = -1ULL;
	uint64_t cur_base_end_ns = -1ULL;
	int opt;
	bool cur_cfg_comp_params_set = false;
	unsigned int i;

	memset(&ctf_legacy_opts, 0, sizeof(ctf_legacy_opts));
	memset(&text_legacy_opts, 0, sizeof(text_legacy_opts));
	*exit_code = 0;

	if (argc <= 1) {
		print_usage(stdout);
		goto end;
	}

	text_legacy_opts.output = g_string_new(NULL);
	if (!text_legacy_opts.output) {
		print_err_oom();
		goto error;
	}

	text_legacy_opts.dbg_info_dir = g_string_new(NULL);
	if (!text_legacy_opts.dbg_info_dir) {
		print_err_oom();
		goto error;
	}

	text_legacy_opts.dbg_info_target_prefix = g_string_new(NULL);
	if (!text_legacy_opts.dbg_info_target_prefix) {
		print_err_oom();
		goto error;
	}

	cur_base_params = bt_value_map_create();
	if (!cur_base_params) {
		print_err_oom();
		goto error;
	}

	/* Create config */
	cfg = g_new0(struct bt_config, 1);
	if (!cfg) {
		print_err_oom();
		goto error;
	}

	bt_object_init(cfg, bt_config_destroy);
	cfg->sources = g_ptr_array_new_with_free_func((GDestroyNotify) bt_put);
	if (!cfg->sources) {
		print_err_oom();
		goto error;
	}

	cfg->sinks = g_ptr_array_new_with_free_func((GDestroyNotify) bt_put);
	if (!cfg->sinks) {
		print_err_oom();
		goto error;
	}

	legacy_input_paths = bt_value_array_create();
	if (!legacy_input_paths) {
		print_err_oom();
		goto error;
	}

	/* Parse options */
	pc = poptGetContext(NULL, argc, (const char **) argv, long_options, 0);
	if (!pc) {
		printf_err("Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_PLUGIN_PATH:
			if (cfg->plugin_paths) {
				printf_err("Duplicate --plugin-path option\n");
				goto error;
			}

			cfg->plugin_paths = plugin_paths_from_arg(arg);
			if (!cfg->plugin_paths) {
				printf_err("Invalid --plugin-path option's argument\n");
				goto error;
			}
			break;
		case OPT_OUTPUT_PATH:
			if (text_legacy_opts.output->len > 0) {
				printf_err("Duplicate --output option\n");
				goto error;
			}

			g_string_assign(text_legacy_opts.output, arg);
			break;
		case OPT_DEBUG_INFO_DIR:
			if (text_legacy_opts.dbg_info_dir->len > 0) {
				printf_err("Duplicate --debug-info-dir option\n");
				goto error;
			}

			g_string_assign(text_legacy_opts.dbg_info_dir, arg);
			break;
		case OPT_DEBUG_INFO_TARGET_PREFIX:
			if (text_legacy_opts.dbg_info_target_prefix->len > 0) {
				printf_err("Duplicate --debug-info-target-prefix option\n");
				goto error;
			}

			g_string_assign(text_legacy_opts.dbg_info_target_prefix, arg);
			break;
		case OPT_INPUT_FORMAT:
		case OPT_SOURCE:
		{
			if (opt == OPT_INPUT_FORMAT) {
				if (!strcmp(arg, "ctf")) {
					/* Legacy CTF input format */
					if (legacy_input_format) {
						print_err_dup_legacy_input();
						goto error;
					}

					legacy_input_format =
						LEGACY_INPUT_FORMAT_CTF;
					break;
				} else if (!strcmp(arg, "lttng-live")) {
					/* Legacy LTTng-live input format */
					if (legacy_input_format) {
						print_err_dup_legacy_input();
						goto error;
					}

					legacy_input_format =
						LEGACY_INPUT_FORMAT_LTTNG_LIVE;
					break;
				}
			}

			/* Non-legacy: try to create a component config */
			if (cur_cfg_comp) {
				add_cfg_comp(cfg, cur_cfg_comp,
					cur_cfg_comp_dest);
			}

			cur_cfg_comp = bt_config_component_from_arg(arg);
			if (!cur_cfg_comp) {
				printf_err("Invalid format for --source option's argument:\n    %s\n",
					arg);
				goto error;
			}

			assert(cur_base_params);
			bt_put(cur_cfg_comp->params);
			cur_cfg_comp->params = bt_value_copy(cur_base_params);
			if (!cur_cfg_comp) {
				print_err_oom();
				goto end;
			}

			cur_cfg_comp->begin_ns = cur_base_begin_ns;
			cur_cfg_comp->end_ns = cur_base_end_ns;
			cur_cfg_comp_dest = BT_CONFIG_COMPONENT_DEST_SOURCE;
			cur_cfg_comp_params_set = false;
			break;
		}
		case OPT_OUTPUT_FORMAT:
		case OPT_SINK:
		{
			if (opt == OPT_OUTPUT_FORMAT) {
				if (!strcmp(arg, "text")) {
					/* Legacy CTF-text output format */
					if (legacy_output_format) {
						print_err_dup_legacy_output();
						goto error;
					}

					legacy_output_format =
						LEGACY_OUTPUT_FORMAT_TEXT;
					break;
				} else if (!strcmp(arg, "dummy")) {
					/* Legacy dummy output format */
					if (legacy_output_format) {
						print_err_dup_legacy_output();
						goto error;
					}

					legacy_output_format =
						LEGACY_OUTPUT_FORMAT_DUMMY;
					break;
				} else if (!strcmp(arg, "ctf-metadata")) {
					/* Legacy CTF-metadata output format */
					if (legacy_output_format) {
						print_err_dup_legacy_output();
						goto error;
					}

					legacy_output_format =
						LEGACY_OUTPUT_FORMAT_CTF_METADATA;
					break;
				}
			}

			/* Non-legacy: try to create a component config */
			if (cur_cfg_comp) {
				add_cfg_comp(cfg, cur_cfg_comp,
					cur_cfg_comp_dest);
			}

			cur_cfg_comp = bt_config_component_from_arg(arg);
			if (!cur_cfg_comp) {
				printf_err("Invalid format for --sink option's argument:\n    %s\n",
					arg);
				goto error;
			}

			assert(cur_base_params);
			bt_put(cur_cfg_comp->params);
			cur_cfg_comp->params = bt_value_copy(cur_base_params);
			if (!cur_cfg_comp) {
				print_err_oom();
				goto end;
			}

			cur_cfg_comp_dest = BT_CONFIG_COMPONENT_DEST_SINK;
			cur_cfg_comp_params_set = false;
			break;
		}
		case OPT_PARAMS:
		{
			struct bt_value *params;
			struct bt_value *params_to_set;

			if (!cur_cfg_comp) {
				printf_err("--params option must follow a --source or --sink option\n");
				goto error;
			}

			if (cur_cfg_comp_params_set) {
				printf_err("Duplicate --params option for the same current component\ninstance (class %s.%s)\n",
					cur_cfg_comp->plugin_name->str,
					cur_cfg_comp->component_name->str);
				goto error;
			}

			params = bt_value_from_arg(arg);
			if (!params) {
				printf_err("Invalid format for --params option's argument:\n    %s\n",
					arg);
				goto error;
			}

			params_to_set = bt_value_map_extend(cur_base_params,
				params);
			BT_PUT(params);
			if (!params_to_set) {
				printf_err("Cannot extend current base parameters with --params option's argument:\n    %s\n",
					arg);
				goto error;
			}

			BT_MOVE(cur_cfg_comp->params, params_to_set);
			cur_cfg_comp_params_set = true;
			break;
		}
		case OPT_PATH:
			if (!cur_cfg_comp) {
				printf_err("--path option must follow a --source or --sink option\n");
				goto error;
			}

			assert(cur_cfg_comp->params);

			if (bt_value_map_insert_string(cur_cfg_comp->params,
					"path", arg)) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_BASE_PARAMS:
		{
			struct bt_value *params = bt_value_from_arg(arg);

			if (!params) {
				printf_err("Invalid format for --base-params option's argument:\n    %s\n",
					arg);
				goto error;
			}

			BT_MOVE(cur_base_params, params);
			break;
		}
		case OPT_RESET_BASE_PARAMS:
			BT_PUT(cur_base_params);
			cur_base_params = bt_value_map_create();
			if (!cur_base_params) {
				print_err_oom();
				goto error;
			}
			break;
		case OPT_BASE_BEGIN_NS:
			if (ns_from_arg(arg, &cur_base_begin_ns)) {
				printf_err("Invalid --base-begin-ns option's argument:\n    %s\n",
					arg);
				goto error;
			}
			break;
		case OPT_BASE_END_NS:
			if (ns_from_arg(arg, &cur_base_end_ns)) {
				printf_err("Invalid --base-end-ns option's argument:\n    %s\n",
					arg);
				goto error;
			}
			break;
		case OPT_RESET_BASE_BEGIN_NS:
			cur_base_begin_ns = -1ULL;
			break;
		case OPT_RESET_BASE_END_NS:
			cur_base_end_ns = -1ULL;
			break;
		case OPT_BEGIN_NS:
			if (!cur_cfg_comp || cur_cfg_comp_dest ==
					BT_CONFIG_COMPONENT_DEST_SINK) {
				printf_err("--begin-ns option must follow a --source option\n");
				goto error;
			}

			if (ns_from_arg(arg, &cur_cfg_comp->begin_ns)) {
				printf_err("Invalid --begin-ns option's argument:\n    %s\n",
					arg);
				goto error;
			}
			break;
		case OPT_END_NS:
			if (!cur_cfg_comp || cur_cfg_comp_dest ==
					BT_CONFIG_COMPONENT_DEST_SINK) {
				printf_err("--end-ns option must follow a --source option\n");
				goto error;
			}

			if (ns_from_arg(arg, &cur_cfg_comp->end_ns)) {
				printf_err("Invalid --end-ns option's argument:\n    %s\n",
					arg);
				goto error;
			}
			break;
		case OPT_NAMES:
			if (text_legacy_opts.names) {
				printf_err("Duplicate --names option\n");
				goto error;
			}

			text_legacy_opts.names = names_from_arg(arg);
			if (!text_legacy_opts.names) {
				printf_err("Invalid --names option's argument\n");
				goto error;
			}
			break;
		case OPT_FIELDS:
			if (text_legacy_opts.fields) {
				printf_err("Duplicate --fields option\n");
				goto error;
			}

			text_legacy_opts.fields = fields_from_arg(arg);
			if (!text_legacy_opts.fields) {
				printf_err("Invalid --fields option's argument\n");
				goto error;
			}
			break;
		case OPT_NO_DELTA:
			text_legacy_opts.no_delta = true;
			break;
		case OPT_CLOCK_CYCLES:
			text_legacy_opts.clock_cycles = true;
			break;
		case OPT_CLOCK_SECONDS:
			text_legacy_opts.clock_seconds = true;
			break;
		case OPT_CLOCK_DATE:
			text_legacy_opts.clock_date = true;
			break;
		case OPT_CLOCK_GMT:
			text_legacy_opts.clock_gmt = true;
			break;
		case OPT_DEBUG_INFO_FULL_PATH:
			text_legacy_opts.dbg_info_full_path = true;
			break;
		case OPT_CLOCK_OFFSET:
		{
			int64_t val;

			if (ctf_legacy_opts.offset_s.is_set) {
				printf_err("Duplicate --clock-offset option\n");
				goto error;
			}

			if (parse_int64(arg, &val)) {
				printf_err("Invalid --clock-offset option's argument\n");
				goto error;
			}

			set_offset_value(&ctf_legacy_opts.offset_s, val);
			break;
		}
		case OPT_CLOCK_OFFSET_NS:
		{
			int64_t val;

			if (ctf_legacy_opts.offset_ns.is_set) {
				printf_err("Duplicate --clock-offset-ns option\n");
				goto error;
			}

			if (parse_int64(arg, &val)) {
				printf_err("Invalid --clock-offset-ns option's argument\n");
				goto error;
			}

			set_offset_value(&ctf_legacy_opts.offset_ns, val);
			break;
		}
		case OPT_STREAM_INTERSECTION:
			ctf_legacy_opts.stream_intersection = true;
			break;
		case OPT_CLOCK_FORCE_CORRELATE:
			cfg->force_correlate = true;
			break;
		case OPT_HELP:
			BT_PUT(cfg);
			print_usage(stdout);
			goto end;
		case OPT_HELP_LEGACY:
			BT_PUT(cfg);
			print_legacy_usage(stdout);
			goto end;
		case OPT_VERSION:
			BT_PUT(cfg);
			print_version();
			goto end;
		case OPT_LIST:
			cfg->do_list = true;
			goto end;
		case OPT_VERBOSE:
			cfg->verbose = true;
			break;
		case OPT_DEBUG:
			cfg->debug = true;
			break;
		default:
			printf_err("Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Append current component configuration, if any */
	if (cur_cfg_comp) {
		add_cfg_comp(cfg, cur_cfg_comp, cur_cfg_comp_dest);
		cur_cfg_comp = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		printf_err("While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	/* Consume leftover arguments as legacy input paths */
	while (true) {
		const char *input_path = poptGetArg(pc);

		if (!input_path) {
			break;
		}

		if (bt_value_array_append_string(legacy_input_paths,
				input_path)) {
			print_err_oom();
			goto error;
		}
	}

	/* Validate legacy/non-legacy options */
	if (!validate_cfg(cfg, &legacy_input_format, &legacy_output_format,
			legacy_input_paths, &ctf_legacy_opts,
			&text_legacy_opts)) {
		printf_err("Command-line options form an invalid configuration\n");
		goto error;
	}

	/*
	 * If there's a legacy input format, convert it to source
	 * component configurations.
	 */
	if (legacy_input_format) {
		if (append_sources_from_legacy_opts(cfg->sources,
				legacy_input_format, &ctf_legacy_opts,
				legacy_input_paths, cur_base_begin_ns,
				cur_base_end_ns)) {
			printf_err("Cannot convert legacy input format options to source component instance(s)\n");
			goto error;
		}
	}

	/*
	 * If there's a legacy output format, convert it to sink
	 * component configurations.
	 */
	if (legacy_output_format) {
		if (append_sinks_from_legacy_opts(cfg->sinks,
				legacy_output_format, &text_legacy_opts)) {
			printf_err("Cannot convert legacy output format options to sink component instance(s)\n");
			goto error;
		}
	}

	for (i = 0; i < cfg->sources->len; i++) {
		struct bt_config_component *cfg_component =
			bt_config_get_component(cfg->sources, i);

		/* Only peek */
		bt_put(cfg_component);

		if (!validate_source_config_component(cfg_component)) {
			printf_err("Invalid source component instance (class %s.%s)\n",
				cfg_component->plugin_name->str,
				cfg_component->component_name->str);
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(cfg);
	cfg = NULL;
	*exit_code = 1;

end:
	if (pc) {
		poptFreeContext(pc);
	}

	if (text_legacy_opts.output) {
		g_string_free(text_legacy_opts.output, TRUE);
	}

	if (text_legacy_opts.dbg_info_dir) {
		g_string_free(text_legacy_opts.dbg_info_dir, TRUE);
	}

	if (text_legacy_opts.dbg_info_target_prefix) {
		g_string_free(text_legacy_opts.dbg_info_target_prefix, TRUE);
	}

	free(arg);
	BT_PUT(cur_cfg_comp);
	BT_PUT(cur_base_params);
	BT_PUT(text_legacy_opts.names);
	BT_PUT(text_legacy_opts.fields);
	BT_PUT(legacy_input_paths);
	return cfg;
}
