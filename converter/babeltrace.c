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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/events.h>
/* TODO: fix object model for format-agnostic callbacks */
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf/iterator.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/debug-info.h>

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
#include <ftw.h>
#include <string.h>
#include <signal.h>

#include <babeltrace/ctf-ir/metadata.h>	/* for clocks */

#define PARTIAL_ERROR_SLEEP	3	/* 3 seconds */

#define DEFAULT_FILE_ARRAY_SIZE	1

#define NET_URL_PREFIX	"net://"
#define NET4_URL_PREFIX	"net4://"
#define NET6_URL_PREFIX	"net6://"

static char *opt_input_format, *opt_output_format;

/*
 * We are not freeing opt_input_paths ipath elements when exiting from
 * main() for backward compatibility with libpop 0.13, which does not
 * allocate copies for arguments returned by poptGetArg(), and for
 * general compatibility with the documented behavior. This is known to
 * cause a small memory leak with libpop 0.16.
 */
static GPtrArray *opt_input_paths;
static char *opt_output_path;
static int opt_stream_intersection;

static struct bt_format *fmt_read;
static struct ctf_text_stream_pos *lttng_live_sout;

static volatile int should_quit = 0;

static
void sighandler(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		should_quit = 1;
		break;
	default:
		break;
	}
}

/* TODO: clean up the signal handler on quit */
static
int setup_sighandler(void)
{
	struct sigaction sa;
	sigset_t sigset;
	int ret;

	if ((ret = sigemptyset(&sigset)) < 0) {
		perror("sigemptyset");
		return ret;
	}
	sa.sa_handler = sighandler;
	sa.sa_mask = sigset;
	sa.sa_flags = 0;
	if ((ret = sigaction(SIGTERM, &sa, NULL)) < 0) {
		perror("sigaction");
		return ret;
	}
	if ((ret = sigaction(SIGINT, &sa, NULL)) < 0) {
		perror("sigaction");
		return ret;
	}
	return 0;
}

int lttng_live_should_quit()
{
	return should_quit;
}

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
	OPT_OUTPUT_PATH,
	OPT_INPUT_FORMAT,
	OPT_OUTPUT_FORMAT,
	OPT_HELP,
	OPT_LIST,
	OPT_VERBOSE,
	OPT_DEBUG,
	OPT_NAMES,
	OPT_FIELDS,
	OPT_NO_DELTA,
	OPT_CLOCK_OFFSET,
	OPT_CLOCK_OFFSET_NS,
	OPT_CLOCK_CYCLES,
	OPT_CLOCK_SECONDS,
	OPT_CLOCK_DATE,
	OPT_CLOCK_GMT,
	OPT_CLOCK_FORCE_CORRELATE,
	OPT_STREAM_INTERSECTION,
	OPT_DEBUG_INFO_DIR,
	OPT_DEBUG_INFO_FULL_PATH,
	OPT_DEBUG_INFO_TARGET_PREFIX,
};

/*
 * We are _not_ using POPT_ARG_STRING ability to store directly into
 * variables, because we want to cast the return to non-const, which is
 * not possible without using poptGetOptArg explicitly. This helps us
 * controlling memory allocation correctly without making assumptions
 * about undocumented behaviors. poptGetOptArg is documented as
 * requiring the returned const char * to be freed by the caller.
 */
static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "output", 'w', POPT_ARG_STRING, NULL, OPT_OUTPUT_PATH, NULL, NULL },
	{ "input-format", 'i', POPT_ARG_STRING, NULL, OPT_INPUT_FORMAT, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, NULL, OPT_OUTPUT_FORMAT, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "list", 'l', POPT_ARG_NONE, NULL, OPT_LIST, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ "names", 'n', POPT_ARG_STRING, NULL, OPT_NAMES, NULL, NULL },
	{ "fields", 'f', POPT_ARG_STRING, NULL, OPT_FIELDS, NULL, NULL },
	{ "no-delta", 0, POPT_ARG_NONE, NULL, OPT_NO_DELTA, NULL, NULL },
	{ "clock-offset", 0, POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET, NULL, NULL },
	{ "clock-offset-ns", 0, POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET_NS, NULL, NULL },
	{ "clock-cycles", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_CYCLES, NULL, NULL },
	{ "clock-seconds", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_SECONDS, NULL, NULL },
	{ "clock-date", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_DATE, NULL, NULL },
	{ "clock-gmt", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_GMT, NULL, NULL },
	{ "clock-force-correlate", 0, POPT_ARG_NONE, NULL, OPT_CLOCK_FORCE_CORRELATE, NULL, NULL },
	{ "stream-intersection", 0, POPT_ARG_NONE, NULL, OPT_STREAM_INTERSECTION, NULL, NULL },
#ifdef ENABLE_DEBUG_INFO
	{ "debug-info-dir", 0, POPT_ARG_STRING, NULL, OPT_DEBUG_INFO_DIR, NULL, NULL },
	{ "debug-info-full-path", 0, POPT_ARG_NONE, NULL, OPT_DEBUG_INFO_FULL_PATH, NULL, NULL },
	{ "debug-info-target-prefix", 0, POPT_ARG_STRING, NULL, OPT_DEBUG_INFO_TARGET_PREFIX, NULL, NULL },
#endif
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
	fprintf(fp, "usage : babeltrace [OPTIONS] FILE...\n");
	fprintf(fp, "\n");
	fprintf(fp, "  FILE                           Input trace file(s) and/or directory(ies)\n");
	fprintf(fp, "                                     (space-separated)\n");
	fprintf(fp, "  -w, --output OUTPUT            Output trace path (default: stdout)\n");
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
	fprintf(fp, "                                     all, trace, trace:hostname, trace:domain,\n");
	fprintf(fp, "                                     trace:procname, trace:vpid, loglevel, emf, callsite.\n");
	fprintf(fp, "                                     (default: trace:hostname,trace:procname,trace:vpid)\n");
	fprintf(fp, "      --clock-cycles             Timestamp in cycles\n");
	fprintf(fp, "      --clock-offset seconds     Clock offset in seconds\n");
	fprintf(fp, "      --clock-offset-ns ns       Clock offset in nanoseconds\n");
	fprintf(fp, "      --clock-seconds            Print the timestamps as [sec.ns]\n");
	fprintf(fp, "                                 (default is: [hh:mm:ss.ns])\n");
	fprintf(fp, "      --clock-date               Print clock date\n");
	fprintf(fp, "      --clock-gmt                Print clock in GMT time zone (default: local time zone)\n");
	fprintf(fp, "      --clock-force-correlate    Assume that clocks are inherently correlated\n");
	fprintf(fp, "                                 across traces.\n");
	fprintf(fp, "      --stream-intersection      Only print events when all streams are active.\n");
#ifdef ENABLE_DEBUG_INFO
	fprintf(fp, "      --debug-info-dir           Directory in which to look for debugging information\n");
	fprintf(fp, "                                 files. (default: /usr/lib/debug/)\n");
	fprintf(fp, "      --debug-info-target-prefix Directory to use as a prefix for executable lookup\n");
	fprintf(fp, "      --debug-info-full-path     Show full debug info source and binary paths (if available)\n");
#endif
	list_formats(fp);
	fprintf(fp, "\n");
}

static int get_names_args(poptContext *pc)
{
	char *str, *strlist, *strctx;
	int ret = 0;

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
			ret = -EINVAL;
			goto end;
		}
	} while ((str = strtok_r(NULL, ",", &strctx)));
end:
	free(strlist);
	return ret;
}

static int get_fields_args(poptContext *pc)
{
	char *str, *strlist, *strctx;
	int ret = 0;

	strlist = (char *) poptGetOptArg(*pc);
	if (!strlist) {
		return -EINVAL;
	}
	str = strtok_r(strlist, ",", &strctx);
	do {
		opt_trace_default_fields = 0;
		if (!strcmp(str, "all"))
			opt_all_fields = 1;
		else if (!strcmp(str, "trace"))
			opt_trace_field = 1;
		else if (!strcmp(str, "trace:hostname"))
			opt_trace_hostname_field = 1;
		else if (!strcmp(str, "trace:domain"))
			opt_trace_domain_field = 1;
		else if (!strcmp(str, "trace:procname"))
			opt_trace_procname_field = 1;
		else if (!strcmp(str, "trace:vpid"))
			opt_trace_vpid_field = 1;
		else if (!strcmp(str, "loglevel"))
			opt_loglevel_field = 1;
		else if (!strcmp(str, "emf"))
			opt_emf_field = 1;
		else if (!strcmp(str, "callsite"))
			opt_callsite_field = 1;
		else {
			fprintf(stderr, "[error] unknown field type %s\n", str);
			ret = -EINVAL;
			goto end;
		}
	} while ((str = strtok_r(NULL, ",", &strctx)));
end:
	free(strlist);
	return ret;
}

/*
 * Return 0 if caller should continue, < 0 if caller should return
 * error, > 0 if caller should exit without reporting error.
 */
static int parse_options(int argc, char **argv)
{
	poptContext pc;
	int opt, ret = 0;
	const char *ipath;

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
		case OPT_OUTPUT_PATH:
			opt_output_path = (char *) poptGetOptArg(pc);
			if (!opt_output_path) {
				ret = -EINVAL;
				goto end;
			}
			break;
		case OPT_INPUT_FORMAT:
			opt_input_format = (char *) poptGetOptArg(pc);
			if (!opt_input_format) {
				ret = -EINVAL;
				goto end;
			}
			break;
		case OPT_OUTPUT_FORMAT:
			opt_output_format = (char *) poptGetOptArg(pc);
			if (!opt_output_format) {
				ret = -EINVAL;
				goto end;
			}
			break;
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
		case OPT_CLOCK_CYCLES:
			opt_clock_cycles = 1;
			break;
		case OPT_CLOCK_OFFSET:
		{
			char *str;
			char *endptr;

			str = (char *) poptGetOptArg(pc);
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
				free(str);
				goto end;
			}
			free(str);
			break;
		}
		case OPT_CLOCK_SECONDS:
			opt_clock_seconds = 1;
			break;
		case OPT_CLOCK_OFFSET_NS:
		{
			char *str;
			char *endptr;

			str = (char *) poptGetOptArg(pc);
			if (!str) {
				fprintf(stderr, "[error] Missing --clock-offset-ns argument\n");
				ret = -EINVAL;
				goto end;
			}
			errno = 0;
			opt_clock_offset_ns = strtoull(str, &endptr, 0);
			if (*endptr != '\0' || str == endptr || errno != 0) {
				fprintf(stderr, "[error] Incorrect --clock-offset-ns argument: %s\n", str);
				ret = -EINVAL;
				free(str);
				goto end;
			}
			free(str);
			break;
		}

		case OPT_CLOCK_DATE:
			opt_clock_date = 1;
			break;
		case OPT_CLOCK_GMT:
			opt_clock_gmt = 1;
			break;
		case OPT_CLOCK_FORCE_CORRELATE:
			opt_clock_force_correlate = 1;
			break;
		case OPT_STREAM_INTERSECTION:
			opt_stream_intersection = 1;
			break;
		case OPT_DEBUG_INFO_DIR:
			opt_debug_info_dir = (char *) poptGetOptArg(pc);
			if (!opt_debug_info_dir) {
				ret = -EINVAL;
				goto end;
			}
			break;
		case OPT_DEBUG_INFO_FULL_PATH:
			opt_debug_info_full_path = 1;
			break;
		case OPT_DEBUG_INFO_TARGET_PREFIX:
			opt_debug_info_target_prefix = (char *) poptGetOptArg(pc);
			if (!opt_debug_info_target_prefix) {
				ret = -EINVAL;
				goto end;
			}
			break;
		default:
			ret = -EINVAL;
			goto end;
		}
	}

	do {
		ipath = poptGetArg(pc);
		if (ipath)
			g_ptr_array_add(opt_input_paths, (gpointer) ipath);
	} while (ipath);
	if (opt_input_paths->len == 0) {
		ret = -EINVAL;
		goto end;
	}

end:
	if (pc) {
		poptFreeContext(pc);
	}
	return ret;
}

static GPtrArray *traversed_paths = 0;

/*
 * traverse_trace_dir() is the callback function for File Tree Walk (nftw).
 * it receives the path of the current entry (file, dir, link..etc) with
 * a flag to indicate the type of the entry.
 * if the entry being visited is a directory and contains a metadata file,
 * then add the path to a global list to be processed later in
 * add_traces_recursive.
 */
static int traverse_trace_dir(const char *fpath, const struct stat *sb,
			int tflag, struct FTW *ftwbuf)
{
	int dirfd, metafd;
	int closeret;

	if (tflag != FTW_D)
		return 0;

	dirfd = open(fpath, 0);
	if (dirfd < 0) {
		fprintf(stderr, "[error] [Context] Unable to open trace "
			"directory file descriptor.\n");
		return 0;	/* partial error */
	}
	metafd = openat(dirfd, "metadata", O_RDONLY);
	if (metafd < 0) {
		closeret = close(dirfd);
		if (closeret < 0) {
			perror("close");
			return -1;
		}
		/* No meta data, just return */
		return 0;
	} else {
		int err_close = 0;

		closeret = close(metafd);
		if (closeret < 0) {
			perror("close");
			err_close = 1;
		}
		closeret = close(dirfd);
		if (closeret < 0) {
			perror("close");
			err_close = 1;
		}
		if (err_close) {
			return -1;
		}

		/* Add path to the global list */
		if (traversed_paths == NULL) {
			fprintf(stderr, "[error] [Context] Invalid open path array.\n");	
			return -1;
		}
		g_ptr_array_add(traversed_paths, g_string_new(fpath));
	}

	return 0;
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
		void (*packet_seek)(struct bt_stream_pos *pos,
			size_t offset, int whence))
{
	int ret = 0, trace_ids = 0;

	if ((strncmp(path, NET4_URL_PREFIX, sizeof(NET4_URL_PREFIX) - 1)) == 0 ||
			(strncmp(path, NET6_URL_PREFIX, sizeof(NET6_URL_PREFIX) - 1)) == 0 ||
			(strncmp(path, NET_URL_PREFIX, sizeof(NET_URL_PREFIX) - 1)) == 0) {
		ret = bt_context_add_trace(ctx,
				path, format_str, packet_seek, NULL, NULL);
		if (ret < 0) {
			fprintf(stderr, "[warning] [Context] cannot open trace \"%s\" "
					"for reading.\n", path);
		}
		return ret;
	}
	/* Should lock traversed_paths mutex here if used in multithread */

	traversed_paths = g_ptr_array_new();
	ret = nftw(path, traverse_trace_dir, 10, 0);

	/* Process the array if ntfw did not return a fatal error */
	if (ret >= 0) {
		int i;

		for (i = 0; i < traversed_paths->len; i++) {
			GString *trace_path = g_ptr_array_index(traversed_paths,
								i);
			int trace_id = bt_context_add_trace(ctx,
							    trace_path->str,
							    format_str,
							    packet_seek,
							    NULL,
							    NULL);
			if (trace_id < 0) {
				fprintf(stderr, "[warning] [Context] cannot open trace \"%s\" from %s "
					"for reading.\n", trace_path->str, path);
				/* Allow to skip erroneous traces. */
				ret = 1;	/* partial error */
			} else {
				trace_ids++;
			}
			g_string_free(trace_path, TRUE);
		}
	}

	g_ptr_array_free(traversed_paths, TRUE);
	traversed_paths = NULL;

	/* Should unlock traversed paths mutex here if used in multithread */

	/*
	 * Return an error if no trace can be opened.
	 */
	if (trace_ids == 0) {
		fprintf(stderr, "[error] Cannot open any trace for reading.\n\n");
		ret = -ENOENT;		/* failure */
	}
	return ret;
}

static
int trace_pre_handler(struct bt_trace_descriptor *td_write,
		  struct bt_context *ctx)
{
	struct ctf_text_stream_pos *sout;
	struct trace_collection *tc;
	int ret, i;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	if (!sout->parent.pre_trace_cb)
		return 0;

	tc = ctx->tc;
	for (i = 0; i < tc->array->len; i++) {
		struct bt_trace_descriptor *td =
			g_ptr_array_index(tc->array, i);

		ret = sout->parent.pre_trace_cb(&sout->parent, td);
		if (ret) {
			fprintf(stderr, "[error] Writing to trace pre handler failed.\n");
			goto end;
		}
	}
	ret = 0;
end:
	return ret;
}

static
int trace_post_handler(struct bt_trace_descriptor *td_write,
		  struct bt_context *ctx)
{
	struct ctf_text_stream_pos *sout;
	struct trace_collection *tc;
	int ret, i;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	if (!sout->parent.post_trace_cb)
		return 0;

	tc = ctx->tc;
	for (i = 0; i < tc->array->len; i++) {
		struct bt_trace_descriptor *td =
			g_ptr_array_index(tc->array, i);

		ret = sout->parent.post_trace_cb(&sout->parent, td);
		if (ret) {
			fprintf(stderr, "[error] Writing to trace post handler failed.\n");
			goto end;
		}
	}
	ret = 0;
end:
	return ret;
}

static
int process_ctf_event(struct ctf_text_stream_pos *sout, const struct bt_ctf_event *event)
{
	int ret;
	
	ret = sout->parent.event_cb(&sout->parent, event->parent->stream);
	if (ret) {
		fprintf(stderr, "[error] Writing event failed.\n");
	}

	return ret;
}

int lttng_live_process_ctf_event(const struct bt_ctf_event *event)
{
	return process_ctf_event(lttng_live_sout, event);
}

static
int convert_trace(struct bt_trace_descriptor *td_write,
		  struct bt_context *ctx)
{
	struct bt_ctf_iter *iter;
	struct ctf_text_stream_pos *sout;
	struct bt_iter_pos *begin_pos = NULL, *end_pos = NULL;
	struct bt_ctf_event *ctf_event;
	int ret;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	if (!sout->parent.event_cb) {
		return 0;
	}

	if (opt_stream_intersection) {
		iter = bt_ctf_iter_create_intersect(ctx, &begin_pos, &end_pos);
	} else {
		begin_pos = bt_iter_create_time_pos(NULL, 0);
		begin_pos->type = BT_SEEK_BEGIN;
		iter = bt_ctf_iter_create(ctx, begin_pos, NULL);
	}
	if (!iter) {
		ret = -1;
		goto error_iter;
	}
	while ((ctf_event = bt_ctf_iter_read_event(iter))) {
		ret = process_ctf_event(sout, ctf_event);
		if (ret) {
			goto end;
		}
		ret = bt_iter_next(bt_ctf_get_iter(iter));
		if (ret < 0) {
			goto end;
		}
	}
	ret = 0;

end:
	bt_ctf_iter_destroy(iter);
error_iter:
	bt_iter_free_pos(begin_pos);
	bt_iter_free_pos(end_pos);
	return ret;
}

int main(int argc, char **argv)
{
	int ret, partial_error = 0, open_success = 0;
	struct bt_format *fmt_write;
	struct bt_trace_descriptor *td_write;
	struct bt_context *ctx;
	int i;

	opt_input_paths = g_ptr_array_new();

	ret = parse_options(argc, argv);
	if (ret < 0) {
		fprintf(stderr, "Error parsing options.\n\n");
		usage(stderr);
		g_ptr_array_free(opt_input_paths, TRUE);
		exit(EXIT_FAILURE);
	} else if (ret > 0) {
		g_ptr_array_free(opt_input_paths, TRUE);
		exit(EXIT_SUCCESS);
	}
	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");

	if (opt_input_format)
		strlower(opt_input_format);
	if (opt_output_format)
		strlower(opt_output_format);

	printf_verbose("Converting from directory(ies):\n");
	for (i = 0; i < opt_input_paths->len; i++) {
		const char *ipath = g_ptr_array_index(opt_input_paths, i);
		printf_verbose("    %s\n", ipath);
	}
	printf_verbose("Converting from format: %s\n",
		opt_input_format ? : "ctf <default>");
	printf_verbose("Converting to target: %s\n",
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

	td_write = fmt_write->open_trace(opt_output_path, O_RDWR, NULL, NULL);
	if (!td_write) {
		fprintf(stderr, "Error opening trace \"%s\" for writing.\n\n",
			opt_output_path ? : "<none>");
		goto error_td_write;
	}
	if (strcmp(opt_input_format, "lttng-live") == 0) {
		if (0 > setup_sighandler()) {
			goto error_td_write;
		}
		lttng_live_sout = container_of(td_write, struct ctf_text_stream_pos, trace_descriptor);
		if (!lttng_live_sout->parent.event_cb) {
			goto error_td_write;
		}
	}

	ctx = bt_context_create();
	if (!ctx) {
		goto error_td_read;
	}

	for (i = 0; i < opt_input_paths->len; i++) {
		const char *ipath = g_ptr_array_index(opt_input_paths, i);
		ret = bt_context_add_traces_recursive(ctx, ipath,
				opt_input_format, NULL);
		if (ret < 0) {
			fprintf(stderr, "[error] opening trace \"%s\" for reading.\n\n",
				ipath);
		} else if (ret > 0) {
			fprintf(stderr, "[warning] errors occurred when opening trace \"%s\" for reading, continuing anyway.\n\n",
				ipath);
			open_success = 1;	/* some traces were OK */
			partial_error = 1;
		} else {
			open_success = 1;	/* all traces were OK */
		}
	}
	if (!open_success) {
		fprintf(stderr, "[error] none of the specified trace paths could be opened.\n\n");
		goto error_td_read;
	}

	/*
	 * Errors happened when opening traces, but we continue anyway.
	 * sleep to let user see the stderr output before stdout.
	 */
	if (partial_error)
		sleep(PARTIAL_ERROR_SLEEP);

	ret = trace_pre_handler(td_write, ctx);
	if (ret) {
		fprintf(stderr, "Error in trace pre handle.\n\n");
		goto error_copy_trace;
	}

	/* For now, we support only CTF iterators */
	if (fmt_read->name == g_quark_from_static_string("ctf")) {
		ret = convert_trace(td_write, ctx);
		if (ret) {
			fprintf(stderr, "Error printing trace.\n\n");
			goto error_copy_trace;
		}
	}

	ret = trace_post_handler(td_write, ctx);
	if (ret) {
		fprintf(stderr, "Error in trace post handle.\n\n");
		goto error_copy_trace;
	}

	fmt_write->close_trace(td_write);

	bt_context_put(ctx);
	printf_verbose("finished converting. Output written to:\n%s\n",
			opt_output_path ? : "<stdout>");
	goto end;

	/* Error handling */
error_copy_trace:
	bt_context_put(ctx);
error_td_read:
	fmt_write->close_trace(td_write);
error_td_write:
	partial_error = 1;
	/* teardown and exit */
end:
	free(opt_input_format);
	free(opt_output_format);
	free(opt_output_path);
	free(opt_debug_info_dir);
	free(opt_debug_info_target_prefix);
	g_ptr_array_free(opt_input_paths, TRUE);
	if (partial_error)
		exit(EXIT_FAILURE);
	else
		exit(EXIT_SUCCESS);
}
