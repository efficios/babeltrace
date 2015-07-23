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
#include <babeltrace/ctf-ir/common-internal.h>
#include <babeltrace/ctf-ir/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler.h>

static
void bt_ctf_event_class_destroy(struct bt_ref *ref);
static
void bt_ctf_event_destroy(struct bt_ref *ref);
static
int set_integer_field_value(struct bt_ctf_field *field, uint64_t value);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	int ret;
	struct bt_value *obj = NULL;
	struct bt_ctf_event_class *event_class = NULL;

	if (bt_ctf_validate_identifier(name)) {
		goto error;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		goto error;
	}

	bt_ctf_base_init(event_class, bt_ctf_event_class_destroy);
	event_class->fields = bt_ctf_field_type_structure_create();
	if (!event_class->fields) {
		goto error;
	}

	event_class->attributes = bt_ctf_attributes_create();
	if (!event_class->attributes) {
		goto error;
	}

	obj = bt_value_integer_create_init(-1);
	if (!obj) {
		goto error;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"id", obj);
	if (ret) {
		goto error;
	}

	BT_VALUE_PUT(obj);

	obj = bt_value_string_create_init(name);
	if (!obj) {
		goto error;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"name", obj);
	if (ret) {
		goto error;
	}

	BT_VALUE_PUT(obj);

	return event_class;

error:
	if (event_class) {
		bt_ctf_event_class_put(event_class);
	}

	BT_VALUE_PUT(obj);

	return NULL;
}

const char *bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class)
{
	struct bt_value *obj = NULL;
	const char *name = NULL;

	if (!event_class) {
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_NAME_INDEX);
	if (!obj) {
		goto end;
	}

	if (bt_value_string_get(obj, &name)) {
		name = NULL;
	}

end:
	BT_VALUE_PUT(obj);

	return name;
}

int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class)
{
	struct bt_value *obj = NULL;
	int64_t ret = 0;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_ID_INDEX);
	if (!obj) {
		goto end;
	}

	if (bt_value_integer_get(obj, &ret)) {
		ret = -1;
	}

	if (ret < 0) {
		/* means ID is not set */
		ret = -1;
		goto end;
	}

end:
	BT_VALUE_PUT(obj);

	return ret;
}

int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint32_t id)
{
	int ret = 0;
	struct bt_value *obj = NULL;

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

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_ID_INDEX);
	if (!obj) {
		goto end;
	}

	if (bt_value_integer_set(obj, id)) {
		ret = -1;
		goto end;
	}

end:
	BT_VALUE_PUT(obj);

	return ret;
}

int bt_ctf_event_class_set_attribute(
		struct bt_ctf_event_class *event_class, const char *name,
		struct bt_value *value)
{
	int ret = 0;

	if (!event_class || !name || !value || event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (!strcmp(name, "id") || !strcmp(name, "loglevel")) {
		if (!bt_value_is_integer(value)) {
			ret = -1;
			goto end;
		}
	} else if (!strcmp(name, "name") || !strcmp(name, "model.emf.uri")) {
		if (!bt_value_is_string(value)) {
			ret = -1;
			goto end;
		}
	} else {
		/* unknown attribute */
		ret = -1;
		goto end;
	}

	/* "id" special case: >= 0 */
	if (!strcmp(name, "id")) {
		int64_t val;

		ret = bt_value_integer_get(value, &val);

		if (ret) {
			goto end;
		}

		if (val < 0) {
			ret = -1;
			goto end;
		}
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		name, value);

end:
	return ret;
}

int bt_ctf_event_class_get_attribute_count(
		struct bt_ctf_event_class *event_class)
{
	int ret = 0;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_attributes_get_count(event_class->attributes);

end:
	return ret;
}

const char *
bt_ctf_event_class_get_attribute_name(
		struct bt_ctf_event_class *event_class, int index)
{
	const char *ret;

	if (!event_class) {
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_name(event_class->attributes, index);

end:
	return ret;
}

struct bt_value *
bt_ctf_event_class_get_attribute_value(struct bt_ctf_event_class *event_class,
		int index)
{
	struct bt_value *ret;

	if (!event_class) {
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value(event_class->attributes, index);

end:
	return ret;
}

struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name)
{
	struct bt_value *ret;

	if (!event_class || !name) {
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value_by_name(event_class->attributes,
		name);

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

	if (!event_class || !payload ||
		bt_ctf_field_type_get_type_id(payload) != CTF_TYPE_STRUCT) {
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
	bt_ctf_get(event_class);
}

void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class)
{
	bt_ctf_put(event_class);
}

BT_HIDDEN
int bt_ctf_event_class_set_stream_id(struct bt_ctf_event_class *event_class,
		uint32_t stream_id)
{
	int ret = 0;
	struct bt_value *obj;

	obj = bt_value_integer_create_init(stream_id);

	if (!obj) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"stream_id", obj);

end:
	BT_VALUE_PUT(obj);

	return ret;
}

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_event *event = NULL;

	if (!event_class) {
		goto end;
	}

	/*
	 * The event class does not keep ownership of the stream class to
	 * which it as been added. Therefore, it can't assume it has been
	 * set. However, we disallow the creation of an event if its
	 * associated stream class has been reclaimed.
	 */
	if (!event_class->stream_class) {
		goto end;
	}
	assert(event_class->stream_class->event_header_type);
	event = g_new0(struct bt_ctf_event, 1);
	if (!event) {
		goto end;
	}

	bt_ctf_base_init(event, bt_ctf_event_destroy);
	bt_ctf_event_class_get(event_class);
	bt_ctf_event_class_freeze(event_class);
	event->event_class = event_class;

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
	if (event) {
		bt_ctf_event_destroy(&event->base.ref_count);
	}

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

struct bt_ctf_stream *bt_ctf_event_get_stream(struct bt_ctf_event *event)
{
	struct bt_ctf_stream *stream = NULL;

	if (!event) {
		goto end;
	}

	stream = event->stream;
	if (stream) {
		bt_ctf_stream_get(stream);
	}
end:
	return stream;
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

struct bt_ctf_field *bt_ctf_event_get_payload_field(struct bt_ctf_event *event)
{
	struct bt_ctf_field *payload = NULL;

	if (!event || !event->fields_payload) {
		goto end;
	}

	payload = event->fields_payload;
	bt_ctf_field_get(payload);
end:
	return payload;
}

int bt_ctf_event_set_payload_field(struct bt_ctf_event *event,
		struct bt_ctf_field *payload)
{
	int ret = 0;
	struct bt_ctf_field_type *payload_type = NULL;

	if (!event || !payload) {
		ret = -1;
		goto end;
	}

	payload_type = bt_ctf_field_get_type(payload);
	if (!payload_type) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(payload_type) != CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_get(payload);
	if (event->fields_payload) {
		bt_ctf_field_put(event->fields_payload);
	}
	event->fields_payload = payload;

end:
	if (payload_type) {
		bt_ctf_field_type_put(payload_type);
	}
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

struct bt_ctf_field *bt_ctf_event_get_header(
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

int bt_ctf_event_set_header(struct bt_ctf_event *event,
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
	bt_ctf_get(event);
}

void bt_ctf_event_put(struct bt_ctf_event *event)
{
	bt_ctf_put(event);
}

static
void bt_ctf_event_class_destroy(struct bt_ref *ref)
{
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_base *base;

	if (!ref) {
		return;
	}

	/*
	 * Don't call put() on the stream class. See comment in
	 * bt_ctf_event_class_set_stream_class for explanation.
	 */
	base = container_of(ref, struct bt_ctf_base, ref_count);
	event_class = container_of(base, struct bt_ctf_event_class, base);
	if (event_class->attributes) {
		bt_ctf_attributes_destroy(event_class->attributes);
	}
	if (event_class->context) {
		bt_ctf_field_type_put(event_class->context);
	}
	if (event_class->fields) {
		bt_ctf_field_type_put(event_class->fields);
	}
	g_free(event_class);
}

static
void bt_ctf_event_destroy(struct bt_ref *ref)
{
	struct bt_ctf_event *event;
	struct bt_ctf_base *base;

	if (!ref) {
		return;
	}

	base = container_of(ref, struct bt_ctf_base, ref_count);
	event = container_of(base, struct bt_ctf_event, base);
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
	bt_ctf_attributes_freeze(event_class->attributes);
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
	int i;
	int count;
	int ret = 0;
	struct bt_value *attr_value = NULL;

	assert(event_class);
	assert(context);

	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append(context->string, "event {\n");
	count = bt_ctf_event_class_get_attribute_count(event_class);

	if (count < 0) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < count; ++i) {
		const char *attr_name = NULL;

		attr_name = bt_ctf_event_class_get_attribute_name(
			event_class, i);
		attr_value = bt_ctf_event_class_get_attribute_value(
			event_class, i);

		if (!attr_name || !attr_value) {
			ret = -1;
			goto end;
		}

		switch (bt_value_get_type(attr_value)) {
		case BT_VALUE_TYPE_INTEGER:
		{
			int64_t value;

			ret = bt_value_integer_get(attr_value, &value);

			if (ret) {
				goto end;
			}

			g_string_append_printf(context->string,
				"\t%s = %" PRId64 ";\n", attr_name, value);
			break;
		}

		case BT_VALUE_TYPE_STRING:
		{
			const char *value;

			ret = bt_value_string_get(attr_value, &value);

			if (ret) {
				goto end;
			}

			g_string_append_printf(context->string,
				"\t%s = \"%s\";\n", attr_name, value);
			break;
		}

		default:
			/* should never happen */
			assert(false);
			break;
		}

		BT_VALUE_PUT(attr_value);
	}

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
	BT_VALUE_PUT(attr_value);
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
			(uint64_t) bt_ctf_event_class_get_id(
				event->event_class));
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

BT_HIDDEN
int bt_ctf_event_set_stream(struct bt_ctf_event *event,
		struct bt_ctf_stream *stream)
{
	int ret = 0;

	if (!event) {
		ret = -1;
		goto end;
	}

	if (event->stream && stream) {
		/* Already attached to a stream */
		ret = -1;
		goto end;
	}

	event->stream = stream;
end:
	return ret;
}

struct bt_ctf_event *bt_ctf_event_copy(struct bt_ctf_event *event)
{
	struct bt_ctf_event *copy = NULL;

	if (!event) {
		goto error;
	}

	copy = g_new0(struct bt_ctf_event, 1);
	if (!copy) {
		goto error;
	}

	bt_ctf_base_init(copy, bt_ctf_event_destroy);
	copy->event_class = event->event_class;
	bt_ctf_event_class_get(copy->event_class);
	copy->stream = event->stream;

	if (event->event_header) {
		copy->event_header = bt_ctf_field_copy(event->event_header);

		if (!copy->event_header) {
			goto error;
		}
	}

	if (event->context_payload) {
		copy->context_payload = bt_ctf_field_copy(
			event->context_payload);

		if (!copy->context_payload) {
			goto error;
		}
	}

	if (event->fields_payload) {
		copy->fields_payload = bt_ctf_field_copy(event->fields_payload);

		if (!copy->fields_payload) {
			goto error;
		}
	}

	return copy;

error:
	if (copy) {
		if (copy->event_class) {
			bt_ctf_event_class_put(copy->event_class);
		}

		if (copy->event_header) {
			bt_ctf_field_put(copy->event_header);
		}

		if (copy->context_payload) {
			bt_ctf_field_put(copy->context_payload);
		}

		if (copy->fields_payload) {
			bt_ctf_field_put(copy->fields_payload);
		}
	}

	g_free(copy);
	return NULL;
}
