/*
 * stream-class.c
 *
 * Babeltrace trace IR - Stream Class
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
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/trace-ir/field-types-internal.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/trace-ir/resolve-field-path-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/property-internal.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

#define BT_ASSERT_PRE_STREAM_CLASS_HOT(_sc) \
	BT_ASSERT_PRE_HOT((_sc), "Stream class", ": %!+S", (_sc))

static
void destroy_stream_class(struct bt_object *obj)
{
	struct bt_stream_class *stream_class = (void *) obj;

	BT_LIB_LOGD("Destroying stream class: %!+S", stream_class);
	BT_LOGD_STR("Putting default clock class.");
	bt_put(stream_class->default_clock_class);

	if (stream_class->event_classes) {
		BT_LOGD_STR("Destroying event classes.");
		g_ptr_array_free(stream_class->event_classes, TRUE);
	}

	if (stream_class->name.str) {
		g_string_free(stream_class->name.str, TRUE);
	}

	BT_LOGD_STR("Putting event header field type.");
	bt_put(stream_class->event_header_ft);
	BT_LOGD_STR("Putting packet context field type.");
	bt_put(stream_class->packet_context_ft);
	BT_LOGD_STR("Putting event common context field type.");
	bt_put(stream_class->event_common_context_ft);
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

BT_ASSERT_PRE_FUNC
static
bool stream_class_id_is_unique(struct bt_trace *trace, uint64_t id)
{
	uint64_t i;
	bool is_unique = true;

	for (i = 0; i < trace->stream_classes->len; i++) {
		struct bt_stream_class *sc =
			trace->stream_classes->pdata[i];

		if (sc->id == id) {
			is_unique = false;
			goto end;
		}
	}

end:
	return is_unique;
}

static
struct bt_stream_class *create_stream_class_with_id(struct bt_trace *trace,
		uint64_t id)
{
	struct bt_stream_class *stream_class = NULL;
	int ret;

	BT_ASSERT(trace);
	BT_ASSERT_PRE(stream_class_id_is_unique(trace, id),
		"Duplicate stream class ID: %![trace-]+t, id=%" PRIu64,
		trace, id);
	BT_LIB_LOGD("Creating stream class object: %![trace-]+t, id=%" PRIu64,
		trace, id);
	stream_class = g_new0(struct bt_stream_class, 1);
	if (!stream_class) {
		BT_LOGE_STR("Failed to allocate one stream class.");
		goto error;
	}

	bt_object_init_shared_with_parent(&stream_class->base,
		destroy_stream_class);

	stream_class->name.str = g_string_new(NULL);
	if (!stream_class->name.str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	stream_class->id = id;
	stream_class->assigns_automatic_event_class_id = true;
	stream_class->assigns_automatic_stream_id = true;
	stream_class->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!stream_class->event_classes) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
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

	bt_object_set_parent(&stream_class->base, &trace->base);
	g_ptr_array_add(trace->stream_classes, stream_class);
	bt_trace_freeze(trace);
	BT_LIB_LOGD("Created stream class object: %!+S", stream_class);
	goto end;

error:
	BT_PUT(stream_class);

end:
	return stream_class;
}

struct bt_stream_class *bt_stream_class_create(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(trace->assigns_automatic_stream_class_id,
		"Trace does not automatically assigns stream class IDs: "
		"%![sc-]+t", trace);
	return create_stream_class_with_id(trace,
		(uint64_t) trace->stream_classes->len);
}

struct bt_stream_class *bt_stream_class_create_with_id(
		struct bt_trace *trace, uint64_t id)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(!trace->assigns_automatic_stream_class_id,
		"Trace automatically assigns stream class IDs: "
		"%![sc-]+t", trace);
	return create_stream_class_with_id(trace, id);
}

struct bt_trace *bt_stream_class_borrow_trace(struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return bt_stream_class_borrow_trace_inline(stream_class);
}

const char *bt_stream_class_get_name(struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->name.value;
}

int bt_stream_class_set_name(struct bt_stream_class *stream_class,
		const char *name)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	g_string_assign(stream_class->name.str, name);
	stream_class->name.value = stream_class->name.str->str;
	BT_LIB_LOGV("Set stream class's name: %!+S", stream_class);
	return 0;
}

uint64_t bt_stream_class_get_id(struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->id;
}

uint64_t bt_stream_class_get_event_class_count(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (uint64_t) stream_class->event_classes->len;
}

struct bt_event_class *bt_stream_class_borrow_event_class_by_index(
		struct bt_stream_class *stream_class, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_VALID_INDEX(index, stream_class->event_classes->len);
	return g_ptr_array_index(stream_class->event_classes, index);
}

struct bt_event_class *bt_stream_class_borrow_event_class_by_id(
		struct bt_stream_class *trace, uint64_t id)
{
	struct bt_event_class *event_class = NULL;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");

	for (i = 0; i < trace->event_classes->len; i++) {
		struct bt_event_class *event_class_candidate =
			g_ptr_array_index(trace->event_classes, i);

		if (event_class_candidate->id == id) {
			event_class = event_class_candidate;
			goto end;
		}
	}

end:
	return event_class;
}

struct bt_field_type *bt_stream_class_borrow_packet_context_field_type(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->packet_context_ft;
}

int bt_stream_class_set_packet_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *field_type)
{
	int ret;
	struct bt_resolve_field_path_context resolve_ctx = {
		.packet_header = NULL,
		.packet_context = field_type,
		.event_header = NULL,
		.event_common_context = NULL,
		.event_specific_context = NULL,
		.event_payload = NULL,
	};

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(field_type, "Field type");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	BT_ASSERT_PRE(bt_field_type_get_type_id(field_type) ==
		BT_FIELD_TYPE_ID_STRUCTURE,
		"Packet context field type is not a structure field type: %!+F",
		field_type);
	resolve_ctx.packet_header =
		bt_stream_class_borrow_trace_inline(stream_class)->packet_header_ft;
	ret = bt_resolve_field_paths(field_type, &resolve_ctx);
	if (ret) {
		goto end;
	}

	bt_field_type_make_part_of_trace(field_type);
	bt_put(stream_class->packet_context_ft);
	stream_class->packet_context_ft = bt_get(field_type);
	bt_field_type_freeze(field_type);
	BT_LIB_LOGV("Set stream class's packet context field type: %!+S",
		stream_class);

end:
	return ret;
}

struct bt_field_type *bt_stream_class_borrow_event_header_field_type(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->event_header_ft;
}

int bt_stream_class_set_event_header_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *field_type)
{
	int ret;
	struct bt_resolve_field_path_context resolve_ctx = {
		.packet_header = NULL,
		.packet_context = NULL,
		.event_header = field_type,
		.event_common_context = NULL,
		.event_specific_context = NULL,
		.event_payload = NULL,
	};

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(field_type, "Field type");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	BT_ASSERT_PRE(bt_field_type_get_type_id(field_type) ==
		BT_FIELD_TYPE_ID_STRUCTURE,
		"Event header field type is not a structure field type: %!+F",
		field_type);
	resolve_ctx.packet_header =
		bt_stream_class_borrow_trace_inline(stream_class)->packet_header_ft;
	resolve_ctx.packet_context = stream_class->packet_context_ft;
	ret = bt_resolve_field_paths(field_type, &resolve_ctx);
	if (ret) {
		goto end;
	}

	bt_field_type_make_part_of_trace(field_type);
	bt_put(stream_class->event_header_ft);
	stream_class->event_header_ft = bt_get(field_type);
	bt_field_type_freeze(field_type);
	BT_LIB_LOGV("Set stream class's event header field type: %!+S",
		stream_class);

end:
	return ret;
}

struct bt_field_type *bt_stream_class_borrow_event_common_context_field_type(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->event_common_context_ft;
}

int bt_stream_class_set_event_common_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *field_type)
{
	int ret;
	struct bt_resolve_field_path_context resolve_ctx = {
		.packet_header = NULL,
		.packet_context = NULL,
		.event_header = NULL,
		.event_common_context = field_type,
		.event_specific_context = NULL,
		.event_payload = NULL,
	};

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(field_type, "Field type");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	BT_ASSERT_PRE(bt_field_type_get_type_id(field_type) ==
		BT_FIELD_TYPE_ID_STRUCTURE,
		"Event common context field type is not a structure field type: %!+F",
		field_type);
	resolve_ctx.packet_header =
		bt_stream_class_borrow_trace_inline(stream_class)->packet_header_ft;
	resolve_ctx.packet_context = stream_class->packet_context_ft;
	resolve_ctx.event_header = stream_class->event_header_ft;
	ret = bt_resolve_field_paths(field_type, &resolve_ctx);
	if (ret) {
		goto end;
	}

	bt_field_type_make_part_of_trace(field_type);
	bt_put(stream_class->event_common_context_ft);
	stream_class->event_common_context_ft = bt_get(field_type);
	bt_field_type_freeze(field_type);
	BT_LIB_LOGV("Set stream class's event common context field type: %!+S",
		stream_class);

end:
	return ret;
}

BT_HIDDEN
void _bt_stream_class_freeze(struct bt_stream_class *stream_class)
{
	/* The field types and default clock class are already frozen */
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Freezing stream class: %!+S", stream_class);
	stream_class->frozen = true;
}

int bt_stream_class_set_default_clock_class(
		struct bt_stream_class *stream_class,
		struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	bt_put(stream_class->default_clock_class);
	stream_class->default_clock_class = bt_get(clock_class);
	bt_clock_class_freeze(clock_class);
	BT_LIB_LOGV("Set stream class's default clock class: %!+S",
		stream_class);
	return 0;
}

struct bt_clock_class *bt_stream_class_borrow_default_clock_class(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->default_clock_class;
}

bt_bool bt_stream_class_assigns_automatic_event_class_id(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (bt_bool) stream_class->assigns_automatic_event_class_id;
}

int bt_stream_class_set_assigns_automatic_event_class_id(
		struct bt_stream_class *stream_class, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	stream_class->assigns_automatic_event_class_id = (bool) value;
	BT_LIB_LOGV("Set stream class's automatic event class ID "
		"assignment property: %!+S", stream_class);
	return 0;
}

bt_bool bt_stream_class_assigns_automatic_stream_id(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (bt_bool) stream_class->assigns_automatic_stream_id;
}

int bt_stream_class_set_assigns_automatic_stream_id(
		struct bt_stream_class *stream_class, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	stream_class->assigns_automatic_stream_id = (bool) value;
	BT_LIB_LOGV("Set stream class's automatic stream ID "
		"assignment property: %!+S", stream_class);
	return 0;
}

bt_bool bt_stream_class_packets_have_discarded_event_counter_snapshot(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (bt_bool) stream_class->packets_have_discarded_event_counter_snapshot;
}

int bt_stream_class_set_packets_have_discarded_event_counter_snapshot(
		struct bt_stream_class *stream_class, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	stream_class->packets_have_discarded_event_counter_snapshot =
		(bool) value;
	BT_LIB_LOGV("Set stream class's "
		"\"packets have discarded event counter snapshot\" property: "
		"%!+S", stream_class);
	return 0;
}

bt_bool bt_stream_class_packets_have_packet_counter_snapshot(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (bt_bool) stream_class->packets_have_packet_counter_snapshot;
}

int bt_stream_class_set_packets_have_packet_counter_snapshot(
		struct bt_stream_class *stream_class, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	stream_class->packets_have_packet_counter_snapshot =
		(bool) value;
	BT_LIB_LOGV("Set stream class's "
		"\"packets have packet counter snapshot\" property: "
		"%!+S", stream_class);
	return 0;
}

bt_bool bt_stream_class_packets_have_default_beginning_clock_value(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (bt_bool) stream_class->packets_have_default_beginning_cv;
}

int bt_stream_class_set_packets_have_default_beginning_clock_value(
		struct bt_stream_class *stream_class, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	BT_ASSERT_PRE(!value || stream_class->default_clock_class,
		"Stream class does not have a default clock class: %!+S",
		stream_class);
	stream_class->packets_have_default_beginning_cv = (bool) value;
	BT_LIB_LOGV("Set stream class's "
		"\"packets have default beginning clock value\" property: "
		"%!+S", stream_class);
	return 0;
}

bt_bool bt_stream_class_packets_have_default_end_clock_value(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return (bt_bool) stream_class->packets_have_default_end_cv;
}

int bt_stream_class_set_packets_have_default_end_clock_value(
		struct bt_stream_class *stream_class, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE_STREAM_CLASS_HOT(stream_class);
	BT_ASSERT_PRE(!value || stream_class->default_clock_class,
		"Stream class does not have a default clock class: %!+S",
		stream_class);
	stream_class->packets_have_default_end_cv = (bool) value;
	BT_LIB_LOGV("Set stream class's "
		"\"packets have default end clock value\" property: "
		"%!+S", stream_class);
	return 0;
}

bt_bool bt_stream_class_default_clock_is_always_known(
		struct bt_stream_class *stream_class)
{
	/* BT_CLOCK_VALUE_STATUS_UNKNOWN is not supported as of 2.0 */
	return BT_TRUE;
}
