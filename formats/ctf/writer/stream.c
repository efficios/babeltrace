/*
 * stream.c
 *
 * Babeltrace CTF Writer
 *
 * Copyright 2013 EfficiOS Inc.
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

#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/align.h>

static
void bt_ctf_stream_destroy(struct bt_ctf_ref *ref);
static
void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref);
static
int init_event_header(struct bt_ctf_stream_class *stream_class,
		enum bt_ctf_byte_order byte_order);
static
int init_packet_context(struct bt_ctf_stream_class *stream_class,
		enum bt_ctf_byte_order byte_order);
static
int set_structure_field_integer(struct bt_ctf_field *, char *, uint64_t);

struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name)
{
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!name || !strlen(name)) {
		goto error;
	}

	stream_class = g_new0(struct bt_ctf_stream_class, 1);
	if (!stream_class) {
		goto error;
	}

	stream_class->name = g_string_new(name);
	stream_class->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_event_class_put);
	if (!stream_class->event_classes) {
		goto error_destroy;
	}

	bt_ctf_ref_init(&stream_class->ref_count);
	return stream_class;

error_destroy:
	bt_ctf_stream_class_destroy(&stream_class->ref_count);
	stream_class = NULL;
error:
	return stream_class;
}

int bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock)
{
	int ret = 0;

	if (!stream_class || !clock || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	if (stream_class->clock) {
		bt_ctf_clock_put(stream_class->clock);
	}

	stream_class->clock = clock;
	bt_ctf_clock_get(clock);
end:
	return ret;
}

int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class)
{
	int ret = 0;

	if (!stream_class || !event_class) {
		ret = -1;
		goto end;
	}

	/* Check for duplicate event classes */
	struct search_query query = { .value = event_class, .found = 0 };
	g_ptr_array_foreach(stream_class->event_classes, value_exists, &query);
	if (query.found) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_event_class_set_id(event_class,
		stream_class->next_event_id++)) {
		/* The event is already associated to a stream class */
		ret = -1;
		goto end;
	}

	bt_ctf_event_class_get(event_class);
	g_ptr_array_add(stream_class->event_classes, event_class);
end:
	return ret;
}

void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}

	bt_ctf_ref_get(&stream_class->ref_count);
}

void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}

	bt_ctf_ref_put(&stream_class->ref_count, bt_ctf_stream_class_destroy);
}

BT_HIDDEN
void bt_ctf_stream_class_freeze(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}

	stream_class->frozen = 1;
	bt_ctf_clock_freeze(stream_class->clock);
	g_ptr_array_foreach(stream_class->event_classes,
		(GFunc)bt_ctf_event_class_freeze, NULL);
}

BT_HIDDEN
int bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class,
	uint32_t id)
{
	int ret = 0;

	if (!stream_class ||
		(stream_class->id_set && (id != stream_class->id))) {
		ret = -1;
		goto end;
	}

	stream_class->id = id;
	stream_class->id_set = 1;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_class_set_byte_order(struct bt_ctf_stream_class *stream_class,
	enum bt_ctf_byte_order byte_order)
{
	int ret = 0;

	ret = init_packet_context(stream_class, byte_order);
	if (ret) {
		goto end;
	}

	ret = init_event_header(stream_class, byte_order);
	if (ret) {
		goto end;
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context)
{
	int ret = 0;
	size_t i;

	g_string_assign(context->field_name, "");
	context->current_indentation_level = 1;
	if (!stream_class->id_set) {
		ret = -1;
		goto end;
	}

	g_string_append_printf(context->string,
		"stream {\n\tid = %" PRIu32 ";\n\tevent.header := ",
		stream_class->id);
	ret = bt_ctf_field_type_serialize(stream_class->event_header_type,
		context);
	if (ret) {
		goto end;
	}

	g_string_append(context->string, ";\n\n\tpacket.context := ");
	ret = bt_ctf_field_type_serialize(stream_class->packet_context_type,
		context);
	if (ret) {
		goto end;
	}

	if (stream_class->event_context_type) {
		g_string_append(context->string, ";\n\n\tevent.context := ");
		ret = bt_ctf_field_type_serialize(
			stream_class->event_context_type, context);
		if (ret) {
			goto end;
		}
	}

	g_string_append(context->string, ";\n};\n\n");

	/* Assign this stream's ID to every event and serialize them */
	g_ptr_array_foreach(stream_class->event_classes,
		(GFunc) bt_ctf_event_class_set_stream_id,
		GUINT_TO_POINTER(stream_class->id));
	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *event_class =
			stream_class->event_classes->pdata[i];

		ret = bt_ctf_event_class_set_stream_id(event_class,
			stream_class->id);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_event_class_serialize(event_class, context);
		if (ret) {
			goto end;
		}
	}
end:
	context->current_indentation_level = 0;
	return ret;
}

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_stream_create(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream *stream = NULL;

	if (!stream_class) {
		goto end;
	}

	stream = g_new0(struct bt_ctf_stream, 1);
	if (!stream) {
		goto end;
	}

	bt_ctf_ref_init(&stream->ref_count);
	stream->pos.fd = -1;
	stream->id = stream_class->next_stream_id++;
	stream->stream_class = stream_class;
	bt_ctf_stream_class_get(stream_class);
	bt_ctf_stream_class_freeze(stream_class);
	stream->events = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_event_put);
end:
	return stream;
}

BT_HIDDEN
int bt_ctf_stream_set_flush_callback(struct bt_ctf_stream *stream,
	flush_func callback, void *data)
{
	int ret = stream ? 0 : -1;

	if (!stream) {
		goto end;
	}

	stream->flush.func = callback;
	stream->flush.data = data;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_set_fd(struct bt_ctf_stream *stream, int fd)
{
	int ret = 0;

	if (stream->pos.fd != -1) {
		ret = -1;
		goto end;
	}

	ctf_init_pos(&stream->pos, NULL, fd, O_RDWR);
	stream->pos.fd = fd;
end:
	return ret;
}

void bt_ctf_stream_append_discarded_events(struct bt_ctf_stream *stream,
		uint64_t event_count)
{
	if (!stream) {
		return;
	}

	stream->events_discarded += event_count;
}

int bt_ctf_stream_append_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event)
{
	int ret = 0;
	uint64_t timestamp;

	if (!stream || !event) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_event_validate(event);
	if (ret) {
		goto end;
	}

	timestamp = bt_ctf_clock_get_time(stream->stream_class->clock);
	ret = bt_ctf_event_set_timestamp(event, timestamp);
	if (ret) {
		goto end;
	}

	bt_ctf_event_get(event);
	g_ptr_array_add(stream->events, event);
end:
	return ret;
}

int bt_ctf_stream_flush(struct bt_ctf_stream *stream)
{
	int ret = 0;
	size_t i;
	uint64_t timestamp_begin, timestamp_end;
	struct bt_ctf_stream_class *stream_class;
	struct bt_ctf_field *integer = NULL;
	struct ctf_stream_pos packet_context_pos;

	if (!stream) {
		ret = -1;
		goto end;
	}

	if (!stream->events->len) {
		goto end;
	}

	if (stream->flush.func) {
		stream->flush.func(stream, stream->flush.data);
	}

	stream_class = stream->stream_class;
	timestamp_begin = ((struct bt_ctf_event *) g_ptr_array_index(
		stream->events, 0))->timestamp;
	timestamp_end = ((struct bt_ctf_event *) g_ptr_array_index(
		stream->events, stream->events->len - 1))->timestamp;
	ret = set_structure_field_integer(stream_class->packet_context,
		"timestamp_begin", timestamp_begin);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream_class->packet_context,
		"timestamp_end", timestamp_end);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream_class->packet_context,
		"events_discarded", stream->events_discarded);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream_class->packet_context,
		"content_size", UINT64_MAX);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream_class->packet_context,
		"packet_size", UINT64_MAX);
	if (ret) {
		goto end;
	}

	/* Write packet context */
	memcpy(&packet_context_pos, &stream->pos,
	       sizeof(struct ctf_stream_pos));
	ret = bt_ctf_field_serialize(stream_class->packet_context,
		&stream->pos);
	if (ret) {
		goto end;
	}

	for (i = 0; i < stream->events->len; i++) {
		struct bt_ctf_event *event = g_ptr_array_index(
			stream->events, i);
		uint32_t event_id = bt_ctf_event_class_get_id(
			event->event_class);
		uint64_t timestamp = bt_ctf_event_get_timestamp(event);

		ret = set_structure_field_integer(stream_class->event_header,
			"id", event_id);
		if (ret) {
			goto end;
		}
		ret = set_structure_field_integer(stream_class->event_header,
			"timestamp", timestamp);
		if (ret) {
			goto end;
		}

		/* Write event header */
		ret = bt_ctf_field_serialize(stream_class->event_header,
			&stream->pos);
		if (ret) {
			goto end;
		}

		/* Write event content */
		ret = bt_ctf_event_serialize(event, &stream->pos);
		if (ret) {
			goto end;
		}
	}

	/*
	 * Update the packet total size and content size and overwrite the
	 * packet context.
	 * Copy base_mma as the packet may have been remapped (e.g. when a
	 * packet is resized).
	 */
	packet_context_pos.base_mma = stream->pos.base_mma;
	ret = set_structure_field_integer(stream_class->packet_context,
		"content_size", stream->pos.offset);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream_class->packet_context,
		"packet_size", stream->pos.packet_size);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_serialize(stream_class->packet_context,
		&packet_context_pos);
	if (ret) {
		goto end;
	}

	g_ptr_array_set_size(stream->events, 0);
	stream->flushed_packet_count++;
end:
	bt_ctf_field_put(integer);
	return ret;
}

void bt_ctf_stream_get(struct bt_ctf_stream *stream)
{
	if (!stream) {
		return;
	}

	bt_ctf_ref_get(&stream->ref_count);
}

void bt_ctf_stream_put(struct bt_ctf_stream *stream)
{
	if (!stream) {
		return;
	}

	bt_ctf_ref_put(&stream->ref_count, bt_ctf_stream_destroy);
}

static
void bt_ctf_stream_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_stream *stream;

	if (!ref) {
		return;
	}

	stream = container_of(ref, struct bt_ctf_stream, ref_count);
	ctf_fini_pos(&stream->pos);
	if (close(stream->pos.fd)) {
		perror("close");
	}
	bt_ctf_stream_class_put(stream->stream_class);
	g_ptr_array_free(stream->events, TRUE);
	g_free(stream);
}

static
void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_stream_class *stream_class;

	if (!ref) {
		return;
	}

	stream_class = container_of(ref, struct bt_ctf_stream_class, ref_count);
	bt_ctf_clock_put(stream_class->clock);

	if (stream_class->event_classes) {
		g_ptr_array_free(stream_class->event_classes, TRUE);
	}

	if (stream_class->name) {
		g_string_free(stream_class->name, TRUE);
	}

	bt_ctf_field_type_put(stream_class->event_header_type);
	bt_ctf_field_put(stream_class->event_header);
	bt_ctf_field_type_put(stream_class->packet_context_type);
	bt_ctf_field_put(stream_class->packet_context);
	bt_ctf_field_type_put(stream_class->event_context_type);
	bt_ctf_field_put(stream_class->event_context);
	g_free(stream_class);
}

static
int init_event_header(struct bt_ctf_stream_class *stream_class,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;
	struct bt_ctf_field_type *event_header_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_ctf_field_type *_uint64_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT64_T);

	if (!event_header_type) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_set_byte_order(_uint32_t, byte_order);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_set_byte_order(_uint64_t, byte_order);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		_uint32_t, "id");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		_uint64_t, "timestamp");
	if (ret) {
		goto end;
	}

	stream_class->event_header_type = event_header_type;
	stream_class->event_header = bt_ctf_field_create(
		stream_class->event_header_type);
	if (!stream_class->event_header) {
		ret = -1;
	}
end:
	if (ret) {
		bt_ctf_field_type_put(event_header_type);
	}

	bt_ctf_field_type_put(_uint32_t);
	bt_ctf_field_type_put(_uint64_t);
	return ret;
}

static
int init_packet_context(struct bt_ctf_stream_class *stream_class,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;
	struct bt_ctf_field_type *packet_context_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *_uint64_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT64_T);

	if (!packet_context_type) {
		ret = -1;
		goto end;
	}

	/*
	 * We create a stream packet context as proposed in the CTF
	 * specification.
	 */
	ret = bt_ctf_field_type_set_byte_order(_uint64_t, byte_order);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "timestamp_begin");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "timestamp_end");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "content_size");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "packet_size");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "events_discarded");
	if (ret) {
		goto end;
	}

	stream_class->packet_context_type = packet_context_type;
	stream_class->packet_context = bt_ctf_field_create(packet_context_type);
	if (!stream_class->packet_context) {
		ret = -1;
	}
end:
	if (ret) {
		bt_ctf_field_type_put(packet_context_type);
		goto end;
	}

	bt_ctf_field_type_put(_uint64_t);
	return ret;
}

static
int set_structure_field_integer(struct bt_ctf_field *structure, char *name,
		uint64_t value)
{
	int ret = 0;

	struct bt_ctf_field *integer =
		bt_ctf_field_structure_get_field(structure, name);
	if (!integer) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_unsigned_integer_set_value(integer, value);
end:
	bt_ctf_field_put(integer);
	return ret;
}
