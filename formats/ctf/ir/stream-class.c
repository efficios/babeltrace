/*
 * stream-class.c
 *
 * Babeltrace CTF IR - Stream Class
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

#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-fields-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/align.h>

static
void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref);
static
int init_event_header(struct bt_ctf_stream_class *stream_class,
		enum bt_ctf_byte_order byte_order);
static
int init_packet_context(struct bt_ctf_stream_class *stream_class,
		enum bt_ctf_byte_order byte_order);

struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name)
{
	int ret;
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

	ret = init_packet_context(stream_class, BT_CTF_BYTE_ORDER_NATIVE);
	if (ret) {
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

const char *bt_ctf_stream_class_get_name(
		struct bt_ctf_stream_class *stream_class)
{
	const char *name = NULL;

	if (!stream_class) {
		goto end;
	}

	name = stream_class->name->str;
end:
	return name;
}

struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_clock *clock = NULL;

	if (!stream_class || !stream_class->clock) {
		goto end;
	}

	clock = stream_class->clock;
	bt_ctf_clock_get(clock);
end:
	return clock;
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

int64_t bt_ctf_stream_class_get_id(struct bt_ctf_stream_class *stream_class)
{
	int64_t ret;

	if (!stream_class || !stream_class->id_set) {
		ret = -1;
		goto end;
	}

	ret = (int64_t) stream_class->id;
end:
	return ret;
}

int bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class,
		uint32_t id)
{
	int ret = 0;

	if (!stream_class) {
		ret = -1;
		goto end;
	}

	stream_class->id = id;
	stream_class->id_set = 1;
end:
	return ret;
}

int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class)
{
	int ret = 0;
	int64_t event_id;

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

	/* Only set an event id if none was explicitly set before */
	event_id = bt_ctf_event_class_get_id(event_class);
	if (event_id < 0) {
		if (bt_ctf_event_class_set_id(event_class,
			stream_class->next_event_id++)) {
			ret = -1;
			goto end;
		}
	}

	ret = bt_ctf_event_class_set_stream_class(event_class, stream_class);
	if (ret) {
		goto end;
	}

	bt_ctf_event_class_get(event_class);
	g_ptr_array_add(stream_class->event_classes, event_class);
end:
	return ret;
}

int bt_ctf_stream_class_get_event_class_count(
		struct bt_ctf_stream_class *stream_class)
{
	int ret;

	if (!stream_class) {
		ret = -1;
		goto end;
	}

	ret = (int) stream_class->event_classes->len;
end:
	return ret;
}

struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class(
		struct bt_ctf_stream_class *stream_class, int index)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class || index < 0 ||
		index >= stream_class->event_classes->len) {
		goto end;
	}

	event_class = g_ptr_array_index(stream_class->event_classes, index);
	bt_ctf_event_class_get(event_class);
end:
	return event_class;
}

struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_name(
		struct bt_ctf_stream_class *stream_class, const char *name)
{
	size_t i;
	GQuark name_quark;
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class || !name) {
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		goto end;
	}

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *current_event_class =
			g_ptr_array_index(stream_class->event_classes, i);

		if (name_quark == current_event_class->name) {
			event_class = current_event_class;
			bt_ctf_event_class_get(event_class);
			goto end;
		}
	}
end:
	return event_class;
}

struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!stream_class) {
		goto end;
	}

	assert(stream_class->packet_context_type);
	bt_ctf_field_type_get(stream_class->packet_context_type);
	ret = stream_class->packet_context_type;
end:
	return ret;
}

int bt_ctf_stream_class_set_packet_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type)
{
	int ret = 0;

	if (!stream_class || !packet_context_type) {
		ret = -1;
		goto end;
	}

	assert(stream_class->packet_context_type);
	if (bt_ctf_field_type_get_type_id(packet_context_type) !=
		CTF_TYPE_STRUCT) {
		/* A packet context must be a structure */
		ret = -1;
		goto end;
	}

	bt_ctf_field_type_put(stream_class->packet_context_type);
	bt_ctf_field_type_get(packet_context_type);
	stream_class->packet_context_type = packet_context_type;
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
	bt_ctf_field_type_freeze(stream_class->packet_context_type);
	bt_ctf_clock_freeze(stream_class->clock);
	g_ptr_array_foreach(stream_class->event_classes,
		(GFunc)bt_ctf_event_class_freeze, NULL);
}

BT_HIDDEN
int bt_ctf_stream_class_set_byte_order(struct bt_ctf_stream_class *stream_class,
	enum bt_ctf_byte_order byte_order)
{
	int ret = 0;

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
	int64_t ret = 0;
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
	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *event_class =
			stream_class->event_classes->pdata[i];

		ret = bt_ctf_event_class_serialize(event_class, context);
		if (ret) {
			goto end;
		}
	}
end:
	context->current_indentation_level = 0;
	return ret;
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
		size_t i;

		/* Unregister this stream class from the event classes */
		for (i = 0; i < stream_class->event_classes->len; i++) {
			struct bt_ctf_event_class *event_class =
				g_ptr_array_index(stream_class->event_classes,
				i);

			bt_ctf_event_class_set_stream_class(event_class, NULL);
		}

		g_ptr_array_free(stream_class->event_classes, TRUE);
	}

	if (stream_class->name) {
		g_string_free(stream_class->name, TRUE);
	}

	bt_ctf_field_type_put(stream_class->event_header_type);
	bt_ctf_field_put(stream_class->event_header);
	bt_ctf_field_type_put(stream_class->packet_context_type);
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
end:
	if (ret) {
		bt_ctf_field_type_put(packet_context_type);
		goto end;
	}

	bt_ctf_field_type_put(_uint64_t);
	return ret;
}
