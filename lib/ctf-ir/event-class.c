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

#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/values-internal.h>
#include <inttypes.h>
#include <stdlib.h>

static
void bt_event_class_destroy(struct bt_object *obj);

struct bt_event_class *bt_event_class_create(const char *name)
{
	struct bt_value *obj = NULL;
	struct bt_event_class *event_class = NULL;

	BT_LOGD("Creating event class object: name=\"%s\"",
		name);

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto error;
	}

	event_class = g_new0(struct bt_event_class, 1);
	if (!event_class) {
		BT_LOGE_STR("Failed to allocate one event class.");
		goto error;
	}

	bt_object_init(event_class, bt_event_class_destroy);
	event_class->fields = bt_field_type_structure_create();
	if (!event_class->fields) {
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
	BT_PUT(obj);
	BT_LOGD("Created event class object: addr=%p, name=\"%s\"",
		event_class, bt_event_class_get_name(event_class));
	return event_class;

error:
        BT_PUT(event_class);
	BT_PUT(obj);
	return event_class;
}

const char *bt_event_class_get_name(struct bt_event_class *event_class)
{
	const char *name = NULL;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	name = event_class->name->str;

end:
	return name;
}

int64_t bt_event_class_get_id(struct bt_event_class *event_class)
{
	int64_t ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	ret = event_class->id;

end:
	return ret;
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
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid event class's ID: "
			"addr=%p, name=\"%s\", id=%" PRIu64,
			event_class, bt_event_class_get_name(event_class),
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
	enum bt_event_class_log_level log_level;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		log_level = BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN;
		goto end;
	}

	log_level = event_class->log_level;

end:
	return log_level;
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
			event_class, bt_event_class_get_name(event_class),
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
		bt_event_class_log_level_string(log_level));

end:
	return ret;
}

const char *bt_event_class_get_emf_uri(
		struct bt_event_class *event_class)
{
	const char *emf_uri = NULL;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	if (event_class->emf_uri->len > 0) {
		emf_uri = event_class->emf_uri->str;
	}

end:
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

struct bt_stream_class *bt_event_class_get_stream_class(
		struct bt_event_class *event_class)
{
	return event_class ?
		bt_get(bt_event_class_borrow_stream_class(event_class)) :
		NULL;
}

struct bt_field_type *bt_event_class_get_payload_type(
		struct bt_event_class *event_class)
{
	struct bt_field_type *payload = NULL;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	bt_get(event_class->fields);
	payload = event_class->fields;
end:
	return payload;
}

int bt_event_class_set_payload_type(struct bt_event_class *event_class,
		struct bt_field_type *payload)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (payload && bt_field_type_get_type_id(payload) !=
			BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's payload field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"payload-ft-addr=%p, payload-ft-id=%s",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class), payload,
			bt_field_type_id_string(
				bt_field_type_get_type_id(payload)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->fields);
	event_class->fields = bt_get(payload);
	BT_LOGV("Set event class's payload field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", payload-ft-addr=%p",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), payload);
end:
	return ret;
}

int bt_event_class_add_field(struct bt_event_class *event_class,
		struct bt_field_type *type,
		const char *name)
{
	int ret = 0;

	if (!event_class || !type) {
		BT_LOGW("Invalid parameter: event class or field type is NULL: "
			"event-class-addr=%p, field-type-addr=%p",
			event_class, type);
		ret = -1;
		goto end;
	}

	if (bt_identifier_is_valid(name)) {
		BT_LOGW("Invalid parameter: event class's payload field type's field name is not a valid CTF identifier: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", field-name=\"%s\"",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class),
			name);
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

	if (!event_class->fields) {
		BT_LOGW("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	assert(bt_field_type_get_type_id(event_class->fields) ==
		BT_FIELD_TYPE_ID_STRUCT);
	ret = bt_field_type_structure_add_field(event_class->fields,
		type, name);
	BT_LOGV("Added field to event class's payload field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", field-name=\"%s\", ft-addr=%p",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), name, type);
end:
	return ret;
}

int64_t bt_event_class_get_payload_type_field_count(
		struct bt_event_class *event_class)
{
	int64_t ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (!event_class->fields) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		ret = (int64_t) -1;
		goto end;
	}

	assert(bt_field_type_get_type_id(event_class->fields) ==
		BT_FIELD_TYPE_ID_STRUCT);
	ret = bt_field_type_structure_get_field_count(event_class->fields);
end:
	return ret;
}

int bt_event_class_get_payload_type_field_by_index(
		struct bt_event_class *event_class,
		const char **field_name, struct bt_field_type **field_type,
		uint64_t index)
{
	int ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (!event_class->fields) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", index=%" PRIu64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class), index);
		ret = -1;
		goto end;
	}

	assert(bt_field_type_get_type_id(event_class->fields) ==
		BT_FIELD_TYPE_ID_STRUCT);
	ret = bt_field_type_structure_get_field_by_index(event_class->fields,
		field_name, field_type, index);
end:
	return ret;
}

struct bt_field_type *
bt_event_class_get_payload_type_field_type_by_name(
		struct bt_event_class *event_class, const char *name)
{
	GQuark name_quark;
	struct bt_field_type *field_type = NULL;

	if (!event_class || !name) {
		BT_LOGW("Invalid parameter: event class or name is NULL: "
			"event-class-addr=%p, name-addr=%p",
			event_class, name);
		goto end;
	}

	if (!event_class->fields) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		goto end;
	}

	assert(bt_field_type_get_type_id(event_class->fields) ==
		BT_FIELD_TYPE_ID_STRUCT);
	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGE("Cannot get GQuark: string=\"%s\"", name);
		goto end;
	}

	/*
	 * No need to increment field_type's reference count since getting it
	 * from the structure already does.
	 */
	field_type = bt_field_type_structure_get_field_type_by_name(
		event_class->fields, name);
end:
	return field_type;
}

struct bt_field_type *bt_event_class_get_context_type(
		struct bt_event_class *event_class)
{
	struct bt_field_type *context_type = NULL;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	if (!event_class->context) {
		BT_LOGV("Event class has no context field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class));
		goto end;
	}

	bt_get(event_class->context);
	context_type = event_class->context;
end:
	return context_type;
}

int bt_event_class_set_context_type(
		struct bt_event_class *event_class,
		struct bt_field_type *context)
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

	if (context && bt_field_type_get_type_id(context) !=
			BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"context-ft-id=%s",
			event_class, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class),
			bt_field_type_id_string(
				bt_field_type_get_type_id(context)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->context);
	event_class->context = bt_get(context);
	BT_LOGV("Set event class's context field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", context-ft-addr=%p",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), context);
end:
	return ret;

}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_event_class_get(struct bt_event_class *event_class)
{
	bt_get(event_class);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_event_class_put(struct bt_event_class *event_class)
{
	bt_put(event_class);
}

static
void bt_event_class_destroy(struct bt_object *obj)
{
	struct bt_event_class *event_class;

	event_class = container_of(obj, struct bt_event_class, base);
	BT_LOGD("Destroying event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));
	g_string_free(event_class->name, TRUE);
	g_string_free(event_class->emf_uri, TRUE);
	BT_LOGD_STR("Putting context field type.");
	bt_put(event_class->context);
	BT_LOGD_STR("Putting payload field type.");
	bt_put(event_class->fields);
	g_free(event_class);
}

BT_HIDDEN
void bt_event_class_freeze(struct bt_event_class *event_class)
{
	assert(event_class);

	if (event_class->frozen) {
		return;
	}

	BT_LOGD("Freezing event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));
	event_class->frozen = 1;
	BT_LOGD_STR("Freezing event class's context field type.");
	bt_field_type_freeze(event_class->context);
	BT_LOGD_STR("Freezing event class's payload field type.");
	bt_field_type_freeze(event_class->fields);
}

BT_HIDDEN
int bt_event_class_serialize(struct bt_event_class *event_class,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_value *attr_value = NULL;

	assert(event_class);
	assert(context);
	BT_LOGD("Serializing event class's metadata: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", metadata-context-addr=%p",
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), context);
	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append(context->string, "event {\n");

	/* Serialize attributes */
	g_string_append_printf(context->string, "\tname = \"%s\";\n",
		event_class->name->str);
	assert(event_class->id >= 0);
	g_string_append_printf(context->string, "\tid = %" PRId64 ";\n",
		event_class->id);
	g_string_append_printf(context->string, "\tstream_id = %" PRId64 ";\n",
		bt_stream_class_get_id(
			bt_event_class_borrow_stream_class(event_class)));

	if (event_class->log_level != BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED) {
		g_string_append_printf(context->string, "\tloglevel = %d;\n",
			(int) event_class->log_level);
	}

	if (event_class->emf_uri->len > 0) {
		g_string_append_printf(context->string, "\tmodel.emf.uri = \"%s\";\n",
			event_class->emf_uri->str);
	}

	/* Serialize context field type */
	if (event_class->context) {
		g_string_append(context->string, "\tcontext := ");
		BT_LOGD_STR("Serializing event class's context field type metadata.");
		ret = bt_field_type_serialize(event_class->context,
			context);
		if (ret) {
			BT_LOGW("Cannot serialize event class's context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	/* Serialize payload field type */
	if (event_class->fields) {
		g_string_append(context->string, "\tfields := ");
		BT_LOGD_STR("Serializing event class's payload field type metadata.");
		ret = bt_field_type_serialize(event_class->fields, context);
		if (ret) {
			BT_LOGW("Cannot serialize event class's payload field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	g_string_append(context->string, "};\n\n");
end:
	context->current_indentation_level = 0;
	BT_PUT(attr_value);
	return ret;
}
