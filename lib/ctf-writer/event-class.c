/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER-EVENT-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-writer/attributes-internal.h>
#include <babeltrace/ctf-writer/event-class-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/fields-internal.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/trace-internal.h>
#include <babeltrace/ctf-writer/utils-internal.h>
#include <babeltrace/ctf-writer/utils.h>
#include <babeltrace/ctf-writer/validation-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/types.h>
#include <babeltrace/values-internal.h>
#include <glib.h>
#include <inttypes.h>
#include <inttypes.h>
#include <stdlib.h>

BT_HIDDEN
void bt_ctf_event_class_common_finalize(struct bt_object *obj)
{
	struct bt_ctf_event_class_common *event_class;

	event_class = container_of(obj, struct bt_ctf_event_class_common, base);
	BT_LOGD("Finalizing common event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_ctf_event_class_common_get_name(event_class),
		bt_ctf_event_class_common_get_id(event_class));

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

BT_HIDDEN
int bt_ctf_event_class_common_initialize(struct bt_ctf_event_class_common *event_class,
		const char *name, bt_object_release_func release_func,
		bt_ctf_field_type_structure_create_func ft_struct_create_func)
{
	int ret = 0;

	BT_LOGD("Initializing common event class object: name=\"%s\"",
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

	event_class->log_level = BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED;
	BT_LOGD("Initialized common event class object: addr=%p, name=\"%s\"",
		event_class, bt_ctf_event_class_common_get_name(event_class));
	return ret;

error:
	ret = -1;
	return ret;
}

BT_HIDDEN
void bt_ctf_event_class_common_freeze(struct bt_ctf_event_class_common *event_class)
{
	BT_ASSERT(event_class);

	if (event_class->frozen) {
		return;
	}

	BT_LOGD("Freezing event class: addr=%p, name=\"%s\", id=%" PRId64,
		event_class, bt_ctf_event_class_common_get_name(event_class),
		bt_ctf_event_class_common_get_id(event_class));
	event_class->frozen = 1;
	BT_LOGD_STR("Freezing event class's context field type.");
	bt_ctf_field_type_common_freeze(event_class->context_field_type);
	BT_LOGD_STR("Freezing event class's payload field type.");
	bt_ctf_field_type_common_freeze(event_class->payload_field_type);
}

BT_HIDDEN
int bt_ctf_event_class_common_validate_single_clock_class(
		struct bt_ctf_event_class_common *event_class,
		struct bt_ctf_clock_class **expected_clock_class)
{
	int ret = 0;

	BT_ASSERT(event_class);
	BT_ASSERT(expected_clock_class);
	ret = bt_ctf_field_type_common_validate_single_clock_class(
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
			bt_ctf_event_class_common_get_name(event_class),
			event_class->id,
			event_class->context_field_type);
		goto end;
	}

	ret = bt_ctf_field_type_common_validate_single_clock_class(
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
			bt_ctf_event_class_common_get_name(event_class),
			event_class->id,
			event_class->payload_field_type);
		goto end;
	}

end:
	return ret;
}

static
void bt_ctf_event_class_destroy(struct bt_object *obj)
{
	bt_ctf_event_class_common_finalize(obj);
	g_free(obj);
}

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	struct bt_ctf_event_class *ctf_event_class = NULL;
	int ret;

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto error;
	}

	BT_LOGD("Creating event class object: name=\"%s\"",
		name);
	ctf_event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!ctf_event_class) {
		BT_LOGE_STR("Failed to allocate one event class.");
		goto error;
	}

	ret = bt_ctf_event_class_common_initialize(BT_CTF_TO_COMMON(ctf_event_class),
		name, bt_ctf_event_class_destroy,
		(bt_ctf_field_type_structure_create_func)
			bt_ctf_field_type_structure_create);
	if (ret) {
		goto error;
	}

	goto end;

error:
	bt_put(ctf_event_class);

end:
	return ctf_event_class;
}

const char *bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class)
{
	return bt_ctf_event_class_common_get_name(BT_CTF_TO_COMMON(event_class));
}

int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class)
{
	return bt_ctf_event_class_common_get_id(BT_CTF_TO_COMMON(event_class));
}

int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint64_t id)
{
	return bt_ctf_event_class_common_set_id(BT_CTF_TO_COMMON(event_class), id);
}

enum bt_ctf_event_class_log_level bt_ctf_event_class_get_log_level(
		struct bt_ctf_event_class *event_class)
{
	return bt_ctf_event_class_common_get_log_level(BT_CTF_TO_COMMON(event_class));
}

int bt_ctf_event_class_set_log_level(struct bt_ctf_event_class *event_class,
		enum bt_ctf_event_class_log_level log_level)
{
	return bt_ctf_event_class_common_set_log_level(BT_CTF_TO_COMMON(event_class),
		log_level);
}

const char *bt_ctf_event_class_get_emf_uri(
		struct bt_ctf_event_class *event_class)
{
	return bt_ctf_event_class_common_get_emf_uri(BT_CTF_TO_COMMON(event_class));
}

int bt_ctf_event_class_set_emf_uri(struct bt_ctf_event_class *event_class,
		const char *emf_uri)
{
	return bt_ctf_event_class_common_set_emf_uri(BT_CTF_TO_COMMON(event_class),
		emf_uri);
}

struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class)
{
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	return bt_get(bt_ctf_event_class_common_borrow_stream_class(
		BT_CTF_TO_COMMON(event_class)));
}

struct bt_ctf_field_type *bt_ctf_event_class_get_payload_field_type(
		struct bt_ctf_event_class *event_class)
{
	return bt_get(bt_ctf_event_class_common_borrow_payload_field_type(
		BT_CTF_TO_COMMON(event_class)));
}

int bt_ctf_event_class_set_payload_field_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *field_type)
{
	return bt_ctf_event_class_common_set_payload_field_type(
		BT_CTF_TO_COMMON(event_class), (void *) field_type);
}

struct bt_ctf_field_type *bt_ctf_event_class_get_context_field_type(
		struct bt_ctf_event_class *event_class)
{
	return bt_get(bt_ctf_event_class_common_borrow_context_field_type(
		BT_CTF_TO_COMMON(event_class)));
}

int bt_ctf_event_class_set_context_field_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *field_type)
{
	return bt_ctf_event_class_common_set_context_field_type(
		BT_CTF_TO_COMMON(event_class), (void *) field_type);
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

	if (!bt_ctf_identifier_is_valid(name)) {
		BT_LOGW("Invalid parameter: event class's payload field type's field name is not a valid CTF identifier: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", field-name=\"%s\"",
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class),
			name);
		ret = -1;
		goto end;
	}

	if (event_class->common.frozen) {
		BT_LOGW("Invalid parameter: event class is frozen: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	if (!event_class->common.payload_field_type) {
		BT_LOGW("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = -1;
		goto end;
	}

	BT_ASSERT(bt_ctf_field_type_common_get_type_id(
		event_class->common.payload_field_type) ==
		BT_CTF_FIELD_TYPE_ID_STRUCT);
	ret = bt_ctf_field_type_structure_add_field(
		(void *) event_class->common.payload_field_type,
		(void *) type, name);
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

	if (!event_class->common.payload_field_type) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		ret = (int64_t) -1;
		goto end;
	}

	BT_ASSERT(bt_ctf_field_type_common_get_type_id(
		event_class->common.payload_field_type) ==
			BT_CTF_FIELD_TYPE_ID_STRUCT);
	ret = bt_ctf_field_type_common_structure_get_field_count(
		event_class->common.payload_field_type);
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

	if (!event_class->common.payload_field_type) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", index=%" PRIu64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class), index);
		ret = -1;
		goto end;
	}

	BT_ASSERT(bt_ctf_field_type_common_get_type_id(
		event_class->common.payload_field_type) ==
			BT_CTF_FIELD_TYPE_ID_STRUCT);
	ret = bt_ctf_field_type_structure_get_field_by_index(
		(void *) event_class->common.payload_field_type,
		field_name, (void *) field_type, index);

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

	if (!event_class->common.payload_field_type) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto end;
	}

	BT_ASSERT(bt_ctf_field_type_common_get_type_id(
		event_class->common.payload_field_type) ==
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
	field_type = (void *)
		bt_ctf_field_type_structure_get_field_type_by_name(
			(void *) event_class->common.payload_field_type, name);

end:
	return field_type;
}

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_value *attr_value = NULL;

	BT_ASSERT(event_class);
	BT_ASSERT(context);
	BT_LOGD("Serializing event class's metadata: "
		"event-class-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", metadata-context-addr=%p",
		event_class, bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), context);
	context->current_indentation_level = 1;
	g_string_assign(context->field_name, "");
	g_string_append(context->string, "event {\n");

	/* Serialize attributes */
	g_string_append_printf(context->string, "\tname = \"%s\";\n",
		event_class->common.name->str);
	BT_ASSERT(event_class->common.id >= 0);
	g_string_append_printf(context->string, "\tid = %" PRId64 ";\n",
		event_class->common.id);
	g_string_append_printf(context->string, "\tstream_id = %" PRId64 ";\n",
		bt_ctf_stream_class_common_get_id(
			bt_ctf_event_class_common_borrow_stream_class(
				BT_CTF_TO_COMMON(event_class))));

	if (event_class->common.log_level !=
			BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED) {
		g_string_append_printf(context->string, "\tloglevel = %d;\n",
			(int) event_class->common.log_level);
	}

	if (event_class->common.emf_uri->len > 0) {
		g_string_append_printf(context->string, "\tmodel.emf.uri = \"%s\";\n",
			event_class->common.emf_uri->str);
	}

	/* Serialize context field type */
	if (event_class->common.context_field_type) {
		g_string_append(context->string, "\tcontext := ");
		BT_LOGD_STR("Serializing event class's context field type metadata.");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) event_class->common.context_field_type,
			context);
		if (ret) {
			BT_LOGW("Cannot serialize event class's context field type's metadata: "
				"ret=%d", ret);
			goto end;
		}
		g_string_append(context->string, ";\n");
	}

	/* Serialize payload field type */
	if (event_class->common.payload_field_type) {
		g_string_append(context->string, "\tfields := ");
		BT_LOGD_STR("Serializing event class's payload field type metadata.");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) event_class->common.payload_field_type,
			context);
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

struct bt_ctf_field_type *bt_ctf_event_class_get_field_by_name(
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

	if (!event_class->common.payload_field_type) {
		BT_LOGV("Event class has no payload field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class,
			bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto end;
	}

	BT_ASSERT(event_class->common.payload_field_type->id ==
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
	field_type = bt_get(
		bt_ctf_field_type_common_structure_borrow_field_type_by_name(
			event_class->common.payload_field_type, name));

end:
	return field_type;
}
