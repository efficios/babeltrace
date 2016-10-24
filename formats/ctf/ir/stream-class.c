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
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler.h>
#include <babeltrace/align.h>
#include <babeltrace/endian.h>

static
void bt_ctf_stream_class_destroy(struct bt_object *obj);
static
int init_event_header(struct bt_ctf_stream_class *stream_class);
static
int init_packet_context(struct bt_ctf_stream_class *stream_class);

struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name)
{
	int ret;
	struct bt_ctf_stream_class *stream_class = NULL;

	if (name && bt_ctf_validate_identifier(name)) {
		goto error;
	}

	stream_class = g_new0(struct bt_ctf_stream_class, 1);
	if (!stream_class) {
		goto error;
	}

	stream_class->name = g_string_new(name);
	stream_class->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_release);
	if (!stream_class->event_classes) {
		goto error;
	}

	ret = init_event_header(stream_class);
	if (ret) {
		goto error;
	}

	ret = init_packet_context(stream_class);
	if (ret) {
		goto error;
	}

	bt_object_init(stream_class, bt_ctf_stream_class_destroy);
	return stream_class;

error:
        BT_PUT(stream_class);
	return stream_class;
}

BT_HIDDEN
struct bt_ctf_trace *bt_ctf_stream_class_get_trace(
		struct bt_ctf_stream_class *stream_class)
{
	return (struct bt_ctf_trace *) bt_object_get_parent(
		stream_class);
}

BT_HIDDEN
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

BT_HIDDEN
int bt_ctf_stream_class_set_name(struct bt_ctf_stream_class *stream_class,
		const char *name)
{
	int ret = 0;

	if (!stream_class || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	g_string_assign(stream_class->name, name);
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_clock *clock = NULL;

	if (!stream_class || !stream_class->clock) {
		goto end;
	}

	clock = stream_class->clock;
	bt_get(clock);
end:
	return clock;
}

int bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	struct bt_ctf_field_type *timestamp_field = NULL;

	if (!stream_class || !clock || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	/*
	 * Look for a "timestamp" field in the stream class' event header type
	 * and map the stream's clock to that field if no current mapping is
	 * currently set.
	 */
	timestamp_field = bt_ctf_field_type_structure_get_field_type_by_name(
		stream_class->event_header_type, "timestamp");
	if (timestamp_field) {
		struct bt_ctf_clock *mapped_clock;

		mapped_clock = bt_ctf_field_type_integer_get_mapped_clock(
			timestamp_field);
		if (mapped_clock) {
			bt_put(mapped_clock);
			goto end;
		}

		ret = bt_ctf_field_type_integer_set_mapped_clock(
			timestamp_field, clock);
		if (ret) {
			goto end;
		}
	}

	if (stream_class->clock) {
		bt_put(stream_class->clock);
	}

	stream_class->clock = clock;
	bt_get(clock);
end:
	if (timestamp_field) {
		bt_put(timestamp_field);
	}
	return ret;
}

BT_HIDDEN
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

BT_HIDDEN
int _bt_ctf_stream_class_set_id(
		struct bt_ctf_stream_class *stream_class, uint32_t id)
{
	stream_class->id = id;
	stream_class->id_set = 1;
	return 0;
}

struct event_class_set_stream_id_data {
	uint32_t stream_id;
	int ret;
};

static
void event_class_set_stream_id(gpointer event_class, gpointer data)
{
	struct event_class_set_stream_id_data *typed_data = data;

	typed_data->ret |= bt_ctf_event_class_set_stream_id(event_class,
		typed_data->stream_id);
}

BT_HIDDEN
int bt_ctf_stream_class_set_id_no_check(
		struct bt_ctf_stream_class *stream_class, uint32_t id)
{
	int ret = 0;
	struct event_class_set_stream_id_data data =
		{ .stream_id = id, .ret = 0 };

	/*
	 * Make sure all event classes have their "stream_id" attribute
	 * set to this value.
	 */
	g_ptr_array_foreach(stream_class->event_classes,
		event_class_set_stream_id, &data);
	ret = data.ret;
	if (ret) {
		goto end;
	}

	ret = _bt_ctf_stream_class_set_id(stream_class, id);
	if (ret) {
		goto end;
	}
end:
	return ret;
}

int bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class,
		uint32_t id)
{
	int ret = 0;

	if (!stream_class || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_stream_class_set_id_no_check(stream_class, id);
end:
	return ret;
}

static
void event_class_exists(gpointer element, gpointer query)
{
	struct bt_ctf_event_class *event_class_a = element;
	struct search_query *search_query = query;
	struct bt_ctf_event_class *event_class_b = search_query->value;
	int64_t id_a, id_b;

	if (search_query->value == element) {
		search_query->found = 1;
		goto end;
	}

	/*
	 * Two event classes cannot share the same name in a given
	 * stream class.
	 */
	if (!strcmp(bt_ctf_event_class_get_name(event_class_a),
			bt_ctf_event_class_get_name(event_class_b))) {
		search_query->found = 1;
		goto end;
	}

	/*
	 * Two event classes cannot share the same ID in a given
	 * stream class.
	 */
	id_a = bt_ctf_event_class_get_id(event_class_a);
	id_b = bt_ctf_event_class_get_id(event_class_b);

	if (id_a < 0 || id_b < 0) {
		/* at least one ID is not set: will be automatically set later */
		goto end;
	}

	if (id_a == id_b) {
		search_query->found = 1;
		goto end;
	}

end:
	return;
}

int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class)
{
	int ret = 0;
	int64_t event_id;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream_class *old_stream_class = NULL;
	struct bt_ctf_validation_output validation_output = { 0 };
	struct bt_ctf_field_type *packet_header_type = NULL;
	struct bt_ctf_field_type *packet_context_type = NULL;
	struct bt_ctf_field_type *event_header_type = NULL;
	struct bt_ctf_field_type *stream_event_ctx_type = NULL;
	struct bt_ctf_field_type *event_context_type = NULL;
	struct bt_ctf_field_type *event_payload_type = NULL;
	const enum bt_ctf_validation_flag validation_flags =
		BT_CTF_VALIDATION_FLAG_EVENT;

	if (!stream_class || !event_class) {
		ret = -1;
		goto end;
	}

	/* Check for duplicate event classes */
	struct search_query query = { .value = event_class, .found = 0 };
	g_ptr_array_foreach(stream_class->event_classes, event_class_exists,
		&query);
	if (query.found) {
		ret = -1;
		goto end;
	}

	old_stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (old_stream_class) {
		/* Event class is already associated to a stream class. */
		ret = -1;
		goto end;
	}

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (trace) {
		/*
		 * If the stream class is associated with a trace, then
		 * both those objects are frozen. Also, this event class
		 * is about to be frozen.
		 *
		 * Therefore the event class must be validated here.
		 * The trace and stream class should be valid at this
		 * point.
		 */
		assert(trace->valid);
		assert(stream_class->valid);
		packet_header_type =
			bt_ctf_trace_get_packet_header_type(trace);
		packet_context_type =
			bt_ctf_stream_class_get_packet_context_type(
				stream_class);
		event_header_type =
			bt_ctf_stream_class_get_event_header_type(stream_class);
		stream_event_ctx_type =
			bt_ctf_stream_class_get_event_context_type(
				stream_class);
		event_context_type =
			bt_ctf_event_class_get_context_type(event_class);
		event_payload_type =
			bt_ctf_event_class_get_payload_type(event_class);
		ret = bt_ctf_validate_class_types(
			trace->environment, packet_header_type,
			packet_context_type, event_header_type,
			stream_event_ctx_type, event_context_type,
			event_payload_type, trace->valid,
			stream_class->valid, event_class->valid,
			&validation_output, validation_flags);
		BT_PUT(packet_header_type);
		BT_PUT(packet_context_type);
		BT_PUT(event_header_type);
		BT_PUT(stream_event_ctx_type);
		BT_PUT(event_context_type);
		BT_PUT(event_payload_type);

		if (ret) {
			/*
			 * This means something went wrong during the
			 * validation process, not that the objects are
			 * invalid.
			 */
			goto end;
		}

		if ((validation_output.valid_flags & validation_flags) !=
				validation_flags) {
			/* Invalid event class */
			ret = -1;
			goto end;
		}
	}

	/* Only set an event ID if none was explicitly set before */
	event_id = bt_ctf_event_class_get_id(event_class);
	if (event_id < 0) {
		if (bt_ctf_event_class_set_id(event_class,
			stream_class->next_event_id++)) {
			ret = -1;
			goto end;
		}
	}

	ret = bt_ctf_event_class_set_stream_id(event_class, stream_class->id);
	if (ret) {
		goto end;
	}

	bt_object_set_parent(event_class, stream_class);

	if (trace) {
		/*
		 * At this point we know that the function will be
		 * successful. Therefore we can replace the event
		 * class's field types with what's in the validation
		 * output structure and mark this event class as valid.
		 */
		bt_ctf_validation_replace_types(NULL, NULL, event_class,
			&validation_output, validation_flags);
		event_class->valid = 1;

		/*
		 * Put what was not moved in
		 * bt_ctf_validation_replace_types().
		 */
		bt_ctf_validation_output_put_types(&validation_output);
	}

	/* Add to the event classes of the stream class */
	g_ptr_array_add(stream_class->event_classes, event_class);

	/* Freeze the event class */
	bt_ctf_event_class_freeze(event_class);

	if (stream_class->byte_order) {
		/*
		 * Only set native byte order if it has been initialized
		 * when the stream class was added to a trace.
		 *
		 * If not set here, this will be set when the stream
		 * class is added to a trace.
		 */
		bt_ctf_event_class_set_native_byte_order(event_class,
			stream_class->byte_order);
	}

end:
	BT_PUT(trace);
	BT_PUT(old_stream_class);
	bt_ctf_validation_output_put_types(&validation_output);
	assert(!packet_header_type);
	assert(!packet_context_type);
	assert(!event_header_type);
	assert(!stream_event_ctx_type);
	assert(!event_context_type);
	assert(!event_payload_type);

	return ret;
}

BT_HIDDEN
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

BT_HIDDEN
struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class(
		struct bt_ctf_stream_class *stream_class, int index)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class || index < 0 ||
		index >= stream_class->event_classes->len) {
		goto end;
	}

	event_class = g_ptr_array_index(stream_class->event_classes, index);
	bt_get(event_class);
end:
	return event_class;
}

BT_HIDDEN
struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_name(
		struct bt_ctf_stream_class *stream_class, const char *name)
{
	size_t i;
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class || !name) {
		goto end;
	}

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *cur_event_class =
			g_ptr_array_index(stream_class->event_classes, i);
		const char *cur_event_class_name =
			bt_ctf_event_class_get_name(cur_event_class);

		if (!strcmp(name, cur_event_class_name)) {
			event_class = cur_event_class;
			bt_get(event_class);
			goto end;
		}
	}
end:
	return event_class;
}

BT_HIDDEN
struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_id(
		struct bt_ctf_stream_class *stream_class, uint32_t id)
{
	size_t i;
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class) {
		goto end;
	}

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *current_event_class =
			g_ptr_array_index(stream_class->event_classes, i);

		if (bt_ctf_event_class_get_id(current_event_class) == id) {
			event_class = current_event_class;
			bt_get(event_class);
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
	bt_get(stream_class->packet_context_type);
	ret = stream_class->packet_context_type;
end:
	return ret;
}

int bt_ctf_stream_class_set_packet_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type)
{
	int ret = 0;

	if (!stream_class || !packet_context_type || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	assert(stream_class->packet_context_type);
	if (stream_class->packet_context_type == packet_context_type) {
		goto end;
	}
	if (bt_ctf_field_type_get_type_id(packet_context_type) !=
		CTF_TYPE_STRUCT) {
		/* A packet context must be a structure */
		ret = -1;
		goto end;
	}

	bt_put(stream_class->packet_context_type);
	bt_get(packet_context_type);
	stream_class->packet_context_type = packet_context_type;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_stream_class_get_event_header_type(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!stream_class || !stream_class->event_header_type) {
		goto end;
	}

	assert(stream_class->event_header_type);
	bt_get(stream_class->event_header_type);
	ret = stream_class->event_header_type;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_class_set_event_header_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_header_type)
{
	int ret = 0;

	if (!stream_class || !event_header_type || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	assert(stream_class->event_header_type);
	if (stream_class->event_header_type == event_header_type) {
		goto end;
	}
	if (bt_ctf_field_type_get_type_id(event_header_type) !=
		CTF_TYPE_STRUCT) {
		/* An event header must be a structure */
		ret = -1;
		goto end;
	}

	bt_put(stream_class->event_header_type);
	bt_get(event_header_type);
	stream_class->event_header_type = event_header_type;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_stream_class_get_event_context_type(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!stream_class || !stream_class->event_context_type) {
		goto end;
	}

	assert(stream_class->event_context_type);
	bt_get(stream_class->event_context_type);
	ret = stream_class->event_context_type;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_stream_class_set_event_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_context_type)
{
	int ret = 0;

	if (!stream_class || !event_context_type || stream_class->frozen) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_context_type) !=
		CTF_TYPE_STRUCT) {
		/* A packet context must be a structure */
		ret = -1;
		goto end;
	}

	bt_put(stream_class->event_context_type);
	bt_get(event_context_type);
	stream_class->event_context_type = event_context_type;
end:
	return ret;
}

void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class)
{
	bt_get(stream_class);
}

void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class)
{
	bt_put(stream_class);
}

BT_HIDDEN
void bt_ctf_stream_class_freeze(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}

	stream_class->frozen = 1;
	bt_ctf_field_type_freeze(stream_class->event_header_type);
	bt_ctf_field_type_freeze(stream_class->packet_context_type);
	bt_ctf_field_type_freeze(stream_class->event_context_type);
	bt_ctf_clock_freeze(stream_class->clock);
}

BT_HIDDEN
void bt_ctf_stream_class_set_byte_order(
	struct bt_ctf_stream_class *stream_class, int byte_order)
{
	int i;

	assert(stream_class);
	assert(byte_order == LITTLE_ENDIAN || byte_order == BIG_ENDIAN);
	stream_class->byte_order = byte_order;

	/* Set native byte order to little or big endian */
	bt_ctf_field_type_set_native_byte_order(
		stream_class->event_header_type, byte_order);
	bt_ctf_field_type_set_native_byte_order(
		stream_class->packet_context_type, byte_order);
	bt_ctf_field_type_set_native_byte_order(
		stream_class->event_context_type, byte_order);

	/* Set all events' native byte order */
	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *event_class =
			g_ptr_array_index(stream_class->event_classes, i);

		bt_ctf_event_class_set_native_byte_order(event_class,
			byte_order);
	}
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
void bt_ctf_stream_class_destroy(struct bt_object *obj)
{
	struct bt_ctf_stream_class *stream_class;

	stream_class = container_of(obj, struct bt_ctf_stream_class, base);
	bt_put(stream_class->clock);

	if (stream_class->event_classes) {
		g_ptr_array_free(stream_class->event_classes, TRUE);
	}

	if (stream_class->name) {
		g_string_free(stream_class->name, TRUE);
	}

	bt_put(stream_class->event_header_type);
	bt_put(stream_class->packet_context_type);
	bt_put(stream_class->event_context_type);
	g_free(stream_class);
}

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
		ret = -1;
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

	if (stream_class->event_header_type) {
		bt_put(stream_class->event_header_type);
	}
	stream_class->event_header_type = event_header_type;
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

	if (!packet_context_type) {
		ret = -1;
		goto end;
	}

	/*
	 * We create a stream packet context as proposed in the CTF
	 * specification.
	 */
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

	bt_put(stream_class->packet_context_type);
	stream_class->packet_context_type = packet_context_type;
end:
	if (ret) {
		bt_put(packet_context_type);
		goto end;
	}

	bt_put(_uint64_t);
	return ret;
}
