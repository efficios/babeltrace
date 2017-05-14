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

#define BT_LOG_TAG "STREAM-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/visitor-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/endian-internal.h>
#include <inttypes.h>
#include <stdint.h>

static
void bt_ctf_stream_class_destroy(struct bt_object *obj);
static
int init_event_header(struct bt_ctf_stream_class *stream_class);
static
int init_packet_context(struct bt_ctf_stream_class *stream_class);

struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name)
{
	struct bt_ctf_stream_class *stream_class;
	int ret;

	BT_LOGD("Creating default stream class object: name=\"%s\"", name);
	stream_class = bt_ctf_stream_class_create_empty(name);
	if (!stream_class) {
		BT_LOGD_STR("Cannot create empty stream class.");
		goto error;
	}

	ret = init_event_header(stream_class);
	if (ret) {
		BT_LOGE_STR("Cannot initialize stream class's event header field type.");
		goto error;
	}

	ret = init_packet_context(stream_class);
	if (ret) {
		BT_LOGE_STR("Cannot initialize stream class's packet context field type.");
		goto error;
	}

	BT_LOGD("Created default stream class object: addr=%p, name=\"%s\"",
		stream_class, name);
	return stream_class;

error:
	BT_PUT(stream_class);
	return stream_class;
}

struct bt_ctf_stream_class *bt_ctf_stream_class_create_empty(const char *name)
{
	struct bt_ctf_stream_class *stream_class = NULL;

	BT_LOGD("Creating empty stream class object: name=\"%s\"", name);

	if (name && bt_ctf_validate_identifier(name)) {
		BT_LOGW("Invalid parameter: stream class's name is not a valid CTF identifier: "
			"name=\"%s\"", name);
		goto error;
	}

	stream_class = g_new0(struct bt_ctf_stream_class, 1);
	if (!stream_class) {
		BT_LOGE_STR("Failed to allocate one stream class.");
		goto error;
	}

	stream_class->name = g_string_new(name);
	stream_class->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_release);
	if (!stream_class->event_classes) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	stream_class->event_classes_ht = g_hash_table_new_full(g_int64_hash,
			g_int64_equal, g_free, NULL);
	if (!stream_class->event_classes_ht) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}

	stream_class->packet_context_type = bt_ctf_field_type_structure_create();
	if (!stream_class->packet_context_type) {
		BT_LOGE_STR("Cannot stream class's initial packet context field type.");
		goto error;
	}

	stream_class->event_header_type = bt_ctf_field_type_structure_create();
	if (!stream_class->event_header_type) {
		BT_LOGE_STR("Cannot stream class's initial event header field type.");
		goto error;
	}

	stream_class->event_context_type = bt_ctf_field_type_structure_create();
	if (!stream_class->event_context_type) {
		BT_LOGE_STR("Cannot stream class's initial event context field type.");
		goto error;
	}

	bt_object_init(stream_class, bt_ctf_stream_class_destroy);
	BT_LOGD("Created empty stream class object: addr=%p, name=\"%s\"",
		stream_class, name);
	return stream_class;

error:
	BT_PUT(stream_class);
	return stream_class;
}

struct bt_ctf_trace *bt_ctf_stream_class_get_trace(
		struct bt_ctf_stream_class *stream_class)
{
	return stream_class ?
		bt_get(bt_ctf_stream_class_borrow_trace(stream_class)) :
		NULL;
}

const char *bt_ctf_stream_class_get_name(
		struct bt_ctf_stream_class *stream_class)
{
	const char *name = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	name = stream_class->name->str;
end:
	return name;
}

int bt_ctf_stream_class_set_name(struct bt_ctf_stream_class *stream_class,
		const char *name)
{
	int ret = 0;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	g_string_assign(stream_class->name, name);
	BT_LOGV("Set stream class's name: "
		"addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
end:
	return ret;
}

struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_clock *clock = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (!stream_class->clock) {
		BT_LOGV("Stream class has no clock: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		goto end;
	}

	clock = bt_get(stream_class->clock);
end:
	return clock;
}

int bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	struct bt_ctf_field_type *timestamp_field = NULL;

	if (!stream_class || !clock) {
		BT_LOGW("Invalid parameter: stream class or clock is NULL: "
			"stream-class-addr=%p, clock-addr=%p",
			stream_class, clock);
		ret = -1;
		goto end;
	}

	if (stream_class->frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	/*
	 * Look for a "timestamp" integer field type in the stream
	 * class's event header field type and map the stream class's
	 * clock's class to that field type if there's no current
	 * mapping.
	 */
	timestamp_field = bt_ctf_field_type_structure_get_field_type_by_name(
		stream_class->event_header_type, "timestamp");
	if (timestamp_field) {
		struct bt_ctf_clock_class *mapped_clock_class =
			bt_ctf_field_type_integer_get_mapped_clock_class(
				timestamp_field);

		if (!mapped_clock_class) {
			ret = bt_ctf_field_type_integer_set_mapped_clock_class(
				timestamp_field, clock->clock_class);
			if (ret) {
				goto end;
			}
		}

		BT_PUT(mapped_clock_class);
	}

	/* Replace the current clock of this stream class. */
	bt_put(stream_class->clock);
	stream_class->clock = bt_get(clock);
	BT_LOGV("Set stream class's clock: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"clock-addr=%p, clock-name=\"%s\"",
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		stream_class->clock,
		bt_ctf_clock_get_name(stream_class->clock));

end:
	bt_put(timestamp_field);
	return ret;
}

int64_t bt_ctf_stream_class_get_id(struct bt_ctf_stream_class *stream_class)
{
	int64_t ret;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (!stream_class->id_set) {
		BT_LOGV("Stream class's ID is not set: addr=%p, name=\"%s\"",
			stream_class,
			bt_ctf_stream_class_get_name(stream_class));
		ret = (int64_t) -1;
		goto end;
	}

	ret = stream_class->id;
end:
	return ret;
}

BT_HIDDEN
void _bt_ctf_stream_class_set_id(
		struct bt_ctf_stream_class *stream_class, int64_t id)
{
	assert(stream_class);
	stream_class->id = id;
	stream_class->id_set = 1;
	BT_LOGV("Set stream class's ID (internal): "
		"addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
}

struct event_class_set_stream_class_id_data {
	int64_t stream_class_id;
	int ret;
};

static
void event_class_set_stream_id(gpointer event_class, gpointer data)
{
	struct event_class_set_stream_class_id_data *typed_data = data;

	typed_data->ret |= bt_ctf_event_class_set_stream_id(event_class,
		typed_data->stream_class_id);
}

BT_HIDDEN
int bt_ctf_stream_class_set_id_no_check(
		struct bt_ctf_stream_class *stream_class, int64_t id)
{
	int ret = 0;
	struct event_class_set_stream_class_id_data data =
		{ .stream_class_id = id, .ret = 0 };

	/*
	 * Make sure all event classes have their "stream_id" attribute
	 * set to this value.
	 */
	g_ptr_array_foreach(stream_class->event_classes,
		event_class_set_stream_id, &data);
	ret = data.ret;
	if (ret) {
		BT_LOGE("Cannot set the IDs of all stream class's event classes: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		goto end;
	}

	_bt_ctf_stream_class_set_id(stream_class, id);
end:
	return ret;
}

int bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class,
		uint64_t id_param)
{
	int ret = 0;
	int64_t id = (int64_t) id_param;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid stream class's ID: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", id=%" PRIu64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class),
			id_param);
		ret = -1;
		goto end;
	}

	ret = bt_ctf_stream_class_set_id_no_check(stream_class, id);
	if (ret == 0) {
		BT_LOGV("Set stream class's ID: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
	}
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
		BT_LOGD("Event class with this ID already exists in the stream class: "
			"id=%" PRId64 ", name=\"%s\"",
			id_a, bt_ctf_event_class_get_name(event_class_a));
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
	int64_t *event_id = NULL;
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

	BT_LOGD("Adding event class to stream class: "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		event_class,
		bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class));

	if (!stream_class || !event_class) {
		BT_LOGW("Invalid parameter: stream class or event class is NULL: "
			"stream-class-addr=%p, event-class-addr=%p",
			stream_class, event_class);
		ret = -1;
		goto end;
	}

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (trace && trace->is_static) {
		ret = -1;
		goto end;
	}

	event_id = g_new(int64_t, 1);
	if (!event_id) {
		BT_LOGE_STR("Failed to allocate one int64_t.");
		ret = -1;
		goto end;
	}

	/* Check for duplicate event classes */
	struct search_query query = { .value = event_class, .found = 0 };
	g_ptr_array_foreach(stream_class->event_classes, event_class_exists,
		&query);
	if (query.found) {
		BT_LOGW_STR("Another event class part of this stream class has the same ID.");
		ret = -1;
		goto end;
	}

	old_stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (old_stream_class) {
		/* Event class is already associated to a stream class. */
		BT_LOGW("Event class is already part of another stream class: "
			"event-class-stream-class-addr=%p, "
			"event-class-stream-class-name=\"%s\", "
			"event-class-stream-class-id=%" PRId64,
			old_stream_class,
			bt_ctf_stream_class_get_name(old_stream_class),
			bt_ctf_stream_class_get_id(old_stream_class));
		ret = -1;
		goto end;
	}

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
			BT_LOGE("Failed to validate event class: ret=%d", ret);
			goto end;
		}

		if ((validation_output.valid_flags & validation_flags) !=
				validation_flags) {
			/* Invalid event class */
			BT_LOGE("Invalid trace, stream class, or event class: "
				"valid-flags=0x%x",
				validation_output.valid_flags);
			ret = -1;
			goto end;
		}
	}

	/* Only set an event ID if none was explicitly set before */
	*event_id = bt_ctf_event_class_get_id(event_class);
	if (*event_id < 0) {
		BT_LOGV("Event class has no ID: automatically setting it: "
			"id=%" PRId64, stream_class->next_event_id);

		if (bt_ctf_event_class_set_id(event_class,
				stream_class->next_event_id)) {
			BT_LOGE("Cannot set event class's ID: id=%" PRId64,
				stream_class->next_event_id);
			ret = -1;
			goto end;
		}
		stream_class->next_event_id++;
		*event_id = stream_class->next_event_id;
	}

	ret = bt_ctf_event_class_set_stream_id(event_class, stream_class->id);
	if (ret) {
		BT_LOGE("Cannot set event class's stream class ID attribute: ret=%d",
			ret);
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
	g_hash_table_insert(stream_class->event_classes_ht, event_id,
			event_class);
	event_id = NULL;

	/* Freeze the event class */
	bt_ctf_event_class_freeze(event_class);

	/* Notifiy listeners of the trace's schema modification. */
	if (trace) {
		struct bt_ctf_object obj = { .object = event_class,
				.type = BT_CTF_OBJECT_TYPE_EVENT_CLASS };

		(void) bt_ctf_trace_object_modification(&obj, trace);
	}

	BT_LOGD("Added event class to stream class: "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		event_class,
		bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class));

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
	g_free(event_id);

	return ret;
}

int64_t bt_ctf_stream_class_get_event_class_count(
		struct bt_ctf_stream_class *stream_class)
{
	int64_t ret;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	ret = (int64_t) stream_class->event_classes->len;
end:
	return ret;
}

struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_index(
		struct bt_ctf_stream_class *stream_class, uint64_t index)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (index >= stream_class->event_classes->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"index=%" PRIu64 ", count=%u",
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class),
			index, stream_class->event_classes->len);
		goto end;
	}

	event_class = g_ptr_array_index(stream_class->event_classes, index);
	bt_get(event_class);
end:
	return event_class;
}

struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id)
{
	int64_t id_key = (int64_t) id;
	struct bt_ctf_event_class *event_class = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (id_key < 0) {
		BT_LOGW("Invalid parameter: invalid event class's ID: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", event-class-id=%" PRIu64,
			stream_class,
			bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class), id);
		goto end;
	}

	event_class = g_hash_table_lookup(stream_class->event_classes_ht,
			&id_key);
	bt_get(event_class);
end:
	return event_class;
}

struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

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

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (packet_context_type &&
			bt_ctf_field_type_get_type_id(packet_context_type) !=
				BT_CTF_FIELD_TYPE_ID_STRUCT) {
		/* A packet context must be a structure. */
		BT_LOGW("Invalid parameter: stream class's packet context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"packet-context-ft-addr=%p, packet-context-ft-id=%s",
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class),
			packet_context_type,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_get_type_id(packet_context_type)));
		ret = -1;
		goto end;
	}

	bt_put(stream_class->packet_context_type);
	bt_get(packet_context_type);
	stream_class->packet_context_type = packet_context_type;
	BT_LOGV("Set stream class's packet context field type: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"packet-context-ft-addr=%p",
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		packet_context_type);

end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_stream_class_get_event_header_type(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (!stream_class->event_header_type) {
		BT_LOGV("Stream class has no event header field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		goto end;
	}

	bt_get(stream_class->event_header_type);
	ret = stream_class->event_header_type;
end:
	return ret;
}

int bt_ctf_stream_class_set_event_header_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_header_type)
{
	int ret = 0;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (event_header_type &&
			bt_ctf_field_type_get_type_id(event_header_type) !=
				BT_CTF_FIELD_TYPE_ID_STRUCT) {
		/* An event header must be a structure. */
		BT_LOGW("Invalid parameter: stream class's event header field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"event-header-ft-addr=%p, event-header-ft-id=%s",
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class),
			event_header_type,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_get_type_id(event_header_type)));
		ret = -1;
		goto end;
	}

	bt_put(stream_class->event_header_type);
	stream_class->event_header_type = bt_get(event_header_type);
	BT_LOGV("Set stream class's event header field type: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"event-header-ft-addr=%p",
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		event_header_type);
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_stream_class_get_event_context_type(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (!stream_class->event_context_type) {
		goto end;
	}

	bt_get(stream_class->event_context_type);
	ret = stream_class->event_context_type;
end:
	return ret;
}

int bt_ctf_stream_class_set_event_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_context_type)
{
	int ret = 0;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->frozen) {
		BT_LOGW("Invalid parameter: stream class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (event_context_type &&
			bt_ctf_field_type_get_type_id(event_context_type) !=
				BT_CTF_FIELD_TYPE_ID_STRUCT) {
		/* A packet context must be a structure. */
		BT_LOGW("Invalid parameter: stream class's event context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"event-context-ft-addr=%p, event-context-ft-id=%s",
			stream_class, bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class),
			event_context_type,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_get_type_id(event_context_type)));
		ret = -1;
		goto end;
	}

	bt_put(stream_class->event_context_type);
	stream_class->event_context_type = bt_get(event_context_type);
	BT_LOGV("Set stream class's event context field type: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"event-context-ft-addr=%p",
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class),
		event_context_type);
end:
	return ret;
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class)
{
	bt_get(stream_class);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class)
{
	bt_put(stream_class);
}

static
int64_t get_event_class_count(void *element)
{
	return bt_ctf_stream_class_get_event_class_count(
			(struct bt_ctf_stream_class *) element);
}

static
void *get_event_class(void *element, int i)
{
	return bt_ctf_stream_class_get_event_class_by_index(
			(struct bt_ctf_stream_class *) element, i);
}

static
int visit_event_class(void *object, bt_ctf_visitor visitor,void *data)
{
	struct bt_ctf_object obj =
			{ .object = object,
			.type = BT_CTF_OBJECT_TYPE_EVENT_CLASS };

	return visitor(&obj, data);
}

int bt_ctf_stream_class_visit(struct bt_ctf_stream_class *stream_class,
		bt_ctf_visitor visitor, void *data)
{
	int ret;
	struct bt_ctf_object obj =
			{ .object = stream_class,
			.type = BT_CTF_OBJECT_TYPE_STREAM_CLASS };

	if (!stream_class || !visitor) {
		BT_LOGW("Invalid parameter: stream class or visitor is NULL: "
			"stream-class-addr=%p, visitor=%p",
			stream_class, visitor);
		ret = -1;
		goto end;
	}

	ret = visitor_helper(&obj, get_event_class_count,
			get_event_class,
			visit_event_class, visitor, data);
	BT_LOGV("visitor_helper() returned: ret=%d", ret);
end:
	return ret;
}

BT_HIDDEN
void bt_ctf_stream_class_freeze(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class || stream_class->frozen) {
		return;
	}

	BT_LOGD("Freezing stream class: addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
	stream_class->frozen = 1;
	bt_ctf_field_type_freeze(stream_class->event_header_type);
	bt_ctf_field_type_freeze(stream_class->packet_context_type);
	bt_ctf_field_type_freeze(stream_class->event_context_type);

	if (stream_class->clock) {
		bt_ctf_clock_class_freeze(stream_class->clock->clock_class);
	}
}

BT_HIDDEN
int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context)
{
	int ret = 0;
	size_t i;

	BT_LOGD("Serializing stream class's metadata: "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", metadata-context-addr=%p",
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class), context);
	g_string_assign(context->field_name, "");
	context->current_indentation_level = 1;
	if (!stream_class->id_set) {
		BT_LOGW_STR("Stream class's ID is not set.");
		ret = -1;
		goto end;
	}

	g_string_append_printf(context->string,
		"stream {\n\tid = %" PRId64 ";\n\tevent.header := ",
		stream_class->id);
	ret = bt_ctf_field_type_serialize(stream_class->event_header_type,
		context);
	if (ret) {
		BT_LOGE("Cannot serialize stream class's event header field type's metadata: "
			"ret=%d", ret);
		goto end;
	}

	if (stream_class->packet_context_type) {
		g_string_append(context->string, ";\n\n\tpacket.context := ");
		ret = bt_ctf_field_type_serialize(stream_class->packet_context_type,
			context);
		if (ret) {
			BT_LOGE("Cannot serialize stream class's packet context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
	}

	if (stream_class->event_context_type) {
		g_string_append(context->string, ";\n\n\tevent.context := ");
		ret = bt_ctf_field_type_serialize(
			stream_class->event_context_type, context);
		if (ret) {
			BT_LOGE("Cannot serialize stream class's event context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
	}

	g_string_append(context->string, ";\n};\n\n");
	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *event_class =
			stream_class->event_classes->pdata[i];

		ret = bt_ctf_event_class_serialize(event_class, context);
		if (ret) {
			BT_LOGE("Cannot serialize event class's metadata: "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event_class,
				bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class));
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
	BT_LOGD("Destroying stream class: addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
	bt_put(stream_class->clock);

	if (stream_class->event_classes_ht) {
		g_hash_table_destroy(stream_class->event_classes_ht);
	}
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
		BT_LOGE_STR("Cannot create empty structure field type.");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		_uint32_t, "id");
	if (ret) {
		BT_LOGE_STR("Cannot add `id` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		_uint64_t, "timestamp");
	if (ret) {
		BT_LOGE_STR("Cannot add `timestamp` field to event header field type.");
		goto end;
	}

	BT_MOVE(stream_class->event_header_type, event_header_type);
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
		BT_LOGE_STR("Cannot create empty structure field type.");
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
		BT_LOGE_STR("Cannot add `timestamp_begin` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "timestamp_end");
	if (ret) {
		BT_LOGE_STR("Cannot add `timestamp_end` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "content_size");
	if (ret) {
		BT_LOGE_STR("Cannot add `content_size` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "packet_size");
	if (ret) {
		BT_LOGE_STR("Cannot add `packet_size` field to event header field type.");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		_uint64_t, "events_discarded");
	if (ret) {
		BT_LOGE_STR("Cannot add `events_discarded` field to event header field type.");
		goto end;
	}

	BT_MOVE(stream_class->packet_context_type, packet_context_type);
end:
	if (ret) {
		bt_put(packet_context_type);
		goto end;
	}

	bt_put(_uint64_t);
	return ret;
}
