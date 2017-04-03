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

#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/align.h>
#include <inttypes.h>

static
void bt_ctf_stream_destroy(struct bt_object *obj);
static
int try_set_structure_field_integer(struct bt_ctf_field *, char *, uint64_t);
static
int set_structure_field_integer(struct bt_ctf_field *, char *, uint64_t);

static
int set_integer_field_value(struct bt_ctf_field* field, uint64_t value)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!field) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	assert(field_type);

	if (bt_ctf_field_type_get_type_id(field_type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		/* Not an integer and the value is unset, error. */
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type)) {
		ret = bt_ctf_field_signed_integer_set_value(field, (int64_t) value);
		if (ret) {
			/* Value is out of range, error. */
			goto end;
		}
	} else {
		ret = bt_ctf_field_unsigned_integer_set_value(field, value);
		if (ret) {
			/* Value is out of range, error. */
			goto end;
		}
	}
end:
	bt_put(field_type);
	return ret;
}

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

	if (bt_ctf_field_is_set(magic_field)) {
		/* Value already set. Not an error, skip. */
		goto end;
	}

	magic_field_type = bt_ctf_field_get_type(magic_field);
	assert(magic_field_type);

	if (bt_ctf_field_type_get_type_id(magic_field_type) !=
		BT_CTF_FIELD_TYPE_ID_INTEGER) {
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
	bt_put(magic_field);
	bt_put(magic_field_type);
	return ret;
}

static
int set_packet_header_uuid(struct bt_ctf_stream *stream)
{
	int i, ret = 0;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_field_type *uuid_field_type = NULL;
	struct bt_ctf_field_type *element_field_type = NULL;
	struct bt_ctf_field *uuid_field = bt_ctf_field_structure_get_field(
		stream->packet_header, "uuid");

	if (!uuid_field) {
		/* No uuid field found. Not an error, skip. */
		goto end;
	}

	if (bt_ctf_field_is_set(uuid_field)) {
		/* Value already set. Not an error, skip. */
		goto end;
	}

	uuid_field_type = bt_ctf_field_get_type(uuid_field);
	assert(uuid_field_type);
	if (bt_ctf_field_type_get_type_id(uuid_field_type) !=
		BT_CTF_FIELD_TYPE_ID_ARRAY) {
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
		BT_CTF_FIELD_TYPE_ID_INTEGER) {
		/* UUID array elements are not integers. Not an error, skip */
		goto end;
	}

	trace = (struct bt_ctf_trace *) bt_object_get_parent(stream);
	for (i = 0; i < 16; i++) {
		struct bt_ctf_field *uuid_element =
			bt_ctf_field_array_get_field(uuid_field, i);

		ret = bt_ctf_field_type_integer_get_signed(element_field_type);
		assert(ret >= 0);

		if (ret) {
			ret = bt_ctf_field_signed_integer_set_value(
				uuid_element, (int64_t) trace->uuid[i]);
		} else {
			ret = bt_ctf_field_unsigned_integer_set_value(
				uuid_element,
				(uint64_t) trace->uuid[i]);
		}
		bt_put(uuid_element);
		if (ret) {
			goto end;
		}
	}

end:
	bt_put(uuid_field);
	bt_put(uuid_field_type);
	bt_put(element_field_type);
	BT_PUT(trace);
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

	if (bt_ctf_field_is_set(stream_id_field)) {
		/* Value already set. Not an error, skip. */
		goto end;
	}

	stream_id_field_type = bt_ctf_field_get_type(stream_id_field);
	assert(stream_id_field_type);
	if (bt_ctf_field_type_get_type_id(stream_id_field_type) !=
		BT_CTF_FIELD_TYPE_ID_INTEGER) {
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
	bt_put(stream_id_field);
	bt_put(stream_id_field_type);
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

static
void release_event(struct bt_ctf_event *event)
{
	if (bt_object_get_ref_count(event)) {
		/*
		 * The event is being orphaned, but it must guarantee the
		 * existence of its event class for the duration of its
		 * lifetime.
		 */
		bt_get(event->event_class);
		BT_PUT(event->base.parent);
	} else {
		bt_object_release(event);
	}
}

static
int create_stream_file(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream)
{
	int fd;
	GString *filename = g_string_new(stream->stream_class->name->str);

	if (stream->stream_class->name->len == 0) {
		int64_t ret;

		ret = bt_ctf_stream_class_get_id(stream->stream_class);
		if (ret < 0) {
			fd = -1;
			goto error;
		}

		g_string_printf(filename, "stream_%" PRId64, ret);
	}

	g_string_append_printf(filename, "_%" PRIu32, stream->id);
	fd = openat(writer->trace_dir_fd, filename->str,
		O_RDWR | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
error:
	g_string_free(filename, TRUE);
	return fd;
}

static
int set_stream_fd(struct bt_ctf_stream *stream, int fd)
{
	int ret = 0;

	if (stream->pos.fd != -1) {
		ret = -1;
		goto end;
	}

	(void) bt_ctf_stream_pos_init(&stream->pos, fd, O_RDWR);
	stream->pos.fd = fd;
end:
	return ret;
}

struct bt_ctf_stream *bt_ctf_stream_create(
		struct bt_ctf_stream_class *stream_class,
		const char *name)
{
	int ret;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_writer *writer = NULL;

	if (!stream_class) {
		goto error;
	}

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		goto error;
	}

	stream = g_new0(struct bt_ctf_stream, 1);
	if (!stream) {
		goto error;
	}

	bt_object_init(stream, bt_ctf_stream_destroy);
	/*
	 * Acquire reference to parent since stream will become publicly
	 * reachable; it needs its parent to remain valid.
	 */
	bt_object_set_parent(stream, trace);
	stream->id = stream_class->next_stream_id++;
	stream->stream_class = stream_class;
	stream->pos.fd = -1;

	if (name) {
		stream->name = g_string_new(name);
		if (!stream->name) {
			goto error;
		}
	}

	if (trace->is_created_by_writer) {
		int fd;
		writer = (struct bt_ctf_writer *)
			bt_object_get_parent(trace);

		assert(writer);
		if (stream_class->packet_context_type) {
			stream->packet_context = bt_ctf_field_create(
				stream_class->packet_context_type);
			if (!stream->packet_context) {
				goto error;
			}

			/* Initialize events_discarded */
			ret = try_set_structure_field_integer(
				stream->packet_context,
				"events_discarded", 0);
			if (ret != 1) {
				goto error;
			}
		}

		stream->events = g_ptr_array_new_with_free_func(
			(GDestroyNotify) release_event);
		if (!stream->events) {
			goto error;
		}

		/* A trace is not allowed to have a NULL packet header */
		assert(trace->packet_header_type);
		stream->packet_header =
			bt_ctf_field_create(trace->packet_header_type);
		if (!stream->packet_header) {
			goto error;
		}

		/*
		 * Attempt to populate the default trace packet header fields
		 * (magic, uuid and stream_id). This will _not_ fail shall the
		 * fields not be found or be of an incompatible type; they will
		 * simply not be populated automatically. The user will have to
		 * make sure to set the trace packet header fields himself
		 * before flushing.
		 */
		ret = set_packet_header(stream);
		if (ret) {
			goto error;
		}

		/* Create file associated with this stream */
		fd = create_stream_file(writer, stream);
		if (fd < 0) {
			goto error;
		}

		ret = set_stream_fd(stream, fd);
		if (ret) {
			goto error;
		}

		/* Freeze the writer */
		bt_ctf_writer_freeze(writer);
	} else {
		/* Non-writer stream indicated by a negative FD */
		ret = set_stream_fd(stream, -1);
		if (ret) {
			goto error;
		}
	}

	/* Add this stream to the trace's streams */
	g_ptr_array_add(trace->streams, stream);

	BT_PUT(trace);
	BT_PUT(writer);
	return stream;
error:
	BT_PUT(stream);
	BT_PUT(trace);
	BT_PUT(writer);
	return stream;
}

struct bt_ctf_stream_class *bt_ctf_stream_get_class(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!stream) {
		goto end;
	}

	stream_class = stream->stream_class;
	bt_get(stream_class);
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

	if (!stream || !count || !stream->packet_context ||
			stream->pos.fd < 0) {
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
	bt_put(events_discarded_field);
	bt_put(events_discarded_field_type);
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

	if (!stream || !stream->packet_context || stream->pos.fd < 0) {
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
	bt_put(events_discarded_field);
	bt_put(events_discarded_field_type);
}

static int auto_populate_event_header(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event)
{
	int ret = 0;
	struct bt_ctf_field *id_field = NULL, *timestamp_field = NULL;
	struct bt_ctf_clock_class *mapped_clock_class = NULL;

	if (!event || event->frozen) {
		ret = -1;
		goto end;
	}

	/*
	 * The condition to automatically set the ID are:
	 *
	 * 1. The event header field "id" exists and is an integer
	 *    field.
	 * 2. The event header field "id" is NOT set.
	 */
	id_field = bt_ctf_field_structure_get_field(event->event_header, "id");
	if (id_field && !bt_ctf_field_is_set(id_field)) {
		ret = set_integer_field_value(id_field,
			(uint64_t) bt_ctf_event_class_get_id(
					event->event_class));
		if (ret) {
			goto end;
		}
	}

	/*
	 * The conditions to automatically set the timestamp are:
	 *
	 * 1. The event header field "timestamp" exists and is an
	 *    integer field.
	 * 2. This stream's class has a registered clock (set with
	 *    bt_ctf_stream_class_set_clock()).
	 * 3. The event header field "timestamp" has its type mapped to
	 *    a clock class which is also the clock class of this
	 *    stream's class's registered clock.
	 * 4. The event header field "timestamp" is NOT set.
	 */
	timestamp_field = bt_ctf_field_structure_get_field(event->event_header,
			"timestamp");
	if (timestamp_field && !bt_ctf_field_is_set(timestamp_field) &&
			stream->stream_class->clock) {
		struct bt_ctf_clock_class *stream_class_clock_class =
			stream->stream_class->clock->clock_class;
		struct bt_ctf_field_type *timestamp_field_type =
			bt_ctf_field_get_type(timestamp_field);

		assert(timestamp_field_type);
		mapped_clock_class =
			bt_ctf_field_type_integer_get_mapped_clock_class(
				timestamp_field_type);
		BT_PUT(timestamp_field_type);
		if (mapped_clock_class == stream_class_clock_class) {
			uint64_t timestamp;

			ret = bt_ctf_clock_get_value(
				stream->stream_class->clock,
				&timestamp);
			if (ret) {
				goto end;
			}

			ret = set_integer_field_value(timestamp_field,
					timestamp);
			if (ret) {
				goto end;
			}
		}
	}

end:
	bt_put(id_field);
	bt_put(timestamp_field);
	bt_put(mapped_clock_class);
	return ret;
}

int bt_ctf_stream_append_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event)
{
	int ret = 0;

	if (!stream || !event || stream->pos.fd < 0) {
		ret = -1;
		goto end;
	}

	/*
	 * The event is not supposed to have a parent stream at this
	 * point. The only other way an event can have a parent stream
	 * is if it was assigned when setting a packet to the event,
	 * in which case the packet's stream is not a writer stream,
	 * and thus the user is trying to append an event which belongs
	 * to another stream.
	 */
	if (event->base.parent) {
		ret = -1;
		goto end;
	}

	bt_object_set_parent(event, stream);
	ret = auto_populate_event_header(stream, event);
	if (ret) {
		goto error;
	}

	/* Make sure the various scopes of the event are set */
	ret = bt_ctf_event_validate(event);
	if (ret) {
		goto error;
	}

	/* Save the new event and freeze it */
	bt_ctf_event_freeze(event);
	g_ptr_array_add(stream->events, event);

	/*
	 * Event had to hold a reference to its event class as long as it wasn't
	 * part of the same trace hierarchy. From now on, the event and its
	 * class share the same lifetime guarantees and the reference is no
	 * longer needed.
	 */
	bt_put(event->event_class);

end:
	return ret;

error:
	/*
	 * Orphan the event; we were not successful in associating it to
	 * a stream.
	 */
	bt_object_set_parent(event, NULL);

	return ret;
}

struct bt_ctf_field *bt_ctf_stream_get_packet_context(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_field *packet_context = NULL;

	if (!stream || stream->pos.fd < 0) {
		goto end;
	}

	packet_context = stream->packet_context;
	if (packet_context) {
		bt_get(packet_context);
	}
end:
	return packet_context;
}

int bt_ctf_stream_set_packet_context(struct bt_ctf_stream *stream,
		struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type;

	if (!stream || stream->pos.fd < 0) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	if (bt_ctf_field_type_compare(field_type,
			stream->stream_class->packet_context_type)) {
		ret = -1;
		goto end;
	}

	bt_put(field_type);
	bt_put(stream->packet_context);
	stream->packet_context = bt_get(field);
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_stream_get_packet_header(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_field *packet_header = NULL;

	if (!stream || stream->pos.fd < 0) {
		goto end;
	}

	packet_header = stream->packet_header;
	if (packet_header) {
		bt_get(packet_header);
	}
end:
	return packet_header;
}

int bt_ctf_stream_set_packet_header(struct bt_ctf_stream *stream,
		struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_field_type *field_type = NULL;

	if (!stream || stream->pos.fd < 0) {
		ret = -1;
		goto end;
	}

	trace = (struct bt_ctf_trace *) bt_object_get_parent(stream);
	field_type = bt_ctf_field_get_type(field);
	if (bt_ctf_field_type_compare(field_type, trace->packet_header_type)) {
		ret = -1;
		goto end;
	}

	bt_put(stream->packet_header);
	stream->packet_header = bt_get(field);
end:
	BT_PUT(trace);
	bt_put(field_type);
	return ret;
}

static
int get_event_header_timestamp(struct bt_ctf_field *event_header, uint64_t *timestamp)
{
	int ret = 0;
	struct bt_ctf_field *timestamp_field = NULL;
	struct bt_ctf_field_type *timestamp_field_type = NULL;

	timestamp_field = bt_ctf_field_structure_get_field(event_header,
		"timestamp");
	if (!timestamp_field) {
		ret = -1;
		goto end;
	}

	timestamp_field_type = bt_ctf_field_get_type(timestamp_field);
	assert(timestamp_field_type);
	if (bt_ctf_field_type_get_type_id(timestamp_field_type) !=
		BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_integer_get_signed(timestamp_field_type)) {
		int64_t val;

		ret = bt_ctf_field_signed_integer_get_value(timestamp_field,
			&val);
		if (ret) {
			goto end;
		}
		*timestamp = (uint64_t) val;
	} else {
		ret = bt_ctf_field_unsigned_integer_get_value(timestamp_field,
			timestamp);
		if (ret) {
			goto end;
		}
	}
end:
	bt_put(timestamp_field);
	bt_put(timestamp_field_type);
	return ret;
}

static
void reset_structure_field(struct bt_ctf_field *structure, const char *name)
{
	struct bt_ctf_field *member;

	member = bt_ctf_field_structure_get_field(structure, name);
	assert(member);
	(void) bt_ctf_field_reset(member);
	bt_put(member);
}

int bt_ctf_stream_flush(struct bt_ctf_stream *stream)
{
	int ret = 0;
	size_t i;
	uint64_t timestamp_begin, timestamp_end;
	struct bt_ctf_field *integer = NULL;
	struct bt_ctf_stream_pos packet_context_pos;
	struct bt_ctf_trace *trace;
	enum bt_ctf_byte_order native_byte_order;
	bool empty_packet;
	uint64_t packet_size_bits;
	struct {
		bool timestamp_begin;
		bool timestamp_end;
		bool content_size;
		bool packet_size;
	} auto_set_fields = { 0 };

	if (!stream || stream->pos.fd < 0) {
		/*
		 * Stream does not have an associated fd. It is,
		 * therefore, not a stream being used to write events.
		 */
		ret = -1;
		goto end;
	}

	if (!stream->packet_context && stream->flushed_packet_count > 0) {
		/*
		 * A stream without a packet context, and thus without
		 * content and packet size members, can't have more than
		 * one packet.
		 */
		ret = -1;
		goto end;
	}

	trace = bt_ctf_stream_class_borrow_trace(stream->stream_class);
	assert(trace);
	native_byte_order = bt_ctf_trace_get_byte_order(trace);
	empty_packet = (stream->events->len == 0);

	/* mmap the next packet */
	bt_ctf_stream_pos_packet_seek(&stream->pos, 0, SEEK_CUR);
	ret = bt_ctf_field_serialize(stream->packet_header, &stream->pos,
		native_byte_order);
	if (ret) {
		goto end;
	}

	if (stream->packet_context) {
		/* Set the default context attributes if present and unset. */
		if (!empty_packet && !get_event_header_timestamp(
			((struct bt_ctf_event *) g_ptr_array_index(
				stream->events, 0))->event_header, &timestamp_begin)) {
			ret = try_set_structure_field_integer(
				stream->packet_context,
				"timestamp_begin", timestamp_begin);
			if (ret < 0) {
				goto end;
			}
			auto_set_fields.timestamp_begin = ret == 1;
		}

		if (!empty_packet && !get_event_header_timestamp(
			((struct bt_ctf_event *) g_ptr_array_index(
				stream->events, stream->events->len - 1))->event_header,
				&timestamp_end)) {

			ret = try_set_structure_field_integer(
				stream->packet_context,
				"timestamp_end", timestamp_end);
			if (ret < 0) {
				goto end;
			}
			auto_set_fields.timestamp_end = ret == 1;
		}
		ret = try_set_structure_field_integer(stream->packet_context,
			"content_size", UINT64_MAX);
		if (ret < 0) {
			goto end;
		}
		auto_set_fields.content_size = ret == 1;

		ret = try_set_structure_field_integer(stream->packet_context,
			"packet_size", UINT64_MAX);
		if (ret < 0) {
			goto end;
		}
		auto_set_fields.packet_size = ret == 1;

		/* Write packet context */
		memcpy(&packet_context_pos, &stream->pos,
			sizeof(packet_context_pos));
		ret = bt_ctf_field_serialize(stream->packet_context,
			&stream->pos, native_byte_order);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < stream->events->len; i++) {
		struct bt_ctf_event *event = g_ptr_array_index(
			stream->events, i);

		/* Write event header */
		ret = bt_ctf_field_serialize(event->event_header,
			&stream->pos, native_byte_order);
		if (ret) {
			goto end;
		}

		/* Write stream event context */
		if (event->stream_event_context) {
			ret = bt_ctf_field_serialize(
				event->stream_event_context, &stream->pos,
				native_byte_order);
			if (ret) {
				goto end;
			}
		}

		/* Write event content */
		ret = bt_ctf_event_serialize(event, &stream->pos,
			native_byte_order);
		if (ret) {
			goto end;
		}
	}

	/* Rounded-up in case content_size is not byte-aligned. */
	packet_size_bits = (stream->pos.offset + (CHAR_BIT - 1)) &
		~(CHAR_BIT - 1);
	stream->pos.packet_size = packet_size_bits;

	if (stream->packet_context) {
		/*
		 * Update the packet total size and content size and overwrite
		 * the packet context.
		 * Copy base_mma as the packet may have been remapped (e.g. when
		 * a packet is resized).
		 */
		packet_context_pos.base_mma = stream->pos.base_mma;
		if (auto_set_fields.content_size) {
			ret = set_structure_field_integer(
					stream->packet_context,
					"content_size", stream->pos.offset);
			if (ret < 0) {
				goto end;
			}
		}

		if (auto_set_fields.packet_size) {
			ret = set_structure_field_integer(stream->packet_context,
					"packet_size", packet_size_bits);
			if (ret < 0) {
				goto end;
			}
		}

		ret = bt_ctf_field_serialize(stream->packet_context,
			&packet_context_pos, native_byte_order);
		if (ret) {
			goto end;
		}
	}

	g_ptr_array_set_size(stream->events, 0);
	stream->flushed_packet_count++;
	stream->size += packet_size_bits / CHAR_BIT;
end:
	/* Reset automatically-set fields. */
	if (auto_set_fields.timestamp_begin) {
		reset_structure_field(stream->packet_context,
				"timestamp_begin");
	}
	if (auto_set_fields.timestamp_end) {
		reset_structure_field(stream->packet_context,
				"timestamp_end");
	}
	if (auto_set_fields.packet_size) {
		reset_structure_field(stream->packet_context,
				"packet_size");
	}
	if (auto_set_fields.content_size) {
		reset_structure_field(stream->packet_context,
				"content_size");
	}
	bt_put(integer);

	if (ret < 0) {
		/*
		 * We failed to write the packet. Its size is therefore set to 0
		 * to ensure the next mapping is done in the same place rather
		 * than advancing by "stream->pos.packet_size", which would
		 * leave a corrupted packet in the trace.
		 */
		stream->pos.packet_size = 0;
	}
	return ret;
}

void bt_ctf_stream_get(struct bt_ctf_stream *stream)
{
	bt_get(stream);
}

void bt_ctf_stream_put(struct bt_ctf_stream *stream)
{
	bt_put(stream);
}

static
void bt_ctf_stream_destroy(struct bt_object *obj)
{
	struct bt_ctf_stream *stream;

	stream = container_of(obj, struct bt_ctf_stream, base);
	(void) bt_ctf_stream_pos_fini(&stream->pos);
	if (stream->pos.fd >= 0) {
		int ret;

		/*
		 * Truncate the file's size to the minimum required to fit the
		 * last packet as we might have grown it too much on the last
		 * mmap.
		 */
		do {
			ret = ftruncate(stream->pos.fd, stream->size);
		} while (ret == -1 && errno == EINTR);
		if (ret) {
			perror("ftruncate");
		}

		if (close(stream->pos.fd)) {
			perror("close");
		}
	}

	if (stream->events) {
		g_ptr_array_free(stream->events, TRUE);
	}

	if (stream->name) {
		g_string_free(stream->name, TRUE);
	}

	bt_put(stream->packet_header);
	bt_put(stream->packet_context);
	g_free(stream);
}

static
int _set_structure_field_integer(struct bt_ctf_field *structure, char *name,
		uint64_t value, bool force)
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
	if (!force && bt_ctf_field_is_set(integer)) {
		/* Payload already set, not an error */
		goto end;
	}

	field_type = bt_ctf_field_get_type(integer);
	assert(field_type);
	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
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
	ret = !ret ? 1 : ret;
end:
	bt_put(integer);
	bt_put(field_type);
	return ret;
}

static
int set_structure_field_integer(struct bt_ctf_field *structure, char *name,
		uint64_t value)
{
	return _set_structure_field_integer(structure, name, value, true);
}

/*
 * Returns the following codes:
 * 1 if the field was found and set,
 * 0 if nothing was done (field not found, or was already set),
 * <0 if an error was encoutered
 */
static
int try_set_structure_field_integer(struct bt_ctf_field *structure, char *name,
		uint64_t value)
{
	return _set_structure_field_integer(structure, name, value, false);
}

const char *bt_ctf_stream_get_name(struct bt_ctf_stream *stream)
{
	const char *name = NULL;

	if (!stream) {
		goto end;
	}

	name = stream->name ? stream->name->str : NULL;

end:
	return name;
}

int bt_ctf_stream_is_writer(struct bt_ctf_stream *stream)
{
	int ret = -1;

	if (!stream) {
		goto end;
	}

	ret = (stream->pos.fd >= 0);

end:
	return ret;
}
