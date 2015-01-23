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
#include <babeltrace/ctf/ctf-index.h>

static
void bt_ctf_stream_destroy(struct bt_ctf_ref *ref);
static
int set_structure_field_integer(struct bt_ctf_field *, char *, uint64_t);

static
int set_packet_header_magic(struct bt_ctf_stream *stream)
{
	int ret = 0;
	struct bt_ctf_field_type *magic_field_type = NULL;
	struct bt_ctf_field *magic_field = bt_ctf_field_structure_get_field(
		stream->packet_header, "magic");

	if (!magic_field) {
		/* No magic field found. Not an error, skip. */
		goto end;
	}

	if (!bt_ctf_field_validate(magic_field)) {
		/* Value already set. Not an error, skip. */
		goto end;
	}

	magic_field_type = bt_ctf_field_get_type(magic_field);
	assert(magic_field_type);

	if (bt_ctf_field_type_get_type_id(magic_field_type) !=
		CTF_TYPE_INTEGER) {
		/* Magic field is not an integer. Not an error, skip. */
		goto end;
	}

	if (bt_ctf_field_type_integer_get_size(magic_field_type) != 32) {
		/*
		 * Magic field is not of the expected size.
		 * Not an error, skip.
		 */
		goto end;
	}

	ret = bt_ctf_field_type_integer_get_signed(magic_field_type);
	assert(ret >= 0);
	if (ret) {
		ret = bt_ctf_field_signed_integer_set_value(magic_field,
			(int64_t) 0xC1FC1FC1);
	} else {
		ret = bt_ctf_field_unsigned_integer_set_value(magic_field,
			(uint64_t) 0xC1FC1FC1);
	}
end:
	if (magic_field) {
		bt_ctf_field_put(magic_field);
	}
	if (magic_field_type) {
		bt_ctf_field_type_put(magic_field_type);
	}
	return ret;
}

static
int set_packet_header_uuid(struct bt_ctf_stream *stream)
{
	int i, ret = 0;
	struct bt_ctf_field_type *uuid_field_type = NULL;
	struct bt_ctf_field_type *element_field_type = NULL;
	struct bt_ctf_field *uuid_field = bt_ctf_field_structure_get_field(
		stream->packet_header, "uuid");

	if (!uuid_field) {
		/* No uuid field found. Not an error, skip. */
		goto end;
	}

	if (!bt_ctf_field_validate(uuid_field)) {
		/* Value already set. Not an error, skip. */
		goto end;
	}

	uuid_field_type = bt_ctf_field_get_type(uuid_field);
	assert(uuid_field_type);
	if (bt_ctf_field_type_get_type_id(uuid_field_type) !=
		CTF_TYPE_ARRAY) {
		/* UUID field is not an array. Not an error, skip. */
		goto end;
	}

	if (bt_ctf_field_type_array_get_length(uuid_field_type) != 16) {
		/*
		 * UUID field is not of the expected size.
		 * Not an error, skip.
		 */
		goto end;
	}

	element_field_type = bt_ctf_field_type_array_get_element_type(
		uuid_field_type);
	assert(element_field_type);
	if (bt_ctf_field_type_get_type_id(element_field_type) !=
		CTF_TYPE_INTEGER) {
		/* UUID array elements are not integers. Not an error, skip */
		goto end;
	}

	for (i = 0; i < 16; i++) {
		struct bt_ctf_field *uuid_element =
			bt_ctf_field_array_get_field(uuid_field, i);

		ret = bt_ctf_field_type_integer_get_signed(element_field_type);
		assert(ret >= 0);

		if (ret) {
			ret = bt_ctf_field_signed_integer_set_value(
				uuid_element, (int64_t) stream->trace->uuid[i]);
		} else {
			ret = bt_ctf_field_unsigned_integer_set_value(
				uuid_element,
				(uint64_t) stream->trace->uuid[i]);
		}
		bt_ctf_field_put(uuid_element);
		if (ret) {
			goto end;
		}
	}

end:
	if (uuid_field) {
		bt_ctf_field_put(uuid_field);
	}
	if (uuid_field_type) {
		bt_ctf_field_type_put(uuid_field_type);
	}
	if (element_field_type) {
		bt_ctf_field_type_put(element_field_type);
	}
	return ret;
}
static
int set_packet_header_stream_id(struct bt_ctf_stream *stream)
{
	int ret = 0;
	uint32_t stream_id;
	struct bt_ctf_field_type *stream_id_field_type = NULL;
	struct bt_ctf_field *stream_id_field = bt_ctf_field_structure_get_field(
		stream->packet_header, "stream_id");

	if (!stream_id_field) {
		/* No stream_id field found. Not an error, skip. */
		goto end;
	}

	if (!bt_ctf_field_validate(stream_id_field)) {
		/* Value already set. Not an error, skip. */
		goto end;
	}

	stream_id_field_type = bt_ctf_field_get_type(stream_id_field);
	assert(stream_id_field_type);
	if (bt_ctf_field_type_get_type_id(stream_id_field_type) !=
		CTF_TYPE_INTEGER) {
		/* stream_id field is not an integer. Not an error, skip. */
		goto end;
	}

	stream_id = stream->stream_class->id;
	ret = bt_ctf_field_type_integer_get_signed(stream_id_field_type);
	assert(ret >= 0);
	if (ret) {
		ret = bt_ctf_field_signed_integer_set_value(stream_id_field,
			(int64_t) stream_id);
	} else {
		ret = bt_ctf_field_unsigned_integer_set_value(stream_id_field,
			(uint64_t) stream_id);
	}
end:
	if (stream_id_field) {
		bt_ctf_field_put(stream_id_field);
	}
	if (stream_id_field_type) {
		bt_ctf_field_type_put(stream_id_field_type);
	}
	return ret;
}

static
int set_packet_header(struct bt_ctf_stream *stream)
{
	int ret;

	ret = set_packet_header_magic(stream);
	if (ret) {
		goto end;
	}

	ret = set_packet_header_uuid(stream);
	if (ret) {
		goto end;
	}

	ret = set_packet_header_stream_id(stream);
	if (ret) {
		goto end;
	}
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_stream_create(
	struct bt_ctf_stream_class *stream_class,
	struct bt_ctf_trace *trace)
{
	int ret;
	struct bt_ctf_stream *stream = NULL;

	if (!stream_class || !trace) {
		goto end;
	}

	stream = g_new0(struct bt_ctf_stream, 1);
	if (!stream) {
		goto end;
	}

	stream->trace = trace;
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
		(GDestroyNotify) bt_ctf_event_put);
	if (!stream->events) {
		goto error_destroy;
	}
	if (stream_class->event_context_type) {
		stream->event_contexts = g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_ctf_field_put);
		if (!stream->event_contexts) {
			goto error_destroy;
		}
	}

	/* A trace is not allowed to have a NULL packet header */
	assert(trace->packet_header_type);
	stream->packet_header = bt_ctf_field_create(trace->packet_header_type);
	/*
	 * Attempt to populate the default trace packet header fields
	 * (magic, uuid and stream_id). This will _not_ fail shall the
	 * fields not be found or be of an incompatible type; they will
	 * simply not be populated automatically. The user will have to
	 * make sure to set the trace packet header fields himself before
	 * flushing.
	 */
	ret = set_packet_header(stream);
	if (ret) {
		goto error_destroy;
	}
end:
	return stream;
error_destroy:
	bt_ctf_stream_destroy(&stream->ref_count);
	return NULL;
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

BT_HIDDEN
void bt_ctf_stream_set_trace(struct bt_ctf_stream *stream,
		struct bt_ctf_trace *trace)
{
	stream->trace = trace;
}

struct bt_ctf_stream_class *bt_ctf_stream_get_class(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!stream) {
		goto end;
	}

	stream_class = stream->stream_class;
	bt_ctf_stream_class_get(stream_class);
end:
	return stream_class;
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

	/* Make sure the event's payload is set */
	ret = bt_ctf_event_validate(event);
	if (ret) {
		goto end;
	}

	/* Sample the current stream event context by copying it */
	if (stream->event_context) {
		/* Make sure the event context's payload is set */
		ret = bt_ctf_field_validate(stream->event_context);
		if (ret) {
			goto end;
		}

		event_context_copy = bt_ctf_field_copy(stream->event_context);
		if (!event_context_copy) {
			ret = -1;
			goto end;
		}
	}

	timestamp = bt_ctf_clock_get_time(stream->stream_class->clock);
	ret = bt_ctf_event_set_timestamp(event, timestamp);
	if (ret) {
		goto end;
	}

	bt_ctf_event_get(event);
	/* Save the new event along with its associated stream event context */
	g_ptr_array_add(stream->events, event);
	if (event_context_copy) {
		g_ptr_array_add(stream->event_contexts, event_context_copy);
	}
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
	if (packet_context) {
		bt_ctf_field_get(packet_context);
	}
end:
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

struct bt_ctf_field *bt_ctf_stream_get_event_context(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_field *event_context = NULL;

	if (!stream) {
		goto end;
	}

	event_context = stream->event_context;
	if (event_context) {
		bt_ctf_field_get(event_context);
	}
end:
	return event_context;
}

int bt_ctf_stream_set_event_context(struct bt_ctf_stream *stream,
		struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!stream || !field) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	if (field_type != stream->stream_class->event_context_type) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_get(field);
	bt_ctf_field_put(stream->event_context);
	stream->event_context = field;
end:
	if (field_type) {
		bt_ctf_field_type_put(field_type);
	}
	return ret;
}

struct bt_ctf_field *bt_ctf_stream_get_packet_header(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_field *packet_header = NULL;

	if (!stream) {
		goto end;
	}

	packet_header = stream->packet_header;
	if (packet_header) {
		bt_ctf_field_get(packet_header);
	}
end:
	return packet_header;
}

int bt_ctf_stream_set_packet_header(struct bt_ctf_stream *stream,
		struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!stream || !field) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	if (field_type != stream->trace->packet_header_type) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_get(field);
	bt_ctf_field_put(stream->packet_header);
	stream->packet_header = field;
end:
	if (field_type) {
		bt_ctf_field_type_put(field_type);
	}
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

	ret = bt_ctf_field_validate(stream->packet_header);
	if (ret) {
		goto end;
	}

	if (stream->flushed_packet_count) {
		/* ctf_init_pos has already initialized the first packet */
		ctf_packet_seek(&stream->pos.parent, 0, SEEK_CUR);
	}

	ret = bt_ctf_field_serialize(stream->packet_header, &stream->pos);
	if (ret) {
		goto end;
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

		/* Write stream event context */
		if (stream->event_contexts) {
			ret = bt_ctf_field_serialize(
				g_ptr_array_index(stream->event_contexts, i),
				&stream->pos);
			if (ret) {
				goto end;
			}
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
	if (stream->event_contexts) {
		g_ptr_array_set_size(stream->event_contexts, 0);
	}
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
	if (stream->event_contexts) {
		g_ptr_array_free(stream->event_contexts, TRUE);
	}
	if (stream->packet_header) {
		bt_ctf_field_put(stream->packet_header);
	}
	if (stream->packet_context) {
		bt_ctf_field_put(stream->packet_context);
	}
	if (stream->event_context) {
		bt_ctf_field_put(stream->event_context);
	}
	g_free(stream);
}

static
int set_structure_field_integer(struct bt_ctf_field *structure, char *name,
		uint64_t value)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;
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

	field_type = bt_ctf_field_get_type(integer);
	/* Something is serioulsly wrong */
	assert(field_type);
	if (bt_ctf_field_type_get_type_id(field_type) != CTF_TYPE_INTEGER) {
		/*
		 * The user most likely meant for us to populate this field
		 * automatically. However, we can only do this if the field
		 * is an integer. Return an error.
		 */
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type)) {
		ret = bt_ctf_field_signed_integer_set_value(integer,
			(int64_t) value);
	} else {
		ret = bt_ctf_field_unsigned_integer_set_value(integer, value);
	}
end:
	if (integer) {
		bt_ctf_field_put(integer);
	}
	if (field_type) {
		bt_ctf_field_type_put(field_type);
	}
	return ret;
}
