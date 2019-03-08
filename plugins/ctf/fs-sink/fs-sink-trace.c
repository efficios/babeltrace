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

static
GString *make_unique_trace_path(struct fs_sink_comp *fs_sink,
		const char *output_dir_path, const char *base)
{
	GString *path = g_string_new(output_dir_path);
	GString *san_base = NULL;
	unsigned int suffix = 0;

	if (fs_sink->assume_single_trace) {
		/* Use output directory directly */
		goto end;
	}

	san_base = sanitize_trace_path(base);
	g_string_append_printf(path, "/%s", san_base->str);

	while (g_file_test(path->str, G_FILE_TEST_IS_DIR)) {
		g_string_assign(path, output_dir_path);
		g_string_append_printf(path, "/%s%u", san_base->str, suffix);
		suffix++;
	}

end:
	if (san_base) {
		g_string_free(san_base, TRUE);
	}

	return path;
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
	const char *trace_name = bt_trace_get_name(ir_trace);
	bt_trace_status trace_status;

	if (!trace) {
		goto end;
	}

	if (!trace_name) {
		trace_name = "trace";
	}

	trace->fs_sink = fs_sink;
	trace->ir_trace = ir_trace;
	trace->ir_trace_destruction_listener_id = UINT64_C(-1);
	trace->tc = translate_trace_class_trace_ir_to_ctf_ir(
		bt_trace_borrow_class_const(ir_trace));
	if (!trace->tc) {
		goto error;
	}

	trace->path = make_unique_trace_path(fs_sink,
		fs_sink->output_dir_path->str, trace_name);
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
