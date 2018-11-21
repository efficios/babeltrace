#ifndef BABELTRACE_CTF_WRITER_STREAM_CLASS_INTERNAL_H
#define BABELTRACE_CTF_WRITER_STREAM_CLASS_INTERNAL_H

/*
 * Copyright 2014 EfficiOS Inc.
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/utils-internal.h>
#include <babeltrace/ctf-writer/validation-internal.h>
#include <babeltrace/ctf-writer/visitor.h>
#include <babeltrace/object-internal.h>
#include <inttypes.h>

struct bt_ctf_stream_class_common {
	struct bt_object base;
	GString *name;

	/* Array of pointers to event class addresses */
	GPtrArray *event_classes;

	/* event class id (int64_t) to event class address */
	GHashTable *event_classes_ht;
	int id_set;
	int64_t id;
	int64_t next_event_id;
	struct bt_ctf_field_type_common *packet_context_field_type;
	struct bt_ctf_field_type_common *event_header_field_type;
	struct bt_ctf_field_type_common *event_context_field_type;
	int frozen;
	int byte_order;

	/*
	 * This flag indicates if the stream class is valid. A valid
	 * stream class is _always_ frozen.
	 */
	int valid;

	/*
	 * Unique clock class mapped to any field type within this
	 * stream class, including all the stream class's event class
	 * field types. This is only set if the stream class is frozen.
	 *
	 * If the stream class is frozen and this is still NULL, it is
	 * still possible that it becomes non-NULL because
	 * bt_ctf_stream_class_add_event_class() can add an event class
	 * containing a field type mapped to some clock class. In this
	 * case, this is the mapped clock class, and at this point, both
	 * the new event class and the stream class are frozen, so the
	 * next added event classes are expected to contain field types
	 * which only map to this specific clock class.
	 *
	 * If this is a CTF writer stream class, then this is the
	 * backing clock class of the `clock` member above.
	 */
	struct bt_ctf_clock_class *clock_class;
};

struct bt_ctf_event_class_common;

BT_HIDDEN
int bt_ctf_stream_class_common_initialize(struct bt_ctf_stream_class_common *stream_class,
		const char *name, bt_object_release_func release_func);

BT_HIDDEN
void bt_ctf_stream_class_common_finalize(struct bt_ctf_stream_class_common *stream_class);

BT_HIDDEN
void bt_ctf_stream_class_common_freeze(struct bt_ctf_stream_class_common *stream_class);

static inline
const char *bt_ctf_stream_class_common_get_name(
		struct bt_ctf_stream_class_common *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->name->len > 0 ? stream_class->name->str : NULL;
}

static inline
int64_t bt_ctf_stream_class_common_get_id(
		struct bt_ctf_stream_class_common *stream_class)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");

	if (!stream_class->id_set) {
		BT_LOGV("Stream class's ID is not set: addr=%p, name=\"%s\"",
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class));
		ret = (int64_t) -1;
		goto end;
	}

	ret = stream_class->id;

end:
	return ret;
}

BT_HIDDEN
void bt_ctf_stream_class_common_set_byte_order(
		struct bt_ctf_stream_class_common *stream_class, int byte_order);

BT_HIDDEN
int bt_ctf_stream_class_common_validate_single_clock_class(
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_clock_class **expected_clock_class);

BT_HIDDEN
int bt_ctf_stream_class_common_add_event_class(
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_event_class_common *event_class,
		bt_ctf_validation_flag_copy_field_type_func copy_field_type_func);

BT_HIDDEN
int bt_ctf_stream_class_common_visit(struct bt_ctf_stream_class_common *stream_class,
		bt_ctf_visitor visitor, void *data);

BT_HIDDEN
int bt_ctf_stream_class_visit(struct bt_ctf_stream_class *stream_class,
		bt_ctf_visitor visitor, void *data);

static inline
struct bt_ctf_trace_common *bt_ctf_stream_class_common_borrow_trace(
		struct bt_ctf_stream_class_common *stream_class)
{
	BT_ASSERT(stream_class);
	return (void *) bt_object_borrow_parent(&stream_class->base);
}

static inline
int bt_ctf_stream_class_common_set_name(struct bt_ctf_stream_class_common *stream_class,
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
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (!name) {
		g_string_assign(stream_class->name, "");
	} else {
		if (strlen(name) == 0) {
			BT_LOGW("Invalid parameter: name is empty.");
			ret = -1;
			goto end;
		}

		g_string_assign(stream_class->name, name);
	}

	BT_LOGV("Set stream class's name: "
		"addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class));
end:
	return ret;
}

static inline
void _bt_ctf_stream_class_common_set_id(
		struct bt_ctf_stream_class_common *stream_class, int64_t id)
{
	BT_ASSERT(stream_class);
	stream_class->id = id;
	stream_class->id_set = 1;
	BT_LOGV("Set stream class's ID (internal): "
		"addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class));
}

static inline
int bt_ctf_stream_class_common_set_id_no_check(
		struct bt_ctf_stream_class_common *stream_class, int64_t id)
{
	_bt_ctf_stream_class_common_set_id(stream_class, id);
	return 0;
}

static inline
int bt_ctf_stream_class_common_set_id(struct bt_ctf_stream_class_common *stream_class,
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
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid stream class's ID: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", id=%" PRIu64,
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class),
			id_param);
		ret = -1;
		goto end;
	}

	ret = bt_ctf_stream_class_common_set_id_no_check(stream_class, id);
	if (ret == 0) {
		BT_LOGV("Set stream class's ID: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
	}
end:
	return ret;
}

static inline
int64_t bt_ctf_stream_class_common_get_event_class_count(
		struct bt_ctf_stream_class_common *stream_class)
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

static inline
struct bt_ctf_event_class_common *bt_ctf_stream_class_common_borrow_event_class_by_index(
		struct bt_ctf_stream_class_common *stream_class, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(index < stream_class->event_classes->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, stream_class->event_classes->len);
	return g_ptr_array_index(stream_class->event_classes, index);
}

static inline
struct bt_ctf_event_class_common *bt_ctf_stream_class_common_borrow_event_class_by_id(
		struct bt_ctf_stream_class_common *stream_class, uint64_t id)
{
	int64_t id_key = (int64_t) id;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(id_key >= 0,
		"Invalid event class ID: %" PRIu64, id);
	return g_hash_table_lookup(stream_class->event_classes_ht,
			&id_key);
}

static inline
struct bt_ctf_field_type_common *
bt_ctf_stream_class_common_borrow_packet_context_field_type(
		struct bt_ctf_stream_class_common *stream_class)
{
	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	return stream_class->packet_context_field_type;
}

static inline
int bt_ctf_stream_class_common_set_packet_context_field_type(
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_field_type_common *packet_context_type)
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
			stream_class, bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (packet_context_type &&
			bt_ctf_field_type_common_get_type_id(packet_context_type) !=
				BT_CTF_FIELD_TYPE_ID_STRUCT) {
		/* A packet context must be a structure. */
		BT_LOGW("Invalid parameter: stream class's packet context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"packet-context-ft-addr=%p, packet-context-ft-id=%s",
			stream_class, bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class),
			packet_context_type,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_common_get_type_id(packet_context_type)));
		ret = -1;
		goto end;
	}

	bt_object_put_ref(stream_class->packet_context_field_type);
	bt_object_get_ref(packet_context_type);
	stream_class->packet_context_field_type = packet_context_type;
	BT_LOGV("Set stream class's packet context field type: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"packet-context-ft-addr=%p",
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class),
		packet_context_type);

end:
	return ret;
}

static inline
struct bt_ctf_field_type_common *
bt_ctf_stream_class_common_borrow_event_header_field_type(
		struct bt_ctf_stream_class_common *stream_class)
{
	struct bt_ctf_field_type_common *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");

	if (!stream_class->event_header_field_type) {
		BT_LOGV("Stream class has no event header field type: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
		goto end;
	}

	ret = stream_class->event_header_field_type;

end:
	return ret;
}

static inline
int bt_ctf_stream_class_common_set_event_header_field_type(
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_field_type_common *event_header_type)
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
			stream_class,
			bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (event_header_type &&
			bt_ctf_field_type_common_get_type_id(event_header_type) !=
				BT_CTF_FIELD_TYPE_ID_STRUCT) {
		/* An event header must be a structure. */
		BT_LOGW("Invalid parameter: stream class's event header field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"event-header-ft-addr=%p, event-header-ft-id=%s",
			stream_class, bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class),
			event_header_type,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_common_get_type_id(event_header_type)));
		ret = -1;
		goto end;
	}

	bt_object_put_ref(stream_class->event_header_field_type);
	stream_class->event_header_field_type = bt_object_get_ref(event_header_type);
	BT_LOGV("Set stream class's event header field type: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"event-header-ft-addr=%p",
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class),
		event_header_type);
end:
	return ret;
}

static inline
struct bt_ctf_field_type_common *
bt_ctf_stream_class_common_borrow_event_context_field_type(
		struct bt_ctf_stream_class_common *stream_class)
{
	struct bt_ctf_field_type_common *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");

	if (!stream_class->event_context_field_type) {
		goto end;
	}

	ret = stream_class->event_context_field_type;

end:
	return ret;
}

static inline
int bt_ctf_stream_class_common_set_event_context_field_type(
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_field_type_common *event_context_type)
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
			stream_class, bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class));
		ret = -1;
		goto end;
	}

	if (event_context_type &&
			bt_ctf_field_type_common_get_type_id(event_context_type) !=
				BT_CTF_FIELD_TYPE_ID_STRUCT) {
		/* A packet context must be a structure. */
		BT_LOGW("Invalid parameter: stream class's event context field type must be a structure: "
			"addr=%p, name=\"%s\", id=%" PRId64 ", "
			"event-context-ft-addr=%p, event-context-ft-id=%s",
			stream_class, bt_ctf_stream_class_common_get_name(stream_class),
			bt_ctf_stream_class_common_get_id(stream_class),
			event_context_type,
			bt_ctf_field_type_id_string(
				bt_ctf_field_type_common_get_type_id(event_context_type)));
		ret = -1;
		goto end;
	}

	bt_object_put_ref(stream_class->event_context_field_type);
	stream_class->event_context_field_type = bt_object_get_ref(event_context_type);
	BT_LOGV("Set stream class's event context field type: "
		"addr=%p, name=\"%s\", id=%" PRId64 ", "
		"event-context-ft-addr=%p",
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class),
		event_context_type);
end:
	return ret;
}

struct bt_ctf_stream_class {
	struct bt_ctf_stream_class_common common;
	struct bt_ctf_clock *clock;
	int64_t next_stream_id;
};

struct metadata_context;

BT_HIDDEN
int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context);

BT_HIDDEN
int bt_ctf_stream_class_map_clock_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type);

#endif /* BABELTRACE_CTF_WRITER_STREAM_CLASS_INTERNAL_H */
