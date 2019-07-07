/*
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "tap/tap.h"
#include "common/assert.h"

#include "argpar/argpar.h"

/*
 * Tests that the command line `cmdline`, with non-quoted
 * space-delimited arguments, once parsed given the option descriptors
 * `descrs` and the option `fail_on_unknown_opt`, succeeds and gives the
 * expected command line `expected_cmd_line` and number of ingested
 * original arguments `expected_ingested_orig_args`.
 *
 * The resulting command-line is built from the resulting arguments,
 * space-delimiting each argument, preferring the `--long-opt=arg` style
 * over the `-s arg` style, and using the `arg<A,B>` form for non-option
 * arguments where `A` is the original argument index and `B` is the
 * non-option argument index.
 */
static
void test_succeed(const char *cmdline,
		const char *expected_cmd_line,
		const struct bt_argpar_opt_descr *descrs,
		unsigned int expected_ingested_orig_args)
{
	struct bt_argpar_parse_ret parse_ret;
	GString *res_str = g_string_new(NULL);
	gchar **argv = g_strsplit(cmdline, " ", 0);
	unsigned int i;

	BT_ASSERT(argv);
	BT_ASSERT(res_str);
	parse_ret = bt_argpar_parse(g_strv_length(argv),
		(const char * const *) argv, descrs, false);
	ok(parse_ret.items,
		"bt_argpar_parse() succeeds for command line `%s`", cmdline);
	ok(!parse_ret.error,
		"bt_argpar_parse() does not write an error for command line `%s`", cmdline);
	ok(parse_ret.ingested_orig_args == expected_ingested_orig_args,
		"bt_argpar_parse() returns the correct number of ingested "
		"original arguments for command line `%s`", cmdline);
	if (parse_ret.ingested_orig_args != expected_ingested_orig_args) {
		diag("Expected: %u    Got: %u", expected_ingested_orig_args,
			parse_ret.ingested_orig_args);
	}

	if (!parse_ret.items) {
		fail("bt_argpar_parse() returns the expected parsed arguments "
			"for command line `%s`", cmdline);
		goto end;
	}

	for (i = 0; i < parse_ret.items->len; i++) {
		const struct bt_argpar_item *arg = parse_ret.items->pdata[i];

		switch (arg->type) {
		case BT_ARGPAR_ITEM_TYPE_OPT:
		{
			const struct bt_argpar_item_opt *arg_opt =
				(const void *) arg;

			if (arg_opt->descr->long_name) {
				g_string_append_printf(res_str, "--%s",
					arg_opt->descr->long_name);

				if (arg_opt->arg) {
					g_string_append_printf(res_str, "=%s",
						arg_opt->arg);
				}

				g_string_append_c(res_str, ' ');
			} else if (arg_opt->descr->short_name) {
				g_string_append_printf(res_str, "-%c",
					arg_opt->descr->short_name);

				if (arg_opt->arg) {
					g_string_append_printf(res_str, " %s",
						arg_opt->arg);
				}

				g_string_append_c(res_str, ' ');
			}

			break;
		}
		case BT_ARGPAR_ITEM_TYPE_NON_OPT:
		{
			const struct bt_argpar_item_non_opt *arg_non_opt =
				(const void *) arg;

			g_string_append_printf(res_str, "%s<%u,%u> ",
				arg_non_opt->arg, arg_non_opt->orig_index,
				arg_non_opt->non_opt_index);
			break;
		}
		default:
			abort();
		}
	}

	if (res_str->len > 0) {
		g_string_truncate(res_str, res_str->len - 1);
	}

	ok(strcmp(expected_cmd_line, res_str->str) == 0,
		"bt_argpar_parse() returns the expected parsed arguments "
		"for command line `%s`", cmdline);
	if (strcmp(expected_cmd_line, res_str->str) != 0) {
		diag("Expected: `%s`", expected_cmd_line);
		diag("Got:      `%s`", res_str->str);
	}

end:
	bt_argpar_parse_ret_fini(&parse_ret);
	g_string_free(res_str, TRUE);
	g_strfreev(argv);
}

static
void succeed_tests(void)
{
	/* No arguments */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"",
			"",
			descrs, 0);
	}

	/* Single long option */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "salut", false },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--salut",
			"--salut",
			descrs, 1);
	}

	/* Single short option */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'f', NULL, false },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f",
			"-f",
			descrs, 1);
	}

	/* Short and long option (aliases) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'f', "flaw", false },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f --flaw",
			"--flaw --flaw",
			descrs, 2);
	}

	/* Long option with argument (space form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "tooth", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--tooth 67",
			"--tooth=67",
			descrs, 2);
	}

	/* Long option with argument (equal form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "polish", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--polish=brick",
			"--polish=brick",
			descrs, 1);
	}

	/* Short option with argument (space form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'c', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-c chilly",
			"-c chilly",
			descrs, 2);
	}

	/* Short option with argument (glued form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'c', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-cchilly",
			"-c chilly",
			descrs, 1);
	}

	/* Short and long option (aliases) with argument (all forms) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'd', "dry", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--dry=rate -dthing --dry street --dry=shape",
			"--dry=rate --dry=thing --dry=street --dry=shape",
			descrs, 5);
	}

	/* Many short options, last one with argument (glued form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'd', NULL, false },
			{ 0, 'e', NULL, false },
			{ 0, 'f', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-defmeow",
			"-d -e -f meow",
			descrs, 1);
	}

	/* Many options */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'd', NULL, false },
			{ 0, 'e', "east", true },
			{ 0, '\0', "mind", false },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-d --mind -destart --mind --east cough -d --east=itch",
			"-d --mind -d --east=start --mind --east=cough -d --east=itch",
			descrs, 8);
	}

	/* Single non-option argument */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"kilojoule",
			"kilojoule<0,0>",
			descrs, 1);
	}

	/* Two non-option arguments */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"kilojoule mitaine",
			"kilojoule<0,0> mitaine<1,1>",
			descrs, 2);
	}

	/* Single non-option argument mixed with options */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'd', NULL, false },
			{ 0, '\0', "squeeze", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-d sprout yes --squeeze little bag -d",
			"-d sprout<1,0> yes<2,1> --squeeze=little bag<5,2> -d",
			descrs, 7);
	}

	/* Unknown short option (space form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'd', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-d salut -e -d meow",
			"-d salut",
			descrs, 2);
	}

	/* Unknown short option (glued form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'd', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-dsalut -e -d meow",
			"-d salut",
			descrs, 1);
	}

	/* Unknown long option (space form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "sink", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--sink party --food --sink impulse",
			"--sink=party",
			descrs, 2);
	}

	/* Unknown long option (equal form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "sink", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--sink=party --food --sink=impulse",
			"--sink=party",
			descrs, 1);
	}

	/* Unknown option before non-option argument */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "thumb", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--thumb=party --food bateau --thumb waves",
			"--thumb=party",
			descrs, 1);
	}

	/* Unknown option after non-option argument */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "thumb", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--thumb=party wound --food --thumb waves",
			"--thumb=party wound<1,0>",
			descrs, 2);
	}

	/* Valid `---opt` */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "-fuel", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"---fuel=three",
			"---fuel=three",
			descrs, 1);
	}

	/* Long option containing `=` in argument (equal form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "zebra", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--zebra=three=yes",
			"--zebra=three=yes",
			descrs, 1);
	}

	/* Short option's argument starting with `-` (glued form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'z', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-z-will",
			"-z -will",
			descrs, 1);
	}

	/* Short option's argument starting with `-` (space form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'z', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-z -will",
			"-z -will",
			descrs, 2);
	}

	/* Long option's argument starting with `-` (space form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "janine", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--janine -sutto",
			"--janine=-sutto",
			descrs, 2);
	}

	/* Long option's argument starting with `-` (equal form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "janine", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--janine=-sutto",
			"--janine=-sutto",
			descrs, 1);
	}

	/* Long option's empty argument (equal form) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'f', NULL, false },
			{ 0, '\0', "yeah", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f --yeah= -f",
			"-f --yeah= -f",
			descrs, 3);
	}
}

/*
 * Tests that the command line `cmdline`, with non-quoted
 * space-delimited arguments, once parsed given the option descriptors
 * `descrs`, fails and gives the expected error `expected_error`.
 */
static
void test_fail(const char *cmdline, const char *expected_error,
		const struct bt_argpar_opt_descr *descrs)
{
	struct bt_argpar_parse_ret parse_ret;
	gchar **argv = g_strsplit(cmdline, " ", 0);

	parse_ret = bt_argpar_parse(g_strv_length(argv),
		(const char * const *) argv, descrs, true);
	ok(!parse_ret.items,
		"bt_argpar_parse() fails for command line `%s`", cmdline);
	ok(parse_ret.error,
		"bt_argpar_parse() writes an error string for command line `%s`",
		cmdline);
	if (parse_ret.items) {
		fail("bt_argpar_parse() writes the expected error string");
		goto end;
	}

	ok(strcmp(expected_error, parse_ret.error->str) == 0,
		"bt_argpar_parse() writes the expected error string "
		"for command line `%s`", cmdline);
	if (strcmp(expected_error, parse_ret.error->str) != 0) {
		diag("Expected: `%s`", expected_error);
		diag("Got:      `%s`", parse_ret.error->str);
	}

end:
	bt_argpar_parse_ret_fini(&parse_ret);
	g_strfreev(argv);
}

static
void fail_tests(void)
{
	/* Unknown long option */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "thumb", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--thumb=party --meow",
			"While parsing argument #2 (`--meow`): Unknown option `--meow`",
			descrs);
	}

	/* Unknown short option */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "thumb", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--thumb=party -x",
			"While parsing argument #2 (`-x`): Unknown option `-x`",
			descrs);
	}

	/* Missing long option argument */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, '\0', "thumb", true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--thumb",
			"While parsing argument #1 (`--thumb`): Missing required argument for option `--thumb`",
			descrs);
	}

	/* Missing short option argument */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'k', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-k",
			"While parsing argument #1 (`-k`): Missing required argument for option `-k`",
			descrs);
	}

	/* Missing short option argument (multiple glued) */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'a', NULL, false },
			{ 0, 'b', NULL, false },
			{ 0, 'c', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-abc",
			"While parsing argument #1 (`-abc`): Missing required argument for option `-c`",
			descrs);
	}

	/* Invalid `-` */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'a', NULL, false },
			{ 0, 'b', NULL, false },
			{ 0, 'c', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-ab - -c",
			"While parsing argument #2 (`-`): Invalid argument",
			descrs);
	}

	/* Invalid `--` */
	{
		const struct bt_argpar_opt_descr descrs[] = {
			{ 0, 'a', NULL, false },
			{ 0, 'b', NULL, false },
			{ 0, 'c', NULL, true },
			BT_ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-ab -- -c",
			"While parsing argument #2 (`--`): Invalid argument",
			descrs);
	}
}

int main(void)
{
	plan_tests(129);
	succeed_tests();
	fail_tests();
	return exit_status();
}
