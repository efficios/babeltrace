#ifndef BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event class internal
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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <glib.h>

struct bt_event_class_common {
	struct bt_object base;
	struct bt_field_type_common *context_field_type;
	struct bt_field_type_common *payload_field_type;
	int frozen;

	/*
	 * This flag indicates if the event class is valid. A valid
	 * event class is _always_ frozen. However, an event class
	 * may be frozen, but not valid yet. This is okay, as long as
	 * no events are created out of this event class.
	 */
	int valid;

	/* Attributes */
	GString *name;
	int64_t id;
	int log_level;
	GString *emf_uri;
};

struct bt_event_class {
	struct bt_event_class_common common;

	/* Pool of `struct bt_event *` */
	struct bt_object_pool event_pool;
};

BT_HIDDEN
void bt_event_class_freeze(struct bt_event_class *event_class);

BT_HIDDEN
void bt_event_class_common_freeze(struct bt_event_class_common *event_class);

BT_HIDDEN
void bt_event_class_common_set_native_byte_order(
		struct bt_event_class_common *event_class, int byte_order);

static inline
struct bt_stream_class_common *bt_event_class_common_borrow_stream_class(
		struct bt_event_class_common *event_class)
{
	BT_ASSERT(event_class);
	return (void *) bt_object_borrow_parent(event_class);
}

typedef struct bt_field_type_common *(*bt_field_type_structure_create_func)();

BT_HIDDEN
int bt_event_class_common_initialize(struct bt_event_class_common *event_class,
		const char *name, bt_object_release_func release_func,
		bt_field_type_structure_create_func ft_struct_create_func);

BT_HIDDEN
void bt_event_class_common_finalize(struct bt_object *obj);

BT_HIDDEN
int bt_event_class_common_validate_single_clock_class(
		struct bt_event_class_common *event_class,
		struct bt_clock_class **expected_clock_class);

BT_HIDDEN
int bt_event_class_update_event_pool_clock_values(
		struct bt_event_class *event_class);

static inline
const char *bt_event_class_common_get_name(
		struct bt_event_class_common *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT(event_class->name);
	return event_class->name->str;
}

static inline
int64_t bt_event_class_common_get_id(
		struct bt_event_class_common *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->id;
}

static inline
int bt_event_class_common_set_id(
		struct bt_event_class_common *event_class, uint64_t id_param)
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
			bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid event class's ID: "
			"addr=%p, name=\"%s\", id=%" PRIu64,
			event_class,
			bt_event_class_common_get_name(event_class),
			id_param);
		ret = -1;
		goto end;
	}

	event_class->id = id;
	BT_LOGV("Set event class's ID: "
		"addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_event_class_common_get_name(event_class), id);

end:
	return ret;
}

static inline
int bt_event_class_common_get_log_level(
		struct bt_event_class_common *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->log_level;
}

static inline
int bt_event_class_common_set_log_level(
		struct bt_event_class_common *event_class, int log_level)
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
			bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class));
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
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class), log_level);
		ret = -1;
		goto end;
	}

	event_class->log_level = log_level;
	BT_LOGV("Set event class's log level: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", log-level=%s",
		event_class, bt_event_class_common_get_name(event_class),
		bt_event_class_common_get_id(event_class),
		bt_common_event_class_log_level_string(log_level));

end:
	return ret;
}

static inline
const char *bt_event_class_common_get_emf_uri(
		struct bt_event_class_common *event_class)
{
	const char *emf_uri = NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");

	if (event_class->emf_uri->len > 0) {
		emf_uri = event_class->emf_uri->str;
	}

	return emf_uri;
}

static inline
int bt_event_class_common_set_emf_uri(
		struct bt_event_class_common *event_class,
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
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (emf_uri) {
		g_string_assign(event_class->emf_uri, emf_uri);
		BT_LOGV("Set event class's EMF URI: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", emf-uri=\"%s\"",
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class), emf_uri);
	} else {
		g_string_assign(event_class->emf_uri, "");
		BT_LOGV("Reset event class's EMF URI: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class));
	}

end:
	return ret;
}

static inline
struct bt_field_type_common *bt_event_class_common_borrow_context_field_type(
		struct bt_event_class_common *event_class)
{
	struct bt_field_type_common *context_ft = NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");

	if (!event_class->context_field_type) {
		BT_LOGV("Event class has no context field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class));
		goto end;
	}

	context_ft = event_class->context_field_type;

end:
	return context_ft;
}

static inline
int bt_event_class_common_set_context_field_type(
		struct bt_event_class_common *event_class,
		struct bt_field_type_common *context_ft)
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
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (context_ft && bt_field_type_common_get_type_id(context_ft) !=
			BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"context-ft-id=%s",
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class),
			bt_common_field_type_id_string(
				bt_field_type_common_get_type_id(context_ft)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->context_field_type);
	event_class->context_field_type = bt_get(context_ft);
	BT_LOGV("Set event class's context field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", context-ft-addr=%p",
		event_class, bt_event_class_common_get_name(event_class),
		bt_event_class_common_get_id(event_class), context_ft);

end:
	return ret;
}

static inline
struct bt_field_type_common *bt_event_class_common_borrow_payload_field_type(
		struct bt_event_class_common *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return event_class->payload_field_type;
}

static inline
int bt_event_class_common_set_payload_field_type(
		struct bt_event_class_common *event_class,
		struct bt_field_type_common *payload_ft)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (payload_ft && bt_field_type_common_get_type_id(payload_ft) !=
			BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's payload field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"payload-ft-addr=%p, payload-ft-id=%s",
			event_class, bt_event_class_common_get_name(event_class),
			bt_event_class_common_get_id(event_class), payload_ft,
			bt_common_field_type_id_string(
				bt_field_type_common_get_type_id(payload_ft)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->payload_field_type);
	event_class->payload_field_type = bt_get(payload_ft);
	BT_LOGV("Set event class's payload field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", payload-ft-addr=%p",
		event_class, bt_event_class_common_get_name(event_class),
		bt_event_class_common_get_id(event_class), payload_ft);
end:
	return ret;
}

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H */
