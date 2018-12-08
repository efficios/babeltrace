/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/trace-ir/stream-const.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/trace.h>
#include <babeltrace/trace-ir/trace-internal.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/property-internal.h>
#include <inttypes.h>
#include <unistd.h>

#define BT_ASSERT_PRE_STREAM_HOT(_stream) \
	BT_ASSERT_PRE_HOT((_stream), "Stream", ": %!+s", (_stream))

static
void destroy_stream(struct bt_object *obj)
{
	struct bt_stream *stream = (void *) obj;

	BT_LIB_LOGD("Destroying stream object: %!+s", stream);

	if (stream->name.str) {
		g_string_free(stream->name.str, TRUE);
		stream->name.str = NULL;
		stream->name.value = NULL;
	}

	BT_LOGD_STR("Putting stream's class.");
	bt_object_put_ref(stream->class);
	bt_object_pool_finalize(&stream->packet_pool);
	g_free(stream);
}

static
void bt_stream_free_packet(struct bt_packet *packet, struct bt_stream *stream)
{
	bt_packet_destroy(packet);
}

BT_ASSERT_PRE_FUNC
static inline
bool stream_id_is_unique(struct bt_trace *trace,
		struct bt_stream_class *stream_class, uint64_t id)
{
	uint64_t i;
	bool is_unique = true;

	for (i = 0; i < trace->streams->len; i++) {
		struct bt_stream *stream = trace->streams->pdata[i];

		if (stream->class != stream_class) {
			continue;
		}

		if (stream->id == id) {
			is_unique = false;
			goto end;
		}
	}

end:
	return is_unique;
}

static
struct bt_stream *create_stream_with_id(struct bt_stream_class *stream_class,
		struct bt_trace *trace, uint64_t id)
{
	int ret;
	struct bt_stream *stream;

	BT_ASSERT(stream_class);
	BT_ASSERT(trace);
	BT_ASSERT_PRE(trace->class ==
		bt_stream_class_borrow_trace_class_inline(stream_class),
		"Trace's class is different from stream class's parent trace class: "
		"%![sc-]+S, %![trace-]+t", stream_class, trace);
	BT_ASSERT_PRE(stream_id_is_unique(trace, stream_class, id),
		"Duplicate stream ID: %![trace-]+t, id=%" PRIu64, trace, id);
	BT_ASSERT_PRE(!trace->is_static,
		"Trace is static: %![trace-]+t", trace);
	BT_LIB_LOGD("Creating stream object: %![trace-]+t, id=%" PRIu64,
		trace, id);
	stream = g_new0(struct bt_stream, 1);
	if (!stream) {
		BT_LOGE_STR("Failed to allocate one stream.");
		goto error;
	}

	bt_object_init_shared_with_parent(&stream->base, destroy_stream);
	stream->name.str = g_string_new(NULL);
	if (!stream->name.str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	stream->id = id;
	ret = bt_object_pool_initialize(&stream->packet_pool,
		(bt_object_pool_new_object_func) bt_packet_new,
		(bt_object_pool_destroy_object_func) bt_stream_free_packet,
		stream);
	if (ret) {
		BT_LOGE("Failed to initialize packet pool: ret=%d", ret);
		goto error;
	}

	stream->class = stream_class;
	bt_object_get_no_null_check(stream_class);

	/* bt_trace_add_stream() sets the parent trace, and freezes the trace */
	bt_trace_add_stream(trace, stream);

	bt_stream_class_freeze(stream_class);
	BT_LIB_LOGD("Created stream object: %!+s", stream);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(stream);

end:
	return stream;
}

struct bt_stream *bt_stream_create(struct bt_stream_class *stream_class,
		struct bt_trace *trace)
{
	uint64_t id;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(stream_class->assigns_automatic_stream_id,
		"Stream class does not automatically assigns stream IDs: "
		"%![sc-]+S", stream_class);
	id = bt_trace_get_automatic_stream_id(trace, stream_class);
	return create_stream_with_id(stream_class, trace, id);
}

struct bt_stream *bt_stream_create_with_id(struct bt_stream_class *stream_class,
		struct bt_trace *trace, uint64_t id)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(!stream_class->assigns_automatic_stream_id,
		"Stream class automatically assigns stream IDs: "
		"%![sc-]+S", stream_class);
	return create_stream_with_id(stream_class, trace, id);
}

struct bt_stream_class *bt_stream_borrow_class(struct bt_stream *stream)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	return stream->class;
}

const struct bt_stream_class *bt_stream_borrow_class_const(
		const struct bt_stream *stream)
{
	return bt_stream_borrow_class((void *) stream);
}

struct bt_trace *bt_stream_borrow_trace(struct bt_stream *stream)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	return bt_stream_borrow_trace_inline(stream);
}

const struct bt_trace *bt_stream_borrow_trace_const(
		const struct bt_stream *stream)
{
	return bt_stream_borrow_trace((void *) stream);
}

const char *bt_stream_get_name(const struct bt_stream *stream)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Stream class");
	return stream->name.value;
}

enum bt_stream_status bt_stream_set_name(struct bt_stream *stream,
		const char *name)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Clock class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_STREAM_HOT(stream);
	g_string_assign(stream->name.str, name);
	stream->name.value = stream->name.str->str;
	BT_LIB_LOGV("Set stream class's name: %!+s", stream);
	return BT_STREAM_STATUS_OK;
}

uint64_t bt_stream_get_id(const struct bt_stream *stream)
{
	BT_ASSERT_PRE_NON_NULL(stream, "Stream class");
	return stream->id;
}

BT_HIDDEN
void _bt_stream_freeze(const struct bt_stream *stream)
{
	/* The field classes and default clock class are already frozen */
	BT_ASSERT(stream);
	BT_LIB_LOGD("Freezing stream: %!+s", stream);
	((struct bt_stream *) stream)->frozen = true;
}

void bt_stream_get_ref(const struct bt_stream *stream)
{
	bt_object_get_ref(stream);
}

void bt_stream_put_ref(const struct bt_stream *stream)
{
	bt_object_put_ref(stream);
}
