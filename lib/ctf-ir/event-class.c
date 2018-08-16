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
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <stdlib.h>

static inline
void bt_event_class_finalize(struct bt_object *obj)
{
	struct bt_event_class *event_class;

	event_class = container_of(obj, struct bt_event_class, base);
	BT_LOGD("Finalizing event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));

	if (event_class->name) {
		g_string_free(event_class->name, TRUE);
	}

	if (event_class->emf_uri) {
		g_string_free(event_class->emf_uri, TRUE);
	}

	BT_LOGD_STR("Putting context field type.");
	bt_put(event_class->context_field_type);
	BT_LOGD_STR("Putting payload field type.");
	bt_put(event_class->payload_field_type);
}

static inline
int bt_event_class_initialize(struct bt_event_class *event_class,
		const char *name, bt_object_release_func release_func,
		bt_field_type_structure_create_func ft_struct_create_func)
{
	int ret = 0;

	BT_LOGD("Initializing event class object: name=\"%s\"",
		name);
	bt_object_init_shared_with_parent(&event_class->base, release_func);
	event_class->payload_field_type = ft_struct_create_func();
	if (!event_class->payload_field_type) {
		BT_LOGE_STR("Cannot create event class's initial payload field type object.");
		goto error;
	}

	event_class->id = -1;
	event_class->name = g_string_new(name);
	if (!event_class->name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	event_class->emf_uri = g_string_new(NULL);
	if (!event_class->emf_uri) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	event_class->log_level = BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED;
	BT_LOGD("Initialized event class object: addr=%p, name=\"%s\"",
		event_class, bt_event_class_get_name(event_class));
	return ret;

error:
	ret = -1;
	return ret;
}

static
void bt_event_class_destroy(struct bt_object *obj)
{
	struct bt_event_class *event_class = (void *) obj;

	BT_LOGD("Destroying event class: addr=%p", obj);
	bt_event_class_finalize(obj);
	bt_object_pool_finalize(&event_class->event_pool);
	g_free(obj);
}

static
void free_event(struct bt_event *event,
		struct bt_event_class *event_class)
{
	bt_event_destroy(event);
}

struct bt_event_class *bt_event_class_create(const char *name)
{
	int ret;
	struct bt_event_class *event_class = NULL;

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto error;
	}

	BT_LOGD("Creating event class object: name=\"%s\"",
		name);
	event_class = g_new0(struct bt_event_class, 1);
	if (!event_class) {
		BT_LOGE_STR("Failed to allocate one event class.");
		goto error;
	}

	ret = bt_event_class_initialize(event_class,
		name, bt_event_class_destroy,
		(bt_field_type_structure_create_func)
			bt_field_type_structure_create);
	if (ret) {
		goto error;
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

	BT_LOGD("Created event class object: addr=%p, name=\"%s\"",
		event_class, bt_event_class_get_name(event_class));
	goto end;

error:
	bt_put(event_class);

end:
	return event_class;
}

const char *bt_event_class_get_name(struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT(event_class->name);
	return event_class->name->str;
}

int64_t bt_event_class_get_id(struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->id;
}

int bt_event_class_set_id(struct bt_event_class *event_class,
		uint64_t id_param)
{
	int ret = 0;
	int64_t id = (int64_t) id_param;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class,
			bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid event class's ID: "
			"addr=%p, name=\"%s\", id=%" PRIu64,
			event_class,
			bt_event_class_get_name(event_class),
			id_param);
		ret = -1;
		goto end;
	}

	event_class->id = id;
	BT_LOGV("Set event class's ID: "
		"addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_event_class_get_name(event_class), id);

end:
	return ret;
}

enum bt_event_class_log_level bt_event_class_get_log_level(
		struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->log_level;
}

int bt_event_class_set_log_level(struct bt_event_class *event_class,
		enum bt_event_class_log_level log_level)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class,
			bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	switch (log_level) {
	case BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED:
	case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
	case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
	case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
	case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
	case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
	case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
	case BT_EVENT_CLASS_LOG_LEVEL_INFO:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
		break;
	default:
		BT_LOGW("Invalid parameter: unknown event class log level: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", log-level=%d",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class), log_level);
		ret = -1;
		goto end;
	}

	event_class->log_level = log_level;
	BT_LOGV("Set event class's log level: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", log-level=%s",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class),
		bt_common_event_class_log_level_string(log_level));

end:
	return ret;
}

const char *bt_event_class_get_emf_uri(struct bt_event_class *event_class)
{
	const char *emf_uri = NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");

	if (event_class->emf_uri->len > 0) {
		emf_uri = event_class->emf_uri->str;
	}

	return emf_uri;
}

int bt_event_class_set_emf_uri(struct bt_event_class *event_class,
		const char *emf_uri)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (emf_uri && strlen(emf_uri) == 0) {
		BT_LOGW_STR("Invalid parameter: EMF URI is empty.");
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (emf_uri) {
		g_string_assign(event_class->emf_uri, emf_uri);
		BT_LOGV("Set event class's EMF URI: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", emf-uri=\"%s\"",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class), emf_uri);
	} else {
		g_string_assign(event_class->emf_uri, "");
		BT_LOGV("Reset event class's EMF URI: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
	}

end:
	return ret;
}

struct bt_stream_class *bt_event_class_borrow_stream_class(
		struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return (void *) bt_object_borrow_parent(&event_class->base);
}

struct bt_field_type *bt_event_class_borrow_payload_field_type(
		struct bt_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->payload_field_type;
}

int bt_event_class_set_payload_field_type(struct bt_event_class *event_class,
		struct bt_field_type *field_type)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (field_type && bt_field_type_get_type_id(field_type) !=
			BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's payload field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"payload-ft-addr=%p, payload-ft-id=%s",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class), field_type,
			bt_common_field_type_id_string(
				bt_field_type_get_type_id(field_type)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->payload_field_type);
	event_class->payload_field_type = bt_get(field_type);
	BT_LOGV("Set event class's payload field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", payload-ft-addr=%p",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), field_type);
end:
	return ret;
}

struct bt_field_type *bt_event_class_borrow_context_field_type(
		struct bt_event_class *event_class)
{
	struct bt_field_type *context_ft = NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");

	if (!event_class->context_field_type) {
		BT_LOGV("Event class has no context field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		goto end;
	}

	context_ft = event_class->context_field_type;

end:
	return context_ft;
}

int bt_event_class_set_context_field_type(
		struct bt_event_class *event_class,
		struct bt_field_type *field_type)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (field_type && bt_field_type_get_type_id(field_type) !=
			BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"context-ft-id=%s",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class),
			bt_common_field_type_id_string(
				bt_field_type_get_type_id(field_type)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->context_field_type);
	event_class->context_field_type = bt_get(field_type);
	BT_LOGV("Set event class's context field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", context-ft-addr=%p",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), field_type);

end:
	return ret;
}

BT_HIDDEN
void bt_event_class_freeze(struct bt_event_class *event_class)
{
	BT_ASSERT(event_class);

	if (event_class->frozen) {
		return;
	}

	BT_LOGD("Freezing event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));
	event_class->frozen = 1;
	BT_LOGD_STR("Freezing event class's context field type.");
	bt_field_type_freeze(event_class->context_field_type);
	BT_LOGD_STR("Freezing event class's payload field type.");
	bt_field_type_freeze(event_class->payload_field_type);
}

BT_HIDDEN
int bt_event_class_validate_single_clock_class(
		struct bt_event_class *event_class,
		struct bt_clock_class **expected_clock_class)
{
	int ret = 0;

	BT_ASSERT(event_class);
	BT_ASSERT(expected_clock_class);
	ret = bt_field_type_validate_single_clock_class(
		event_class->context_field_type,
		expected_clock_class);
	if (ret) {
		BT_LOGW("Event class's context field type "
			"is not recursively mapped to the "
			"expected clock class: "
			"event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", "
			"ft-addr=%p",
			event_class,
			bt_event_class_get_name(event_class),
			event_class->id,
			event_class->context_field_type);
		goto end;
	}

	ret = bt_field_type_validate_single_clock_class(
		event_class->payload_field_type,
		expected_clock_class);
	if (ret) {
		BT_LOGW("Event class's payload field type "
			"is not recursively mapped to the "
			"expected clock class: "
			"event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", "
			"ft-addr=%p",
			event_class,
			bt_event_class_get_name(event_class),
			event_class->id,
			event_class->payload_field_type);
		goto end;
	}

end:
	return ret;
}
