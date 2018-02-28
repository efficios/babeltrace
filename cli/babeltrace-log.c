/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>
#include <popt.h>
#include <glib.h>

static
void print_usage(FILE *fp)
{
	fprintf(stderr, "Usage: babeltrace-log [OPTIONS] OUTPUT-PATH\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -t, --with-timestamps  Extract timestamps from lines and map them to\n");
	fprintf(stderr, "                         a CTF clock class\n");
}

static
int parse_params(int argc, char *argv[], char **output_path,
		bool *no_extract_ts)
{
	enum {
		OPT_WITH_TIMESTAMPS = 1,
		OPT_HELP = 2,
	};
	static struct poptOption opts[] = {
		{ "with-timestamps", 't', POPT_ARG_NONE, NULL, OPT_WITH_TIMESTAMPS, NULL, NULL },
		{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL },
	};
	poptContext pc = NULL;
	int opt;
	int ret = 0;
	const char *leftover;

	*no_extract_ts = true;
	pc = poptGetContext(NULL, argc, (const char **) argv, opts, 0);
	if (!pc) {
		fprintf(stderr, "Cannot get popt context\n");
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		switch (opt) {
		case OPT_HELP:
			print_usage(stdout);
			goto end;
		case OPT_WITH_TIMESTAMPS:
			*no_extract_ts = false;
			break;
		default:
			fprintf(stderr, "Unknown command-line option specified (option code %d)\n",
				opt);
			goto error;
		}
	}

	if (opt < -1) {
		fprintf(stderr, "While parsing command-line options, at option %s: %s\n",
			poptBadOption(pc, 0), poptStrerror(opt));
		goto error;
	}

	leftover = poptGetArg(pc);
	if (!leftover) {
		fprintf(stderr, "Command line error: Missing output path\n");
		print_usage(stderr);
		goto error;
	}

	*output_path = strdup(leftover);
	BT_ASSERT(*output_path);
	goto end;

error:
	ret = 1;

end:
	if (pc) {
		poptFreeContext(pc);
	}

	return ret;
}

int main(int argc, char *argv[])
{
	char *output_path = NULL;
	bool no_extract_ts;
	int retcode;
	GError *error = NULL;
	gchar *bt_argv[] = {
		BT_CLI_PATH,
		"run",
		"--component",
		"dmesg:src.text.dmesg",
		"--params",
		NULL, /* no-extract-timestamp=? placeholder */
		"--component",
		"ctf:sink.ctf.fs",
		"--key",
		"path",
		"--value",
		NULL, /* output path placeholder */
		"--params",
		"single-trace=yes",
		"--connect",
		"dmesg:ctf",
		NULL, /* sentinel */
	};

	retcode = parse_params(argc, argv, &output_path, &no_extract_ts);
	if (retcode) {
		goto end;
	}

	if (no_extract_ts) {
		bt_argv[5] = "no-extract-timestamp=yes";
	} else {
		bt_argv[5] = "no-extract-timestamp=no";
	}

	bt_argv[11] = output_path;
	(void) g_spawn_sync(NULL, bt_argv, NULL,
		G_SPAWN_CHILD_INHERITS_STDIN, NULL, NULL,
		NULL, NULL, &retcode, &error);

	if (error) {
		fprintf(stderr, "Failed to execute \"%s\": %s (%d)\n",
			bt_argv[0], error->message, error->code);
	}

end:
	free(output_path);

	if (error) {
		g_error_free(error);
	}

	return retcode;
}
