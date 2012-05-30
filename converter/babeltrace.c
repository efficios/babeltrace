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
 */

#define _GNU_SOURCE
#include <config.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/context.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/events.h>
/* TODO: fix object model for format-agnostic callbacks */
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf/iterator.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/iterator.h>
#include <popt.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <fts.h>
#include <string.h>

#include <babeltrace/ctf-ir/metadata.h>	/* for clocks */

#define DEFAULT_FILE_ARRAY_SIZE	1
static char *opt_input_format, *opt_output_format;
/* Pointer into const argv */
static const char *opt_input_format_arg, *opt_output_format_arg;

static const char *opt_input_path;
static const char *opt_output_path;

static struct format *fmt_read;

static
void strlower(char *str)
{
	while (*str) {
		*str = tolower((int) *str);
		str++;
	}
}

enum {
	OPT_NONE = 0,
	OPT_HELP,
	OPT_LIST,
	OPT_VERBOSE,
	OPT_DEBUG,
	OPT_NAMES,
	OPT_FIELDS,
	OPT_NO_DELTA,
	OPT_CLOCK_OFFSET,
	OPT_CLOCK_RAW,
	OPT_CLOCK_SECONDS,
	OPT_CLOCK_DATE,
	OPT_CLOCK_GMT,
	OPT_CLOCK_FORCE_CORRELATE,
};

static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "input-format", 'i', POPT_ARG_STRING, &opt_input_format_arg, OPT_NONE, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, &opt_output_format_arg, OPT_NONE, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "list", 'l', POPT_ARG_NONE, NULL, OPT_LIST, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ "names", 'n', POPT_ARG_STRING, NULL, OPT_NAMES, NULL, NULL },
	{ "fields", 'f', POPT_ARG_STRING, NULL, OPT_FIELDS, NULL, NULL },
	{ "no-delta", 0, POPT_ARG_NONE, NULL, OPT_NO_DELTA, NULL, NULL },
	{ "clock-offset", 0, POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET, NULL, NULL },
	{ "clock-raw", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_RAW, NULL, NULL },
	{ "clock-seconds", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_SECONDS, NULL, NULL },
	{ "clock-date", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_DATE, NULL, NULL },
	{ "clock-gmt", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_GMT, NULL, NULL },
	{ "clock-force-correlate", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_FORCE_CORRELATE, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL },
};

static void list_formats(FILE *fp)
{
	fprintf(fp, "\n");
	bt_fprintf_format_list(fp);
}

static void usage(FILE *fp)
{
	fprintf(fp, "BabelTrace Trace Viewer and Converter %s\n\n", VERSION);
	fprintf(fp, "usage : babeltrace [OPTIONS] INPUT <OUTPUT>\n");
	fprintf(fp, "\n");
	fprintf(fp, "  INPUT                          Input trace path\n");
	fprintf(fp, "  OUTPUT                         Output trace path (default: stdout)\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -i, --input-format FORMAT      Input trace format (default: ctf)\n");
	fprintf(fp, "  -o, --output-format FORMAT     Output trace format (default: text)\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -h, --help                     This help message\n");
	fprintf(fp, "  -l, --list                     List available formats\n");
	fprintf(fp, "  -v, --verbose                  Verbose mode\n");
	fprintf(fp, "                                 (or set BABELTRACE_VERBOSE environment variable)\n");
	fprintf(fp, "  -d, --debug                    Debug mode\n");
	fprintf(fp, "                                 (or set BABELTRACE_DEBUG environment variable)\n");
	fprintf(fp, "      --no-delta                 Do not print time delta between consecutive events\n");
	fprintf(fp, "  -n, --names name1<,name2,...>  Print field names:\n");
	fprintf(fp, "                                     (payload OR args OR arg)\n");
	fprintf(fp, "                                     none, all, scope, header, (context OR ctx)\n");
	fprintf(fp, "                                        (default: payload,context)\n");
	fprintf(fp, "  -f, --fields name1<,name2,...> Print additional fields:\n");
	fprintf(fp, "                                     all, trace, trace:domain, trace:procname,\n");
	fprintf(fp, "                                     trace:vpid, loglevel.\n");
	fprintf(fp, "      --clock-raw                Disregard internal clock offset (use raw value)\n");
	fprintf(fp, "      --clock-offset seconds     Clock offset in seconds\n");
	fprintf(fp, "      --clock-seconds            Print the timestamps as [sec.ns]\n");
	fprintf(fp, "                                 (default is: [hh:mm:ss.ns])\n");
	fprintf(fp, "      --clock-date               Print clock date\n");
	fprintf(fp, "      --clock-gmt                Print clock in GMT time zone (default: local time zone)\n");
	fprintf(fp, "      --clock-force-correlate    Assume that clocks are inherently correlated\n");
	fprintf(fp, "                                 across traces.\n");
	list_formats(fp);
	fprintf(fp, "\n");
}

static int get_names_args(poptContext *pc)
{
	char *str, *strlist, *strctx;

	opt_payload_field_names = 0;
	opt_context_field_names = 0;
	strlist = (char *) poptGetOptArg(*pc);
	if (!strlist) {
		return -EINVAL;
	}
	str = strtok_r(strlist, ",", &strctx);
	do {
		if (!strcmp(str, "all"))
			opt_all_field_names = 1;
		else if (!strcmp(str, "scope"))
			opt_scope_field_names = 1;
		else if (!strcmp(str, "context") || !strcmp(str, "ctx"))
			opt_context_field_names = 1;
		else if (!strcmp(str, "header"))
			opt_header_field_names = 1;
		else if (!strcmp(str, "payload") || !strcmp(str, "args") || !strcmp(str, "arg"))
			opt_payload_field_names = 1;
		else if (!strcmp(str, "none")) {
			opt_all_field_names = 0;
			opt_scope_field_names = 0;
			opt_context_field_names = 0;
			opt_header_field_names = 0;
			opt_payload_field_names = 0;
		} else {
			fprintf(stderr, "[error] unknown field name type %s\n", str);
			return -EINVAL;
		}
	} while ((str = strtok_r(NULL, ",", &strctx)));
	return 0;
}

static int get_fields_args(poptContext *pc)
{
	char *str, *strlist, *strctx;

	strlist = (char *) poptGetOptArg(*pc);
	if (!strlist) {
		return -EINVAL;
	}
	str = strtok_r(strlist, ",", &strctx);
	do {
		if (!strcmp(str, "all"))
			opt_all_fields = 1;
		else if (!strcmp(str, "trace"))
			opt_trace_field = 1;
		else if (!strcmp(str, "trace:domain"))
			opt_trace_domain_field = 1;
		else if (!strcmp(str, "trace:procname"))
			opt_trace_procname_field = 1;
		else if (!strcmp(str, "trace:vpid"))
			opt_trace_vpid_field = 1;
		else if (!strcmp(str, "loglevel"))
			opt_loglevel_field = 1;
		else {
			fprintf(stderr, "[error] unknown field type %s\n", str);
			return -EINVAL;
		}
	} while ((str = strtok_r(NULL, ",", &strctx)));
	return 0;
}

/*
 * Return 0 if caller should continue, < 0 if caller should return
 * error, > 0 if caller should exit without reporting error.
 */
static int parse_options(int argc, char **argv)
{
	poptContext pc;
	int opt, ret = 0;

	if (argc == 1) {
		usage(stdout);
		return 1;	/* exit cleanly */
	}

	pc = poptGetContext(NULL, argc, (const char **) argv, long_options, 0);
	poptReadDefaultConfig(pc, 0);

	/* set default */
	opt_context_field_names = 1;
	opt_payload_field_names = 1;

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_HELP:
			usage(stdout);
			ret = 1;	/* exit cleanly */
			goto end;
		case OPT_LIST:
			list_formats(stdout);
			ret = 1;
			goto end;
		case OPT_VERBOSE:
			babeltrace_verbose = 1;
			break;
		case OPT_NAMES:
			if (get_names_args(&pc)) {
				ret = -EINVAL;
				goto end;
			}
			break;
		case OPT_FIELDS:
			if (get_fields_args(&pc)) {
				ret = -EINVAL;
				goto end;
			}
			break;
		case OPT_DEBUG:
			babeltrace_debug = 1;
			break;
		case OPT_NO_DELTA:
			opt_delta_field = 0;
			break;
		case OPT_CLOCK_RAW:
			opt_clock_raw = 1;
			break;
		case OPT_CLOCK_OFFSET:
		{
			const char *str;
			char *endptr;

			str = poptGetOptArg(pc);
			if (!str) {
				fprintf(stderr, "[error] Missing --clock-offset argument\n");
				ret = -EINVAL;
				goto end;
			}
			errno = 0;
			opt_clock_offset = strtoull(str, &endptr, 0);
			if (*endptr != '\0' || str == endptr || errno != 0) {
				fprintf(stderr, "[error] Incorrect --clock-offset argument: %s\n", str);
				ret = -EINVAL;
				goto end;
			}
			break;
		}
		case OPT_CLOCK_SECONDS:
			opt_clock_seconds = 1;
			break;
		case OPT_CLOCK_DATE:
			opt_clock_date = 1;
			break;
		case OPT_CLOCK_GMT:
			opt_clock_gmt = 1;
			break;
		case OPT_CLOCK_FORCE_CORRELATE:
			opt_clock_force_correlate = 1;
			break;

		default:
			ret = -EINVAL;
			goto end;
		}
	}

	opt_input_path = poptGetArg(pc);
	if (!opt_input_path) {
		ret = -EINVAL;
		goto end;
	}
	opt_output_path = poptGetArg(pc);

end:
	if (pc) {
		poptFreeContext(pc);
	}
	return ret;
}


/*
 * bt_context_add_traces_recursive: Open a trace recursively
 *
 * Find each trace present in the subdirectory starting from the given
 * path, and add them to the context. The packet_seek parameter can be
 * NULL: this specify to use the default format packet_seek.
 *
 * Return: 0 on success, < 0 on failure, > 0 on partial failure.
 * Unable to open toplevel: failure.
 * Unable to open some subdirectory or file: warn and continue (partial
 * failure);
 */
int bt_context_add_traces_recursive(struct bt_context *ctx, const char *path,
		const char *format_str,
		void (*packet_seek)(struct stream_pos *pos,
			size_t offset, int whence))
{
	FTS *tree;
	FTSENT *node;
	GArray *trace_ids;
	char lpath[PATH_MAX];
	char * const paths[2] = { lpath, NULL };
	int ret = 0;

	/*
	 * Need to copy path, because fts_open can change it.
	 * It is the pointer array, not the strings, that are constant.
	 */
	strncpy(lpath, path, PATH_MAX);
	lpath[PATH_MAX - 1] = '\0';

	tree = fts_open(paths, FTS_NOCHDIR | FTS_LOGICAL, 0);
	if (tree == NULL) {
		fprintf(stderr, "[error] [Context] Cannot traverse \"%s\" for reading.\n",
				path);
		return -EINVAL;
	}

	trace_ids = g_array_new(FALSE, TRUE, sizeof(int));

	while ((node = fts_read(tree))) {
		int dirfd, metafd;
		int closeret;

		if (!(node->fts_info & FTS_D))
			continue;

		dirfd = open(node->fts_accpath, 0);
		if (dirfd < 0) {
			fprintf(stderr, "[error] [Context] Unable to open trace "
				"directory file descriptor.\n");
			ret = 1;	/* partial error */
			goto error;
		}
		metafd = openat(dirfd, "metadata", O_RDONLY);
		if (metafd < 0) {
			closeret = close(dirfd);
			if (closeret < 0) {
				perror("close");
				ret = -1;	/* failure */
				goto error;
			}
			continue;
		} else {
			int trace_id;

			closeret = close(metafd);
			if (closeret < 0) {
				perror("close");
				ret = -1;	/* failure */
				goto error;
			}
			closeret = close(dirfd);
			if (closeret < 0) {
				perror("close");
				ret = -1;	/* failure */
				goto error;
			}

			trace_id = bt_context_add_trace(ctx,
				node->fts_accpath, format_str,
				packet_seek, NULL, NULL);
			if (trace_id < 0) {
				fprintf(stderr, "[warning] [Context] opening trace \"%s\" from %s "
					"for reading.\n", node->fts_accpath, path);
				/* Allow to skip erroneous traces. */
				ret = 1;	/* partial error */
				continue;
			}
			g_array_append_val(trace_ids, trace_id);
		}
	}

error:
	/*
	 * Return an error if no trace can be opened.
	 */
	if (trace_ids->len == 0) {
		fprintf(stderr, "[error] Cannot open any trace for reading.\n\n");
		ret = -ENOENT;		/* failure */
	}
	g_array_free(trace_ids, TRUE);
	return ret;
}



int convert_trace(struct trace_descriptor *td_write,
		  struct bt_context *ctx)
{
	struct bt_ctf_iter *iter;
	struct ctf_text_stream_pos *sout;
	struct bt_iter_pos begin_pos;
	struct bt_ctf_event *ctf_event;
	int ret;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	begin_pos.type = BT_SEEK_BEGIN;
	iter = bt_ctf_iter_create(ctx, &begin_pos, NULL);
	if (!iter) {
		ret = -1;
		goto error_iter;
	}
	while ((ctf_event = bt_ctf_iter_read_event(iter))) {
		ret = sout->parent.event_cb(&sout->parent, ctf_event->parent->stream);
		if (ret) {
			fprintf(stderr, "[error] Writing event failed.\n");
			goto end;
		}
		ret = bt_iter_next(bt_ctf_get_iter(iter));
		if (ret < 0)
			goto end;
	}
	ret = 0;

end:
	bt_ctf_iter_destroy(iter);
error_iter:
	return ret;
}

int main(int argc, char **argv)
{
	int ret, partial_error = 0;
	struct format *fmt_write;
	struct trace_descriptor *td_write;
	struct bt_context *ctx;

	ret = parse_options(argc, argv);
	if (ret < 0) {
		fprintf(stderr, "Error parsing options.\n\n");
		usage(stderr);
		exit(EXIT_FAILURE);
	} else if (ret > 0) {
		exit(EXIT_SUCCESS);
	}
	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");

	if (opt_input_format_arg) {
		opt_input_format = strdup(opt_input_format_arg);
		if (!opt_input_format) {
			partial_error = 1;
			goto end;
		}
		strlower(opt_input_format);
	}
	if (opt_output_format_arg) {
		opt_output_format = strdup(opt_output_format_arg);
		if (!opt_output_format) {
			partial_error = 1;
			goto end;
		}
		strlower(opt_output_format);
	}

	printf_verbose("Converting from directory: %s\n", opt_input_path);
	printf_verbose("Converting from format: %s\n",
		opt_input_format ? : "ctf <default>");
	printf_verbose("Converting to directory: %s\n",
		opt_output_path ? : "<stdout>");
	printf_verbose("Converting to format: %s\n",
		opt_output_format ? : "text <default>");

	if (!opt_input_format) {
		opt_input_format = strdup("ctf");
		if (!opt_input_format) {
			partial_error = 1;
			goto end;
		}
	}
	if (!opt_output_format) {
		opt_output_format = strdup("text");
		if (!opt_output_format) {
			partial_error = 1;
			goto end;
		}
	}
	fmt_read = bt_lookup_format(g_quark_from_static_string(opt_input_format));
	if (!fmt_read) {
		fprintf(stderr, "[error] Format \"%s\" is not supported.\n\n",
			opt_input_format);
		partial_error = 1;
		goto end;
	}
	fmt_write = bt_lookup_format(g_quark_from_static_string(opt_output_format));
	if (!fmt_write) {
		fprintf(stderr, "[error] format \"%s\" is not supported.\n\n",
			opt_output_format);
		partial_error = 1;
		goto end;
	}

	ctx = bt_context_create();

	ret = bt_context_add_traces_recursive(ctx, opt_input_path,
			opt_input_format, NULL);
	if (ret < 0) {
		fprintf(stderr, "[error] opening trace \"%s\" for reading.\n\n",
			opt_input_path);
		goto error_td_read;
	} else if (ret > 0) {
		fprintf(stderr, "[warning] errors occurred when opening trace \"%s\" for reading, continuing anyway.\n\n",
			opt_input_path);
		partial_error = 1;
	}

	td_write = fmt_write->open_trace(opt_output_path, O_RDWR, NULL, NULL);
	if (!td_write) {
		fprintf(stderr, "Error opening trace \"%s\" for writing.\n\n",
			opt_output_path ? : "<none>");
		goto error_td_write;
	}

	ret = convert_trace(td_write, ctx);
	if (ret) {
		fprintf(stderr, "Error printing trace.\n\n");
		goto error_copy_trace;
	}

	fmt_write->close_trace(td_write);

	bt_context_put(ctx);
	printf_verbose("finished converting. Output written to:\n%s\n",
			opt_output_path ? : "<stdout>");
	goto end;

	/* Error handling */
error_copy_trace:
	fmt_write->close_trace(td_write);
error_td_write:
	bt_context_put(ctx);
error_td_read:
	partial_error = 1;

	/* teardown and exit */
end:
	free(opt_input_format);
	free(opt_output_format);
	if (partial_error)
		exit(EXIT_FAILURE);
	else
		exit(EXIT_SUCCESS);
}
