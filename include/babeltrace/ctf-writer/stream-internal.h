#ifndef BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H
#define BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H

/*
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

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/utils-internal.h>
#include <babeltrace/ctf-writer/object-internal.h>
#include <stdint.h>

struct bt_ctf_stream_common;

struct bt_ctf_stream_common {
	struct bt_ctf_object base;
	int64_t id;
	struct bt_ctf_stream_class_common *stream_class;
	GString *name;
};

BT_HIDDEN
int bt_ctf_stream_common_initialize(
		struct bt_ctf_stream_common *stream,
		struct bt_ctf_stream_class_common *stream_class, const char *name,
		uint64_t id, bt_ctf_object_release_func release_func);

BT_HIDDEN
void bt_ctf_stream_common_finalize(struct bt_ctf_stream_common *stream);

static inline
struct bt_ctf_stream_class_common *bt_ctf_stream_common_borrow_class(
		struct bt_ctf_stream_common *stream)
{
	BT_ASSERT(stream);
	return stream->stream_class;
}

static inline
const char *bt_ctf_stream_common_get_name(struct bt_ctf_stream_common *stream)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	return stream->name ? stream->name->str : NULL;
}

static inline
int64_t bt_ctf_stream_common_get_id(struct bt_ctf_stream_common *stream)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	ret = stream->id;
	if (ret < 0) {
		BT_LOGV("Stream's ID is not set: addr=%p, name=\"%s\"",
			stream, bt_ctf_stream_common_get_name(stream));
	}

	return ret;
}

struct bt_ctf_stream {
	struct bt_ctf_stream_common common;
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
int bt_ctf_stream_set_fd(struct bt_ctf_stream *stream, int fd);

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_stream_create_with_id(
		struct bt_ctf_stream_class *stream_class,
		const char *name, uint64_t id);

#endif /* BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H */
