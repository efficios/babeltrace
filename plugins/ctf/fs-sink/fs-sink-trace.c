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

#define BT_LOG_TAG "PLUGIN-CTF-FS-SINK-TRACE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/ctfser-internal.h>

#include "translate-trace-ir-to-ctf-ir.h"
#include "translate-ctf-ir-to-tsdl.h"
#include "fs-sink.h"
#include "fs-sink-trace.h"
#include "fs-sink-stream.h"

/*
 * Sanitizes `path` so as to:
 *
 * * Replace `.` subdirectories with `_`.
 * * Replace `..` subdirectories with `__`.
 * * Remove trailing slashes.
 */
static
GString *sanitize_trace_path(const char *path)
{
	GString *san_path = g_string_new(NULL);
	const char *ch = path;
	bool dir_start = true;

	BT_ASSERT(san_path);
	BT_ASSERT(path);

	while (*ch != '\0') {
		switch (*ch) {
		case '/':
			/* Start of directory */
			dir_start = true;
			g_string_append_c(san_path, *ch);
			ch++;
			continue;
		case '.':
			if (dir_start) {
				switch (ch[1]) {
				case '\0':
				case '/':
					/* `.` -> `_` */
					g_string_append_c(san_path, '_');
					ch++;
					continue;
				case '.':
					switch (ch[2]) {
					case '\0':
					case '/':
						/* `..` -> `__` */
						g_string_append(san_path, "__");
						ch += 2;
						continue;
					default:
						break;
					}
				default:
					break;
				}
			}
		default:
			break;
		}

		/* Not a special character */
		g_string_append_c(san_path, *ch);
		ch++;
		dir_start = false;
	}

	/* Remove trailing slashes */
	while (san_path->len > 0 &&
			san_path->str[san_path->len - 1] == '/') {
		/* Remove trailing slash */
		g_string_set_size(san_path, san_path->len - 1);
	}

	if (san_path->len == 0) {
		/* Looks like there's nothing left: just use `trace` */
		g_string_assign(san_path, "trace");
	}

	return san_path;
}

/*
 * Find a path based on `path` that doesn't exist yet.  First, try `path`
 * itself, then try with incrementing suffixes.
 */

static
GString *make_unique_trace_path(const char *path)
{
	GString *unique_path;
	unsigned int suffix = 0;

	unique_path = g_string_new(path);

	while (g_file_test(unique_path->str, G_FILE_TEST_EXISTS)) {
		g_string_printf(unique_path, "%s-%u", path, suffix);
		suffix++;
	}

	return unique_path;
}

/*
 * Validate that the input string `datetime` is an ISO8601-compliant string (the
 * format used by LTTng in the metadata).
 */

static
int lttng_validate_datetime(const char *datetime)
{
	GTimeZone *default_tz;
	GDateTime *gdatetime = NULL;
	int ret = -1;

	default_tz = g_time_zone_new_utc();
	if (!default_tz) {
		BT_LOGD("Failed to allocate timezone");
		goto end;
	}

	gdatetime = g_date_time_new_from_iso8601(datetime, default_tz);
	if (!gdatetime) {
		BT_LOGD("Couldn't parse datetime as iso8601: date=\"%s\"", datetime);
		goto end;
	}

	ret = 0;

end:
	if (default_tz) {
		g_time_zone_unref(default_tz);
		default_tz = NULL;
	}

	if (gdatetime) {
		g_date_time_unref(gdatetime);
		gdatetime = NULL;
	}

	return ret;
}

static
int append_lttng_trace_path_ust_uid(GString *path, const bt_trace_class *tc)
{
	const bt_value *v;
	int ret;

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "tracer_buffering_id");
	if (!v || !bt_value_is_integer(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"tracer_buffering_id\"");
		goto error;
	}

	g_string_append_printf(path, G_DIR_SEPARATOR_S "%" PRId64, bt_value_integer_get(v));

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "isa_length");
	if (!v || !bt_value_is_integer(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"isa_length\"");
		goto error;
	}

	g_string_append_printf(path, G_DIR_SEPARATOR_S "%" PRIu64 "-bit", bt_value_integer_get(v));

	ret = 0;
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int append_lttng_trace_path_ust_pid(GString *path, const bt_trace_class *tc)
{
	const bt_value *v;
	const char *datetime;
	int ret;

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "procname");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"procname\"");
		goto error;
	}

	g_string_append_printf(path, G_DIR_SEPARATOR_S "%s", bt_value_string_get(v));

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "vpid");
	if (!v || !bt_value_is_integer(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"vpid\"");
		goto error;
	}

	g_string_append_printf(path, "-%" PRId64, bt_value_integer_get(v));

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "vpid_datetime");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"vpid_datetime\"");
		goto error;
	}

	datetime = bt_value_string_get(v);

	if (lttng_validate_datetime(datetime)) {
		goto error;
	}

	g_string_append_printf(path, "-%s", datetime);

	ret = 0;
	goto end;

error:
	ret = -1;

end:
	return ret;
}

/*
 * Try to build a trace path based on environment values put in the trace
 * environment by the LTTng tracer, starting with version 2.11.
 */
static
GString *make_lttng_trace_path_rel(const struct fs_sink_trace *trace)
{
	const bt_trace_class *tc;
	const bt_value *v;
	const char *tracer_name, *domain, *datetime;
	int64_t tracer_major, tracer_minor;
	GString *path;

	path = g_string_new(NULL);
	if (!path) {
		goto error;
	}

	tc = bt_trace_borrow_class_const(trace->ir_trace);

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "tracer_name");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"tracer_name\"");
		goto error;
	}

	tracer_name = bt_value_string_get(v);

	if (!g_str_equal(tracer_name, "lttng-ust")
			&& !g_str_equal(tracer_name, "lttng-modules")) {
		BT_LOGD("Unrecognized tracer name: name=\"%s\"", tracer_name);
		goto error;
	}

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "tracer_major");
	if (!v || !bt_value_is_integer(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"tracer_major\"");
		goto error;
	}

	tracer_major = bt_value_integer_get(v);

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "tracer_minor");
	if (!v || !bt_value_is_integer(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"tracer_minor\"");
		goto error;
	}

	tracer_minor = bt_value_integer_get(v);

	if (!(tracer_major >= 3 || (tracer_major == 2 && tracer_minor >= 11))) {
		BT_LOGD("Unsupported LTTng version for automatic trace path: major=%" PRId64 ", minor=%" PRId64,
			tracer_major, tracer_minor);
		goto error;
	}

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "hostname");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"tracer_hostname\"");
		goto error;
	}

	g_string_assign(path, bt_value_string_get(v));

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "trace_name");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"trace_name\"");
		goto error;
	}

	g_string_append_printf(path, G_DIR_SEPARATOR_S "%s", bt_value_string_get(v));

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "trace_creation_datetime");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"trace_creation_datetime\"");
		goto error;
	}

	datetime = bt_value_string_get(v);

	if (lttng_validate_datetime(datetime)) {
		goto error;
	}

	g_string_append_printf(path, "-%s", datetime);

	v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "domain");
	if (!v || !bt_value_is_string(v)) {
		BT_LOGD_STR("Couldn't get environment value: name=\"domain\"");
		goto error;
	}

	domain = bt_value_string_get(v);
	g_string_append_printf(path, G_DIR_SEPARATOR_S "%s", domain);

	if (g_str_equal(domain, "ust")) {
		const char *tracer_buffering_scheme;

		v = bt_trace_class_borrow_environment_entry_value_by_name_const(tc, "tracer_buffering_scheme");
		if (!v || !bt_value_is_string(v)) {
			BT_LOGD_STR("Couldn't get environment value: name=\"tracer_buffering_scheme\"");
			goto error;
		}

		tracer_buffering_scheme = bt_value_string_get(v);
		g_string_append_printf(path, G_DIR_SEPARATOR_S "%s", tracer_buffering_scheme);

		if (g_str_equal(tracer_buffering_scheme, "uid")) {
			if (append_lttng_trace_path_ust_uid(path, tc)) {
				goto error;
			}
		} else if (g_str_equal(tracer_buffering_scheme, "pid")){
			if (append_lttng_trace_path_ust_pid(path, tc)) {
				goto error;
			}
		} else {
			/* Unknown buffering scheme. */
			BT_LOGD("Unknown buffering scheme: tracer_buffering_scheme=\"%s\"", tracer_buffering_scheme);
			goto error;
		}
	} else if (!g_str_equal(domain, "kernel")) {
		/* Unknown domain. */
		BT_LOGD("Unknown domain: domain=\"%s\"", domain);
		goto error;
	}

	goto end;

error:
	if (path) {
		g_string_free(path, TRUE);
		path = NULL;
	}

end:
	return path;
}

/* Build the relative output path for `trace`. */

static
GString *make_trace_path_rel(const struct fs_sink_trace *trace)
{
	GString *path = NULL;

	if (trace->fs_sink->assume_single_trace) {
		/* Use output directory directly */
		path = g_string_new("");
		goto end;
	}

	/* First, try to build a path using environment fields written by LTTng. */
	path = make_lttng_trace_path_rel(trace);
	if (path) {
		goto end;
	}

	/* Otherwise, use the trace name, if available. */
	const char *trace_name = bt_trace_get_name(trace->ir_trace);
	if (trace_name) {
		path = g_string_new(trace_name);
		goto end;
	}

	/* Otherwise, use "trace". */
	path = g_string_new("trace");

end:
	return path;
}

/*
 * Compute the trace output path for `trace`, rooted at `output_base_directory`.
 */

static
GString *make_trace_path(const struct fs_sink_trace *trace, const char *output_base_directory)
{
	GString *rel_path = NULL;
	GString *rel_path_san = NULL;
	GString *full_path = NULL;
	GString *unique_full_path = NULL;

	rel_path = make_trace_path_rel(trace);
	if (!rel_path) {
		goto end;
	}

	rel_path_san = sanitize_trace_path(rel_path->str);
	if (!rel_path_san) {
		goto end;
	}

	full_path = g_string_new(NULL);
	if (!full_path) {
		goto end;
	}

	g_string_printf(full_path, "%s" G_DIR_SEPARATOR_S "%s",
		output_base_directory, rel_path_san->str);

	unique_full_path = make_unique_trace_path(full_path->str);

end:
	if (rel_path) {
		g_string_free(rel_path, TRUE);
	}

	if (rel_path_san) {
		g_string_free(rel_path_san, TRUE);
	}

	if (full_path) {
		g_string_free(full_path, TRUE);
	}

	return unique_full_path;
}

BT_HIDDEN
void fs_sink_trace_destroy(struct fs_sink_trace *trace)
{
	GString *tsdl = NULL;
	FILE *fh = NULL;
	size_t len;

	if (!trace) {
		goto end;
	}

	if (trace->ir_trace_destruction_listener_id != UINT64_C(-1)) {
		/*
		 * Remove the destruction listener, otherwise it could
		 * be called in the future, and its private data is this
		 * CTF FS sink trace object which won't exist anymore.
		 */
		(void) bt_trace_remove_destruction_listener(trace->ir_trace,
			trace->ir_trace_destruction_listener_id);
		trace->ir_trace_destruction_listener_id = UINT64_C(-1);
	}

	if (trace->streams) {
		g_hash_table_destroy(trace->streams);
		trace->streams = NULL;
	}

	tsdl = g_string_new(NULL);
	BT_ASSERT(tsdl);
	translate_trace_class_ctf_ir_to_tsdl(trace->tc, tsdl);
	fh = fopen(trace->metadata_path->str, "wb");
	if (!fh) {
		BT_LOGF_ERRNO("In trace destruction listener: "
			"cannot open metadata file for writing: ",
			": path=\"%s\"", trace->metadata_path->str);
		abort();
	}

	len = fwrite(tsdl->str, sizeof(*tsdl->str), tsdl->len, fh);
	if (len != tsdl->len) {
		BT_LOGF_ERRNO("In trace destruction listener: "
			"cannot write metadata file: ",
			": path=\"%s\"", trace->metadata_path->str);
		abort();
	}

	if (!trace->fs_sink->quiet) {
		printf("Created CTF trace `%s`.\n", trace->path->str);
	}

	if (trace->path) {
		g_string_free(trace->path, TRUE);
		trace->path = NULL;
	}

	if (trace->metadata_path) {
		g_string_free(trace->metadata_path, TRUE);
		trace->metadata_path = NULL;
	}

	fs_sink_ctf_trace_class_destroy(trace->tc);
	trace->tc = NULL;
	g_free(trace);

end:
	if (fh) {
		int ret = fclose(fh);

		if (ret != 0) {
			BT_LOGW_ERRNO("In trace destruction listener: "
				"cannot close metadata file: ",
				": path=\"%s\"", trace->metadata_path->str);
		}
	}

	if (tsdl) {
		g_string_free(tsdl, TRUE);
	}

	return;
}

static
void ir_trace_destruction_listener(const bt_trace *ir_trace, void *data)
{
	struct fs_sink_trace *trace = data;

	/*
	 * Prevent bt_trace_remove_destruction_listener() from being
	 * called in fs_sink_trace_destroy(), which is called by
	 * g_hash_table_remove() below.
	 */
	trace->ir_trace_destruction_listener_id = UINT64_C(-1);
	g_hash_table_remove(trace->fs_sink->traces, ir_trace);
}

BT_HIDDEN
struct fs_sink_trace *fs_sink_trace_create(struct fs_sink_comp *fs_sink,
		const bt_trace *ir_trace)
{
	int ret;
	struct fs_sink_trace *trace = g_new0(struct fs_sink_trace, 1);
	bt_trace_status trace_status;

	if (!trace) {
		goto end;
	}

	trace->fs_sink = fs_sink;
	trace->ir_trace = ir_trace;
	trace->ir_trace_destruction_listener_id = UINT64_C(-1);
	trace->tc = translate_trace_class_trace_ir_to_ctf_ir(
		bt_trace_borrow_class_const(ir_trace));
	if (!trace->tc) {
		goto error;
	}

	trace->path = make_trace_path(trace, fs_sink->output_dir_path->str);
	BT_ASSERT(trace->path);
	ret = g_mkdir_with_parents(trace->path->str, 0755);
	if (ret) {
		BT_LOGE_ERRNO("Cannot create directories for trace directory",
			": path=\"%s\"", trace->path->str);
		goto error;
	}

	trace->metadata_path = g_string_new(trace->path->str);
	BT_ASSERT(trace->metadata_path);
	g_string_append(trace->metadata_path, "/metadata");
	trace->streams = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) fs_sink_stream_destroy);
	BT_ASSERT(trace->streams);
	trace_status = bt_trace_add_destruction_listener(ir_trace,
		ir_trace_destruction_listener, trace,
		&trace->ir_trace_destruction_listener_id);
	if (trace_status) {
		goto error;
	}

	g_hash_table_insert(fs_sink->traces, (gpointer) ir_trace, trace);
	goto end;

error:
	fs_sink_trace_destroy(trace);
	trace = NULL;

end:
	return trace;
}
