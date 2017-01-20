/*
 * writer.c
 *
 * Babeltrace CTF Writer Output Plugin Event Handling
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/stream.h>

#include "writer.h"

static
enum bt_component_status copy_clock_class(FILE *err, struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_clock_class *clock_class)
{
	int64_t offset, offset_s;
	int int_ret;
	uint64_t u64_ret;
	const char *name, *description;
	struct bt_ctf_clock_class *writer_clock_class = NULL;
	struct bt_ctf_trace *trace = NULL;
	enum bt_component_status ret;

	name = bt_ctf_clock_class_get_name(clock_class);
	if (!name) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	writer_clock_class = bt_ctf_clock_class_create(name);
	if (!writer_clock_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	description = bt_ctf_clock_class_get_description(clock_class);
	if (!description) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_description(writer_clock_class,
			description);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	u64_ret = bt_ctf_clock_class_get_frequency(clock_class);
	if (u64_ret == -1ULL) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}
	int_ret = bt_ctf_clock_class_set_frequency(writer_clock_class, u64_ret);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	u64_ret = bt_ctf_clock_class_get_precision(clock_class);
	if (u64_ret == -1ULL) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}
	int_ret = bt_ctf_clock_class_set_precision(writer_clock_class,
		u64_ret);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_get_offset_s(clock_class, &offset_s);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_offset_s(writer_clock_class, offset_s);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_get_offset_cycles(clock_class, &offset);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_offset_cycles(writer_clock_class, offset);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_get_is_absolute(clock_class);
	if (int_ret == -1) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_clock_class_set_is_absolute(writer_clock_class, int_ret);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	trace = bt_ctf_writer_get_trace(writer);
	if (!trace) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	int_ret = bt_ctf_trace_add_clock_class(trace, writer_clock_class);
	if (int_ret != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_destroy;
	}

	/*
	 * Ownership transferred to the writer and the stream_class.
	 */
	bt_put(writer_clock_class);
	ret = BT_COMPONENT_STATUS_OK;

	goto end;

end_destroy:
	BT_PUT(writer_clock_class);
end:
	BT_PUT(trace);
	return ret;
}

static
struct bt_ctf_event_class *copy_event_class(FILE *err, struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_event_class *writer_event_class = NULL;
	const char *name;
	struct bt_ctf_field_type *context;
	int count, i, ret;

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
			BT_PUT(writer_event_class);
			goto end;
		}
		attr_value = bt_ctf_event_class_get_attribute_value(event_class, i);
		if (!attr_value) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			BT_PUT(writer_event_class);
			goto end;
		}

		ret = bt_ctf_event_class_set_attribute(writer_event_class,
				attr_name, attr_value);
		bt_put(attr_value);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			BT_PUT(writer_event_class);
			goto end;
		}
	}

	context = bt_ctf_event_class_get_context_type(event_class);
	ret = bt_ctf_event_class_set_context_type(writer_event_class, context);
	bt_put(context);
	if (ret < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
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
			BT_PUT(writer_event_class);
			goto end;
		}

		ret = bt_ctf_event_class_add_field(writer_event_class, field_type,
				field_name);
		if (ret < 0) {
			fprintf(err, "[error] Cannot add field %s\n", field_name);
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
			bt_put(field_type);
			BT_PUT(writer_event_class);
			goto end;
		}
		bt_put(field_type);
	}

end:
	return writer_event_class;
}

static
enum bt_component_status copy_event_classes(FILE *err,
		struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream_class *writer_stream_class)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int count, i;

	count = bt_ctf_stream_class_get_event_class_count(stream_class);
	if (count < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	for (i = 0; i < count; i++) {
		struct bt_ctf_event_class *event_class, *writer_event_class;
		int int_ret;

		event_class = bt_ctf_stream_class_get_event_class(
				stream_class, i);
		if (!event_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			bt_put(event_class);
			goto end;
		}
		writer_event_class = copy_event_class(err, event_class);
		if (!writer_event_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			bt_put(event_class);
			goto end;
		}
		int_ret = bt_ctf_stream_class_add_event_class(writer_stream_class,
				writer_event_class);
		if (int_ret < 0) {
			fprintf(err, "[error] Failed to add event class\n");
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			bt_put(event_class);
			goto end;
		}
		bt_put(event_class);
		bt_put(writer_event_class);
	}

end:
	return ret;
}

static
enum bt_component_status copy_stream_class(FILE *err,
		struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream_class *writer_stream_class)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *type;
	int ret_int, clock_class_count, i;
	struct bt_ctf_trace *trace;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	clock_class_count = bt_ctf_trace_get_clock_class_count(trace);

	for (i = 0; i < clock_class_count; i++) {
		struct bt_ctf_clock_class *clock_class =
			bt_ctf_trace_get_clock_class(trace, i);

		if (!clock_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end_put_trace;
		}

		ret = copy_clock_class(err, writer, writer_stream_class, clock_class);
		bt_put(clock_class);
		if (ret != BT_COMPONENT_STATUS_OK) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto end_put_trace;
		}
	}

	type = bt_ctf_stream_class_get_packet_context_type(stream_class);
	if (!type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}

	ret_int = bt_ctf_stream_class_set_packet_context_type(
			writer_stream_class, type);
	bt_put(type);
	if (ret_int < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}

	type = bt_ctf_stream_class_get_event_header_type(stream_class);
	if (!type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}

	ret_int = bt_ctf_stream_class_set_event_header_type(
			writer_stream_class, type);
	bt_put(type);
	if (ret_int < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}

	type = bt_ctf_stream_class_get_event_context_type(stream_class);
	if (!type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}
	ret_int = bt_ctf_stream_class_set_event_context_type(
			writer_stream_class, type);
	bt_put(type);
	if (ret_int < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}

	ret = copy_event_classes(err, writer, stream_class, writer_stream_class);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(err, "[error] Failed to copy event classes\n");
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_trace;
	}

end_put_trace:
	bt_put(trace);
end:
	return ret;
}

static
enum bt_component_status copy_trace(FILE *err, struct bt_ctf_writer *ctf_writer,
		struct bt_ctf_trace *trace)
{
	struct bt_ctf_trace *writer_trace;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int field_count, i, int_ret;
	struct bt_ctf_field_type *header_type;

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	if (!writer_trace) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	field_count = bt_ctf_trace_get_environment_field_count(trace);
	for (i = 0; i < field_count; i++) {
		int ret_int;
		const char *name;
		struct bt_value *value;

		name = bt_ctf_trace_get_environment_field_name(trace, i);
		if (!name) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end_put_writer_trace;
		}
		value = bt_ctf_trace_get_environment_field_value(trace, i);
		if (!value) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end_put_writer_trace;
		}

		ret_int = bt_ctf_trace_set_environment_field(writer_trace,
				name, value);
		bt_put(value);
		if (ret_int < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			fprintf(err, "[error] Unable to set environment field %s\n",
					name);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end_put_writer_trace;
		}
	}

	header_type = bt_ctf_trace_get_packet_header_type(writer_trace);
	if (!header_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_put_writer_trace;
	}

	int_ret = bt_ctf_trace_set_packet_header_type(writer_trace, header_type);
	if (int_ret < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_put_header_type;
	}

end_put_header_type:
	bt_put(header_type);
end_put_writer_trace:
	bt_put(writer_trace);
end:
	return ret;
}

static
struct bt_ctf_stream_class *insert_new_stream_class(
		struct writer_component *writer_component,
		struct bt_ctf_writer *ctf_writer,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream_class *writer_stream_class;
	const char *name = bt_ctf_stream_class_get_name(stream_class);
	enum bt_component_status ret;

	if (strlen(name) == 0) {
		name = NULL;
	}

	writer_stream_class = bt_ctf_stream_class_create(name);
	if (!writer_stream_class) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	ret = copy_stream_class(writer_component->err,
			ctf_writer, stream_class, writer_stream_class);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(writer_component->err, "[error] Failed to copy stream class\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		BT_PUT(writer_stream_class);
		goto end;
	}
	g_hash_table_insert(writer_component->stream_class_map,
			(gpointer) stream_class, writer_stream_class);

end:
	return writer_stream_class;
}

static
struct bt_ctf_stream *insert_new_stream(
		struct writer_component *writer_component,
		struct bt_ctf_writer *ctf_writer,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream *writer_stream;
	struct bt_ctf_stream_class *writer_stream_class;

	writer_stream_class = g_hash_table_lookup(
			writer_component->stream_class_map,
			(gpointer) stream_class);
	if (writer_stream_class) {
		if (!bt_get(writer_stream_class)) {
			writer_stream = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end;
		}
	} else {
		writer_stream_class = insert_new_stream_class(
				writer_component, ctf_writer, stream_class);
		if (!writer_stream_class) {
			writer_stream = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end_put;
		}
	}

	writer_stream = bt_ctf_writer_create_stream(ctf_writer,
			writer_stream_class);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	g_hash_table_insert(writer_component->stream_map, (gpointer) stream,
			writer_stream);

	bt_ctf_writer_flush_metadata(ctf_writer);

end_put:
	bt_put(writer_stream_class);
end:
	return writer_stream;
}

static
struct bt_ctf_stream *lookup_stream(struct writer_component *writer_component,
		struct bt_ctf_stream *stream)
{
	return (struct bt_ctf_stream *) g_hash_table_lookup(
			writer_component->stream_map,
			(gpointer) stream);
}

static
struct bt_ctf_event_class *get_event_class(struct writer_component *writer_component,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_event_class *event_class)
{
	return bt_ctf_stream_class_get_event_class_by_name(writer_stream_class,
			bt_ctf_event_class_get_name(event_class));
}

struct bt_ctf_writer *insert_new_writer(
		struct writer_component *writer_component,
		struct bt_ctf_trace *trace)
{
	struct bt_ctf_writer *ctf_writer;
	char trace_name[PATH_MAX];
	enum bt_component_status ret;

	snprintf(trace_name, PATH_MAX, "%s/%s_%03d",
			writer_component->base_path->str,
			writer_component->trace_name_base->str,
			writer_component->trace_id++);
	printf_verbose("CTF-Writer creating trace in %s\n", trace_name);

	ctf_writer = bt_ctf_writer_create(trace_name);
	if (!ctf_writer) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	ret = copy_trace(writer_component->err, ctf_writer, trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(writer_component->err, "[error] Failed to copy trace\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		BT_PUT(ctf_writer);
		goto end;
	}

	g_hash_table_insert(writer_component->trace_map, (gpointer) trace,
			ctf_writer);

end:
	return ctf_writer;
}

static
struct bt_ctf_writer *get_writer(struct writer_component *writer_component,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_trace *trace;
	struct bt_ctf_writer *ctf_writer;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		ctf_writer = NULL;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	ctf_writer = g_hash_table_lookup(writer_component->trace_map,
			(gpointer) trace);
	if (ctf_writer) {
		if (!bt_get(ctf_writer)) {
			ctf_writer = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end;
		}
	} else {
		ctf_writer = insert_new_writer(writer_component, trace);
	}
	bt_put(trace);

end:
	return ctf_writer;
}

static
struct bt_ctf_stream *get_writer_stream(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet, struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class;
	struct bt_ctf_writer *ctf_writer;
	struct bt_ctf_stream *writer_stream;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		writer_stream = NULL;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	ctf_writer = get_writer(writer_component, stream_class);
	if (!ctf_writer) {
		writer_stream = NULL;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put_stream_class;
	}

	writer_stream = lookup_stream(writer_component, stream);

	if (writer_stream) {
		if (!bt_get(writer_stream)) {
			writer_stream = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end_put_stream_class;
		}
	} else {
		writer_stream = insert_new_stream(writer_component, ctf_writer,
				stream_class, stream);
		bt_get(writer_stream);
	}

	bt_put(ctf_writer);
end_put_stream_class:
	bt_put(stream_class);
end:
	return writer_stream;
}

BT_HIDDEN
enum bt_component_status writer_new_packet(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream, *writer_stream;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	/* TODO: copy values for event discarded and packet_seq_num */
	writer_stream = get_writer_stream(writer_component, packet, stream);
	if (!writer_stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	bt_put(writer_stream);

end_put:
	bt_put(stream);
end:
	return ret;
}

static
enum bt_component_status copy_packet_context_field(FILE *err,
		struct bt_ctf_field *field, const char *field_name,
		struct bt_ctf_field *writer_packet_context,
		struct bt_ctf_field_type *writer_packet_context_type)
{
	enum bt_component_status ret;
	struct bt_ctf_field *writer_field;
	int int_ret;
	uint64_t value;

	/*
	 * TODO: handle the special case of the first/last packet that might
	 * be trimmed. In these cases, the timestamp_begin/end need to be
	 * explicitely set to the first/last event timestamps.
	 */

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
		goto end_put_writer_field;
	}

	int_ret = bt_ctf_field_unsigned_integer_set_value(writer_field, value);
	if (int_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_writer_field;
	}

	ret = BT_COMPONENT_STATUS_OK;

end_put_writer_field:
	bt_put(writer_field);
end:
	return ret;
}

static
enum bt_component_status copy_packet_context(FILE *err,
		struct bt_ctf_packet *packet,
		struct bt_ctf_stream *writer_stream)
{
	enum bt_component_status ret;
	struct bt_ctf_field *packet_context, *writer_packet_context;
	struct bt_ctf_field_type *struct_type, *writer_packet_context_type;
	struct bt_ctf_stream_class *writer_stream_class;
	int nr_fields, i, int_ret;

	packet_context = bt_ctf_packet_get_context(packet);
	if (!packet_context) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	writer_stream_class = bt_ctf_stream_get_class(writer_stream);
	if (!writer_stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_packet_context;
	}

	writer_packet_context_type = bt_ctf_stream_class_get_packet_context_type(
			writer_stream_class);
	if (!writer_packet_context_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_writer_stream_class;
	}

	struct_type = bt_ctf_field_get_type(packet_context);
	if (!struct_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_writer_packet_context_type;
	}

	writer_packet_context = bt_ctf_field_create(writer_packet_context_type);
	if (!writer_packet_context) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_struct_type;
	}

	nr_fields = bt_ctf_field_type_structure_get_field_count(struct_type);
	for (i = 0; i < nr_fields; i++) {
		struct bt_ctf_field *field;
		struct bt_ctf_field_type *field_type;
		const char *field_name;

		field =  bt_ctf_field_structure_get_field_by_index(
				packet_context, i);
		if (!field) {
			ret = BT_COMPONENT_STATUS_ERROR;
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end_put_writer_packet_context;
		}
		if (bt_ctf_field_type_structure_get_field(struct_type,
					&field_name, &field_type, i) < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			bt_put(field);
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end_put_writer_packet_context;
		}

		if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_TYPE_ID_INTEGER) {
			fprintf(err, "[error] Unexpected packet context field type\n");
			bt_put(field);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end_put_writer_packet_context;
		}

		ret = copy_packet_context_field(err, field, field_name,
				writer_packet_context, writer_packet_context_type);
		bt_put(field_type);
		bt_put(field);
		if (ret != BT_COMPONENT_STATUS_OK) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end_put_writer_packet_context;
		}
	}

	int_ret = bt_ctf_stream_set_packet_context(writer_stream,
			writer_packet_context);
	if (int_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_put_writer_packet_context;
	}

end_put_writer_packet_context:
	bt_put(writer_packet_context);
end_put_struct_type:
	bt_put(struct_type);
end_put_writer_packet_context_type:
	bt_put(writer_packet_context_type);
end_put_writer_stream_class:
	bt_put(writer_stream_class);
end_put_packet_context:
	bt_put(packet_context);
end:
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_close_packet(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream, *writer_stream;
	enum bt_component_status ret;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	if (!bt_get(writer_stream)) {
		fprintf(writer_component->err,
				"[error] Failed to get reference on writer stream\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_put;
	}

	ret = copy_packet_context(writer_component->err, packet, writer_stream);
	if (ret != BT_COMPONENT_STATUS_OK) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	ret = bt_ctf_stream_flush(writer_stream);
	if (ret < 0) {
		fprintf(writer_component->err,
				"[error] Failed to flush packet\n");
		ret = BT_COMPONENT_STATUS_ERROR;
	}

	ret = BT_COMPONENT_STATUS_OK;

	bt_put(writer_stream);

end_put:
	bt_put(stream);
end:
	return ret;
}

static
struct bt_ctf_event *copy_event(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event_class *writer_event_class)
{
	struct bt_ctf_event *writer_event;
	struct bt_ctf_field *field, *copy_field;
	int ret;

	writer_event = bt_ctf_event_create(writer_event_class);
	if (!writer_event) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	field = bt_ctf_event_get_header(event);
	if (!field) {
		BT_PUT(writer_event);
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	copy_field = bt_ctf_field_copy(field);
	bt_put(field);
	if (copy_field) {
		ret = bt_ctf_event_set_header(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		bt_put(copy_field);
	}

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_stream_event_context(event);
	copy_field = bt_ctf_field_copy(field);
	bt_put(field);
	if (copy_field) {
		ret = bt_ctf_event_set_stream_event_context(writer_event,
				copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		bt_put(copy_field);
	}

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_event_context(event);
	copy_field = bt_ctf_field_copy(field);
	bt_put(field);
	if (copy_field) {
		ret = bt_ctf_event_set_event_context(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		bt_put(copy_field);
	}

	field = bt_ctf_event_get_payload_field(event);
	if (!field) {
		BT_PUT(writer_event);
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	copy_field = bt_ctf_field_copy(field);
	bt_put(field);
	if (copy_field) {
		ret = bt_ctf_event_set_payload_field(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		bt_put(copy_field);
	}
	goto end;

error:
	bt_put(copy_field);
	BT_PUT(writer_event);
end:
	return writer_event;
}

BT_HIDDEN
enum bt_component_status writer_output_event(
		struct writer_component *writer_component,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret;
	struct bt_ctf_event_class *event_class, *writer_event_class;
	struct bt_ctf_stream *stream, *writer_stream;
	struct bt_ctf_stream_class *stream_class, *writer_stream_class;
	struct bt_ctf_event *writer_event;
	const char *event_name;
	int int_ret;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	event_name = bt_ctf_event_class_get_name(event_class);
	if (!event_name) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_event_class;
	}

	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_event_class;
	}

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream || !bt_get(writer_stream)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_stream;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_writer_stream;
	}

	writer_stream_class = g_hash_table_lookup(
			writer_component->stream_class_map,
			(gpointer) stream_class);
	if (!writer_stream_class || !bt_get(writer_stream_class)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_stream_class;
	}

	writer_event_class = get_event_class(writer_component,
			writer_stream_class, event_class);
	if (!writer_event_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_writer_stream_class;
	}

	writer_event = copy_event(writer_component->err, event, writer_event_class);
	if (!writer_event) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(writer_component->err, "[error] Failed to copy event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto end_put_writer_event_class;
	}

	int_ret = bt_ctf_stream_append_event(writer_stream, writer_event);
	if (int_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(writer_component->err, "[error] Failed to append event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto end_put_writer_event;
	}

	ret = BT_COMPONENT_STATUS_OK;

end_put_writer_event:
	bt_put(writer_event);
end_put_writer_event_class:
	bt_put(writer_event_class);
end_put_writer_stream_class:
	bt_put(writer_stream_class);
end_put_stream_class:
	bt_put(stream_class);
end_put_writer_stream:
	bt_put(writer_stream);
end_put_stream:
	bt_put(stream);
end_put_event_class:
	bt_put(event_class);
end:
	return ret;
}
