/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * CTF Metadata Dump.
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

#include <babeltrace/format.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/events-internal.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>

static
struct bt_trace_descriptor *ctf_metadata_open_trace(const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp);
static
int ctf_metadata_close_trace(struct bt_trace_descriptor *descriptor);

static
struct bt_format ctf_metadata_format = {
	.open_trace = ctf_metadata_open_trace,
	.close_trace = ctf_metadata_close_trace,
};

void bt_ctf_metadata_hook(void)
{
	/*
	 * Dummy function to prevent the linker from discarding this format as
	 * "unused" in static builds.
	 */
}

static
int ctf_metadata_trace_pre_handler(struct bt_stream_pos *ppos,
			struct bt_trace_descriptor *td)
{
	struct ctf_text_stream_pos *pos =
		container_of(ppos, struct ctf_text_stream_pos, parent);
	struct ctf_trace *trace;

	trace = container_of(td, struct ctf_trace, parent);
	if (!trace->metadata_string)
		return -EINVAL;
	if (trace->metadata_packetized) {
		fprintf(pos->fp, "/* CTF %u.%u */\n",
			BT_CTF_MAJOR, BT_CTF_MINOR);
	}
	fprintf(pos->fp, "%s", trace->metadata_string);
	return 0;
}

static
struct bt_trace_descriptor *ctf_metadata_open_trace(const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp)
{
	struct ctf_text_stream_pos *pos;
	FILE *fp;

	pos = g_new0(struct ctf_text_stream_pos, 1);

	pos->last_real_timestamp = -1ULL;
	pos->last_cycles_timestamp = -1ULL;
	switch (flags & O_ACCMODE) {
	case O_RDWR:
		if (!path)
			fp = stdout;
		else
			fp = fopen(path, "w");
		if (!fp)
			goto error;
		pos->fp = fp;
		pos->parent.pre_trace_cb = ctf_metadata_trace_pre_handler;
		pos->parent.trace = &pos->trace_descriptor;
		pos->print_names = 0;
		break;
	case O_RDONLY:
	default:
		fprintf(stderr, "[error] Incorrect open flags.\n");
		goto error;
	}

	return &pos->trace_descriptor;
error:
	g_free(pos);
	return NULL;
}

static
int ctf_metadata_close_trace(struct bt_trace_descriptor *td)
{
	int ret;
	struct ctf_text_stream_pos *pos =
		container_of(td, struct ctf_text_stream_pos, trace_descriptor);
	if (pos->fp != stdout) {
		ret = fclose(pos->fp);
		if (ret) {
			perror("Error on fclose");
			return -1;
		}
	}
	g_free(pos);
	return 0;
}

static
void __attribute__((constructor)) ctf_metadata_init(void)
{
	int ret;

	ctf_metadata_format.name = g_quark_from_static_string("ctf-metadata");
	ret = bt_register_format(&ctf_metadata_format);
	assert(!ret);
}

static
void __attribute__((destructor)) ctf_metadata_exit(void)
{
	bt_unregister_format(&ctf_metadata_format);
}
