/*
 * stream.c
 *
 * Babeltrace CTF IR - Stream
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

#define BT_LOG_TAG "STREAM"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <unistd.h>

BT_HIDDEN
void bt_stream_common_finalize(struct bt_stream_common *stream)
{
	BT_LOGD("Finalizing common stream object: addr=%p, name=\"%s\"",
		stream, bt_stream_common_get_name(stream));

	if (stream->name) {
		g_string_free(stream->name, TRUE);
	}
}

static
void bt_stream_destroy(struct bt_object *obj)
{
	struct bt_stream *stream = (void *) obj;

	BT_LOGD("Destroying stream object: addr=%p, name=\"%s\"",
		stream, bt_stream_get_name(stream));
	bt_object_pool_finalize(&stream->packet_pool);
	bt_stream_common_finalize((void *) obj);
	g_free(stream);
}

BT_HIDDEN
int bt_stream_common_initialize(
		struct bt_stream_common *stream,
		struct bt_stream_class_common *stream_class, const char *name,
		uint64_t id, bt_object_release_func release_func)
{
	int ret = 0;
	struct bt_trace_common *trace = NULL;

	bt_object_init_shared_with_parent(&stream->base, release_func);

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto error;
	}

	BT_LOGD("Initializing common stream object: stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-name=\"%s\", "
		"stream-id=%" PRIu64,
		stream_class, bt_stream_class_common_get_name(stream_class),
		name, id);
	trace = bt_stream_class_common_borrow_trace(stream_class);
	if (!trace) {
		BT_LOGW("Invalid parameter: cannot create stream from a stream class which is not part of trace: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\"",
			stream_class,
			bt_stream_class_common_get_name(stream_class), name);
		goto error;
	}

	if (id != -1ULL) {
		/*
		 * Validate that the given ID is unique amongst all the
		 * existing trace's streams created from the same stream
		 * class.
		 */
		size_t i;

		for (i = 0; i < trace->streams->len; i++) {
			struct bt_stream_common *trace_stream =
				g_ptr_array_index(trace->streams, i);

			if (trace_stream->stream_class != (void *) stream_class) {
				continue;
			}

			if (trace_stream->id == id) {
				BT_LOGW_STR("Invalid parameter: another stream in the same trace already has this ID.");
				goto error;
			}
		}
	}

	/*
	 * Acquire reference to parent since stream will become publicly
	 * reachable; it needs its parent to remain valid.
	 */
	bt_object_set_parent(&stream->base, &trace->base);
	stream->stream_class = stream_class;
	stream->id = (int64_t) id;

	if (name) {
		stream->name = g_string_new(name);
		if (!stream->name) {
			BT_LOGE_STR("Failed to allocate a GString.");
			goto error;
		}
	}

	BT_LOGD("Set common stream's trace parent: trace-addr=%p", trace);

	/* Add this stream to the trace's streams */
	BT_LOGD("Created common stream object: addr=%p", stream);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
void bt_stream_free_packet(struct bt_packet *packet, struct bt_stream *stream)
{
	bt_packet_destroy(packet);
}

static
struct bt_stream *bt_stream_create_with_id_no_check(
		struct bt_stream_class *stream_class,
		const char *name, uint64_t id)
{
	int ret;
	struct bt_stream *stream = NULL;
	struct bt_trace *trace = NULL;

	BT_LOGD("Creating stream object: stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-name=\"%s\", "
		"stream-id=%" PRIu64,
		stream_class, bt_stream_class_get_name(stream_class),
		name, id);

	trace = BT_FROM_COMMON(bt_stream_class_common_borrow_trace(
		BT_TO_COMMON(stream_class)));
	if (!trace) {
		BT_LOGW("Invalid parameter: cannot create stream from a stream class which is not part of trace: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\"",
			stream_class, bt_stream_class_get_name(stream_class),
			name);
		goto error;
	}

	if (bt_trace_is_static(trace)) {
		/*
		 * A static trace has the property that all its stream
		 * classes, clock classes, and streams are definitive:
		 * no more can be added, and each object is also frozen.
		 */
		BT_LOGW("Invalid parameter: cannot create stream from a stream class which is part of a static trace: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\", trace-addr=%p",
			stream_class, bt_stream_class_get_name(stream_class),
			name, trace);
		goto error;
	}

	stream = g_new0(struct bt_stream, 1);
	if (!stream) {
		BT_LOGE_STR("Failed to allocate one stream.");
		goto error;
	}

	ret = bt_stream_common_initialize(BT_TO_COMMON(stream),
		BT_TO_COMMON(stream_class), name, id, bt_stream_destroy);
	if (ret) {
		/* bt_stream_common_initialize() logs errors */
		goto error;
	}

	ret = bt_object_pool_initialize(&stream->packet_pool,
		(bt_object_pool_new_object_func) bt_packet_new,
		(bt_object_pool_destroy_object_func) bt_stream_free_packet,
		stream);
	if (ret) {
		BT_LOGE("Failed to initialize packet pool: ret=%d", ret);
		goto error;
	}

	g_ptr_array_add(trace->common.streams, stream);
	BT_LOGD("Created stream object: addr=%p", stream);
	goto end;

error:
	BT_PUT(stream);

end:
	return stream;
}

struct bt_stream *bt_stream_create(struct bt_stream_class *stream_class,
		const char *name, uint64_t id_param)
{
	struct bt_stream *stream = NULL;
	int64_t id = (int64_t) id_param;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid stream's ID: "
			"name=\"%s\", id=%" PRIu64,
			name, id_param);
		goto end;
	}

	stream = bt_stream_create_with_id_no_check(stream_class,
		name, id_param);

end:
	return stream;
}

struct bt_stream_class *bt_stream_borrow_class(struct bt_stream *stream)
{
	return BT_FROM_COMMON(bt_stream_common_borrow_class(BT_TO_COMMON(stream)));
}

const char *bt_stream_get_name(struct bt_stream *stream)
{
	return bt_stream_common_get_name(BT_TO_COMMON(stream));
}

int64_t bt_stream_get_id(struct bt_stream *stream)
{
	return bt_stream_common_get_id(BT_TO_COMMON(stream));
}
