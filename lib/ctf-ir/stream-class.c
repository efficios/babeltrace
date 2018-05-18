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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/visitor-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ctf-ir/field-wrapper-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

BT_HIDDEN
int bt_stream_class_common_initialize(struct bt_stream_class_common *stream_class,
		const char *name, bt_object_release_func release_func)
{
	BT_LOGD("Initializing common stream class object: name=\"%s\"", name);

	bt_object_init(stream_class, release_func);
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

	BT_LOGD("Initialized common stream class object: addr=%p, name=\"%s\"",
		stream_class, name);
	return 0;

error:
	return -1;
}

BT_HIDDEN
void bt_stream_class_common_finalize(struct bt_stream_class_common *stream_class)
{
	BT_LOGD("Finalizing common stream class: addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_stream_class_common_get_name(stream_class),
		bt_stream_class_common_get_id(stream_class));
	bt_put(stream_class->clock_class);

	if (stream_class->event_classes_ht) {
		g_hash_table_destroy(stream_class->event_classes_ht);
	}
	if (stream_class->event_classes) {
		BT_LOGD_STR("Destroying event classes.");
		g_ptr_array_free(stream_class->event_classes, TRUE);
	}

	if (stream_class->name) {
		g_string_free(stream_class->name, TRUE);
	}

	BT_LOGD_STR("Putting event header field type.");
	bt_put(stream_class->event_header_field_type);
	BT_LOGD_STR("Putting packet context field type.");
	bt_put(stream_class->packet_context_field_type);
	BT_LOGD_STR("Putting event context field type.");
	bt_put(stream_class->event_context_field_type);
}

static
void bt_stream_class_destroy(struct bt_object *obj)
{
	struct bt_stream_class *stream_class;

	stream_class = (void *) obj;
	BT_LOGD("Destroying stream class: addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class));

	/*
	 * IMPORTANT: Finalize the common stream class BEFORE finalizing
	 * the pools because otherwise this scenario is possible:
	 *
	 * 1. Event header field object pool is finalized, thus
	 *    destroying its internal array and state.
	 *
	 * 2. Stream class is finalized: each event class is destroyed.
	 *
	 * 3. Destroying an event class finalizes its event pool,
	 *    destroying each contained event.
	 *
	 * 4. Destroying an event makes it recycle its event header
	 *    field to its stream class's event header field pool. But
	 *    said pool is already destroyed.
	 */
	bt_stream_class_common_finalize(BT_TO_COMMON(stream_class));
	bt_object_pool_finalize(&stream_class->event_header_field_pool);
	bt_object_pool_finalize(&stream_class->packet_context_field_pool);
	g_free(stream_class);
}

static
void free_field_wrapper(struct bt_field_wrapper *field_wrapper,
		struct bt_stream_class *stream_class)
{
	bt_field_wrapper_destroy((void *) field_wrapper);
}

struct bt_stream_class *bt_stream_class_create(const char *name)
{
	struct bt_stream_class *stream_class = NULL;
	int ret;

	BT_LOGD("Creating stream class object: name=\"%s\"", name);
	stream_class = g_new0(struct bt_stream_class, 1);
	if (!stream_class) {
		BT_LOGE_STR("Failed to allocate one stream class.");
		goto error;
	}

	ret = bt_stream_class_common_initialize(BT_TO_COMMON(stream_class),
		name, bt_stream_class_destroy);
	if (ret) {
		/* bt_stream_class_common_initialize() logs errors */
		goto error;
	}

	ret = bt_object_pool_initialize(&stream_class->event_header_field_pool,
		(bt_object_pool_new_object_func) bt_field_wrapper_new,
		(bt_object_pool_destroy_object_func) free_field_wrapper,
		stream_class);
	if (ret) {
		BT_LOGE("Failed to initialize event header field pool: ret=%d",
			ret);
		goto error;
	}

	ret = bt_object_pool_initialize(&stream_class->packet_context_field_pool,
		(bt_object_pool_new_object_func) bt_field_wrapper_new,
		(bt_object_pool_destroy_object_func) free_field_wrapper,
		stream_class);
	if (ret) {
		BT_LOGE("Failed to initialize packet context field pool: ret=%d",
			ret);
		goto error;
	}

	BT_LOGD("Created stream class object: addr=%p, name=\"%s\"",
		stream_class, name);
	return stream_class;

error:
	bt_put(stream_class);
	return NULL;
}

struct bt_event_header_field *bt_stream_class_create_event_header_field(
		struct bt_stream_class *stream_class)
{
	struct bt_field_wrapper *field_wrapper;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(stream_class->common.frozen,
		"Stream class is not part of a trace: %!+S", stream_class);
	BT_ASSERT_PRE(stream_class->common.event_header_field_type,
		"Stream class has no event header field type: %!+S",
		stream_class);
	field_wrapper = bt_field_wrapper_create(
		&stream_class->event_header_field_pool,
		(void *) stream_class->common.event_header_field_type);
	if (!field_wrapper) {
		BT_LIB_LOGE("Cannot allocate one event header field from stream class: "
			"%![sc-]+S", stream_class);
		goto error;
	}

	BT_ASSERT(field_wrapper->field);
	goto end;

error:
	if (field_wrapper) {
		bt_field_wrapper_destroy(field_wrapper);
		field_wrapper = NULL;
	}

end:
	return (void *) field_wrapper;
}

struct bt_packet_context_field *bt_stream_class_create_packet_context_field(
		struct bt_stream_class *stream_class)
{
	struct bt_field_wrapper *field_wrapper;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(stream_class->common.frozen,
		"Stream class is not part of a trace: %!+S", stream_class);
	BT_ASSERT_PRE(stream_class->common.packet_context_field_type,
		"Stream class has no packet context field type: %!+S",
		stream_class);
	field_wrapper = bt_field_wrapper_create(
		&stream_class->packet_context_field_pool,
		(void *) stream_class->common.packet_context_field_type);
	if (!field_wrapper) {
		BT_LIB_LOGE("Cannot allocate one packet context field from stream class: "
			"%![sc-]+S", stream_class);
		goto error;
	}

	BT_ASSERT(field_wrapper->field);
	goto end;

error:
	if (field_wrapper) {
		bt_field_wrapper_destroy(field_wrapper);
		field_wrapper = NULL;
	}

end:
	return (void *) field_wrapper;
}

struct bt_trace *bt_stream_class_borrow_trace(struct bt_stream_class *stream_class)
{
	return BT_FROM_COMMON(bt_stream_class_common_borrow_trace(
		BT_TO_COMMON(stream_class)));
}

const char *bt_stream_class_get_name(struct bt_stream_class *stream_class)
{
	return bt_stream_class_common_get_name(BT_TO_COMMON(stream_class));
}

int bt_stream_class_set_name(struct bt_stream_class *stream_class,
		const char *name)
{
	return bt_stream_class_common_set_name(BT_TO_COMMON(stream_class),
		name);
}

int64_t bt_stream_class_get_id(struct bt_stream_class *stream_class)
{
	return bt_stream_class_common_get_id(BT_TO_COMMON(stream_class));
}

int bt_stream_class_set_id(struct bt_stream_class *stream_class, uint64_t id)
{
	return bt_stream_class_common_set_id(BT_TO_COMMON(stream_class), id);
}

static
void event_class_exists(gpointer element, gpointer query)
{
	struct bt_event_class_common *event_class_a = element;
	struct search_query *search_query = query;
	struct bt_event_class_common *event_class_b = search_query->value;
	int64_t id_a, id_b;

	if (search_query->value == element) {
		search_query->found = 1;
		goto end;
	}

	/*
	 * Two event classes cannot share the same ID in a given
	 * stream class.
	 */
	id_a = bt_event_class_common_get_id(event_class_a);
	id_b = bt_event_class_common_get_id(event_class_b);

	if (id_a < 0 || id_b < 0) {
		/* at least one ID is not set: will be automatically set later */
		goto end;
	}

	if (id_a == id_b) {
		BT_LOGW("Event class with this ID already exists in the stream class: "
			"id=%" PRId64 ", name=\"%s\"",
			id_a, bt_event_class_common_get_name(event_class_a));
		search_query->found = 1;
		goto end;
	}

end:
	return;
}

BT_HIDDEN
int bt_stream_class_common_add_event_class(
		struct bt_stream_class_common *stream_class,
		struct bt_event_class_common *event_class,
		bt_validation_flag_copy_field_type_func copy_field_type_func)
{
	int ret = 0;
	int64_t *event_id = NULL;
	struct bt_trace_common *trace = NULL;
	struct bt_stream_class_common *old_stream_class = NULL;
	struct bt_validation_output validation_output = { 0 };
	struct bt_field_type_common *packet_header_type = NULL;
	struct bt_field_type_common *packet_context_type = NULL;
	struct bt_field_type_common *event_header_type = NULL;
	struct bt_field_type_common *stream_event_ctx_type = NULL;
	struct bt_field_type_common *event_context_type = NULL;
	struct bt_field_type_common *event_payload_type = NULL;
	const enum bt_validation_flag validation_flags =
		BT_VALIDATION_FLAG_EVENT;
	struct bt_clock_class *expected_clock_class = NULL;

	BT_ASSERT(copy_field_type_func);

	if (!stream_class || !event_class) {
		BT_LOGW("Invalid parameter: stream class or event class is NULL: "
			"stream-class-addr=%p, event-class-addr=%p",
			stream_class, event_class);
		ret = -1;
		goto end;
	}

	BT_LOGD("Adding event class to stream class: "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		stream_class, bt_stream_class_common_get_name(stream_class),
		bt_stream_class_common_get_id(stream_class),
		event_class,
		bt_event_class_common_get_name(event_class),
		bt_event_class_common_get_id(event_class));
	trace = bt_stream_class_common_borrow_trace(stream_class);

	if (stream_class->frozen) {
		/*
		 * We only check that the event class to be added has a
		 * single class which matches the stream class's
		 * expected clock class if the stream class is frozen.
		 * If it's not, then this event class is added "as is"
		 * and the validation will be performed when calling
		 * either bt_trace_add_stream_class() or
		 * bt_event_create(). This is because the stream class's
		 * field types (packet context, event header, event
		 * context) could change before the next call to one of
		 * those two functions.
		 */
		expected_clock_class = bt_get(stream_class->clock_class);

		/*
		 * At this point, `expected_clock_class` can be NULL,
		 * and bt_event_class_validate_single_clock_class()
		 * below can set it.
		 */
		ret = bt_event_class_common_validate_single_clock_class(
			event_class, &expected_clock_class);
		if (ret) {
			BT_LOGW("Event class contains a field type which is not "
				"recursively mapped to its stream class's "
				"expected clock class: "
				"stream-class-addr=%p, "
				"stream-class-id=%" PRId64 ", "
				"stream-class-name=\"%s\", "
				"expected-clock-class-addr=%p, "
				"expected-clock-class-name=\"%s\"",
				stream_class,
				bt_stream_class_common_get_id(stream_class),
				bt_stream_class_common_get_name(stream_class),
				expected_clock_class,
				expected_clock_class ?
					bt_clock_class_get_name(expected_clock_class) :
					NULL);
			goto end;
		}
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

	old_stream_class = bt_event_class_common_borrow_stream_class(event_class);
	if (old_stream_class) {
		/* Event class is already associated to a stream class. */
		BT_LOGW("Event class is already part of another stream class: "
			"event-class-stream-class-addr=%p, "
			"event-class-stream-class-name=\"%s\", "
			"event-class-stream-class-id=%" PRId64,
			old_stream_class,
			bt_stream_class_common_get_name(old_stream_class),
			bt_stream_class_common_get_id(old_stream_class));
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
		BT_ASSERT(trace->valid);
		BT_ASSERT(stream_class->valid);
		packet_header_type =
			bt_trace_common_borrow_packet_header_field_type(trace);
		packet_context_type =
			bt_stream_class_common_borrow_packet_context_field_type(
				stream_class);
		event_header_type =
			bt_stream_class_common_borrow_event_header_field_type(
				stream_class);
		stream_event_ctx_type =
			bt_stream_class_common_borrow_event_context_field_type(
				stream_class);
		event_context_type =
			bt_event_class_common_borrow_context_field_type(
				event_class);
		event_payload_type =
			bt_event_class_common_borrow_payload_field_type(
				event_class);
		ret = bt_validate_class_types(
			trace->environment, packet_header_type,
			packet_context_type, event_header_type,
			stream_event_ctx_type, event_context_type,
			event_payload_type, trace->valid,
			stream_class->valid, event_class->valid,
			&validation_output, validation_flags,
			copy_field_type_func);

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
			BT_LOGW("Invalid trace, stream class, or event class: "
				"valid-flags=0x%x",
				validation_output.valid_flags);
			ret = -1;
			goto end;
		}
	}

	/* Only set an event ID if none was explicitly set before */
	*event_id = bt_event_class_common_get_id(event_class);
	if (*event_id < 0) {
		BT_LOGV("Event class has no ID: automatically setting it: "
			"id=%" PRId64, stream_class->next_event_id);

		if (bt_event_class_common_set_id(event_class,
				stream_class->next_event_id)) {
			BT_LOGE("Cannot set event class's ID: id=%" PRId64,
				stream_class->next_event_id);
			ret = -1;
			goto end;
		}
		stream_class->next_event_id++;
		*event_id = stream_class->next_event_id;
	}

	bt_object_set_parent(event_class, stream_class);

	if (trace) {
		/*
		 * At this point we know that the function will be
		 * successful. Therefore we can replace the event
		 * class's field types with what's in the validation
		 * output structure and mark this event class as valid.
		 */
		bt_validation_replace_types(NULL, NULL, event_class,
			&validation_output, validation_flags);
		event_class->valid = 1;

		/*
		 * Put what was not moved in
		 * bt_validation_replace_types().
		 */
		bt_validation_output_put_types(&validation_output);
	}

	/* Add to the event classes of the stream class */
	g_ptr_array_add(stream_class->event_classes, event_class);
	g_hash_table_insert(stream_class->event_classes_ht, event_id,
			event_class);
	event_id = NULL;

	/* Freeze the event class */
	bt_event_class_common_freeze(event_class);

	/*
	 * It is safe to set the stream class's unique clock class
	 * now if the stream class is frozen.
	 */
	if (stream_class->frozen && expected_clock_class) {
		BT_ASSERT(!stream_class->clock_class ||
			stream_class->clock_class == expected_clock_class);
		BT_MOVE(stream_class->clock_class, expected_clock_class);
	}

	BT_LOGD("Added event class to stream class: "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		stream_class, bt_stream_class_common_get_name(stream_class),
		bt_stream_class_common_get_id(stream_class),
		event_class,
		bt_event_class_common_get_name(event_class),
		bt_event_class_common_get_id(event_class));

end:
	bt_validation_output_put_types(&validation_output);
	bt_put(expected_clock_class);
	g_free(event_id);
	return ret;
}

int bt_stream_class_add_event_class(
		struct bt_stream_class *stream_class,
		struct bt_event_class *event_class)
{
	struct bt_trace *trace;
	int ret = 0;
	uint64_t i;
	struct bt_clock_class *old_clock_class;

	if (!stream_class) {
		BT_LOGW("Invalid parameter: stream class is NULL: "
			"stream-class-addr=%p", stream_class);
		ret = -1;
		goto end;
	}

	old_clock_class = stream_class->common.clock_class;
	trace = BT_FROM_COMMON(bt_stream_class_common_borrow_trace(
		BT_TO_COMMON(stream_class)));
	if (trace && trace->is_static) {
		BT_LOGW("Invalid parameter: stream class's trace is static: "
			"trace-addr=%p, trace-name=\"%s\"",
			trace, bt_trace_get_name(trace));
		ret = -1;
		goto end;
	}

	ret = bt_stream_class_common_add_event_class(
		BT_TO_COMMON(stream_class), BT_TO_COMMON(event_class),
		(bt_validation_flag_copy_field_type_func) bt_field_type_copy);
	if (ret) {
		goto end;
	}

	/* Notifiy listeners of the trace's schema modification. */
	if (trace) {
		struct bt_visitor_object obj = { .object = event_class,
				.type = BT_VISITOR_OBJECT_TYPE_EVENT_CLASS };

		(void) bt_trace_object_modification(&obj, trace);
	}

	if (!old_clock_class && stream_class->common.clock_class) {
		/*
		 * Adding this event class updated the stream class's
		 * single clock class: make sure all the events which
		 * exist in event pools have an existing clock value for
		 * this clock class so that any created event object in
		 * the future (from a pool or not) has this clock value
		 * available.
		 */
		for (i = 0; i < stream_class->common.event_classes->len; i++) {
			struct bt_event_class *event_class =
				stream_class->common.event_classes->pdata[i];

			BT_ASSERT(event_class);
			ret = bt_event_class_update_event_pool_clock_values(
				event_class);
			if (ret) {
				goto end;
			}
		}
	}

end:
	return ret;
}

int64_t bt_stream_class_get_event_class_count(
		struct bt_stream_class *stream_class)
{
	return bt_stream_class_common_get_event_class_count(
		BT_TO_COMMON(stream_class));
}

struct bt_event_class *bt_stream_class_borrow_event_class_by_index(
		struct bt_stream_class *stream_class, uint64_t index)
{
	return BT_FROM_COMMON(bt_stream_class_common_borrow_event_class_by_index(
		BT_TO_COMMON(stream_class), index));
}

struct bt_event_class *bt_stream_class_borrow_event_class_by_id(
		struct bt_stream_class *stream_class, uint64_t id)
{
	return BT_FROM_COMMON(bt_stream_class_common_borrow_event_class_by_id(
		BT_TO_COMMON(stream_class), id));
}

struct bt_field_type *bt_stream_class_borrow_packet_context_field_type(
		struct bt_stream_class *stream_class)
{
	return BT_FROM_COMMON(bt_stream_class_common_borrow_packet_context_field_type(
		BT_TO_COMMON(stream_class)));
}

int bt_stream_class_set_packet_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *packet_context_type)
{
	return bt_stream_class_common_set_packet_context_field_type(
		BT_TO_COMMON(stream_class), (void *) packet_context_type);
}

struct bt_field_type *bt_stream_class_borrow_event_header_field_type(
		struct bt_stream_class *stream_class)
{
	return BT_FROM_COMMON(bt_stream_class_common_borrow_event_header_field_type(
		BT_TO_COMMON(stream_class)));
}

int bt_stream_class_set_event_header_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *event_header_type)
{
	return bt_stream_class_common_set_event_header_field_type(
		BT_TO_COMMON(stream_class), (void *) event_header_type);
}

struct bt_field_type *bt_stream_class_borrow_event_context_field_type(
		struct bt_stream_class *stream_class)
{
	return BT_FROM_COMMON(bt_stream_class_common_borrow_event_context_field_type(
		BT_TO_COMMON(stream_class)));
}

int bt_stream_class_set_event_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *event_context_type)
{
	return bt_stream_class_common_set_event_context_field_type(
		BT_TO_COMMON(stream_class), (void *) event_context_type);
}

static
int64_t get_event_class_count(void *element)
{
	return bt_stream_class_get_event_class_count(
			(struct bt_stream_class *) element);
}

static
void *get_event_class(void *element, int i)
{
	return bt_stream_class_get_event_class_by_index(
			(struct bt_stream_class *) element, i);
}

static
int visit_event_class(void *object, bt_visitor visitor,void *data)
{
	struct bt_visitor_object obj = {
		.object = object,
		.type = BT_VISITOR_OBJECT_TYPE_EVENT_CLASS
	};

	return visitor(&obj, data);
}

BT_HIDDEN
int bt_stream_class_common_visit(struct bt_stream_class_common *stream_class,
		bt_visitor visitor, void *data)
{
	int ret;
	struct bt_visitor_object obj = {
		.object = stream_class,
		.type = BT_VISITOR_OBJECT_TYPE_STREAM_CLASS
	};

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

int bt_stream_class_visit(struct bt_stream_class *stream_class,
		bt_visitor visitor, void *data)
{
	return bt_stream_class_common_visit(BT_FROM_COMMON(stream_class),
		visitor, data);
}

BT_HIDDEN
void bt_stream_class_common_freeze(struct bt_stream_class_common *stream_class)
{
	if (!stream_class || stream_class->frozen) {
		return;
	}

	BT_LOGD("Freezing stream class: addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_stream_class_common_get_name(stream_class),
		bt_stream_class_common_get_id(stream_class));
	stream_class->frozen = 1;
	bt_field_type_common_freeze(stream_class->event_header_field_type);
	bt_field_type_common_freeze(stream_class->packet_context_field_type);
	bt_field_type_common_freeze(stream_class->event_context_field_type);
	bt_clock_class_freeze(stream_class->clock_class);
}

void bt_stream_class_freeze(struct bt_stream_class *stream_class)
{
	bt_stream_class_common_freeze(BT_TO_COMMON(stream_class));
}

BT_HIDDEN
int bt_stream_class_common_validate_single_clock_class(
		struct bt_stream_class_common *stream_class,
		struct bt_clock_class **expected_clock_class)
{
	int ret;
	uint64_t i;

	BT_ASSERT(stream_class);
	BT_ASSERT(expected_clock_class);
	ret = bt_field_type_common_validate_single_clock_class(
		stream_class->packet_context_field_type,
		expected_clock_class);
	if (ret) {
		BT_LOGW("Stream class's packet context field type "
			"is not recursively mapped to the "
			"expected clock class: "
			"stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", "
			"ft-addr=%p",
			stream_class,
			bt_stream_class_common_get_name(stream_class),
			stream_class->id,
			stream_class->packet_context_field_type);
		goto end;
	}

	ret = bt_field_type_common_validate_single_clock_class(
		stream_class->event_header_field_type,
		expected_clock_class);
	if (ret) {
		BT_LOGW("Stream class's event header field type "
			"is not recursively mapped to the "
			"expected clock class: "
			"stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", "
			"ft-addr=%p",
			stream_class,
			bt_stream_class_common_get_name(stream_class),
			stream_class->id,
			stream_class->event_header_field_type);
		goto end;
	}

	ret = bt_field_type_common_validate_single_clock_class(
		stream_class->event_context_field_type,
		expected_clock_class);
	if (ret) {
		BT_LOGW("Stream class's event context field type "
			"is not recursively mapped to the "
			"expected clock class: "
			"stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", "
			"ft-addr=%p",
			stream_class,
			bt_stream_class_common_get_name(stream_class),
			stream_class->id,
			stream_class->event_context_field_type);
		goto end;
	}

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_event_class_common *event_class =
			g_ptr_array_index(stream_class->event_classes, i);

		BT_ASSERT(event_class);
		ret = bt_event_class_common_validate_single_clock_class(
			event_class, expected_clock_class);
		if (ret) {
			BT_LOGW("Stream class's event class contains a "
				"field type which is not recursively mapped to "
				"the expected clock class: "
				"stream-class-addr=%p, "
				"stream-class-name=\"%s\", "
				"stream-class-id=%" PRId64,
				stream_class,
				bt_stream_class_common_get_name(stream_class),
				stream_class->id);
			goto end;
		}
	}

end:
	return ret;
}
