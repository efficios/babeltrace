/*
 * BabelTrace - Dummy Output
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

#include <babeltrace/ctf-text/types.h>
#include <babeltrace/format.h>
#include <babeltrace/babeltrace-internal.h>
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
int bt_dummy_write_event(struct stream_pos *ppos, struct ctf_stream_definition *stream)
{
	return 0;
}

static
struct trace_descriptor *bt_dummy_open_trace(const char *path, int flags,
		void (*packet_seek)(struct stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp)
{
	struct ctf_text_stream_pos *pos;

	pos = g_new0(struct ctf_text_stream_pos, 1);
	pos->parent.rw_table = NULL;
	pos->parent.event_cb = bt_dummy_write_event;
	return &pos->trace_descriptor;
}

static
int bt_dummy_close_trace(struct trace_descriptor *td)
{
	struct ctf_text_stream_pos *pos =
		container_of(td, struct ctf_text_stream_pos,
			trace_descriptor);
	free(pos);
	return 0;
}

static
struct format bt_dummy_format = {
	.open_trace = bt_dummy_open_trace,
	.close_trace = bt_dummy_close_trace,
};

static
void __attribute__((constructor)) bt_dummy_init(void)
{
	int ret;

	bt_dummy_format.name = g_quark_from_static_string("dummy");
	ret = bt_register_format(&bt_dummy_format);
	assert(!ret);
}

static
void __attribute__((destructor)) bt_dummy_exit(void)
{
	bt_unregister_format(&bt_dummy_format);
}
