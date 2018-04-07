#ifndef BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H
#define BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Stream
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/babeltrace.h>
#include <stdint.h>

struct bt_ctf_stream {
	struct bt_stream_common common;
	struct bt_ctf_field *packet_header;
	struct bt_ctf_field *packet_context;

	/* Array of pointers to bt_ctf_event for the current packet */
	GPtrArray *events;
	struct bt_ctf_stream_pos pos;
	unsigned int flushed_packet_count;
	uint64_t discarded_events;
	uint64_t size;
	uint64_t last_ts_end;
};

BT_HIDDEN
int bt_stream_set_fd(struct bt_stream *stream, int fd);

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_stream_create_with_id(
		struct bt_ctf_stream_class *stream_class,
		const char *name, uint64_t id);

#endif /* BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H */
