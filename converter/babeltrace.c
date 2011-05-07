/*
 * babeltrace.c
 *
 * Babeltrace Trace Converter
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <popt.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

static char *opt_input_format;
static char *opt_output_format;

static const char *opt_input_path;
static const char *opt_output_path;

int babeltrace_verbose, babeltrace_debug;

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
};

static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "input-format", 'i', POPT_ARG_STRING, &opt_input_format, OPT_NONE, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, &opt_output_format, OPT_NONE, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "list", 'l', POPT_ARG_NONE, NULL, OPT_LIST, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL },
};

static void list_formats(FILE *fp)
{
	fprintf(fp, "\n");
	bt_fprintf_format_list(fp);
}

static void usage(FILE *fp)
{
	fprintf(fp, "BabelTrace Trace Converter %u.%u\n\n",
		BABELTRACE_VERSION_MAJOR,
		BABELTRACE_VERSION_MINOR);
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
	fprintf(fp, "  -d, --debug                    Debug mode\n");
	list_formats(fp);
	fprintf(fp, "\n");
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
		case OPT_DEBUG:
			babeltrace_debug = 1;
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

int main(int argc, char **argv)
{
	int ret;
	struct format *fmt_read, *fmt_write;
	struct trace_descriptor *td_read, *td_write;

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

	printf_verbose("Converting from file: %s\n", opt_input_path);
	printf_verbose("Converting from format: %s\n",
		opt_input_format ? : "ctf <default>");
	printf_verbose("Converting to file: %s\n",
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
	if (!opt_output_format)
		opt_output_format = "ctf";
	fmt_write = bt_lookup_format(g_quark_from_static_string(opt_output_format));
	if (!fmt_write) {
		fprintf(stdout, "[error] format \"%s\" is not supported.\n\n",
			opt_output_format);
		exit(EXIT_FAILURE);
	}

	td_read = fmt_read->open_trace(opt_input_path, O_RDONLY);
	if (!td_read) {
		fprintf(stdout, "[error] opening trace \"%s\" for reading.\n\n",
			opt_input_path);
		goto error_td_read;
	}

	td_write = fmt_write->open_trace(opt_output_path, O_WRONLY);
	if (!td_write) {
		fprintf(stdout, "Error opening trace \"%s\" for writing.\n\n",
			opt_output_path ? : "<none>");
		goto error_td_write;
	}

	ret = convert_trace(td_write, td_read);
	if (ret) {
		fprintf(stdout, "Error printing trace.\n\n");
		goto error_copy_trace;
	}

	fmt_write->close_trace(td_write);
	fmt_read->close_trace(td_read);
	exit(EXIT_SUCCESS);

	/* Error handling */
error_copy_trace:
	fmt_write->close_trace(td_write);
error_td_write:
	fmt_read->close_trace(td_read);
error_td_read:
	exit(EXIT_FAILURE);
}
