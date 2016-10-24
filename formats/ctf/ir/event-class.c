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
#include <babeltrace/compiler.h>
#include <babeltrace/endian.h>

static
void bt_ctf_event_class_destroy(struct bt_object *obj);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	int ret;
	struct bt_value *obj = NULL;
	struct bt_ctf_event_class *event_class = NULL;

	if (bt_ctf_validate_identifier(name)) {
		goto error;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		goto error;
	}

	bt_object_init(event_class, bt_ctf_event_class_destroy);
	event_class->fields = bt_ctf_field_type_structure_create();
	if (!event_class->fields) {
		goto error;
	}

	event_class->attributes = bt_ctf_attributes_create();
	if (!event_class->attributes) {
		goto error;
	}

	obj = bt_value_integer_create_init(-1);
	if (!obj) {
		goto error;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"id", obj);
	if (ret) {
		goto error;
	}

	BT_PUT(obj);

	obj = bt_value_string_create_init(name);
	if (!obj) {
		goto error;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"name", obj);
	if (ret) {
		goto error;
	}

	BT_PUT(obj);

	return event_class;

error:
        BT_PUT(event_class);
	BT_PUT(obj);
	return event_class;
}

BT_HIDDEN
const char *bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class)
{
	struct bt_value *obj = NULL;
	const char *name = NULL;

	if (!event_class) {
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_NAME_INDEX);
	if (!obj) {
		goto end;
	}

	if (bt_value_string_get(obj, &name)) {
		name = NULL;
	}

end:
	BT_PUT(obj);
	return name;
}

BT_HIDDEN
int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class)
{
	struct bt_value *obj = NULL;
	int64_t ret = 0;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_ID_INDEX);
	if (!obj) {
		goto end;
	}

	if (bt_value_integer_get(obj, &ret)) {
		ret = -1;
	}

	if (ret < 0) {
		/* means ID is not set */
		ret = -1;
		goto end;
	}

end:
	BT_PUT(obj);
	return ret;
}

BT_HIDDEN
int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint32_t id)
{
	int ret = 0;
	struct bt_value *obj = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;


	if (!event_class) {
		ret = -1;
		goto end;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (stream_class) {
		/*
		 * We don't allow changing the id if the event class has already
		 * been added to a stream class.
		 */
		ret = -1;
		goto end;
	}

	obj = bt_ctf_attributes_get_field_value(event_class->attributes,
		BT_CTF_EVENT_CLASS_ATTR_ID_INDEX);
	if (!obj) {
		goto end;
	}

	if (bt_value_integer_set(obj, id)) {
		ret = -1;
		goto end;
	}

end:
	BT_PUT(obj);
	BT_PUT(stream_class);
	return ret;
}

int bt_ctf_event_class_set_attribute(
		struct bt_ctf_event_class *event_class, const char *name,
		struct bt_value *value)
{
	int ret = 0;

	if (!event_class || !name || !value || event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (!strcmp(name, "id") || !strcmp(name, "loglevel")) {
		if (!bt_value_is_integer(value)) {
			ret = -1;
			goto end;
		}
	} else if (!strcmp(name, "name") || !strcmp(name, "model.emf.uri")) {
		if (!bt_value_is_string(value)) {
			ret = -1;
			goto end;
		}
	} else {
		/* unknown attribute */
		ret = -1;
		goto end;
	}

	/* "id" special case: >= 0 */
	if (!strcmp(name, "id")) {
		int64_t val;

		ret = bt_value_integer_get(value, &val);

		if (ret) {
			goto end;
		}

		if (val < 0) {
			ret = -1;
			goto end;
		}
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		name, value);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_class_get_attribute_count(
		struct bt_ctf_event_class *event_class)
{
	int ret = 0;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_attributes_get_count(event_class->attributes);

end:
	return ret;
}

BT_HIDDEN
const char *
bt_ctf_event_class_get_attribute_name(
		struct bt_ctf_event_class *event_class, int index)
{
	const char *ret;

	if (!event_class) {
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_name(event_class->attributes, index);

end:
	return ret;
}

BT_HIDDEN
struct bt_value *
bt_ctf_event_class_get_attribute_value(struct bt_ctf_event_class *event_class,
		int index)
{
	struct bt_value *ret;

	if (!event_class) {
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value(event_class->attributes, index);

end:
	return ret;
}

BT_HIDDEN
struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name)
{
	struct bt_value *ret;

	if (!event_class || !name) {
		ret = NULL;
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value_by_name(event_class->attributes,
		name);

end:
	return ret;

}

BT_HIDDEN
struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class)
{
	return (struct bt_ctf_stream_class *) bt_object_get_parent(event_class);
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_field_type *payload = NULL;

	if (!event_class) {
		goto end;
	}

	bt_get(event_class->fields);
	payload = event_class->fields;
end:
	return payload;
}

BT_HIDDEN
int bt_ctf_event_class_set_payload_type(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload)
{
	int ret = 0;

	if (!event_class || !payload ||
		bt_ctf_field_type_get_type_id(payload) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	bt_get(payload);
	bt_put(event_class->fields);
	event_class->fields = payload;
end:
	return ret;
}

int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name)
{
	int ret = 0;

	if (!event_class || !type || bt_ctf_validate_identifier(name) ||
		event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_class->fields,
		type, name);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_class_get_field_count(
		struct bt_ctf_event_class *event_class)
{
	int ret;

	if (!event_class) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_get_field_count(event_class->fields);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_class_get_field(struct bt_ctf_event_class *event_class,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index)
{
	int ret;

	if (!event_class || index < 0) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_get_field(event_class->fields,
		field_name, field_type, index);
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_event_class_get_field_by_name(
		struct bt_ctf_event_class *event_class, const char *name)
{
	GQuark name_quark;
	struct bt_ctf_field_type *field_type = NULL;

	if (!event_class || !name) {
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(event_class->fields) !=
		CTF_TYPE_STRUCT) {
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
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

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_field_type *context_type = NULL;

	if (!event_class || !event_class->context) {
		goto end;
	}

	bt_get(event_class->context);
	context_type = event_class->context;
end:
	return context_type;
}

BT_HIDDEN
int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context)
{
	int ret = 0;

	if (!event_class || !context || event_class->frozen) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(context) != CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	bt_get(context);
	bt_put(event_class->context);
	event_class->context = context;
end:
	return ret;

}

void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class)
{
	bt_get(event_class);
}

void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class)
{
	bt_put(event_class);
}

BT_HIDDEN
int bt_ctf_event_class_set_stream_id(struct bt_ctf_event_class *event_class,
		uint32_t stream_id)
{
	int ret = 0;
	struct bt_value *obj;

	obj = bt_value_integer_create_init(stream_id);

	if (!obj) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_attributes_set_field_value(event_class->attributes,
		"stream_id", obj);

	if (event_class->frozen) {
		bt_ctf_attributes_freeze(event_class->attributes);
	}

end:
	BT_PUT(obj);
	return ret;
}

static
void bt_ctf_event_class_destroy(struct bt_object *obj)
{
	struct bt_ctf_event_class *event_class;

	event_class = container_of(obj, struct bt_ctf_event_class, base);
	bt_ctf_attributes_destroy(event_class->attributes);
	bt_put(event_class->context);
	bt_put(event_class->fields);
	g_free(event_class);
}

BT_HIDDEN
void bt_ctf_event_class_freeze(struct bt_ctf_event_class *event_class)
{
	assert(event_class);
	event_class->frozen = 1;
	bt_ctf_field_type_freeze(event_class->context);
	bt_ctf_field_type_freeze(event_class->fields);
	bt_ctf_attributes_freeze(event_class->attributes);
}

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context)
{
	int i;
	int count;
	int ret = 0;
	struct bt_value *attr_value = NULL;

	assert(event_class);
	assert(context);

	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append(context->string, "event {\n");
	count = bt_ctf_event_class_get_attribute_count(event_class);

	if (count < 0) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < count; ++i) {
		const char *attr_name = NULL;

		attr_name = bt_ctf_event_class_get_attribute_name(
			event_class, i);
		attr_value = bt_ctf_event_class_get_attribute_value(
			event_class, i);

		if (!attr_name || !attr_value) {
			ret = -1;
			goto end;
		}

		switch (bt_value_get_type(attr_value)) {
		case BT_VALUE_TYPE_INTEGER:
		{
			int64_t value;

			ret = bt_value_integer_get(attr_value, &value);

			if (ret) {
				goto end;
			}

			g_string_append_printf(context->string,
				"\t%s = %" PRId64 ";\n", attr_name, value);
			break;
		}

		case BT_VALUE_TYPE_STRING:
		{
			const char *value;

			ret = bt_value_string_get(attr_value, &value);

			if (ret) {
				goto end;
			}

			g_string_append_printf(context->string,
				"\t%s = \"%s\";\n", attr_name, value);
			break;
		}

		default:
			/* should never happen */
			assert(false);
			break;
		}

		BT_PUT(attr_value);
	}

	if (event_class->context) {
		g_string_append(context->string, "\tcontext := ");
		ret = bt_ctf_field_type_serialize(event_class->context,
			context);
		if (ret) {
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	if (event_class->fields) {
		g_string_append(context->string, "\tfields := ");
		ret = bt_ctf_field_type_serialize(event_class->fields, context);
		if (ret) {
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

void bt_ctf_event_class_set_native_byte_order(
		struct bt_ctf_event_class *event_class,
		int byte_order)
{
	if (!event_class) {
		return;
	}

	assert(byte_order == 0 || byte_order == LITTLE_ENDIAN ||
		byte_order == BIG_ENDIAN);

	bt_ctf_field_type_set_native_byte_order(event_class->context,
		byte_order);
	bt_ctf_field_type_set_native_byte_order(event_class->fields,
		byte_order);
}
