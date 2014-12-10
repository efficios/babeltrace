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

#include <babeltrace/ctf-ir/clock.h>
#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-fields-internal.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-internal.h>
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
	int ret;
	struct bt_ctf_stream *stream = NULL;

	if (!stream_class) {
		goto end;
	}

	stream = g_new0(struct bt_ctf_stream, 1);
	if (!stream) {
		goto end;
	}

	bt_ctf_ref_init(&stream->ref_count);
	stream->packet_context = bt_ctf_field_create(
		stream_class->packet_context_type);
	if (!stream->packet_context) {
		goto error_destroy;
	}

	/*
	 * A stream class may not have a stream event context defined
	 * in which case this stream will never have a stream_event_context
	 * member since, after a stream's creation, the parent stream class
	 * is "frozen" (immutable).
	 */
	if (stream_class->event_context_type) {
		stream->event_context = bt_ctf_field_create(
			stream_class->event_context_type);
		if (!stream->packet_context) {
			goto error_destroy;
		}
	}

	/* Initialize events_discarded */
	ret = set_structure_field_integer(stream->packet_context,
		"events_discarded", 0);
	if (ret) {
		goto error_destroy;
	}

	stream->pos.fd = -1;
	stream->id = stream_class->next_stream_id++;
	stream->stream_class = stream_class;
	bt_ctf_stream_class_get(stream_class);
	bt_ctf_stream_class_freeze(stream_class);
	stream->events = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_event_put);
end:
	return stream;
error_destroy:
	bt_ctf_stream_destroy(&stream->ref_count);
	return NULL;
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

int bt_ctf_stream_get_discarded_events_count(
		struct bt_ctf_stream *stream, uint64_t *count)
{
	int64_t ret = 0;
	int field_signed;
	struct bt_ctf_field *events_discarded_field = NULL;
	struct bt_ctf_field_type *events_discarded_field_type = NULL;

	if (!stream || !count || !stream->packet_context) {
		ret = -1;
		goto end;
	}

	events_discarded_field = bt_ctf_field_structure_get_field(
		stream->packet_context, "events_discarded");
	if (!events_discarded_field) {
		ret = -1;
		goto end;
	}

	events_discarded_field_type = bt_ctf_field_get_type(
		events_discarded_field);
	if (!events_discarded_field_type) {
		ret = -1;
		goto end;
	}

	field_signed = bt_ctf_field_type_integer_get_signed(
		events_discarded_field_type);
	if (field_signed < 0) {
		ret = field_signed;
		goto end;
	}

	if (field_signed) {
		int64_t signed_count;

		ret = bt_ctf_field_signed_integer_get_value(
			events_discarded_field, &signed_count);
		if (ret) {
			goto end;
		}
		if (signed_count < 0) {
			/* Invalid value */
			ret = -1;
			goto end;
		}
		*count = (uint64_t) signed_count;
	} else {
		ret = bt_ctf_field_unsigned_integer_get_value(
			events_discarded_field, count);
		if (ret) {
			goto end;
		}
	}
end:
	if (events_discarded_field) {
		bt_ctf_field_put(events_discarded_field);
	}
	if (events_discarded_field_type) {
		bt_ctf_field_type_put(events_discarded_field_type);
	}
	return ret;
}

void bt_ctf_stream_append_discarded_events(struct bt_ctf_stream *stream,
		uint64_t event_count)
{
	int ret;
	int field_signed;
	uint64_t previous_count;
	uint64_t new_count;
	struct bt_ctf_field *events_discarded_field = NULL;
	struct bt_ctf_field_type *events_discarded_field_type = NULL;

	if (!stream || !stream->packet_context) {
		goto end;
	}

	ret = bt_ctf_stream_get_discarded_events_count(stream,
		&previous_count);
	if (ret) {
		goto end;
	}

	events_discarded_field = bt_ctf_field_structure_get_field(
		stream->packet_context, "events_discarded");
	if (!events_discarded_field) {
		goto end;
	}

	events_discarded_field_type = bt_ctf_field_get_type(
		events_discarded_field);
	if (!events_discarded_field_type) {
		goto end;
	}

	field_signed = bt_ctf_field_type_integer_get_signed(
		events_discarded_field_type);
	if (field_signed < 0) {
		goto end;
	}

	new_count = previous_count + event_count;
	if (field_signed) {
		ret = bt_ctf_field_signed_integer_set_value(
			events_discarded_field, (int64_t) new_count);
		if (ret) {
			goto end;
		}
	} else {
		ret = bt_ctf_field_unsigned_integer_set_value(
			events_discarded_field, new_count);
		if (ret) {
			goto end;
		}
	}

end:
	if (events_discarded_field) {
		bt_ctf_field_put(events_discarded_field);
	}
	if (events_discarded_field_type) {
		bt_ctf_field_type_put(events_discarded_field_type);
	}
}

int bt_ctf_stream_append_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event)
{
	int ret = 0;
	uint64_t timestamp;
	struct bt_ctf_field *event_context_copy = NULL;

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

struct bt_ctf_field *bt_ctf_stream_get_packet_context(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_field *packet_context = NULL;

	if (!stream) {
		goto end;
	}

	packet_context = stream->packet_context;
end:
	if (packet_context) {
		bt_ctf_field_get(packet_context);
	}
	return packet_context;
}

int bt_ctf_stream_set_packet_context(struct bt_ctf_stream *stream,
		struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type;

	if (!stream || !field) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	if (field_type != stream->stream_class->packet_context_type) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_type_put(field_type);
	bt_ctf_field_get(field);
	bt_ctf_field_put(stream->packet_context);
	stream->packet_context = field;
end:
	return ret;
}

int bt_ctf_stream_flush(struct bt_ctf_stream *stream)
{
	int ret = 0;
	size_t i;
	uint64_t timestamp_begin, timestamp_end, events_discarded;
	struct bt_ctf_stream_class *stream_class;
	struct bt_ctf_field *integer = NULL;
	struct ctf_stream_pos packet_context_pos;

	if (!stream || stream->pos.fd < 0) {
		/*
		 * Stream does not have an associated fd. It is,
		 * therefore, not a stream being used to write events.
		 */
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

	/* Set the default context attributes if present and unset. */
	ret = set_structure_field_integer(stream->packet_context,
		"timestamp_begin", timestamp_begin);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream->packet_context,
		"timestamp_end", timestamp_end);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream->packet_context,
		"content_size", UINT64_MAX);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream->packet_context,
		"packet_size", UINT64_MAX);
	if (ret) {
		goto end;
	}

	/* Write packet context */
	memcpy(&packet_context_pos, &stream->pos,
	       sizeof(struct ctf_stream_pos));
	ret = bt_ctf_field_serialize(stream->packet_context,
		&stream->pos);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_stream_get_discarded_events_count(stream,
		&events_discarded);
	if (ret) {
		goto end;
	}

	/* Unset the packet context's fields. */
	ret = bt_ctf_field_reset(stream->packet_context);
	if (ret) {
		goto end;
	}

	/* Set the previous number of discarded events. */
	ret = set_structure_field_integer(stream->packet_context,
		"events_discarded", events_discarded);
	if (ret) {
		goto end;
	}

	for (i = 0; i < stream->events->len; i++) {
		struct bt_ctf_event *event = g_ptr_array_index(
			stream->events, i);
		uint32_t event_id = bt_ctf_event_class_get_id(
			event->event_class);
		uint64_t timestamp = bt_ctf_event_get_timestamp(event);

		ret = bt_ctf_field_reset(stream_class->event_header);
		if (ret) {
			goto end;
		}

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
	ret = set_structure_field_integer(stream->packet_context,
		"content_size", stream->pos.offset);
	if (ret) {
		goto end;
	}

	ret = set_structure_field_integer(stream->packet_context,
		"packet_size", stream->pos.packet_size);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_serialize(stream->packet_context,
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

	if (stream->stream_class) {
		bt_ctf_stream_class_put(stream->stream_class);
	}
	if (stream->events) {
		g_ptr_array_free(stream->events, TRUE);
	}
	if (stream->packet_context) {
		bt_ctf_field_put(stream->packet_context);
	}
	g_free(stream);
}

static
int set_structure_field_integer(struct bt_ctf_field *structure, char *name,
		uint64_t value)
{
	int ret = 0;
	struct bt_ctf_field *integer =
		bt_ctf_field_structure_get_field(structure, name);

	if (!structure || !name) {
		ret = -1;
		goto end;
	}

	if (!integer) {
		/* Field not found, not an error. */
		goto end;
	}

	/* Make sure the payload has not already been set. */
	if (!bt_ctf_field_validate(integer)) {
		/* Payload already set, not an error */
		goto end;
	}

	ret = bt_ctf_field_unsigned_integer_set_value(integer, value);
end:
	bt_ctf_field_put(integer);
	return ret;
}
