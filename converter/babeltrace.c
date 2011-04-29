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

static const char *opt_input_format;
static const char *opt_output_format;

static const char *opt_input_path;
static const char *opt_output_path;

int babeltrace_verbose, babeltrace_debug;

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
	fprintf(fp, "Babeltrace %u.%u\n\n", BABELTRACE_VERSION_MAJOR,
		BABELTRACE_VERSION_MINOR);
	fprintf(fp, "usage : babeltrace [OPTIONS] INPUT OUTPUT\n");
	fprintf(fp, "\n");
	fprintf(fp, "  INPUT               Input trace path\n");
	fprintf(fp, "  OUTPUT              Output trace path\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -i, --input-format  Input trace path\n");
	fprintf(fp, "  -o, --output-format Input trace path\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -h, --help          This help message\n");
	fprintf(fp, "  -l, --list          List available formats\n");
	fprintf(fp, "  -v, --verbose       Verbose mode\n");
	fprintf(fp, "  -d, --debug         Debug mode\n");
	list_formats(fp);
	fprintf(fp, "\n");
}

/*
 * Return 0 if caller should continue, < 0 if caller should return
 * error, > 0 if caller should exit without reporting error.
 */
static int parse_options(int argc, const char **argv)
{
	poptContext pc;
	int opt, ret = 0;

	pc = poptGetContext(NULL, argc, argv, long_options, 0);
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
	if (!opt_output_path) {
		ret = -EINVAL;
		goto end;
	}
end:
	if (pc) {
		poptFreeContext(pc);
	}
	return ret;
}

int main(int argc, const char **argv)
{
	int ret;

	ret = parse_options(argc, argv);
	if (ret < 0) {
		fprintf(stdout, "Error parsing options.\n");
		usage(stdout);
		exit(EXIT_FAILURE);
	} else if (ret > 0) {
		exit(EXIT_SUCCESS);
	}
	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");

	printf_verbose("Converting from file: %s\n", opt_input_path);
	printf_verbose("Converting from format: %s\n",
		opt_input_format ? : "<autodetect>");
	printf_verbose("Converting to file: %s\n", opt_output_path);
	printf_verbose("Converting to format: %s\n",
		opt_output_format ? : "CTF");

	return 0;
}
