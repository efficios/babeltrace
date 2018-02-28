/*
 * copytrace.c
 *
 * Babeltrace library to create a copy of a CTF trace
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTFCOPYTRACE-LIB"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/assert-internal.h>

#include "ctfcopytrace.h"
#include "clock-fields.h"

BT_HIDDEN
struct bt_clock_class *ctf_copy_clock_class(FILE *err,
		struct bt_clock_class *clock_class)
{
	int64_t offset, offset_s;
	int int_ret;
	uint64_t u64_ret;
	const char *name, *description;
	struct bt_clock_class *writer_clock_class = NULL;

	BT_ASSERT(err && clock_class);

	name = bt_clock_class_get_name(clock_class);
	BT_ASSERT(name);

	writer_clock_class = bt_clock_class_create(name,
		bt_clock_class_get_frequency(clock_class));
	if (!writer_clock_class) {
		BT_LOGE_STR("Failed to create clock class.");
		goto end;
	}

	description = bt_clock_class_get_description(clock_class);
	if (description) {
		int_ret = bt_clock_class_set_description(writer_clock_class,
				description);
		BT_ASSERT(!int_ret);
	}

	u64_ret = bt_clock_class_get_precision(clock_class);
	BT_ASSERT(u64_ret != -1ULL);

	int_ret = bt_clock_class_set_precision(writer_clock_class,
		u64_ret);
	BT_ASSERT(!int_ret);

	int_ret = bt_clock_class_get_offset_s(clock_class, &offset_s);
	BT_ASSERT(!int_ret);

	int_ret = bt_clock_class_set_offset_s(writer_clock_class, offset_s);
	BT_ASSERT(!int_ret);

	int_ret = bt_clock_class_get_offset_cycles(clock_class, &offset);
	BT_ASSERT(!int_ret);

	int_ret = bt_clock_class_set_offset_cycles(writer_clock_class, offset);
	BT_ASSERT(!int_ret);

	int_ret = bt_clock_class_is_absolute(clock_class);
	BT_ASSERT(int_ret >= 0);

	int_ret = bt_clock_class_set_is_absolute(writer_clock_class, int_ret);
	BT_ASSERT(!int_ret);

end:
	return writer_clock_class;
}

BT_HIDDEN
enum bt_component_status ctf_copy_clock_classes(FILE *err,
		struct bt_trace *writer_trace,
		struct bt_stream_class *writer_stream_class,
		struct bt_trace *trace)
{
	enum bt_component_status ret;
	int int_ret, clock_class_count, i;

	clock_class_count = bt_trace_get_clock_class_count(trace);

	for (i = 0; i < clock_class_count; i++) {
		struct bt_clock_class *writer_clock_class;
		struct bt_clock_class *clock_class =
			bt_trace_get_clock_class_by_index(trace, i);

		BT_ASSERT(clock_class);

		writer_clock_class = ctf_copy_clock_class(err, clock_class);
		bt_put(clock_class);
		if (!writer_clock_class) {
			BT_LOGE_STR("Failed to copy clock class.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		int_ret = bt_trace_add_clock_class(writer_trace, writer_clock_class);
		if (int_ret != 0) {
			BT_PUT(writer_clock_class);
			BT_LOGE_STR("Failed to add clock class.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		/*
		 * Ownership transferred to the trace.
		 */
		bt_put(writer_clock_class);
	}

	ret = BT_COMPONENT_STATUS_OK;

end:
	return ret;
}

static
void replace_clock_classes(struct bt_trace *trace_copy,
		struct bt_field_type *field_type)
{
	int ret;

	BT_ASSERT(trace_copy);
	BT_ASSERT(field_type);

	switch (bt_field_type_get_type_id(field_type)) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_clock_class *mapped_clock_class =
			bt_field_type_integer_get_mapped_clock_class(field_type);
		struct bt_clock_class *clock_class_copy = NULL;
		const char *name;

		if (!mapped_clock_class) {
			break;
		}

		name = bt_clock_class_get_name(mapped_clock_class);
		BT_ASSERT(name);
		clock_class_copy = bt_trace_get_clock_class_by_name(
			trace_copy, name);
		BT_ASSERT(clock_class_copy);
		ret = bt_field_type_integer_set_mapped_clock_class(
			field_type, clock_class_copy);
		BT_ASSERT(ret == 0);
		bt_put(mapped_clock_class);
		bt_put(clock_class_copy);
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	case BT_FIELD_TYPE_ID_ARRAY:
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type *subtype = NULL;

		switch (bt_field_type_get_type_id(field_type)) {
		case BT_FIELD_TYPE_ID_ENUM:
			subtype = bt_field_type_enumeration_get_container_type(
				field_type);
			break;
		case BT_FIELD_TYPE_ID_ARRAY:
			subtype = bt_field_type_array_get_element_type(
				field_type);
			break;
		case BT_FIELD_TYPE_ID_SEQUENCE:
			subtype = bt_field_type_sequence_get_element_type(
				field_type);
			break;
		default:
			BT_LOGF("Unexpected field type ID: id=%d",
				bt_field_type_get_type_id(field_type));
			abort();
		}

		BT_ASSERT(subtype);
		replace_clock_classes(trace_copy, subtype);
		bt_put(subtype);
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		uint64_t i;
		int64_t count = bt_field_type_structure_get_field_count(
			field_type);

		for (i = 0; i < count; i++) {
			const char *name;
			struct bt_field_type *member_type;

			ret = bt_field_type_structure_get_field_by_index(
				field_type, &name, &member_type, i);
			BT_ASSERT(ret == 0);
			replace_clock_classes(trace_copy, member_type);
			bt_put(member_type);
		}

		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		uint64_t i;
		int64_t count = bt_field_type_variant_get_field_count(
			field_type);

		for (i = 0; i < count; i++) {
			const char *name;
			struct bt_field_type *member_type;

			ret = bt_field_type_variant_get_field_by_index(
				field_type, &name, &member_type, i);
			BT_ASSERT(ret == 0);
			replace_clock_classes(trace_copy, member_type);
			bt_put(member_type);
		}

		break;
	}
	default:
		break;
	}
}

BT_HIDDEN
struct bt_event_class *ctf_copy_event_class(FILE *err,
		struct bt_trace *trace_copy,
		struct bt_event_class *event_class)
{
	struct bt_event_class *writer_event_class = NULL;
	struct bt_field_type *context = NULL, *payload_type = NULL;
	const char *name;
	int ret;
	int64_t id;
	enum bt_event_class_log_level log_level;
	const char *emf_uri;

	name = bt_event_class_get_name(event_class);

	writer_event_class = bt_event_class_create(name);
	BT_ASSERT(writer_event_class);

	id = bt_event_class_get_id(event_class);
	BT_ASSERT(id >= 0);

	ret = bt_event_class_set_id(writer_event_class, id);
	if (ret) {
		BT_LOGE_STR("Failed to set event_class id.");
		goto error;
	}

	log_level = bt_event_class_get_log_level(event_class);
	if (log_level < 0) {
		BT_LOGE_STR("Failed to get log_level.");
		goto error;
	}

	ret = bt_event_class_set_log_level(writer_event_class, log_level);
	if (ret) {
		BT_LOGE_STR("Failed to set log_level.");
		goto error;
	}

	emf_uri = bt_event_class_get_emf_uri(event_class);
	if (emf_uri) {
		ret = bt_event_class_set_emf_uri(writer_event_class,
			emf_uri);
		if (ret) {
			BT_LOGE_STR("Failed to set emf uri.");
			goto error;
		}
	}

	payload_type = bt_event_class_get_payload_type(event_class);
	if (payload_type) {
		struct bt_field_type *ft_copy =
			bt_field_type_copy(payload_type);

		if (!ft_copy) {
			BT_LOGE_STR("Cannot copy payload field type.");
		}

		replace_clock_classes(trace_copy, ft_copy);
		ret = bt_event_class_set_payload_type(writer_event_class,
				ft_copy);
		bt_put(ft_copy);
		if (ret < 0) {
			BT_LOGE_STR("Failed to set payload type.");
			goto error;
		}
	}

	context = bt_event_class_get_context_type(event_class);
	if (context) {
		struct bt_field_type *ft_copy =
			bt_field_type_copy(context);

		if (!ft_copy) {
			BT_LOGE_STR("Cannot copy context field type.");
		}

		ret = bt_event_class_set_context_type(
				writer_event_class, ft_copy);
		bt_put(ft_copy);
		if (ret < 0) {
			BT_LOGE_STR("Failed to set context type.");
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(writer_event_class);
end:
	BT_PUT(context);
	BT_PUT(payload_type);
	return writer_event_class;
}

BT_HIDDEN
enum bt_component_status ctf_copy_event_classes(FILE *err,
		struct bt_stream_class *stream_class,
		struct bt_stream_class *writer_stream_class)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_event_class *event_class = NULL, *writer_event_class = NULL;
	int count, i;
	struct bt_trace *writer_trace =
		bt_stream_class_get_trace(writer_stream_class);

	BT_ASSERT(writer_trace);
	count = bt_stream_class_get_event_class_count(stream_class);
	BT_ASSERT(count >= 0);

	for (i = 0; i < count; i++) {
		int int_ret;

		event_class = bt_stream_class_get_event_class_by_index(
				stream_class, i);
		BT_ASSERT(event_class);

		if (i < bt_stream_class_get_event_class_count(writer_stream_class)) {
			writer_event_class = bt_stream_class_get_event_class_by_index(
					writer_stream_class, i);
			if (writer_event_class) {
				/*
				 * If the writer_event_class already exists,
				 * just skip it. It can be used to resync the
				 * event_classes after a trace has become
				 * static.
				 */
				BT_PUT(writer_event_class);
				BT_PUT(event_class);
				continue;
			}
		}

		writer_event_class = ctf_copy_event_class(err, writer_trace,
			event_class);
		if (!writer_event_class) {
			BT_LOGE_STR("Failed to copy event_class.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto error;
		}

		int_ret = bt_stream_class_add_event_class(writer_stream_class,
				writer_event_class);
		if (int_ret < 0) {
			BT_LOGE_STR("Failed to add event class.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto error;
		}
		BT_PUT(writer_event_class);
		BT_PUT(event_class);
	}

	goto end;

error:
	bt_put(event_class);
	bt_put(writer_event_class);
end:
	bt_put(writer_trace);
	return ret;
}

BT_HIDDEN
struct bt_stream_class *ctf_copy_stream_class(FILE *err,
		struct bt_stream_class *stream_class,
		struct bt_trace *writer_trace,
		bool override_ts64)
{
	struct bt_field_type *type = NULL;
	struct bt_field_type *type_copy = NULL;
	struct bt_stream_class *writer_stream_class = NULL;
	int ret_int;
	const char *name = bt_stream_class_get_name(stream_class);

	writer_stream_class = bt_stream_class_create_empty(name);
	BT_ASSERT(writer_stream_class);

	type = bt_stream_class_get_packet_context_type(stream_class);
	if (type) {
		type_copy = bt_field_type_copy(type);
		if (!type_copy) {
			BT_LOGE_STR("Cannot copy packet context field type.");
		}

		replace_clock_classes(writer_trace, type_copy);
		ret_int = bt_stream_class_set_packet_context_type(
				writer_stream_class, type_copy);
		BT_PUT(type_copy);
		if (ret_int < 0) {
			BT_LOGE_STR("Failed to set packet_context type.");
			goto error;
		}
		BT_PUT(type);
	}

	type = bt_stream_class_get_event_header_type(stream_class);
	if (type) {
		type_copy = bt_field_type_copy(type);
		if (!type_copy) {
			BT_LOGE_STR("Cannot copy event header field type.");
		}

		ret_int = bt_trace_get_clock_class_count(writer_trace);
		BT_ASSERT(ret_int >= 0);
		if (override_ts64 && ret_int > 0) {
			struct bt_field_type *new_event_header_type;

			new_event_header_type = override_header_type(err, type_copy,
					writer_trace);
			if (!new_event_header_type) {
				BT_LOGE_STR("Failed to override header type.");
				goto error;
			}
			replace_clock_classes(writer_trace, type_copy);
			ret_int = bt_stream_class_set_event_header_type(
					writer_stream_class, new_event_header_type);
			BT_PUT(type_copy);
			BT_PUT(new_event_header_type);
			if (ret_int < 0) {
				BT_LOGE_STR("Failed to set event_header type.");
				goto error;
			}
		} else {
			replace_clock_classes(writer_trace, type_copy);
			ret_int = bt_stream_class_set_event_header_type(
					writer_stream_class, type_copy);
			BT_PUT(type_copy);
			if (ret_int < 0) {
				BT_LOGE_STR("Failed to set event_header type.");
				goto error;
			}
		}
		BT_PUT(type);
	}

	type = bt_stream_class_get_event_context_type(stream_class);
	if (type) {
		type_copy = bt_field_type_copy(type);
		if (!type_copy) {
			BT_LOGE_STR("Cannot copy event context field type.");
		}

		replace_clock_classes(writer_trace, type_copy);
		ret_int = bt_stream_class_set_event_context_type(
				writer_stream_class, type_copy);
		BT_PUT(type_copy);
		if (ret_int < 0) {
			BT_LOGE_STR("Failed to set event_contexttype.");
			goto error;
		}
	}
	BT_PUT(type);

	goto end;

error:
	BT_PUT(writer_stream_class);
end:
	bt_put(type);
	bt_put(type_copy);
	return writer_stream_class;
}

BT_HIDDEN
int ctf_stream_copy_packet_header(FILE *err, struct bt_packet *packet,
		struct bt_stream *writer_stream)
{
	struct bt_field *packet_header = NULL, *writer_packet_header = NULL;
	int ret = 0;

	packet_header = bt_packet_get_header(packet);
	if (!packet_header) {
		goto end;
	}

	writer_packet_header = bt_field_copy(packet_header);
	if (!writer_packet_header) {
		BT_LOGE_STR("Failed to copy field from stream packet header.");
		goto error;
	}

	ret = bt_stream_set_packet_header(writer_stream,
			writer_packet_header);
	if (ret) {
		BT_LOGE_STR("Failed to set stream packet header.");
		goto error;
	}

	goto end;

error:
	ret = -1;
end:
	bt_put(writer_packet_header);
	bt_put(packet_header);
	return ret;
}

BT_HIDDEN
int ctf_packet_copy_header(FILE *err, struct bt_packet *packet,
		struct bt_packet *writer_packet)
{
	struct bt_field *packet_header = NULL, *writer_packet_header = NULL;
	int ret = 0;

	packet_header = bt_packet_get_header(packet);
	if (!packet_header) {
		goto end;
	}

	writer_packet_header = bt_field_copy(packet_header);
	if (!writer_packet_header) {
		BT_LOGE_STR("Failed to copy field from packet header.");
		goto error;
	}

	ret = bt_packet_set_header(writer_packet, writer_packet_header);
	if (ret) {
		BT_LOGE_STR("Failed to set packet header.");
		goto error;
	}

	goto end;

error:
	ret = -1;
end:
	bt_put(packet_header);
	bt_put(writer_packet_header);
	return ret;
}

BT_HIDDEN
int ctf_stream_copy_packet_context(FILE *err, struct bt_packet *packet,
		struct bt_stream *writer_stream)
{
	struct bt_field *packet_context = NULL, *writer_packet_context = NULL;
	int ret = 0;

	packet_context = bt_packet_get_context(packet);
	if (!packet_context) {
		goto end;
	}

	writer_packet_context = bt_field_copy(packet_context);
	if (!writer_packet_context) {
		BT_LOGE_STR("Failed to copy field from stream packet context.");
		goto error;
	}

	ret = bt_stream_set_packet_context(writer_stream,
			writer_packet_context);
	if (ret) {
		BT_LOGE_STR("Failed to set stream packet context.");
		goto error;
	}

	goto end;

error:
	ret = -1;
end:
	bt_put(packet_context);
	bt_put(writer_packet_context);
	return ret;
}

BT_HIDDEN
int ctf_packet_copy_context(FILE *err, struct bt_packet *packet,
		struct bt_stream *writer_stream,
		struct bt_packet *writer_packet)
{
	struct bt_field *packet_context = NULL, *writer_packet_context = NULL;
	int ret = 0;

	packet_context = bt_packet_get_context(packet);
	if (!packet_context) {
		goto end;
	}

	writer_packet_context = bt_field_copy(packet_context);
	if (!writer_packet_context) {
		BT_LOGE_STR("Failed to copy field from packet context.");
		goto error;
	}

	ret = bt_packet_set_context(writer_packet, writer_packet_context);
	if (ret) {
		BT_LOGE_STR("Failed to set packet context.");
		goto error;
	}

	goto end;

error:
	ret = -1;
end:
	bt_put(writer_packet_context);
	bt_put(packet_context);
	return ret;
}

BT_HIDDEN
int ctf_copy_event_header(FILE *err, struct bt_event *event,
		struct bt_event_class *writer_event_class,
		struct bt_event *writer_event,
		struct bt_field *event_header)
{
	struct bt_clock_class *clock_class = NULL, *writer_clock_class = NULL;
	struct bt_clock_value *clock_value = NULL, *writer_clock_value = NULL;

	int ret;
	struct bt_field *writer_event_header = NULL;
	uint64_t value;

	clock_class = event_get_clock_class(err, event);
	if (!clock_class) {
		BT_LOGE_STR("Failed to get event clock_class.");
		goto error;
	}

	clock_value = bt_event_get_clock_value(event, clock_class);
	BT_PUT(clock_class);
	BT_ASSERT(clock_value);

	ret = bt_clock_value_get_value(clock_value, &value);
	BT_PUT(clock_value);
	if (ret) {
		BT_LOGE_STR("Failed to get clock value.");
		goto error;
	}

	writer_clock_class = event_get_clock_class(err, writer_event);
	if (!writer_clock_class) {
		BT_LOGE_STR("Failed to get event clock_class.");
		goto error;
	}

	writer_clock_value = bt_clock_value_create(writer_clock_class, value);
	BT_PUT(writer_clock_class);
	if (!writer_clock_value) {
		BT_LOGE_STR("Failed to create clock value.");
		goto error;
	}

	ret = bt_event_set_clock_value(writer_event, writer_clock_value);
	BT_PUT(writer_clock_value);
	if (ret) {
		BT_LOGE_STR("Failed to set clock value.");
		goto error;
	}

	writer_event_header = bt_field_copy(event_header);
	if (!writer_event_header) {
		BT_LOGE_STR("Failed to copy event_header.");
		goto end;
	}

	ret = bt_event_set_header(writer_event, writer_event_header);
	BT_PUT(writer_event_header);
	if (ret < 0) {
		BT_LOGE_STR("Failed to set event_header.");
		goto error;
	}

	ret = 0;

	goto end;

error:
	ret = -1;
end:
	return ret;
}

static
struct bt_trace *event_class_get_trace(FILE *err,
		struct bt_event_class *event_class)
{
	struct bt_trace *trace = NULL;
	struct bt_stream_class *stream_class = NULL;

	stream_class = bt_event_class_get_stream_class(event_class);
	BT_ASSERT(stream_class);

	trace = bt_stream_class_get_trace(stream_class);
	BT_ASSERT(trace);

	bt_put(stream_class);
	return trace;
}

BT_HIDDEN
struct bt_event *ctf_copy_event(FILE *err, struct bt_event *event,
		struct bt_event_class *writer_event_class,
		bool override_ts64)
{
	struct bt_event *writer_event = NULL;
	struct bt_field *field = NULL, *copy_field = NULL;
	struct bt_trace *writer_trace = NULL;
	int ret;

	writer_event = bt_event_create(writer_event_class);
	if (!writer_event) {
		BT_LOGE_STR("Failed to create event.");
		goto error;
	}

	writer_trace = event_class_get_trace(err, writer_event_class);
	if (!writer_trace) {
		BT_LOGE_STR("Failed to get trace from event_class.");
		goto error;
	}

	field = bt_event_get_header(event);
	if (field) {
		/*
		 * If override_ts64, we override all integer fields mapped to a
		 * clock to a uint64_t field type, otherwise, we just copy it as
		 * is.
		 */
		ret = bt_trace_get_clock_class_count(writer_trace);
		BT_ASSERT(ret >= 0);

		if (override_ts64 && ret > 0) {
			copy_field = bt_event_get_header(writer_event);
			BT_ASSERT(copy_field);

			ret = copy_override_field(err, event, writer_event, field,
					copy_field);
			if (ret) {
				BT_LOGE_STR("Failed to copy and override field.");
				goto error;
			}
			BT_PUT(copy_field);
		} else {
			ret = ctf_copy_event_header(err, event, writer_event_class,
					writer_event, field);
			if (ret) {
				BT_LOGE_STR("Failed to copy event_header.");
				goto error;
			}
		}
		BT_PUT(field);
	}

	/* Optional field, so it can fail silently. */
	field = bt_event_get_stream_event_context(event);
	if (field) {
		copy_field = bt_field_copy(field);
		if (!copy_field) {
			BT_LOGE_STR("Failed to copy field.");
			goto error;
		}
		ret = bt_event_set_stream_event_context(writer_event,
				copy_field);
		if (ret < 0) {
			BT_LOGE_STR("Failed to set stream_event_context.");
			goto error;
		}
		BT_PUT(field);
		BT_PUT(copy_field);
	}

	/* Optional field, so it can fail silently. */
	field = bt_event_get_event_context(event);
	if (field) {
		copy_field = bt_field_copy(field);
		if (!copy_field) {
			BT_LOGE_STR("Failed to copy field.");
			goto error;
		}
		ret = bt_event_set_event_context(writer_event, copy_field);
		if (ret < 0) {
			BT_LOGE_STR("Failed to set event_context.");
			goto error;
		}
		BT_PUT(field);
		BT_PUT(copy_field);
	}

	field = bt_event_get_event_payload(event);
	if (field) {
		copy_field = bt_field_copy(field);
		if (!copy_field) {
			BT_LOGE_STR("Failed to copy field.");
			goto error;
		}
		ret = bt_event_set_event_payload(writer_event, copy_field);
		if (ret < 0) {
			BT_LOGE_STR("Failed to set event_payload.");
			goto error;
		}
		BT_PUT(field);
		BT_PUT(copy_field);
	}

	goto end;

error:
	BT_PUT(writer_event);
end:
	bt_put(field);
	bt_put(copy_field);
	bt_put(writer_trace);
	return writer_event;
}

BT_HIDDEN
enum bt_component_status ctf_copy_trace(FILE *err, struct bt_trace *trace,
		struct bt_trace *writer_trace)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int field_count, i, int_ret;
	struct bt_field_type *header_type = NULL;
	enum bt_byte_order order;
	const char *trace_name;
	const unsigned char *trace_uuid;

	field_count = bt_trace_get_environment_field_count(trace);
	for (i = 0; i < field_count; i++) {
		int ret_int;
		const char *name;
		struct bt_value *value = NULL;

		name = bt_trace_get_environment_field_name_by_index(
			trace, i);
		BT_ASSERT(name);

		value = bt_trace_get_environment_field_value_by_index(
			trace, i);
		BT_ASSERT(value);

		ret_int = bt_trace_set_environment_field(writer_trace,
				name, value);
		BT_PUT(value);
		if (ret_int < 0) {
			BT_LOGE("Failed to set environment: field-name=\"%s\"",
					name);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

	order = bt_trace_get_native_byte_order(trace);
	BT_ASSERT(order != BT_BYTE_ORDER_UNKNOWN);

	/*
	 * Only explicitly set the writer trace's native byte order if
	 * the original trace has a specific one. Otherwise leave what
	 * the CTF writer object chooses, which is the machine's native
	 * byte order.
	 */
	if (order != BT_BYTE_ORDER_UNSPECIFIED) {
		ret = bt_trace_set_native_byte_order(writer_trace, order);
		if (ret) {
			BT_LOGE_STR("Failed to set native byte order.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

	header_type = bt_trace_get_packet_header_type(trace);
	if (header_type) {
		int_ret = bt_trace_set_packet_header_type(writer_trace, header_type);
		BT_PUT(header_type);
		if (int_ret < 0) {
			BT_LOGE_STR("Failed to set packet header type.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

	trace_name = bt_trace_get_name(trace);
	if (trace_name) {
		int_ret = bt_trace_set_name(writer_trace, trace_name);
		if (int_ret < 0) {
			BT_LOGE_STR("Failed to set trace name.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

	trace_uuid = bt_trace_get_uuid(trace);
	if (trace_uuid) {
		int_ret = bt_trace_set_uuid(writer_trace, trace_uuid);
		if (int_ret < 0) {
			BT_LOGE_STR("Failed to set trace UUID.");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

end:
	return ret;
}
