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
#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-fields-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/align.h>

static
void bt_ctf_stream_destroy(struct bt_ctf_ref *ref);
static
int set_structure_field_integer(struct bt_ctf_field *, char *, uint64_t);

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
