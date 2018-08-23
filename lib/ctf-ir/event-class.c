/*
 * event-class.c
 *
 * Babeltrace CTF IR - Event class
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

#define BT_LOG_TAG "EVENT-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/clock-value-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ctf-ir/resolve-field-path-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <stdlib.h>

#define BT_ASSERT_PRE_EVENT_CLASS_HOT(_ec) \
	BT_ASSERT_PRE_HOT((_ec), "Event class", ": %!+E", (_ec))

static
void destroy_event_class(struct bt_object *obj)
{
	struct bt_event_class *event_class = (void *) obj;

	BT_LIB_LOGD("Destroying event class: %!+E", event_class);

	if (event_class->name.str) {
		g_string_free(event_class->name.str, TRUE);
	}

	if (event_class->emf_uri.str) {
		g_string_free(event_class->emf_uri.str, TRUE);
	}

	BT_LOGD_STR("Putting context field type.");
	bt_put(event_class->specific_context_ft);
	BT_LOGD_STR("Putting payload field type.");
	bt_put(event_class->payload_ft);
	bt_object_pool_finalize(&event_class->event_pool);
	g_free(obj);
}

static
void free_event(struct bt_event *event,
		struct bt_event_class *event_class)
{
	bt_event_destroy(event);
}

BT_ASSERT_PRE_FUNC
static
bool event_class_id_is_unique(struct bt_stream_class *stream_class, uint64_t id)
{
	uint64_t i;
	bool is_unique = true;

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_event_class *ec =
			stream_class->event_classes->pdata[i];

		if (ec->id == id) {
			is_unique = false;
			goto end;
		}
	}

end:
	return is_unique;
}

static
struct bt_event_class *create_event_class_with_id(
		struct bt_stream_class *stream_class, uint64_t id)
{
	int ret;
	struct bt_event_class *event_class;

	BT_ASSERT(stream_class);
	BT_ASSERT_PRE(event_class_id_is_unique(stream_class, id),
		"Duplicate event class ID: %![sc-]+S, id=%" PRIu64,
		stream_class, id);
	BT_LIB_LOGD("Creating event class object: %![sc-]+S, id=%" PRIu64,
		stream_class, id);
	event_class = g_new0(struct bt_event_class, 1);
	if (!event_class) {
		BT_LOGE_STR("Failed to allocate one event class.");
		goto error;
	}

	bt_object_init_shared_with_parent(&event_class->base,
		destroy_event_class);
	event_class->id = id;
	bt_property_uint_init(&event_class->log_level,
			BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE, 0);
	event_class->name.str = g_string_new(NULL);
	if (!event_class->name.str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	event_class->emf_uri.str = g_string_new(NULL);
	if (!event_class->emf_uri.str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	ret = bt_object_pool_initialize(&event_class->event_pool,
		(bt_object_pool_new_object_func) bt_event_new,
		(bt_object_pool_destroy_object_func) free_event,
		event_class);
	if (ret) {
		BT_LOGE("Failed to initialize event pool: ret=%d",
			ret);
		goto error;
	}

	bt_object_set_parent(&event_class->base, &stream_class->base);
	g_ptr_array_add(stream_class->event_classes, event_class);
	bt_stream_class_freeze(stream_class);
	BT_LIB_LOGD("Created event class object: %!+E", event_class);
	goto end;

error:
	BT_PUT(event_class);

end:
	return event_class;
}

struct bt_event_class *bt_event_class_create(
		struct bt_stream_class *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(stream_class->assigns_automatic_event_class_id,
		"Stream class does not automatically assigns event class IDs: "
		"%![sc-]+S", stream_class);
	return create_event_class_with_id(stream_class,
		(uint64_t) stream_class->event_classes->len);
}

struct bt_event_class *bt_event_class_create_with_id(
		struct bt_stream_class *stream_class, uint64_t id)
{
	BT_ASSERT_PRE(!stream_class->assigns_automatic_event_class_id,
		"Stream class automatically assigns event class IDs: "
		"%![sc-]+S", stream_class);
	return create_event_class_with_id(stream_class, id);
}

const char *bt_event_class_get_name(struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->name.value;
}

int bt_event_class_set_name(struct bt_event_class *event_class,
		const char *name)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_EVENT_CLASS_HOT(event_class);
	g_string_assign(event_class->name.str, name);
	event_class->name.value = event_class->name.str->str;
	BT_LIB_LOGV("Set event class's name: %!+E", event_class);
	return 0;
}

uint64_t bt_event_class_get_id(struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->id;
}

enum bt_property_availability bt_event_class_get_log_level(
		struct bt_event_class *event_class,
		enum bt_event_class_log_level *log_level)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(log_level, "Log level (output)");
	*log_level = (enum bt_event_class_log_level)
		event_class->log_level.value;
	return event_class->log_level.base.avail;
}

int bt_event_class_set_log_level(struct bt_event_class *event_class,
		enum bt_event_class_log_level log_level)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_EVENT_CLASS_HOT(event_class);
	bt_property_uint_set(&event_class->log_level,
		(uint64_t) log_level);
	BT_LIB_LOGV("Set event class's log level: %!+E", event_class);
	return 0;
}

const char *bt_event_class_get_emf_uri(struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->emf_uri.value;
}

int bt_event_class_set_emf_uri(struct bt_event_class *event_class,
		const char *emf_uri)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(emf_uri, "EMF URI");
	BT_ASSERT_PRE_EVENT_CLASS_HOT(event_class);
	g_string_assign(event_class->emf_uri.str, emf_uri);
	event_class->emf_uri.value = event_class->emf_uri.str->str;
	BT_LIB_LOGV("Set event class's EMF URI: %!+E", event_class);
	return 0;
}

struct bt_stream_class *bt_event_class_borrow_stream_class(
		struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return bt_event_class_borrow_stream_class_inline(event_class);
}

struct bt_field_type *bt_event_class_borrow_specific_context_field_type(
		struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->specific_context_ft;
}

int bt_event_class_set_specific_context_field_type(
		struct bt_event_class *event_class,
		struct bt_field_type *field_type)
{
	int ret;
	struct bt_stream_class *stream_class;
	struct bt_trace *trace;
	struct bt_resolve_field_path_context resolve_ctx = {
		.packet_header = NULL,
		.packet_context = NULL,
		.event_header = NULL,
		.event_common_context = NULL,
		.event_specific_context = field_type,
		.event_payload = NULL,
	};

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(field_type, "Field type");
	BT_ASSERT_PRE_EVENT_CLASS_HOT(event_class);
	BT_ASSERT_PRE(bt_field_type_get_type_id(field_type) ==
		BT_FIELD_TYPE_ID_STRUCTURE,
		"Specific context field type is not a structure field type: "
		"%!+F", field_type);
	stream_class = bt_event_class_borrow_stream_class_inline(
		event_class);
	trace = bt_stream_class_borrow_trace_inline(stream_class);
	resolve_ctx.packet_header = trace->packet_header_ft;
	resolve_ctx.packet_context = stream_class->packet_context_ft;
	resolve_ctx.event_header = stream_class->event_header_ft;
	resolve_ctx.event_common_context =
		stream_class->event_common_context_ft;

	ret = bt_resolve_field_paths(field_type, &resolve_ctx);
	if (ret) {
		goto end;
	}

	bt_field_type_make_part_of_trace(field_type);
	bt_put(event_class->specific_context_ft);
	event_class->specific_context_ft = bt_get(field_type);
	bt_field_type_freeze(field_type);
	BT_LIB_LOGV("Set event class's specific context field type: %!+E",
		event_class);

end:
	return ret;
}

struct bt_field_type *bt_event_class_borrow_payload_field_type(
		struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->payload_ft;
}

int bt_event_class_set_payload_field_type(struct bt_event_class *event_class,
		struct bt_field_type *field_type)
{
	int ret;
	struct bt_stream_class *stream_class;
	struct bt_trace *trace;
	struct bt_resolve_field_path_context resolve_ctx = {
		.packet_header = NULL,
		.packet_context = NULL,
		.event_header = NULL,
		.event_common_context = NULL,
		.event_specific_context = NULL,
		.event_payload = field_type,
	};

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(field_type, "Field type");
	BT_ASSERT_PRE_EVENT_CLASS_HOT(event_class);
	BT_ASSERT_PRE(bt_field_type_get_type_id(field_type) ==
		BT_FIELD_TYPE_ID_STRUCTURE,
		"Payload field type is not a structure field type: %!+F",
		field_type);
	stream_class = bt_event_class_borrow_stream_class_inline(
		event_class);
	trace = bt_stream_class_borrow_trace_inline(stream_class);
	resolve_ctx.packet_header = trace->packet_header_ft;
	resolve_ctx.packet_context = stream_class->packet_context_ft;
	resolve_ctx.event_header = stream_class->event_header_ft;
	resolve_ctx.event_common_context =
		stream_class->event_common_context_ft;
	resolve_ctx.event_specific_context = event_class->specific_context_ft;

	ret = bt_resolve_field_paths(field_type, &resolve_ctx);
	if (ret) {
		goto end;
	}

	bt_field_type_make_part_of_trace(field_type);
	bt_put(event_class->payload_ft);
	event_class->payload_ft = bt_get(field_type);
	bt_field_type_freeze(field_type);
	BT_LIB_LOGV("Set event class's payload field type: %!+E", event_class);

end:
	return ret;
}

BT_HIDDEN
void _bt_event_class_freeze(struct bt_event_class *event_class)
{
	/* The field types are already frozen */
	BT_ASSERT(event_class);
	BT_LIB_LOGD("Freezing event class: %!+E", event_class);
	event_class->frozen = true;
}
