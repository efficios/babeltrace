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

static
void bt_ctf_event_class_destroy(struct bt_object *obj);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	int ret;
	struct bt_value *obj = NULL;
	struct bt_ctf_event_class *event_class = NULL;

	BT_LOGD("Creating event class object: name=\"%s\"",
		name);

	if (bt_ctf_validate_identifier(name)) {
		BT_LOGW("Invalid parameter: event class's name is not a valid CTF identifier: "
			"name=\"%s\"", name);
		goto error;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		BT_LOGE_STR("Failed to allocate one event class.");
		goto error;
	}

	event_class->id = -1;
	bt_object_init(event_class, bt_ctf_event_class_destroy);
	event_class->fields = bt_ctf_field_type_structure_create();
	if (!event_class->fields) {
		BT_LOGE_STR("Cannot create event class's initial payload field type object.");
		goto error;
	}

	event_class->context = bt_ctf_field_type_structure_create();
	if (!event_class->context) {
		BT_LOGE_STR("Cannot create event class's initial context field type object.");
		goto error;
	}

	event_class->attributes = bt_ctf_attributes_create();
	if (!event_class->attributes) {
		BT_LOGE_STR("Cannot create event class's attributes object.");
		goto error;
	}

	obj = bt_value_integer_create_init(-1);
	if (!obj) {
		BT_LOGE_STR("Cannot create integer value object.");
		goto error;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"id", obj);
	if (ret) {
		BT_LOGE("Cannot set event class's attributes's `id` field: ret=%d",
			ret);
		goto error;
	}

	BT_PUT(obj);

	obj = bt_value_string_create_init(name);
	if (!obj) {
		BT_LOGE_STR("Cannot create string value object.");
		goto error;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"name", obj);
	if (ret) {
		BT_LOGE("Cannot set event class's attributes's `name` field: ret=%d",
			ret);
		goto error;
	}

	BT_PUT(obj);
	BT_LOGD("Created event class object: addr=%p, name=\"%s\"",
		event_class, bt_ctf_event_class_get_name(event_class));
	return event_class;

error:
        BT_PUT(event_class);
	BT_PUT(obj);
	return event_class;
}

const char *bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class)
{
	struct bt_value *obj = NULL;
	const char *name = NULL;
	int ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	if (event_class->name) {
		name = event_class->name;
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_NAME_INDEX);
	assert(obj);
	ret = bt_value_string_get(obj, &name);
	assert(ret == 0);

end:
	BT_PUT(obj);
	return name;
}

int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class)
{
	struct bt_value *obj = NULL;
	int64_t ret = 0;
	int int_ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (event_class->id >= 0) {
		ret = (int64_t) event_class->id;
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_ID_INDEX);
	assert(obj);
	int_ret = bt_value_integer_get(obj, &ret);
	assert(int_ret == 0);
	if (ret < 0) {
		/* means ID is not set */
		BT_LOGV("Event class's ID is not set: addr=%p, name=\"%s\"",
			event_class, bt_ctf_event_class_get_name(event_class));
		ret = (int64_t) -1;
		goto end;
	}

end:
	BT_PUT(obj);
	return ret;
}

int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint64_t id_param)
{
	int ret = 0;
	struct bt_value *obj = NULL;
	int64_t id = (int64_t) id_param;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid event class's ID: "
			"addr=%p, name=\"%s\", id=%" PRIu64,
			event_class, bt_ctf_event_class_get_name(event_class),
			id_param);
		ret = -1;
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_ID_INDEX);
	assert(obj);
	ret = bt_value_integer_set(obj, id);
	assert(ret == 0);
	BT_LOGV("Set event class's ID: "
		"addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_ctf_event_class_get_name(event_class), id);

end:
	BT_PUT(obj);
	return ret;
}

int bt_ctf_event_class_set_attribute(
		struct bt_ctf_event_class *event_class, const char *name,
		struct bt_value *value)
{
	int ret = 0;

	if (!event_class || !name || !value) {
		BT_LOGW("Invalid parameter: event class, name, or value is NULL: "
			"event-class-addr=%p, name-addr=%p, value-addr=%p",
			event_class, name, value);
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", attr-name=\"%s\"",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), name);
		ret = -1;
		goto end;
	}

	if (!strcmp(name, "id") || !strcmp(name, "loglevel") ||
			!strcmp(name, "stream_id")) {
		if (!bt_value_is_integer(value)) {
			BT_LOGW("Invalid parameter: this event class's attribute must have an integer value: "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", attr-name=\"%s\", "
				"attr-type=%s", event_class,
				bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class), name,
				bt_value_type_string(bt_value_get_type(value)));
			ret = -1;
			goto end;
		}
	} else if (!strcmp(name, "name") || !strcmp(name, "model.emf.uri") ||
			!strcmp(name, "loglevel_string")) {
		if (!bt_value_is_string(value)) {
			BT_LOGW("Invalid parameter: this event class's attribute must have a string value: "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", attr-name=\"%s\", "
				"attr-type=%s", event_class,
				bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class), name,
				bt_value_type_string(bt_value_get_type(value)));
			ret = -1;
			goto end;
		}
	} else {
		/* unknown attribute */
		BT_LOGW("Invalid parameter: unknown event class's attribute name: "
			"event-class-addr=%p, event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", attr-name=\"%s\"",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), name);
		ret = -1;
		goto end;
	}

	/* "id" special case: >= 0 */
	if (!strcmp(name, "id")) {
		int64_t val;

		ret = bt_value_integer_get(value, &val);
		assert(ret == 0);
		BT_LOGV("Setting event's ID: id=%" PRId64, val);
		ret = bt_ctf_event_class_set_id(event_class, (uint64_t) val);
		if (ret) {
			goto end;
		}
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
			name, value);
	assert(ret == 0);

	if (BT_LOG_ON_VERBOSE) {
		if (bt_value_is_integer(value)) {
			int64_t val;

			ret = bt_value_integer_get(value, &val);
			assert(ret == 0);
			BT_LOGV("Set event class's integer attribute: "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", attr-name=\"%s\", "
				"attr-value=%" PRId64,
				event_class, bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class), name,
				val);
		} else if (bt_value_is_string(value)) {
			const char *val;

			ret = bt_value_string_get(value, &val);
			assert(ret == 0);
			BT_LOGV("Set event class's string attribute: "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", attr-name=\"%s\", "
				"attr-value=\"%s\"",
				event_class, bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class), name,
				val);
		}
	}

end:
	return ret;
}

int64_t bt_ctf_event_class_get_attribute_count(
		struct bt_ctf_event_class *event_class)
{
	int64_t ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	ret = bt_ctf_attributes_get_count(event_class->attributes);
	assert(ret >= 0);

end:
	return ret;
}

const char *
bt_ctf_event_class_get_attribute_name_by_index(
		struct bt_ctf_event_class *event_class, uint64_t index)
{
	const char *ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_name(event_class->attributes, index);
	if (!ret) {
		BT_LOGW("Cannot get event class's attribute name by index: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", index=%" PRIu64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), index);
	}

end:
	return ret;
}

struct bt_value *
bt_ctf_event_class_get_attribute_value_by_index(
		struct bt_ctf_event_class *event_class, uint64_t index)
{
	struct bt_value *ret;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value(event_class->attributes, index);
	if (!ret) {
		BT_LOGW("Cannot get event class's attribute value by index: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", index=%" PRIu64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), index);
	}

end:
	return ret;
}

struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name)
{
	struct bt_value *ret;

	if (!event_class || !name) {
		BT_LOGW("Invalid parameter: event class or name is NULL: "
			"event-class-addr=%p, name-addr=%p",
			event_class, name);
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value_by_name(event_class->attributes,
		name);
	if (!ret) {
		BT_LOGV("Cannot find event class's attribute: "
			"addr=%p, event-class-name=\"%s\", id=%" PRId64 ", "
			"attr-name=\"%s\"",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), name);
	}

end:
	return ret;

}

struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class)
{
	return event_class ?
		bt_get(bt_ctf_event_class_borrow_stream_class(event_class)) :
		NULL;
}

struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_field_type *payload = NULL;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	bt_get(event_class->fields);
	payload = event_class->fields;
end:
	return payload;
}

int bt_ctf_event_class_set_payload_type(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload)
{
	int ret = 0;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		ret = -1;
		goto end;
	}

	if (payload && bt_ctf_field_type_get_type_id(payload) !=
			BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's payload field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"payload-ft-addr=%p, payload-ft-id=%s",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), payload,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_get_type_id(payload)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->fields);
	event_class->fields = bt_get(payload);
	BT_LOGV("Set event class's payload field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", payload-ft-addr=%p",
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), payload);
end:
	return ret;
}

int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
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

	if (bt_ctf_validate_identifier(name)) {
		BT_LOGW("Invalid parameter: event class's payload field type's field name is not a valid CTF identifier: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", field-name=\"%s\"",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class),
			name);
		ret = -1;
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (!event_class->fields) {
		BT_LOGW("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	assert(bt_ctf_field_type_get_type_id(event_class->fields) ==
		BT_CTF_FIELD_TYPE_ID_STRUCT);
	ret = bt_ctf_field_type_structure_add_field(event_class->fields,
		type, name);
	BT_LOGV("Added field to event class's payload field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", field-name=\"%s\", ft-addr=%p",
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), name, type);
end:
	return ret;
}

int64_t bt_ctf_event_class_get_payload_type_field_count(
		struct bt_ctf_event_class *event_class)
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
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = (int64_t) -1;
		goto end;
	}

	assert(bt_ctf_field_type_get_type_id(event_class->fields) ==
		BT_CTF_FIELD_TYPE_ID_STRUCT);
	ret = bt_ctf_field_type_structure_get_field_count(event_class->fields);
end:
	return ret;
}

int bt_ctf_event_class_get_payload_type_field_by_index(
		struct bt_ctf_event_class *event_class,
		const char **field_name, struct bt_ctf_field_type **field_type,
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
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), index);
		ret = -1;
		goto end;
	}

	assert(bt_ctf_field_type_get_type_id(event_class->fields) ==
		BT_CTF_FIELD_TYPE_ID_STRUCT);
	ret = bt_ctf_field_type_structure_get_field(event_class->fields,
		field_name, field_type, index);
end:
	return ret;
}

struct bt_ctf_field_type *
bt_ctf_event_class_get_payload_type_field_type_by_name(
		struct bt_ctf_event_class *event_class, const char *name)
{
	GQuark name_quark;
	struct bt_ctf_field_type *field_type = NULL;

	if (!event_class || !name) {
		BT_LOGW("Invalid parameter: event class or name is NULL: "
			"event-class-addr=%p, name-addr=%p",
			event_class, name);
		goto end;
	}

	if (!event_class->fields) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto end;
	}

	assert(bt_ctf_field_type_get_type_id(event_class->fields) ==
		BT_CTF_FIELD_TYPE_ID_STRUCT);
	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGE("Cannot get GQuark: string=\"%s\"", name);
		goto end;
	}

	/*
	 * No need to increment field_type's reference count since getting it
	 * from the structure already does.
	 */
	field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		event_class->fields, name);
end:
	return field_type;
}

struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_field_type *context_type = NULL;

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto end;
	}

	if (!event_class->context) {
		BT_LOGV("Event class has no context field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto end;
	}

	bt_get(event_class->context);
	context_type = event_class->context;
end:
	return context_type;
}

int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context)
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
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (context && bt_ctf_field_type_get_type_id(context) !=
			BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: event class's context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"context-ft-id=%s",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class),
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_get_type_id(context)));
		ret = -1;
		goto end;
	}

	bt_put(event_class->context);
	event_class->context = bt_get(context);
	BT_LOGV("Set event class's context field type: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", context-ft-addr=%p",
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), context);
end:
	return ret;

}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class)
{
	bt_get(event_class);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class)
{
	bt_put(event_class);
}

BT_HIDDEN
int bt_ctf_event_class_set_stream_id(struct bt_ctf_event_class *event_class,
		uint64_t stream_id_param)
{
	int ret = 0;
	struct bt_value *obj = NULL;
	int64_t stream_id = (int64_t) stream_id_param;

	assert(event_class);
	assert(stream_id >= 0);
	obj = bt_value_integer_create_init(stream_id);
	if (!obj) {
		BT_LOGE_STR("Cannot create integer value object.");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"stream_id", obj);
	if (ret) {
		BT_LOGE("Cannot set event class's attributes's `stream_id` field: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", ret=%d",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), ret);
		goto end;
	}

	if (event_class->frozen) {
		BT_LOGV_STR("Freezing event class's attributes because event class is frozen.");
		bt_ctf_attributes_freeze(event_class->attributes);
	}

	BT_LOGV("Set event class's stream class ID: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", stream-class-id=%" PRId64,
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), stream_id);

end:
	BT_PUT(obj);
	return ret;
}

static
void bt_ctf_event_class_destroy(struct bt_object *obj)
{
	struct bt_ctf_event_class *event_class;

	event_class = container_of(obj, struct bt_ctf_event_class, base);
	BT_LOGD("Destroying event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class));
	BT_LOGD_STR("Destroying event class's attributes.");
	bt_ctf_attributes_destroy(event_class->attributes);
	BT_LOGD_STR("Putting context field type.");
	bt_put(event_class->context);
	BT_LOGD_STR("Putting payload field type.");
	bt_put(event_class->fields);
	g_free(event_class);
}

BT_HIDDEN
void bt_ctf_event_class_freeze(struct bt_ctf_event_class *event_class)
{
	assert(event_class);

	if (event_class->frozen) {
		return;
	}

	BT_LOGD("Freezing event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class));
	event_class->frozen = 1;
	event_class->name = bt_ctf_event_class_get_name(event_class);
	event_class->id = bt_ctf_event_class_get_id(event_class);
	BT_LOGD_STR("Freezing event class's context field type.");
	bt_ctf_field_type_freeze(event_class->context);
	BT_LOGD_STR("Freezing event class's payload field type.");
	bt_ctf_field_type_freeze(event_class->fields);
	BT_LOGD_STR("Freezing event class's attributes.");
	bt_ctf_attributes_freeze(event_class->attributes);
}

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context)
{
	int64_t i;
	int64_t count;
	int ret = 0;
	struct bt_value *attr_value = NULL;

	assert(event_class);
	assert(context);
	BT_LOGD("Serializing event class's metadata: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", metadata-context-addr=%p",
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), context);
	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append(context->string, "event {\n");
	count = bt_ctf_event_class_get_attribute_count(event_class);
	assert(count >= 0);

	for (i = 0; i < count; ++i) {
		const char *attr_name = NULL;

		attr_name = bt_ctf_event_class_get_attribute_name_by_index(
			event_class, i);
		assert(attr_name);
		attr_value = bt_ctf_event_class_get_attribute_value_by_index(
			event_class, i);
		assert(attr_value);

		switch (bt_value_get_type(attr_value)) {
		case BT_VALUE_TYPE_INTEGER:
		{
			int64_t value;

			ret = bt_value_integer_get(attr_value, &value);
			assert(ret == 0);
			g_string_append_printf(context->string,
				"\t%s = %" PRId64 ";\n", attr_name, value);
			break;
		}

		case BT_VALUE_TYPE_STRING:
		{
			const char *value;

			ret = bt_value_string_get(attr_value, &value);
			assert(ret == 0);
			g_string_append_printf(context->string,
				"\t%s = \"%s\";\n", attr_name, value);
			break;
		}

		default:
			/* should never happen */
			assert(BT_FALSE);
			break;
		}

		BT_PUT(attr_value);
	}

	if (event_class->context) {
		g_string_append(context->string, "\tcontext := ");
		BT_LOGD_STR("Serializing event class's context field type metadata.");
		ret = bt_ctf_field_type_serialize(event_class->context,
			context);
		if (ret) {
			BT_LOGW("Cannot serialize event class's context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	if (event_class->fields) {
		g_string_append(context->string, "\tfields := ");
		BT_LOGD_STR("Serializing event class's payload field type metadata.");
		ret = bt_ctf_field_type_serialize(event_class->fields, context);
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
