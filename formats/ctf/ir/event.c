/*
 * event.c
 *
 * Babeltrace CTF IR - Event
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

#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-ir/event-fields-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/compiler.h>

static
void bt_ctf_event_class_destroy(struct bt_ctf_ref *ref);
static
void bt_ctf_event_destroy(struct bt_ctf_ref *ref);
static
int set_integer_field_value(struct bt_ctf_field *field, uint64_t value);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (bt_ctf_validate_identifier(name)) {
		goto end;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		goto end;
	}

	bt_ctf_ref_init(&event_class->ref_count);
	event_class->fields = bt_ctf_field_type_structure_create();
	if (!event_class->fields) {
		bt_ctf_event_class_put(event_class);
		event_class = NULL;
		goto end;
	}

	event_class->name = g_quark_from_string(name);
end:
	return event_class;
}

const char *bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class)
{
	const char *name = NULL;

	if (!event_class) {
		goto end;
	}

	name = g_quark_to_string(event_class->name);
end:
	return name;
}

int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class)
{
	int64_t ret;

	if (!event_class || !event_class->id_set) {
		ret = -1;
		goto end;
	}

	ret = (int64_t) event_class->id;
end:
	return ret;
}

int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint32_t id)
{
	int ret = 0;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	if (event_class->stream_class) {
		/*
		 * We don't allow changing the id if the event class has already
		 * been added to a stream class.
		 */
		ret = -1;
		goto end;
	}

	event_class->id = id;
	event_class->id_set = 1;
end:
	return ret;
}

struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!event_class) {
		goto end;
	}

	stream_class = event_class->stream_class;
	bt_ctf_stream_class_get(stream_class);
end:
	return stream_class;
}

struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_field_type *payload = NULL;

	if (!event_class) {
		goto end;
	}

	bt_ctf_field_type_get(event_class->fields);
	payload = event_class->fields;
end:
	return payload;
}

int bt_ctf_event_class_set_payload_type(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload)
{
	int ret = 0;

	if (!event_class || !payload) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_type_get(payload);
	bt_ctf_field_type_put(event_class->fields);
	event_class->fields = payload;
end:
	return ret;
}

int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name)
{
	int ret = 0;

	if (!event_class || !type || bt_ctf_validate_identifier(name) ||
		event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_class->fields,
		type, name);
end:
	return ret;
}

int bt_ctf_event_class_get_field_count(
		struct bt_ctf_event_class *event_class)
{
	int ret;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_get_field_count(event_class->fields);
end:
	return ret;
}

int bt_ctf_event_class_get_field(struct bt_ctf_event_class *event_class,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index)
{
	int ret;

	if (!event_class || index < 0) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_get_field(event_class->fields,
		field_name, field_type, index);
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_event_class_get_field_by_name(
		struct bt_ctf_event_class *event_class, const char *name)
{
	GQuark name_quark;
	struct bt_ctf_field_type *field_type = NULL;

	if (!event_class || !name) {
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		goto end;
	}

	/*
	 * No need to increment field_type's reference count since getting it
	 * from the structure already does.
	 */
	field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		event_class->fields, name);
end:
	return field_type;
}

struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_field_type *context_type = NULL;

	if (!event_class || !event_class->context) {
		goto end;
	}

	bt_ctf_field_type_get(event_class->context);
	context_type = event_class->context;
end:
	return context_type;
}

int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context)
{
	int ret = 0;

	if (!event_class || !context || event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(context) != CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_type_get(context);
	bt_ctf_field_type_put(event_class->context);
	event_class->context = context;
end:
	return ret;

}

void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class)
{
	if (!event_class) {
		return;
	}

	bt_ctf_ref_get(&event_class->ref_count);
}

void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class)
{
	if (!event_class) {
		return;
	}

	bt_ctf_ref_put(&event_class->ref_count, bt_ctf_event_class_destroy);
}

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_event *event = NULL;

	if (!event_class) {
		goto end;
	}

	event = g_new0(struct bt_ctf_event, 1);
	if (!event) {
		goto end;
	}

	bt_ctf_ref_init(&event->ref_count);
	bt_ctf_event_class_get(event_class);
	bt_ctf_event_class_freeze(event_class);
	event->event_class = event_class;

	/*
	 * The event class does not keep ownership of the stream class to
	 * which it as been added. Therefore, it can't assume it has been
	 * set. However, we disallow the creation of an event if its
	 * associated stream class has been reclaimed.
	 */
	if (!event_class->stream_class) {
		goto error_destroy;
	}
	assert(event_class->stream_class->event_header_type);

	event->event_header = bt_ctf_field_create(
		event_class->stream_class->event_header_type);
	if (!event->event_header) {
		goto error_destroy;
	}
	if (event_class->context) {
		event->context_payload = bt_ctf_field_create(
			event_class->context);
		if (!event->context_payload) {
			goto error_destroy;
		}
	}
	event->fields_payload = bt_ctf_field_create(event_class->fields);
	if (!event->fields_payload) {
		goto error_destroy;
	}

	/*
	 * Freeze the stream class since the event header must not be changed
	 * anymore.
	 */
	bt_ctf_stream_class_freeze(event_class->stream_class);
end:
	return event;
error_destroy:
	bt_ctf_event_destroy(&event->ref_count);
	return NULL;
}

struct bt_ctf_event_class *bt_ctf_event_get_class(struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (!event) {
		goto end;
	}

	event_class = event->event_class;
	bt_ctf_event_class_get(event_class);
end:
	return event_class;
}

struct bt_ctf_clock *bt_ctf_event_get_clock(struct bt_ctf_event *event)
{
	struct bt_ctf_clock *clock = NULL;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_stream_class *stream_class;

	if (!event) {
		goto end;
	}

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		goto end;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		goto error_put_event_class;
	}

	clock = bt_ctf_stream_class_get_clock(stream_class);
	if (!clock) {
		goto error_put_stream_class;
	}

error_put_stream_class:
	bt_ctf_stream_class_put(stream_class);
error_put_event_class:
	bt_ctf_event_class_put(event_class);
end:
	return clock;
}

int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *payload)
{
	int ret = 0;

	if (!event || !payload) {
		ret = -1;
		goto end;
	}

	if (name) {
		ret = bt_ctf_field_structure_set_field(event->fields_payload,
			name, payload);
	} else {
		struct bt_ctf_field_type *payload_type;

		payload_type = bt_ctf_field_get_type(payload);
		if (payload_type == event->event_class->fields) {
			bt_ctf_field_put(event->fields_payload);
			bt_ctf_field_get(payload);
			event->fields_payload = payload;
		} else {
			ret = -1;
		}

		bt_ctf_field_type_put(payload_type);
	}
end:
	return ret;
}


struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name)
{
	struct bt_ctf_field *field = NULL;

	if (!event) {
		goto end;
	}

	if (name) {
		field = bt_ctf_field_structure_get_field(event->fields_payload,
			name);
	} else {
		field = event->fields_payload;
		bt_ctf_field_get(field);
	}
end:
	return field;
}

struct bt_ctf_field *bt_ctf_event_get_payload_by_index(
		struct bt_ctf_event *event, int index)
{
	struct bt_ctf_field *field = NULL;

	if (!event || index < 0) {
		goto end;
	}

	field = bt_ctf_field_structure_get_field_by_index(event->fields_payload,
		index);
end:
	return field;
}

struct bt_ctf_field *bt_ctf_event_get_event_header(
		struct bt_ctf_event *event)
{
	struct bt_ctf_field *header = NULL;

	if (!event || !event->event_header) {
		goto end;
	}

	header = event->event_header;
	bt_ctf_field_get(header);
end:
	return header;
}

int bt_ctf_event_set_event_header(struct bt_ctf_event *event,
		struct bt_ctf_field *header)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!event || !header) {
		ret = -1;
		goto end;
	}

	/* Could be NULL since an event class doesn't own a stream class */
	if (!event->event_class->stream_class) {
		ret = -1;
		goto end;
	}

	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	field_type = bt_ctf_field_get_type(header);
	if (field_type != event->event_class->stream_class->event_header_type) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_get(header);
	bt_ctf_field_put(event->event_header);
	event->event_header = header;
end:
	if (field_type) {
		bt_ctf_field_type_put(field_type);
	}
	return ret;
}

struct bt_ctf_field *bt_ctf_event_get_event_context(
		struct bt_ctf_event *event)
{
	struct bt_ctf_field *context = NULL;

	if (!event || !event->context_payload) {
		goto end;
	}

	context = event->context_payload;
	bt_ctf_field_get(context);
end:
	return context;
}

int bt_ctf_event_set_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!event || !context) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(context);
	if (field_type != event->event_class->context) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_get(context);
	bt_ctf_field_put(event->context_payload);
	event->context_payload = context;
end:
	if (field_type) {
		bt_ctf_field_type_put(field_type);
	}
	return ret;
}

void bt_ctf_event_get(struct bt_ctf_event *event)
{
	if (!event) {
		return;
	}

	bt_ctf_ref_get(&event->ref_count);
}

void bt_ctf_event_put(struct bt_ctf_event *event)
{
	if (!event) {
		return;
	}

	bt_ctf_ref_put(&event->ref_count, bt_ctf_event_destroy);
}

static
void bt_ctf_event_class_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_event_class *event_class;

	if (!ref) {
		return;
	}

	/*
	 * Don't call put() on the stream class. See comment in
	 * bt_ctf_event_class_set_stream_class for explanation.
	 */
	event_class = container_of(ref, struct bt_ctf_event_class, ref_count);
	if (event_class->context) {
		bt_ctf_field_type_put(event_class->context);
	}
	if (event_class->fields) {
		bt_ctf_field_type_put(event_class->fields);
	}
	g_free(event_class);
}

static
void bt_ctf_event_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_event *event;

	if (!ref) {
		return;
	}

	event = container_of(ref, struct bt_ctf_event,
		ref_count);
	if (event->event_class) {
		bt_ctf_event_class_put(event->event_class);
	}
	if (event->event_header) {
		bt_ctf_field_put(event->event_header);
	}
	if (event->context_payload) {
		bt_ctf_field_put(event->context_payload);
	}
	if (event->fields_payload) {
		bt_ctf_field_put(event->fields_payload);
	}
	g_free(event);
}

static
int set_integer_field_value(struct bt_ctf_field* field, uint64_t value)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!field) {
		ret = -1;
		goto end;
	}

	if (!bt_ctf_field_validate(field)) {
		/* Payload already set, skip! (not an error) */
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	assert(field_type);

	if (bt_ctf_field_type_get_type_id(field_type) != CTF_TYPE_INTEGER) {
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
	bt_ctf_field_type_put(field_type);
	return ret;
}

BT_HIDDEN
void bt_ctf_event_class_freeze(struct bt_ctf_event_class *event_class)
{
	assert(event_class);
	event_class->frozen = 1;
	bt_ctf_field_type_freeze(event_class->context);
	bt_ctf_field_type_freeze(event_class->fields);
}

BT_HIDDEN
int bt_ctf_event_class_set_stream_class(struct bt_ctf_event_class *event_class,
		struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	/* Allow a NULL stream_class to unset the current stream_class */
	if (stream_class && event_class->stream_class) {
		ret = -1;
		goto end;
	}

	event_class->stream_class = stream_class;
	/*
	 * We don't get() the stream_class since doing so would introduce
	 * a circular ownership between event classes and stream classes.
	 *
	 * A stream class will always unset itself from its events before
	 * being destroyed. This ensures that a user won't get a pointer
	 * to a stale stream class instance from an event class.
	 */
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context)
{
	int ret = 0;
	int64_t stream_id;

	assert(event_class);
	assert(context);
	stream_id = bt_ctf_stream_class_get_id(event_class->stream_class);
	if (stream_id < 0) {
		ret = -1;
		goto end;
	}

	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append_printf(context->string,
		"event {\n\tname = \"%s\";\n\tid = %u;\n\tstream_id = %" PRId64 ";\n",
		g_quark_to_string(event_class->name),
		event_class->id,
		stream_id);

	if (event_class->context) {
		g_string_append(context->string, "\tcontext := ");
		ret = bt_ctf_field_type_serialize(event_class->context,
			context);
		if (ret) {
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	if (event_class->fields) {
		g_string_append(context->string, "\tfields := ");
		ret = bt_ctf_field_type_serialize(event_class->fields, context);
		if (ret) {
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	g_string_append(context->string, "};\n\n");
end:
	context->current_indentation_level = 0;
	return ret;
}

void bt_ctf_event_class_set_native_byte_order(
		struct bt_ctf_event_class *event_class,
		int byte_order)
{
	if (!event_class) {
		return;
	}

	bt_ctf_field_type_set_native_byte_order(event_class->context,
		byte_order);
	bt_ctf_field_type_set_native_byte_order(event_class->fields,
		byte_order);
}

BT_HIDDEN
int bt_ctf_event_validate(struct bt_ctf_event *event)
{
	/* Make sure each field's payload has been set */
	int ret;

	assert(event);
	ret = bt_ctf_field_validate(event->event_header);
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_validate(event->fields_payload);
	if (ret) {
		goto end;
	}

	if (event->event_class->context) {
		ret = bt_ctf_field_validate(event->context_payload);
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_serialize(struct bt_ctf_event *event,
		struct ctf_stream_pos *pos)
{
	int ret = 0;

	assert(event);
	assert(pos);
	if (event->context_payload) {
		ret = bt_ctf_field_serialize(event->context_payload, pos);
		if (ret) {
			goto end;
		}
	}

	if (event->fields_payload) {
		ret = bt_ctf_field_serialize(event->fields_payload, pos);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_populate_event_header(struct bt_ctf_event *event)
{
	int ret = 0;
	struct bt_ctf_field *id_field = NULL, *timestamp_field = NULL;

	if (!event) {
		ret = -1;
		goto end;
	}

	id_field = bt_ctf_field_structure_get_field(event->event_header, "id");
	if (id_field) {
		ret = set_integer_field_value(id_field,
			(uint64_t) event->event_class->id);
		if (ret) {
			goto end;
		}
	}

	timestamp_field = bt_ctf_field_structure_get_field(event->event_header,
		"timestamp");
	if (timestamp_field) {
		struct bt_ctf_field_type *timestamp_field_type =
			bt_ctf_field_get_type(timestamp_field);
		struct bt_ctf_clock *mapped_clock;

		assert(timestamp_field_type);
		mapped_clock = bt_ctf_field_type_integer_get_mapped_clock(
			timestamp_field_type);
		bt_ctf_field_type_put(timestamp_field_type);
		if (mapped_clock) {
			uint64_t timestamp = bt_ctf_clock_get_time(
				mapped_clock);

			bt_ctf_clock_put(mapped_clock);
			if (timestamp == (uint64_t) -1ULL) {
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
	if (id_field) {
		bt_ctf_field_put(id_field);
	}
	if (timestamp_field) {
		bt_ctf_field_put(timestamp_field);
	}
	return ret;
}
