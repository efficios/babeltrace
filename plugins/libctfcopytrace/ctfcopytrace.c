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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-writer/stream.h>
#include <assert.h>

#include "ctfcopytrace.h"
#include "clock-fields.h"

struct bt_ctf_clock_class *ctf_copy_clock_class(FILE *err,
		struct bt_ctf_clock_class *clock_class)
{
	int64_t offset, offset_s;
	int int_ret;
	uint64_t u64_ret;
	const char *name, *description;
	struct bt_ctf_clock_class *writer_clock_class = NULL;

	assert(err && clock_class);

	name = bt_ctf_clock_class_get_name(clock_class);
	if (!name) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	writer_clock_class = bt_ctf_clock_class_create(name);
	if (!writer_clock_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	description = bt_ctf_clock_class_get_description(clock_class);
	if (!description) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_description(writer_clock_class,
			description);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	u64_ret = bt_ctf_clock_class_get_frequency(clock_class);
	if (u64_ret == -1ULL) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}
	int_ret = bt_ctf_clock_class_set_frequency(writer_clock_class, u64_ret);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	u64_ret = bt_ctf_clock_class_get_precision(clock_class);
	if (u64_ret == -1ULL) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}
	int_ret = bt_ctf_clock_class_set_precision(writer_clock_class,
		u64_ret);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_get_offset_s(clock_class, &offset_s);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_offset_s(writer_clock_class, offset_s);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_get_offset_cycles(clock_class, &offset);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_offset_cycles(writer_clock_class, offset);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_get_is_absolute(clock_class);
	if (int_ret == -1) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_is_absolute(writer_clock_class, int_ret);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_destroy;
	}

	goto end;

end_destroy:
	BT_PUT(writer_clock_class);
end:
	return writer_clock_class;
}

enum bt_component_status ctf_copy_clock_classes(FILE *err,
		struct bt_ctf_trace *writer_trace,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_trace *trace)
{
	enum bt_component_status ret;
	int int_ret, clock_class_count, i;

	clock_class_count = bt_ctf_trace_get_clock_class_count(trace);

	for (i = 0; i < clock_class_count; i++) {
		struct bt_ctf_clock_class *writer_clock_class;
		struct bt_ctf_clock_class *clock_class =
			bt_ctf_trace_get_clock_class(trace, i);

		if (!clock_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		writer_clock_class = ctf_copy_clock_class(err, clock_class);
		bt_put(clock_class);
		if (!writer_clock_class) {
			fprintf(err, "Failed to copy clock class");
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		int_ret = bt_ctf_trace_add_clock_class(writer_trace, writer_clock_class);
		if (int_ret != 0) {
			BT_PUT(writer_clock_class);
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
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

struct bt_ctf_event_class *ctf_copy_event_class(FILE *err,
		struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_event_class *writer_event_class = NULL;
	const char *name;
	int count, i;

	name = bt_ctf_event_class_get_name(event_class);
	if (!name) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	writer_event_class = bt_ctf_event_class_create(name);
	if (!writer_event_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	count = bt_ctf_event_class_get_attribute_count(event_class);
	for (i = 0; i < count; i++) {
		const char *attr_name;
		struct bt_value *attr_value;
		int ret;

		attr_name = bt_ctf_event_class_get_attribute_name(event_class, i);
		if (!attr_name) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		attr_value = bt_ctf_event_class_get_attribute_value(event_class, i);
		if (!attr_value) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}

		ret = bt_ctf_event_class_set_attribute(writer_event_class,
				attr_name, attr_value);
		BT_PUT(attr_value);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}

	count = bt_ctf_event_class_get_field_count(event_class);
	for (i = 0; i < count; i++) {
		const char *field_name;
		struct bt_ctf_field_type *field_type;
		int ret;

		ret = bt_ctf_event_class_get_field(event_class, &field_name,
				&field_type, i);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
			goto error;
		}

		ret = bt_ctf_event_class_add_field(writer_event_class, field_type,
				field_name);
		BT_PUT(field_type);
		if (ret < 0) {
			fprintf(err, "[error] Cannot add field %s\n", field_name);
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(writer_event_class);
end:
	return writer_event_class;
}

enum bt_component_status ctf_copy_event_classes(FILE *err,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream_class *writer_stream_class)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_event_class *event_class = NULL, *writer_event_class = NULL;
	int count, i;

	count = bt_ctf_stream_class_get_event_class_count(stream_class);
	if (count < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	for (i = 0; i < count; i++) {
		struct bt_ctf_field_type *context;
		int int_ret;

		event_class = bt_ctf_stream_class_get_event_class(
				stream_class, i);
		if (!event_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto error;
		}
		writer_event_class = ctf_copy_event_class(err, event_class);
		if (!writer_event_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto error;
		}

		context = bt_ctf_event_class_get_context_type(event_class);
		if (!context) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto error;
		}
		ret = bt_ctf_event_class_set_context_type(writer_event_class, context);
		BT_PUT(context);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		int_ret = bt_ctf_stream_class_add_event_class(writer_stream_class,
				writer_event_class);
		if (int_ret < 0) {
			fprintf(err, "[error] Failed to add event class\n");
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
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
	return ret;
}

struct bt_ctf_stream_class *ctf_copy_stream_class(FILE *err,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_trace *writer_trace,
		bool override_ts64)
{
	struct bt_ctf_field_type *type = NULL;
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	int ret_int;
	const char *name = bt_ctf_stream_class_get_name(stream_class);

	if (strlen(name) == 0) {
		name = NULL;
	}

	writer_stream_class = bt_ctf_stream_class_create(name);
	if (!writer_stream_class) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	type = bt_ctf_stream_class_get_packet_context_type(stream_class);
	if (!type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret_int = bt_ctf_stream_class_set_packet_context_type(
			writer_stream_class, type);
	if (ret_int < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}
	BT_PUT(type);

	type = bt_ctf_stream_class_get_event_header_type(stream_class);
	if (!type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	if (override_ts64) {
		struct bt_ctf_field_type *new_event_header_type;

		new_event_header_type = override_header_type(err, type,
				writer_trace);
		if (!new_event_header_type) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
		ret_int = bt_ctf_stream_class_set_event_header_type(
				writer_stream_class, new_event_header_type);
		BT_PUT(new_event_header_type);
		if (ret_int < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
	} else {
		ret_int = bt_ctf_stream_class_set_event_header_type(
				writer_stream_class, type);
		if (ret_int < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
	}
	BT_PUT(type);

	type = bt_ctf_stream_class_get_event_context_type(stream_class);
	if (type) {
		ret_int = bt_ctf_stream_class_set_event_context_type(
				writer_stream_class, type);
		if (ret_int < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
	}
	BT_PUT(type);

	goto end;

error:
	BT_PUT(writer_stream_class);
end:
	bt_put(type);
	return writer_stream_class;
}

enum bt_component_status ctf_copy_packet_context_field(FILE *err,
		struct bt_ctf_field *field, const char *field_name,
		struct bt_ctf_field *writer_packet_context,
		struct bt_ctf_field_type *writer_packet_context_type)
{
	enum bt_component_status ret;
	struct bt_ctf_field *writer_field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	int int_ret;
	uint64_t value;

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}
	/*
	 * Only support for integers for now.
	 */
	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		fprintf(err, "[error] Unsupported packet context field type\n");
		ret = BT_COMPONENT_STATUS_ERROR;
		goto error;
	}
	BT_PUT(field_type);

	writer_field = bt_ctf_field_structure_get_field(writer_packet_context,
			field_name);
	if (!writer_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	int_ret = bt_ctf_field_unsigned_integer_get_value(field, &value);
	if (int_ret < 0) {
		fprintf(err, "[error] Wrong packet_context field type\n");
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	int_ret = bt_ctf_field_unsigned_integer_set_value(writer_field, value);
	if (int_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	ret = BT_COMPONENT_STATUS_OK;

	goto end;

error:
	bt_put(field_type);
end:
	bt_put(writer_field);
	return ret;
}

struct bt_ctf_field *ctf_copy_packet_context(FILE *err,
		struct bt_ctf_packet *packet,
		struct bt_ctf_stream *writer_stream)
{
	enum bt_component_status ret;
	struct bt_ctf_field *packet_context = NULL, *writer_packet_context = NULL;
	struct bt_ctf_field_type *struct_type = NULL, *writer_packet_context_type = NULL;
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type;
	int nr_fields, i;

	packet_context = bt_ctf_packet_get_context(packet);
	if (!packet_context) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_stream_class = bt_ctf_stream_get_class(writer_stream);
	if (!writer_stream_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_packet_context_type = bt_ctf_stream_class_get_packet_context_type(
			writer_stream_class);
	BT_PUT(writer_stream_class);
	if (!writer_packet_context_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	struct_type = bt_ctf_field_get_type(packet_context);
	if (!struct_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_packet_context = bt_ctf_field_create(writer_packet_context_type);
	if (!writer_packet_context) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	nr_fields = bt_ctf_field_type_structure_get_field_count(struct_type);
	for (i = 0; i < nr_fields; i++) {
		const char *field_name;

		field = bt_ctf_field_structure_get_field_by_index(
				packet_context, i);
		if (!field) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		if (bt_ctf_field_type_structure_get_field(struct_type,
					&field_name, &field_type, i) < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		if (!strncmp(field_name, "content_size", strlen("content_size")) ||
				!strncmp(field_name, "packet_size",
					strlen("packet_size"))) {
			BT_PUT(field_type);
			BT_PUT(field);
			continue;
		}

		if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			fprintf(err, "[error] Unexpected packet context field type\n");
			goto error;
		}

		ret = ctf_copy_packet_context_field(err, field, field_name,
				writer_packet_context, writer_packet_context_type);
		BT_PUT(field_type);
		BT_PUT(field);
		if (ret != BT_COMPONENT_STATUS_OK) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(writer_packet_context);
end:
	bt_put(field);
	bt_put(field_type);
	bt_put(struct_type);
	bt_put(writer_packet_context_type);
	bt_put(writer_stream_class);
	bt_put(packet_context);
	return writer_packet_context;
}

int ctf_copy_event_header(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event_class *writer_event_class,
		struct bt_ctf_event *writer_event,
		struct bt_ctf_field *event_header)
{
	struct bt_ctf_clock_class *clock_class = NULL, *writer_clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL, *writer_clock_value = NULL;

	int ret;
	struct bt_ctf_field *writer_event_header = NULL;
	uint64_t value;

	clock_class = event_get_clock_class(err, event);
	if (!clock_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	BT_PUT(clock_class);
	if (!clock_value) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_clock_value_get_value(clock_value, &value);
	BT_PUT(clock_value);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_clock_class = event_get_clock_class(err, writer_event);
	if (!writer_clock_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_clock_value = bt_ctf_clock_value_create(writer_clock_class, value);
	BT_PUT(writer_clock_class);
	if (!writer_clock_value) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_event_set_clock_value(writer_event, writer_clock_value);
	BT_PUT(writer_clock_value);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_event_header = bt_ctf_field_copy(event_header);
	if (!writer_event_header) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	ret = bt_ctf_event_set_header(writer_event, writer_event_header);
	BT_PUT(writer_event_header);
	if (ret < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = 0;

	goto end;

error:
	ret = -1;
end:
	return ret;
}

struct bt_ctf_event *ctf_copy_event(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event_class *writer_event_class,
		bool override_ts64)
{
	struct bt_ctf_event *writer_event = NULL;
	struct bt_ctf_field *field = NULL, *copy_field = NULL;
	int ret;

	writer_event = bt_ctf_event_create(writer_event_class);
	if (!writer_event) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	field = bt_ctf_event_get_header(event);
	if (!field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	/*
	 * If override_ts64, we override all integer fields mapped to a clock
	 * to a uint64_t field type, otherwise, we just copy it as is.
	 */
	if (override_ts64) {
		copy_field = bt_ctf_event_get_header(writer_event);
		if (!copy_field) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}

		ret = copy_override_field(err, event, writer_event, field,
				copy_field);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(copy_field);
	} else {
		ret = ctf_copy_event_header(err, event, writer_event_class,
				writer_event, field);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}
	BT_PUT(field);

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_stream_event_context(event);
	copy_field = bt_ctf_field_copy(field);
	if (copy_field) {
		ret = bt_ctf_event_set_stream_event_context(writer_event,
				copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}
	BT_PUT(field);
	BT_PUT(copy_field);

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_event_context(event);
	copy_field = bt_ctf_field_copy(field);
	if (copy_field) {
		ret = bt_ctf_event_set_event_context(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}
	BT_PUT(field);
	BT_PUT(copy_field);

	field = bt_ctf_event_get_payload_field(event);
	if (!field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	copy_field = bt_ctf_field_copy(field);
	if (copy_field) {
		ret = bt_ctf_event_set_payload_field(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}
	BT_PUT(field);
	BT_PUT(copy_field);

	goto end;

error:
	BT_PUT(writer_event);
end:
	bt_put(field);
	bt_put(copy_field);
	return writer_event;
}

enum bt_component_status ctf_copy_trace(FILE *err, struct bt_ctf_trace *trace,
		struct bt_ctf_trace *writer_trace)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int field_count, i, int_ret;
	struct bt_ctf_field_type *header_type = NULL;

	field_count = bt_ctf_trace_get_environment_field_count(trace);
	for (i = 0; i < field_count; i++) {
		int ret_int;
		const char *name;
		struct bt_value *value = NULL;

		name = bt_ctf_trace_get_environment_field_name(trace, i);
		if (!name) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		value = bt_ctf_trace_get_environment_field_value(trace, i);
		if (!value) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		ret_int = bt_ctf_trace_set_environment_field(writer_trace,
				name, value);
		BT_PUT(value);
		if (ret_int < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			fprintf(err, "[error] Unable to set environment field %s\n",
					name);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

	header_type = bt_ctf_trace_get_packet_header_type(writer_trace);
	if (!header_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	int_ret = bt_ctf_trace_set_packet_header_type(writer_trace, header_type);
	BT_PUT(header_type);
	if (int_ret < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return ret;
}
