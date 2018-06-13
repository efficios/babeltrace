#ifndef BABELTRACE_CTF_IR_STREAM_INTERNAL_H
#define BABELTRACE_CTF_IR_STREAM_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Stream internal
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
 */

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

struct bt_stream_class;
struct bt_stream_common;

struct bt_stream_common {
	struct bt_object base;
	int64_t id;
	struct bt_stream_class_common *stream_class;
	GString *name;
};

struct bt_stream {
	struct bt_stream_common common;

	/* Pool of `struct bt_packet *` */
	struct bt_object_pool packet_pool;
};

BT_HIDDEN
int bt_stream_common_initialize(
		struct bt_stream_common *stream,
		struct bt_stream_class_common *stream_class, const char *name,
		uint64_t id, bt_object_release_func release_func);

BT_HIDDEN
void bt_stream_common_finalize(struct bt_stream_common *stream);

static inline
struct bt_stream_class_common *bt_stream_common_borrow_class(
		struct bt_stream_common *stream)
{
	BT_ASSERT(stream);
	return stream->stream_class;
}

static inline
const char *bt_stream_common_get_name(struct bt_stream_common *stream)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	return stream->name ? stream->name->str : NULL;
}

static inline
int64_t bt_stream_common_get_id(struct bt_stream_common *stream)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	ret = stream->id;
	if (ret < 0) {
		BT_LOGV("Stream's ID is not set: addr=%p, name=\"%s\"",
			stream, bt_stream_common_get_name(stream));
	}

	return ret;
}

#endif /* BABELTRACE_CTF_IR_STREAM_INTERNAL_H */
