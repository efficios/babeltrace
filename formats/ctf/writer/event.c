/*
 * event.c
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

#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/compiler.h>

static
void bt_ctf_event_class_destroy(struct bt_ctf_ref *ref);
static
void bt_ctf_event_destroy(struct bt_ctf_ref *ref);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (validate_identifier(name)) {
		goto end;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		goto end;
	}

	bt_ctf_ref_init(&event_class->ref_count);
	event_class->name = g_quark_from_string(name);
end:
	return event_class;
}

int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name)
{
	int ret = 0;

	if (!event_class || !type || validate_identifier(name) ||
		event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (!event_class->fields) {
		event_class->fields = bt_ctf_field_type_structure_create();
		if (!event_class->fields) {
			ret = -1;
			goto end;
		}
	}

	ret = bt_ctf_field_type_structure_add_field(event_class->fields,
		type, name);
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
	event->context_payload = bt_ctf_field_create(event_class->context);
	event->fields_payload = bt_ctf_field_create(event_class->fields);
end:
	return event;
}

int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *value)
{
	int ret = 0;

	if (!event || !value || validate_identifier(name)) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_structure_set_field(event->fields_payload,
		name, value);
end:
	return ret;
}


struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name)
{
	struct bt_ctf_field *field = NULL;

	if (!event || !name) {
		goto end;
	}

	field = bt_ctf_field_structure_get_field(event->fields_payload, name);
end:
	return field;
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

	event_class = container_of(ref, struct bt_ctf_event_class, ref_count);
	bt_ctf_field_type_put(event_class->context);
	bt_ctf_field_type_put(event_class->fields);
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
	bt_ctf_event_class_put(event->event_class);
	bt_ctf_field_put(event->context_payload);
	bt_ctf_field_put(event->fields_payload);
	g_free(event);
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
int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint32_t id)
{
	int ret = 0;

	if (event_class->id_set && id != event_class->id) {
		ret = -1;
		goto end;
	}

	event_class->id = id;
	event_class->id_set = 1;
end:
	return ret;
}

BT_HIDDEN
uint32_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class)
{
	assert(event_class);
	return event_class->id;
}

BT_HIDDEN
int bt_ctf_event_class_set_stream_id(struct bt_ctf_event_class *event_class,
		uint32_t id)
{
	int ret = 0;

	assert(event_class);
	if (event_class->stream_id_set && id != event_class->stream_id) {
		ret = -1;
		goto end;
	}

	event_class->stream_id = id;
	event_class->stream_id_set = 1;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context)
{
	int ret = 0;

	assert(event_class);
	assert(context);
	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append_printf(context->string, "event {\n\tname = \"%s\";\n\tid = %u;\n\tstream_id = %" PRIu32 ";\n",
		g_quark_to_string(event_class->name),
		event_class->id,
		event_class->stream_id);

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

BT_HIDDEN
int bt_ctf_event_validate(struct bt_ctf_event *event)
{
	/* Make sure each field's payload has been set */
	int ret;

	assert(event);
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
int bt_ctf_event_set_timestamp(struct bt_ctf_event *event,
		uint64_t timestamp)
{
	int ret = 0;

	assert(event);
	if (event->timestamp) {
		ret = -1;
		goto end;
	}

	event->timestamp = timestamp;
end:
	return ret;
}

BT_HIDDEN
uint64_t bt_ctf_event_get_timestamp(struct bt_ctf_event *event)
{
	assert(event);
	return event->timestamp;
}
