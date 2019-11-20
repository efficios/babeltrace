/*
 * Copyright 2016-2019 Philippe Proulx <pproulx@efficios.com>
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

#include "param-parse.h"

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

/* INI-style parsing FSM states */
enum ini_parsing_fsm_state {
	/* Expect a map key (identifier) */
	INI_EXPECT_MAP_KEY,

	/* Expect an equal character (`=`) */
	INI_EXPECT_EQUAL,

	/* Expect a value */
	INI_EXPECT_VALUE,

	/* Expect a comma character (`,`) */
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
	GString *last_map_key;

	/* Complete INI-style string to parse */
	const char *arg;

	/* Error buffer (weak) */
	GString *ini_error;
};

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

	/* Only append error if there's one line */
	if (strchr(state->arg, '\n') || strlen(state->arg) == 0) {
		return;
	}

	g_string_append_printf(state->ini_error, "\n    %s\n", state->arg);
	pos = g_scanner_cur_position(scanner) + 4;

	if (!g_scanner_eof(scanner)) {
		pos--;
	}

	for (i = 0; i < pos; ++i) {
		g_string_append_c(state->ini_error, ' ');
	}

	g_string_append_c(state->ini_error, '^');
}

static
void ini_append_oom_error(GString *error)
{
	BT_ASSERT(error);
	g_string_append(error, "Out of memory\n");
}

/*
 * Parses the next token as an unsigned integer.
 */
static
bt_value *ini_parse_uint(struct ini_parsing_state *state)
{
	bt_value *value = NULL;
	GTokenType token_type = g_scanner_get_next_token(state->scanner);

	if (token_type != G_TOKEN_INT) {
		ini_append_error_expecting(state, state->scanner,
			"integer value");
		goto end;
	}

	value = bt_value_integer_unsigned_create_init(
		state->scanner->value.v_int64);

end:
	return value;
}

/*
 * Parses the next token as a number and returns its negation.
 */
static
bt_value *ini_parse_neg_number(struct ini_parsing_state *state)
{
	bt_value *value = NULL;
	GTokenType token_type = g_scanner_get_next_token(state->scanner);

	switch (token_type) {
	case G_TOKEN_INT:
	{
		/* Negative integer */
		uint64_t int_val = state->scanner->value.v_int64;

		if (int_val > (((uint64_t) INT64_MAX) + 1)) {
			g_string_append_printf(state->ini_error,
				"Integer value -%" PRIu64 " is outside the range of a 64-bit signed integer\n",
				int_val);
		} else {
			value = bt_value_integer_signed_create_init(
				-((int64_t) int_val));
		}

		break;
	}
	case G_TOKEN_FLOAT:
		/* Negative floating point number */
		value = bt_value_real_create_init(
			-state->scanner->value.v_float);
		break;
	default:
		ini_append_error_expecting(state, state->scanner, "value");
		break;
	}

	return value;
}

static bt_value *ini_parse_value(struct ini_parsing_state *state);

/*
 * Parses the current and following tokens as an array. Arrays are
 * formatted as an opening `[`, a list of comma-separated values, and a
 * closing `]`. For convenience, this function supports an optional
 * trailing comma after the last value.
 *
 * The current token of the parser must be the opening square bracket
 * (`[`) of the array.
 */
static
bt_value *ini_parse_array(struct ini_parsing_state *state)
{
	bt_value *array_value;
	GTokenType token_type;

	/* The `[` character must have already been ingested */
	BT_ASSERT(g_scanner_cur_token(state->scanner) == G_TOKEN_CHAR);
	BT_ASSERT(g_scanner_cur_value(state->scanner).v_char == '[');

	array_value = bt_value_array_create ();
	if (!array_value) {
		ini_append_oom_error(state->ini_error);
		goto error;
	}

	token_type = g_scanner_get_next_token(state->scanner);

	/* While the current token is not a `]` */
	while (!(token_type == G_TOKEN_CHAR &&
			g_scanner_cur_value(state->scanner).v_char == ']')) {
		bt_value *item_value;
		bt_value_array_append_element_status append_status;

		/* Parse the item... */
		item_value = ini_parse_value(state);
		if (!item_value) {
			goto error;
		}

		/* ... and add it to the result array */
		append_status = bt_value_array_append_element(array_value,
			item_value);
		BT_VALUE_PUT_REF_AND_RESET(item_value);
		if (append_status < 0) {
			goto error;
		}

		/*
		 * Ingest the token following the value. It should be
		 * either a comma or closing square bracket.
		 */
		token_type = g_scanner_get_next_token(state->scanner);
		if (token_type == G_TOKEN_CHAR &&
				g_scanner_cur_value(state->scanner).v_char == ',') {
			/*
			 * Ingest the token following the comma. If it
			 * happens to be a closing square bracket, exit
			 * the loop and we are done (we allow trailing
			 * commas). Otherwise, we are ready for the next
			 * ini_parse_value() call.
			 */
			token_type = g_scanner_get_next_token(state->scanner);
		} else if (token_type != G_TOKEN_CHAR ||
				g_scanner_cur_value(state->scanner).v_char != ']') {
			ini_append_error_expecting(state, state->scanner,
				"`,` or `]`");
			goto error;
		}
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(array_value);

end:
	return array_value;
}

/*
 * Parses the current and following tokens as a map. Maps are
 * formatted as an opening `{`, a list of comma-separated entries, and a
 * closing `}`. And entry is a key (an unquoted string), an equal sign and
 * a value. For convenience, this function supports an optional trailing comma
 * after the last value.
 *
 * The current token of the parser must be the opening curly bracket
 * (`{`) of the array.
 */
static
bt_value *ini_parse_map(struct ini_parsing_state *state)
{
	bt_value *map_value;
	GTokenType token_type;
	gchar *key = NULL;

	/* The `{` character must have already been ingested */
	BT_ASSERT(g_scanner_cur_token(state->scanner) == G_TOKEN_CHAR);
	BT_ASSERT(g_scanner_cur_value(state->scanner).v_char == '{');

	map_value = bt_value_map_create ();
	if (!map_value) {
		ini_append_oom_error(state->ini_error);
		goto error;
	}

	token_type = g_scanner_get_next_token(state->scanner);

	/* While the current token is not a `}` */
	while (!(token_type == G_TOKEN_CHAR &&
			g_scanner_cur_value(state->scanner).v_char == '}')) {
		bt_value *entry_value;
		bt_value_map_insert_entry_status insert_entry_status;

		/* Expect map key. */
		if (token_type != G_TOKEN_IDENTIFIER) {
			ini_append_error_expecting(state, state->scanner,
				"unquoted map key");
			goto error;
		}

		g_free(key);
		key = g_strdup(g_scanner_cur_value(state->scanner).v_identifier);

		token_type = g_scanner_get_next_token(state->scanner);

		/* Expect equal sign. */
		if (token_type != G_TOKEN_CHAR ||
				g_scanner_cur_value(state->scanner).v_char != '=') {
			ini_append_error_expecting(state,
				state->scanner, "'='");
			goto error;
		}

		token_type = g_scanner_get_next_token(state->scanner);

		/* Parse the entry value... */
		entry_value = ini_parse_value(state);
		if (!entry_value) {
			goto error;
		}

		/* ... and add it to the result map */
		insert_entry_status =
			bt_value_map_insert_entry(map_value, key, entry_value);
		BT_VALUE_PUT_REF_AND_RESET(entry_value);
		if (insert_entry_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
			goto error;
		}

		/*
		 * Ingest the token following the value. It should be
		 * either a comma or closing curly bracket.
		 */
		token_type = g_scanner_get_next_token(state->scanner);
		if (token_type == G_TOKEN_CHAR &&
				g_scanner_cur_value(state->scanner).v_char == ',') {
			/*
			 * Ingest the token following the comma. If it
			 * happens to be a closing curly bracket, exit
			 * the loop and we are done (we allow trailing
			 * commas). Otherwise, we are ready for the next
			 * ini_parse_value() call.
			 */
			token_type = g_scanner_get_next_token(state->scanner);
		} else if (token_type != G_TOKEN_CHAR ||
				g_scanner_cur_value(state->scanner).v_char != '}') {
			ini_append_error_expecting(state, state->scanner,
				"`,` or `}`");
			goto error;
		}
	}

	goto end;
error:
	BT_VALUE_PUT_REF_AND_RESET(map_value);

end:
	g_free(key);

	return map_value;
}

/*
 * Parses the current token (and the following ones if needed) as a
 * value, returning it as a `bt_value *`.
 */
static
bt_value *ini_parse_value(struct ini_parsing_state *state)
{
	bt_value *value = NULL;
	GTokenType token_type = state->scanner->token;

	switch (token_type) {
	case G_TOKEN_CHAR:
		if (state->scanner->value.v_char == '-') {
			/* Negative number */
			value = ini_parse_neg_number(state);
		} else if (state->scanner->value.v_char == '+') {
			/* Unsigned integer */
			value = ini_parse_uint(state);
		} else if (state->scanner->value.v_char == '[') {
			/* Array */
			value = ini_parse_array(state);
		} else if (state->scanner->value.v_char == '{') {
			/* Map */
			value = ini_parse_map(state);
		} else {
			ini_append_error_expecting(state, state->scanner, "value");
			goto end;
		}

		break;
	case G_TOKEN_INT:
	{
		/* Positive, signed integer */
		uint64_t int_val = state->scanner->value.v_int64;

		if (int_val > INT64_MAX) {
			g_string_append_printf(state->ini_error,
				"Integer value %" PRIu64 " is outside the range of a 64-bit signed integer\n",
				int_val);
			goto end;
		} else {
			value = bt_value_integer_signed_create_init(
				(int64_t) int_val);
		}

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
		 * Using symbols would be appropriate here, but said
		 * symbols are allowed as map key, so it's easier to
		 * consider everything an identifier.
		 *
		 * If one of the known symbols is not recognized here,
		 * then fall back to creating a string value.
		 */
		const char *id = state->scanner->value.v_identifier;

		if (strcmp(id, "null") == 0 || strcmp(id, "NULL") == 0 ||
				strcmp(id, "nul") == 0) {
			value = bt_value_null;
			bt_value_get_ref(value);
		} else if (strcmp(id, "true") == 0 || strcmp(id, "TRUE") == 0 ||
				strcmp(id, "yes") == 0 ||
				strcmp(id, "YES") == 0) {
			value = bt_value_bool_create_init(true);
		} else if (strcmp(id, "false") == 0 ||
				strcmp(id, "FALSE") == 0 ||
				strcmp(id, "no") == 0 ||
				strcmp(id, "NO") == 0) {
			value = bt_value_bool_create_init(false);
		} else {
			value = bt_value_string_create_init(id);
		}
		break;
	}
	default:
		/* Unset return value variable will trigger the error */
		ini_append_error_expecting(state, state->scanner, "value");
		break;
	}

end:
	return value;
}

/*
 * Handles the current state of the INI parser.
 *
 * Returns 0 to continue, 1 to end, or a negative value on error.
 */
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
					state->scanner, "`=`");
				break;
			case INI_EXPECT_VALUE:
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

		g_string_assign(state->last_map_key,
			state->scanner->value.v_identifier);

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
		value = ini_parse_value(state);
		if (!value) {
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
		bt_common_abort();
	}

error:
	ret = -1;
	goto end;

success:
	if (value) {
		if (bt_value_map_insert_entry(state->params,
				state->last_map_key->str, value)) {
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
BT_HIDDEN
bt_value *bt_param_parse(const char *arg, GString *ini_error)
{
	/* Lexical scanner configuration */
	GScannerConfig scanner_config = {
		/* Skip whitespaces */
		.cset_skip_characters = (gchar *) " \t\n",

		/* Identifier syntax is: [a-zA-Z_][a-zA-Z0-9_.:-]* */
		.cset_identifier_first = (gchar *)
			G_CSET_a_2_z
			"_"
			G_CSET_A_2_Z,
		.cset_identifier_nth = (gchar *)
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

		/* Support both integers and floating point numbers */
		.int_2_float = FALSE,

		/* Scan integers as 64-bit signed integers */
		.store_int64 = TRUE,

		/* Only scan double-quoted strings */
		.scan_string_sq = FALSE,
		.scan_string_dq = TRUE,

		/* Do not converter identifiers to string tokens */
		.identifier_2_string = FALSE,

		/* Scan characters as `G_TOKEN_CHAR` token */
		.char_2_token = FALSE,
	};
	struct ini_parsing_state state = {
		.scanner = NULL,
		.params = NULL,
		.expecting = INI_EXPECT_MAP_KEY,
		.arg = arg,
		.ini_error = ini_error,
	};

	BT_ASSERT(ini_error);
	g_string_assign(ini_error, "");
	state.params = bt_value_map_create();
	if (!state.params) {
		ini_append_oom_error(ini_error);
		goto error;
	}

	state.scanner = g_scanner_new(&scanner_config);
	if (!state.scanner) {
		ini_append_oom_error(ini_error);
		goto error;
	}

	state.last_map_key = g_string_new(NULL);
	if (!state.last_map_key) {
		ini_append_oom_error(ini_error);
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

	if (state.last_map_key) {
		g_string_free(state.last_map_key, TRUE);
	}

	return state.params;
}
