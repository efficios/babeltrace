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

#define _XOPEN_SOURCE 700
#include <config.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/format.h>
#include <popt.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ftw.h>
#include <dirent.h>
#include <unistd.h>

#define DEFAULT_FILE_ARRAY_SIZE	1
static char *opt_input_format;
static char *opt_output_format;

static const char *opt_input_path;
static const char *opt_output_path;

static struct trace_collection trace_collection_read;
static struct format *fmt_read;

void strlower(char *str)
{
	while (*str) {
		*str = tolower(*str);
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
	OPT_NO_DELTA,
};

static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "input-format", 'i', POPT_ARG_STRING, &opt_input_format, OPT_NONE, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, &opt_output_format, OPT_NONE, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "list", 'l', POPT_ARG_NONE, NULL, OPT_LIST, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ "names", 'n', POPT_ARG_STRING, NULL, OPT_NAMES, NULL, NULL },
	{ "no-delta", 0, POPT_ARG_NONE, NULL, OPT_NO_DELTA, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL },
};

static void list_formats(FILE *fp)
{
	fprintf(fp, "\n");
	bt_fprintf_format_list(fp);
}

static void usage(FILE *fp)
{
	fprintf(fp, "BabelTrace Trace Converter %s\n\n", VERSION);
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
	fprintf(fp, "  -n, --names name1<,name2,...>  Print field names.\n");
	fprintf(fp, "                                 Available field names:\n");
	fprintf(fp, "                                     (payload OR args OR arg)\n");
	fprintf(fp, "                                     all, scope, header, (context OR ctx)\n");
	fprintf(fp, "                                     trace, trace:domain, trace:procname, trace:vpid,\n");
	fprintf(fp, "                                     loglevel.\n");
	fprintf(fp, "                                        (payload active by default)\n");
	list_formats(fp);
	fprintf(fp, "\n");
}

static int get_names_args(poptContext *pc)
{
	char *str, *strlist, *strctx;

	opt_payload_field_names = 0;
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
		else if (!strcmp(str, "trace"))
			opt_trace_name = 1;
		else if (!strcmp(str, "trace:domain"))
			opt_trace_domain = 1;
		else if (!strcmp(str, "trace:procname"))
			opt_trace_procname = 1;
		else if (!strcmp(str, "trace:vpid"))
			opt_trace_vpid = 1;
		else if (!strcmp(str, "loglevel"))
			opt_loglevel = 1;
		else {
			fprintf(stdout, "[error] unknown field name type %s\n", str);
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
		case OPT_DEBUG:
			babeltrace_debug = 1;
			break;
		case OPT_NO_DELTA:
			opt_delta = 0;
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

static void init_trace_collection(struct trace_collection *tc)
{
	tc->array = g_ptr_array_sized_new(DEFAULT_FILE_ARRAY_SIZE);
}

/*
 * finalize_trace_collection() closes the opened traces for read
 * and free the memory allocated for trace collection
 */
static void finalize_trace_collection(struct trace_collection *tc)
{
	int i;

	for (i = 0; i < tc->array->len; i++) {
		struct trace_descriptor *temp =
			g_ptr_array_index(tc->array, i);
		fmt_read->close_trace(temp);
	}
	g_ptr_array_free(tc->array, TRUE);
}

static void trace_collection_add(struct trace_collection *tc,
				 struct trace_descriptor *td)
{
	g_ptr_array_add(tc->array, td);
}

/*
 * traverse_dir() is the callback functiion for File Tree Walk (nftw).
 * it receives the path of the current entry (file, dir, link..etc) with
 * a flag to indicate the type of the entry.
 * if the entry being visited is a directory and contains a metadata file,
 * then open it for reading and save a trace_descriptor to that directory
 * in the read trace collection.
 */
static int traverse_dir(const char *fpath, const struct stat *sb,
			int tflag, struct FTW *ftwbuf)
{
	int dirfd;
	int fd;
	struct trace_descriptor *td_read;

	if (tflag != FTW_D)
		return 0;
	dirfd = open(fpath, 0);
	if (dirfd < 0) {
		fprintf(stdout, "[error] unable to open trace "
			"directory file descriptor.\n");
		return -1;
	}
	fd = openat(dirfd, "metadata", O_RDONLY);
	if (fd < 0) {
		close(dirfd);
	} else {
		close(fd);
		close(dirfd);
		td_read = fmt_read->open_trace(opt_input_path,
				fpath, O_RDONLY, ctf_move_pos_slow,
				NULL);
		if (!td_read) {
			fprintf(stdout, "Error opening trace \"%s\" "
					"for reading.\n\n", fpath);
			return -1;	/* error */
		}
		trace_collection_add(&trace_collection_read, td_read);
	}
	return 0;	/* success */
}

int main(int argc, char **argv)
{
	int ret;
	struct format *fmt_write;
	struct trace_descriptor *td_write;

	ret = parse_options(argc, argv);
	if (ret < 0) {
		fprintf(stdout, "Error parsing options.\n\n");
		usage(stdout);
		exit(EXIT_FAILURE);
	} else if (ret > 0) {
		exit(EXIT_SUCCESS);
	}
	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");

	if (opt_input_format)
		strlower(opt_input_format);
	if (opt_output_format)
		strlower(opt_output_format);

	printf_verbose("Converting from directory: %s\n", opt_input_path);
	printf_verbose("Converting from format: %s\n",
		opt_input_format ? : "ctf <default>");
	printf_verbose("Converting to directory: %s\n",
		opt_output_path ? : "<stdout>");
	printf_verbose("Converting to format: %s\n",
		opt_output_format ? : "text <default>");

	if (!opt_input_format)
		opt_input_format = "ctf";
	if (!opt_output_format)
		opt_output_format = "text";
	fmt_read = bt_lookup_format(g_quark_from_static_string(opt_input_format));
	if (!fmt_read) {
		fprintf(stdout, "[error] Format \"%s\" is not supported.\n\n",
			opt_input_format);
		exit(EXIT_FAILURE);
	}
	fmt_write = bt_lookup_format(g_quark_from_static_string(opt_output_format));
	if (!fmt_write) {
		fprintf(stdout, "[error] format \"%s\" is not supported.\n\n",
			opt_output_format);
		exit(EXIT_FAILURE);
	}

	/*
	 * pass the input path to nftw() .
	 * specify traverse_dir() as the callback function.
	 * depth = 10 which is the max number of file descriptors
	 * that nftw() can open at a given time.
	 * flags = 0  check nftw documentation for more info .
	 */
	init_trace_collection(&trace_collection_read);
	ret = nftw(opt_input_path, traverse_dir, 10, 0);
	if (ret != 0) {
		fprintf(stdout, "[error] opening trace \"%s\" for reading.\n\n",
			opt_input_path);
		goto error_td_read;
	}
	if (trace_collection_read.array->len == 0) {
		fprintf(stdout, "[warning] no metadata file was found."
						" no output was generated\n");
		return 0;
	}

	td_write = fmt_write->open_trace(NULL, opt_output_path, O_RDWR, NULL, NULL);
	if (!td_write) {
		fprintf(stdout, "Error opening trace \"%s\" for writing.\n\n",
			opt_output_path ? : "<none>");
		goto error_td_write;
	}

	ret = convert_trace(td_write, &trace_collection_read);
	if (ret) {
		fprintf(stdout, "Error printing trace.\n\n");
		goto error_copy_trace;
	}

	fmt_write->close_trace(td_write);
	finalize_trace_collection(&trace_collection_read);
	printf_verbose("finished converting. Output written to:\n%s\n",
			opt_output_path ? : "<stdout>");
	exit(EXIT_SUCCESS);

	/* Error handling */
error_copy_trace:
	fmt_write->close_trace(td_write);
error_td_write:
	finalize_trace_collection(&trace_collection_read);
error_td_read:
	exit(EXIT_FAILURE);
}
