/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "common/assert.h"

#include "argpar.h"

static
void destroy_item(struct bt_argpar_item * const item)
{
	if (!item) {
		goto end;
	}

	if (item->type == BT_ARGPAR_ITEM_TYPE_OPT) {
		struct bt_argpar_item_opt * const opt_item = (void *) item;

		g_free((void *) opt_item->arg);
	}

	g_free(item);

end:
	return;
}

static
struct bt_argpar_item_opt *create_opt_item(
		const struct bt_argpar_opt_descr * const descr,
		const char * const arg)
{
	struct bt_argpar_item_opt *opt_item =
		g_new0(struct bt_argpar_item_opt, 1);

	if (!opt_item) {
		goto end;
	}

	opt_item->base.type = BT_ARGPAR_ITEM_TYPE_OPT;
	opt_item->descr = descr;

	if (arg) {
		opt_item->arg = g_strdup(arg);
		if (!opt_item) {
			goto error;
		}
	}

	goto end;

error:
	destroy_item(&opt_item->base);
	opt_item = NULL;

end:
	return opt_item;
}

static
struct bt_argpar_item_non_opt *create_non_opt_item(const char * const arg,
		const unsigned int orig_index,
		const unsigned int non_opt_index)
{
	struct bt_argpar_item_non_opt * const non_opt_item =
		g_new0(struct bt_argpar_item_non_opt, 1);

	if (!non_opt_item) {
		goto end;
	}

	non_opt_item->base.type = BT_ARGPAR_ITEM_TYPE_NON_OPT;
	non_opt_item->arg = arg;
	non_opt_item->orig_index = orig_index;
	non_opt_item->non_opt_index = non_opt_index;

end:
	return non_opt_item;
}

static
const struct bt_argpar_opt_descr *find_descr(
		const struct bt_argpar_opt_descr * const descrs,
		const char short_name, const char * const long_name)
{
	const struct bt_argpar_opt_descr *descr;

	for (descr = descrs; descr->short_name || descr->long_name; descr++) {
		if (short_name && descr->short_name &&
				short_name == descr->short_name) {
			goto end;
		}

		if (long_name && descr->long_name &&
				strcmp(long_name, descr->long_name) == 0) {
			goto end;
		}
	}

end:
	return !descr->short_name && !descr->long_name ? NULL : descr;
}

enum parse_orig_arg_opt_ret {
	PARSE_ORIG_ARG_OPT_RET_OK,
	PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT = -2,
	PARSE_ORIG_ARG_OPT_RET_ERROR = -1,
};

static
enum parse_orig_arg_opt_ret parse_short_opts(const char * const short_opts,
		const char * const next_orig_arg,
		const struct bt_argpar_opt_descr * const descrs,
		struct bt_argpar_parse_ret * const parse_ret,
		bool * const used_next_orig_arg)
{
	enum parse_orig_arg_opt_ret ret = PARSE_ORIG_ARG_OPT_RET_OK;
	const char *short_opt_ch = short_opts;

	if (strlen(short_opts) == 0) {
		g_string_append(parse_ret->error, "Invalid argument");
		goto error;
	}

	while (*short_opt_ch) {
		const char *opt_arg = NULL;
		const struct bt_argpar_opt_descr *descr;
		struct bt_argpar_item_opt *opt_item;

		/* Find corresponding option descriptor */
		descr = find_descr(descrs, *short_opt_ch, NULL);
		if (!descr) {
			ret = PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT;
			g_string_append_printf(parse_ret->error,
				"Unknown option `-%c`", *short_opt_ch);
			goto error;
		}

		if (descr->with_arg) {
			if (short_opt_ch[1]) {
				/* `-oarg` form */
				opt_arg = &short_opt_ch[1];
			} else {
				/* `-o arg` form */
				opt_arg = next_orig_arg;
				*used_next_orig_arg = true;
			}

			/*
			 * We accept `-o ''` (empty option's argument),
			 * but not `-o` alone if an option's argument is
			 * expected.
			 */
			if (!opt_arg || (short_opt_ch[1] && strlen(opt_arg) == 0)) {
				g_string_append_printf(parse_ret->error,
					"Missing required argument for option `-%c`",
					*short_opt_ch);
				*used_next_orig_arg = false;
				goto error;
			}
		}

		/* Create and append option argument */
		opt_item = create_opt_item(descr, opt_arg);
		if (!opt_item) {
			goto error;
		}

		g_ptr_array_add(parse_ret->items, opt_item);

		if (descr->with_arg) {
			/* Option has an argument: no more options */
			break;
		}

		/* Go to next short option */
		short_opt_ch++;
	}

	goto end;

error:
	if (ret == PARSE_ORIG_ARG_OPT_RET_OK) {
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR;
	}

end:
	return ret;
}

static
enum parse_orig_arg_opt_ret parse_long_opt(const char * const long_opt_arg,
		const char * const next_orig_arg,
		const struct bt_argpar_opt_descr * const descrs,
		struct bt_argpar_parse_ret * const parse_ret,
		bool * const used_next_orig_arg)
{
	const size_t max_len = 127;
	enum parse_orig_arg_opt_ret ret = PARSE_ORIG_ARG_OPT_RET_OK;
	const struct bt_argpar_opt_descr *descr;
	struct bt_argpar_item_opt *opt_item;

	/* Option's argument, if any */
	const char *opt_arg = NULL;

	/* Position of first `=`, if any */
	const char *eq_pos;

	/* Buffer holding option name when `long_opt_arg` contains `=` */
	char buf[max_len + 1];

	/* Option name */
	const char *long_opt_name = long_opt_arg;

	if (strlen(long_opt_arg) == 0) {
		g_string_append(parse_ret->error, "Invalid argument");
		goto error;
	}

	/* Find the first `=` in original argument */
	eq_pos = strchr(long_opt_arg, '=');
	if (eq_pos) {
		const size_t long_opt_name_size = eq_pos - long_opt_arg;

		/* Isolate the option name */
		if (long_opt_name_size > max_len) {
			g_string_append_printf(parse_ret->error,
				"Invalid argument `--%s`", long_opt_arg);
			goto error;
		}

		memcpy(buf, long_opt_arg, long_opt_name_size);
		buf[long_opt_name_size] = '\0';
		long_opt_name = buf;
	}

	/* Find corresponding option descriptor */
	descr = find_descr(descrs, '\0', long_opt_name);
	if (!descr) {
		g_string_append_printf(parse_ret->error,
			"Unknown option `--%s`", long_opt_name);
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT;
		goto error;
	}

	/* Find option's argument if any */
	if (descr->with_arg) {
		if (eq_pos) {
			/* `--long-opt=arg` style */
			opt_arg = eq_pos + 1;
		} else {
			/* `--long-opt arg` style */
			if (!next_orig_arg) {
				g_string_append_printf(parse_ret->error,
					"Missing required argument for option `--%s`",
					long_opt_name);
				goto error;
			}

			opt_arg = next_orig_arg;
			*used_next_orig_arg = true;
		}
	}

	/* Create and append option argument */
	opt_item = create_opt_item(descr, opt_arg);
	if (!opt_item) {
		goto error;
	}

	g_ptr_array_add(parse_ret->items, opt_item);
	goto end;

error:
	if (ret == PARSE_ORIG_ARG_OPT_RET_OK) {
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR;
	}

end:
	return ret;
}

static
enum parse_orig_arg_opt_ret parse_orig_arg_opt(const char * const orig_arg,
		const char * const next_orig_arg,
		const struct bt_argpar_opt_descr * const descrs,
		struct bt_argpar_parse_ret * const parse_ret,
		bool * const used_next_orig_arg)
{
	enum parse_orig_arg_opt_ret ret = PARSE_ORIG_ARG_OPT_RET_OK;

	BT_ASSERT(orig_arg[0] == '-');

	if (orig_arg[1] == '-') {
		/* Long option */
		ret = parse_long_opt(&orig_arg[2],
			next_orig_arg, descrs, parse_ret,
			used_next_orig_arg);
	} else {
		/* Short option */
		ret = parse_short_opts(&orig_arg[1],
			next_orig_arg, descrs, parse_ret,
			used_next_orig_arg);
	}

	return ret;
}

static
void prepend_while_parsing_arg_to_error(GString * const error,
		const unsigned int i, const char * const arg)
{
	/* 🙁 There's no g_string_prepend_printf()! */
	GString * const tmp_str = g_string_new(NULL);

	BT_ASSERT(error);
	BT_ASSERT(arg);

	if (!tmp_str) {
		goto end;
	}

	g_string_append_printf(tmp_str, "While parsing argument #%u (`%s`): %s",
		i + 1, arg, error->str);
	g_string_assign(error, tmp_str->str);
	g_string_free(tmp_str, TRUE);

end:
	return;
}

BT_HIDDEN
struct bt_argpar_parse_ret bt_argpar_parse(unsigned int argc,
		const char * const *argv,
		const struct bt_argpar_opt_descr * const descrs,
		bool fail_on_unknown_opt)
{
	struct bt_argpar_parse_ret parse_ret = { 0 };
	unsigned int i;
	unsigned int non_opt_index = 0;

	parse_ret.error = g_string_new(NULL);
	if (!parse_ret.error) {
		goto error;
	}

	parse_ret.items = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_item);
	if (!parse_ret.items) {
		goto error;
	}

	for (i = 0; i < argc; i++) {
		enum parse_orig_arg_opt_ret parse_orig_arg_opt_ret;
		bool used_next_orig_arg = false;
		const char * const orig_arg = argv[i];
		const char * const next_orig_arg =
			i < argc - 1 ? argv[i + 1] : NULL;

		if (orig_arg[0] != '-') {
			/* Non-option argument */
			struct bt_argpar_item_non_opt *non_opt_item =
				create_non_opt_item(orig_arg, i, non_opt_index);

			if (!non_opt_item) {
				goto error;
			}

			non_opt_index++;
			g_ptr_array_add(parse_ret.items, non_opt_item);
			continue;
		}

		/* Option argument */
		parse_orig_arg_opt_ret = parse_orig_arg_opt(orig_arg,
			next_orig_arg, descrs, &parse_ret, &used_next_orig_arg);
		switch (parse_orig_arg_opt_ret) {
		case PARSE_ORIG_ARG_OPT_RET_OK:
			break;
		case PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT:
			BT_ASSERT(!used_next_orig_arg);

			if (fail_on_unknown_opt) {
				prepend_while_parsing_arg_to_error(
					parse_ret.error, i, orig_arg);
				goto error;
			}

			/*
			 * The current original argument is not
			 * considered ingested because it triggered an
			 * unknown option.
			 */
			parse_ret.ingested_orig_args = i;
			g_string_free(parse_ret.error, TRUE);
			parse_ret.error = NULL;
			goto end;
		case PARSE_ORIG_ARG_OPT_RET_ERROR:
			prepend_while_parsing_arg_to_error(
				parse_ret.error, i, orig_arg);
			goto error;
		default:
			abort();
		}

		if (used_next_orig_arg) {
			i++;
		}
	}

	parse_ret.ingested_orig_args = argc;
	g_string_free(parse_ret.error, TRUE);
	parse_ret.error = NULL;
	goto end;

error:
	if (parse_ret.items) {
		/* That's how we indicate that an error occured */
		g_ptr_array_free(parse_ret.items, TRUE);
		parse_ret.items = NULL;
	}

end:
	return parse_ret;
}

BT_HIDDEN
void bt_argpar_parse_ret_fini(struct bt_argpar_parse_ret *ret)
{
	BT_ASSERT(ret);

	if (ret->items) {
		g_ptr_array_free(ret->items, TRUE);
		ret->items = NULL;
	}

	if (ret->error) {
		g_string_free(ret->error, TRUE);
		ret->error = NULL;
	}
}
