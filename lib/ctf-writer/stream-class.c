/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER-STREAM-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ref.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

static
int init_event_header(struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;
	struct bt_ctf_field_type *event_header_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_ctf_field_type *_uint64_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT64_T);

	if (!event_header_type) {
		BT_LOGE_STR("Cannot create empty structure field type.");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		_uint32_t, "id");
	if (ret) {
		BT_LOGE_STR("Cannot add `id` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		_uint64_t, "timestamp");
	if (ret) {
		BT_LOGE_STR("Cannot add `timestamp` field to event header field type.");
		goto end;
	}

	bt_put(stream_class->common.event_header_field_type);
	stream_class->common.event_header_field_type =
		(void *) event_header_type;
	event_header_type = NULL;

end:
	if (ret) {
		bt_put(event_header_type);
	}

	bt_put(_uint32_t);
	bt_put(_uint64_t);
	return ret;
}

static
int init_packet_context(struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;
	struct bt_ctf_field_type *packet_context_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *_uint64_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT64_T);
	struct bt_ctf_field_type *ts_begin_end_uint64_t;

	if (!packet_context_type) {
		BT_LOGE_STR("Cannot create empty structure field type.");
		ret = -1;
		goto end;
	}

	ts_begin_end_uint64_t = bt_ctf_field_type_copy(_uint64_t);
	if (!ts_begin_end_uint64_t) {
		BT_LOGE_STR("Cannot copy integer field type for `timestamp_begin` and `timestamp_end` fields.");
		ret = -1;
		goto end;
	}

	/*
	 * We create a stream packet context as proposed in the CTF
	 * specification.
	 */
	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		ts_begin_end_uint64_t, "timestamp_begin");
	if (ret) {
		BT_LOGE_STR("Cannot add `timestamp_begin` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		ts_begin_end_uint64_t, "timestamp_end");
	if (ret) {
		BT_LOGE_STR("Cannot add `timestamp_end` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "content_size");
	if (ret) {
		BT_LOGE_STR("Cannot add `content_size` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "packet_size");
	if (ret) {
		BT_LOGE_STR("Cannot add `packet_size` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "events_discarded");
	if (ret) {
		BT_LOGE_STR("Cannot add `events_discarded` field to event header field type.");
		goto end;
	}

	bt_put(stream_class->common.packet_context_field_type);
	stream_class->common.packet_context_field_type =
		(void *) packet_context_type;
	packet_context_type = NULL;

end:
	if (ret) {
		bt_put(packet_context_type);
		goto end;
	}

	bt_put(_uint64_t);
	bt_put(ts_begin_end_uint64_t);
	return ret;
}

static
void bt_ctf_stream_class_destroy(struct bt_object *obj)
{
	struct bt_ctf_stream_class *stream_class;

	stream_class = (void *) obj;
	BT_LOGD("Destroying CTF writer stream class: addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
	bt_stream_class_common_finalize(BT_TO_COMMON(stream_class));
	bt_put(stream_class->clock);
	g_free(stream_class);
}

struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name)
{
	struct bt_ctf_stream_class *stream_class;
	int ret;

	BT_LOGD("Creating CTF writer stream class object: name=\"%s\"", name);
	stream_class = g_new0(struct bt_ctf_stream_class, 1);
	if (!stream_class) {
		BT_LOGE_STR("Failed to allocate one CTF writer stream class.");
		goto error;
	}

	ret = bt_stream_class_common_initialize(BT_TO_COMMON(stream_class),
		name, bt_ctf_stream_class_destroy);
	if (ret) {
		/* bt_stream_class_common_initialize() logs errors */
		goto error;
	}

	ret = init_event_header(stream_class);
	if (ret) {
		BT_LOGE_STR("Cannot initialize stream class's event header field type.");
		goto error;
	}

	ret = init_packet_context(stream_class);
	if (ret) {
		BT_LOGE_STR("Cannot initialize stream class's packet context field type.");
		goto error;
	}

	BT_LOGD("Created CTF writer stream class object: addr=%p, name=\"%s\"",
		stream_class, name);
	return stream_class;

error:
	BT_PUT(stream_class);
	return stream_class;
}

static
int try_map_clock_class(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *parent_ft, const char *field_name)
{
	struct bt_ctf_clock_class *mapped_clock_class = NULL;
	int ret = 0;
	struct bt_ctf_field_type *ft =
		bt_ctf_field_type_structure_get_field_type_by_name(parent_ft,
			field_name);

	BT_ASSERT(stream_class->clock);

	if (!ft) {
		/* Field does not exist: not an error */
		goto end;
	}

	BT_ASSERT(((struct bt_field_type_common *) ft)->id ==
		BT_CTF_FIELD_TYPE_ID_INTEGER);
	mapped_clock_class =
		bt_ctf_field_type_integer_get_mapped_clock_class(ft);
	if (!mapped_clock_class) {
		struct bt_ctf_field_type *ft_copy;

		if (!stream_class->clock) {
			BT_LOGW("Cannot automatically set field's type mapped clock class: stream class's clock is not set: "
				"stream-class-addr=%p, stream-class-name=\"%s\", "
				"stream-class-id=%" PRId64 ", ft-addr=%p",
				stream_class,
				bt_ctf_stream_class_get_name(stream_class),
				bt_ctf_stream_class_get_id(stream_class), ft);
			ret = -1;
			goto end;
		}

		ft_copy = bt_ctf_field_type_copy(ft);
		if (!ft_copy) {
			BT_LOGE("Failed to copy integer field type: ft-addr=%p",
				ft);
		}

		ret = bt_field_type_common_integer_set_mapped_clock_class_no_check_frozen(
			(void *) ft_copy,
			BT_TO_COMMON(stream_class->clock->clock_class));
		BT_ASSERT(ret == 0);

		ret = bt_field_type_common_structure_replace_field(
			(void *) parent_ft, field_name, (void *) ft_copy);
		bt_put(ft_copy);
		BT_LOGV("Automatically mapped field type to stream class's clock class: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", ft-addr=%p, "
			"ft-copy-addr=%p",
			stream_class,
			bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class), ft, ft_copy);
	}

end:
	bt_put(ft);
	bt_put(mapped_clock_class);
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_class_map_clock_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type)
{
	int ret = 0;

	BT_ASSERT(stream_class);

	if (!stream_class->clock) {
		/* No clock class to map to */
		goto end;
	}

	if (packet_context_type) {
		if (try_map_clock_class(stream_class, packet_context_type,
				"timestamp_begin")) {
			BT_LOGE_STR("Cannot automatically set stream class's packet context field type's `timestamp_begin` field's mapped clock class.");
			ret = -1;
			goto end;
		}

		if (try_map_clock_class(stream_class, packet_context_type,
				"timestamp_end")) {
			BT_LOGE_STR("Cannot automatically set stream class's packet context field type's `timestamp_end` field's mapped clock class.");
			ret = -1;
			goto end;
		}
	}

	if (event_header_type) {
		if (try_map_clock_class(stream_class, event_header_type,
				"timestamp")) {
			BT_LOGE_STR("Cannot automatically set stream class's event header field type's `timestamp` field's mapped clock class.");
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_clock *clock = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (!stream_class->clock) {
		BT_LOGV("Stream class has no clock: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class,
			bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		goto end;
	}

	clock = bt_get(stream_class->clock);

end:
	return clock;
}

int bt_ctf_stream_class_set_clock(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock)
{
	int ret = 0;

	if (!stream_class || !clock) {
		BT_LOGW("Invalid parameter: stream class or clock is NULL: "
			"stream-class-addr=%p, clock-addr=%p",
			stream_class, clock);
		ret = -1;
		goto end;
	}

	if (stream_class->common.frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class,
			bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	/* Replace the current clock of this stream class. */
	bt_put(stream_class->clock);
	stream_class->clock = bt_get(clock);
	BT_LOGV("Set stream class's clock: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"clock-addr=%p, clock-name=\"%s\"",
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		stream_class->clock,
		bt_ctf_clock_get_name(stream_class->clock));

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context)
{
	int ret = 0;
	size_t i;
	struct bt_ctf_trace *trace;
	struct bt_ctf_field_type *packet_header_type = NULL;

	BT_LOGD("Serializing stream class's metadata: "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", metadata-context-addr=%p",
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class), context);
	g_string_assign(context->field_name, "");
	context->current_indentation_level = 1;
	if (!stream_class->common.id_set) {
		BT_LOGW_STR("Stream class's ID is not set.");
		ret = -1;
		goto end;
	}

	g_string_append(context->string, "stream {\n");

	/*
	 * The reference to the trace is only borrowed since the
	 * serialization of the stream class might have been triggered
	 * by the trace's destruction. In such a case, the trace's
	 * reference count would, unexepectedly, go through the sequence
	 * 1 -> 0 -> 1 -> 0 -> ..., provoking an endless loop of destruction
	 * and serialization.
	 */
	trace = BT_FROM_COMMON(bt_stream_class_common_borrow_trace(
		BT_TO_COMMON(stream_class)));
	BT_ASSERT(trace);
	packet_header_type = bt_ctf_trace_get_packet_header_field_type(trace);
	trace = NULL;
	if (packet_header_type) {
		struct bt_ctf_field_type *stream_id_type;

		stream_id_type =
			bt_ctf_field_type_structure_get_field_type_by_name(
				packet_header_type, "stream_id");
		if (stream_id_type) {
			/*
			 * Only set the stream's id if the trace's packet header
			 * contains a stream_id field. This field is only
			 * needed if the trace contains only one stream
			 * class.
			 */
			g_string_append_printf(context->string,
				"\tid = %" PRId64 ";\n",
				stream_class->common.id);
		}
		bt_put(stream_id_type);
	}
	if (stream_class->common.event_header_field_type) {
		BT_LOGD_STR("Serializing stream class's event header field type's metadata.");
		g_string_append(context->string, "\tevent.header := ");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) stream_class->common.event_header_field_type,
			context);
		if (ret) {
			BT_LOGW("Cannot serialize stream class's event header field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";");
	}


	if (stream_class->common.packet_context_field_type) {
		BT_LOGD_STR("Serializing stream class's packet context field type's metadata.");
		g_string_append(context->string, "\n\n\tpacket.context := ");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) stream_class->common.packet_context_field_type,
			context);
		if (ret) {
			BT_LOGW("Cannot serialize stream class's packet context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";");
	}

	if (stream_class->common.event_context_field_type) {
		BT_LOGD_STR("Serializing stream class's event context field type's metadata.");
		g_string_append(context->string, "\n\n\tevent.context := ");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) stream_class->common.event_context_field_type,
			context);
		if (ret) {
			BT_LOGW("Cannot serialize stream class's event context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";");
	}

	g_string_append(context->string, "\n};\n\n");

	for (i = 0; i < stream_class->common.event_classes->len; i++) {
		struct bt_ctf_event_class *event_class =
			stream_class->common.event_classes->pdata[i];

		ret = bt_ctf_event_class_serialize(event_class, context);
		if (ret) {
			BT_LOGW("Cannot serialize event class's metadata: "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event_class,
				bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class));
			goto end;
		}
	}

end:
	bt_put(packet_header_type);
	context->current_indentation_level = 0;
	return ret;
}

struct bt_ctf_trace *bt_ctf_stream_class_get_trace(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_get(bt_stream_class_common_borrow_trace(
		BT_TO_COMMON(stream_class)));
}

const char *bt_ctf_stream_class_get_name(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_stream_class_common_get_name(BT_TO_COMMON(stream_class));
}

int bt_ctf_stream_class_set_name(
		struct bt_ctf_stream_class *stream_class, const char *name)
{
	return bt_stream_class_common_set_name(BT_TO_COMMON(stream_class),
		name);
}

int64_t bt_ctf_stream_class_get_id(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_stream_class_common_get_id(BT_TO_COMMON(stream_class));
}

int bt_ctf_stream_class_set_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id)
{
	return bt_stream_class_common_set_id(BT_TO_COMMON(stream_class), id);
}

struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_get(
		bt_stream_class_common_borrow_packet_context_field_type(
			BT_TO_COMMON(stream_class)));
}

int bt_ctf_stream_class_set_packet_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type)
{
	return bt_stream_class_common_set_packet_context_field_type(
		BT_TO_COMMON(stream_class), (void *) packet_context_type);
}

struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_header_type(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_get(
		bt_stream_class_common_borrow_event_header_field_type(
			BT_TO_COMMON(stream_class)));
}

int bt_ctf_stream_class_set_event_header_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_header_type)
{
	return bt_stream_class_common_set_event_header_field_type(
		BT_TO_COMMON(stream_class), (void *) event_header_type);
}

struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_context_type(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_get(
		bt_stream_class_common_borrow_event_context_field_type(
			BT_TO_COMMON(stream_class)));
}

int bt_ctf_stream_class_set_event_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_context_type)
{
	return bt_stream_class_common_set_event_context_field_type(
		BT_TO_COMMON(stream_class), (void *) event_context_type);
}

int64_t bt_ctf_stream_class_get_event_class_count(
		struct bt_ctf_stream_class *stream_class)
{
	return bt_stream_class_common_get_event_class_count(
		BT_TO_COMMON(stream_class));
}

struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_index(
		struct bt_ctf_stream_class *stream_class, uint64_t index)
{
	return bt_get(
		bt_stream_class_common_borrow_event_class_by_index(
			BT_TO_COMMON(stream_class), index));
}

struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id)
{
	return bt_get(
		bt_stream_class_common_borrow_event_class_by_id(
			BT_TO_COMMON(stream_class), id));
}

int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class)
{
	return bt_stream_class_common_add_event_class(
		BT_TO_COMMON(stream_class), BT_TO_COMMON(event_class),
		(bt_validation_flag_copy_field_type_func) bt_ctf_field_type_copy);
}
