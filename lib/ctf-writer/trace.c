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

#define BT_LOG_TAG "CTF-WRITER-TRACE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/trace-internal.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>

#define DEFAULT_IDENTIFIER_SIZE		128
#define DEFAULT_METADATA_STRING_SIZE	4096

static
void bt_ctf_trace_destroy(struct bt_object *obj)
{
	struct bt_ctf_trace *trace = (void *) obj;

	BT_LOGD("Destroying CTF writer trace object: addr=%p, name=\"%s\"",
		trace, bt_ctf_trace_get_name(trace));
	bt_trace_common_finalize(BT_TO_COMMON(trace));
	g_free(trace);
}

BT_HIDDEN
struct bt_ctf_trace *bt_ctf_trace_create(void)
{
	struct bt_ctf_trace *trace = NULL;
	int ret;

	BT_LOGD_STR("Creating CTF writer trace object.");
	trace = g_new0(struct bt_ctf_trace, 1);
	if (!trace) {
		BT_LOGE_STR("Failed to allocate one CTF writer trace.");
		goto error;
	}

	ret = bt_trace_common_initialize(BT_TO_COMMON(trace),
		bt_ctf_trace_destroy);
	if (ret) {
		/* bt_trace_common_initialize() logs errors */
		goto error;
	}

	BT_LOGD("Created CTF writer trace object: addr=%p", trace);
	return trace;

error:
	BT_PUT(trace);
	return trace;
}

const unsigned char *bt_ctf_trace_get_uuid(struct bt_ctf_trace *trace)
{
	return bt_trace_common_get_uuid(BT_TO_COMMON(trace));
}

int bt_ctf_trace_set_uuid(struct bt_ctf_trace *trace,
		const unsigned char *uuid)
{
	return bt_trace_common_set_uuid(BT_TO_COMMON(trace), uuid);
}

int bt_ctf_trace_set_environment_field(struct bt_ctf_trace *trace,
		const char *name, struct bt_value *value)
{
	return bt_trace_common_set_environment_field(BT_TO_COMMON(trace),
		name, value);
}

int bt_ctf_trace_set_environment_field_string(struct bt_ctf_trace *trace,
		const char *name, const char *value)
{
	return bt_trace_common_set_environment_field_string(BT_TO_COMMON(trace),
		name, value);
}

int bt_ctf_trace_set_environment_field_integer(
		struct bt_ctf_trace *trace, const char *name, int64_t value)
{
	return bt_trace_common_set_environment_field_integer(
		BT_TO_COMMON(trace), name, value);
}

int64_t bt_ctf_trace_get_environment_field_count(struct bt_ctf_trace *trace)
{
	return bt_trace_common_get_environment_field_count(BT_TO_COMMON(trace));
}

const char *
bt_ctf_trace_get_environment_field_name_by_index(struct bt_ctf_trace *trace,
		uint64_t index)
{
	return bt_trace_common_get_environment_field_name_by_index(
		BT_TO_COMMON(trace), index);
}

struct bt_value *bt_ctf_trace_get_environment_field_value_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_get(bt_trace_common_borrow_environment_field_value_by_index(
		BT_TO_COMMON(trace), index));
}

struct bt_value *bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace, const char *name)
{
	return bt_get(bt_trace_common_borrow_environment_field_value_by_name(
		BT_TO_COMMON(trace), name));
}

int bt_ctf_trace_add_clock_class(struct bt_ctf_trace *trace,
		struct bt_ctf_clock_class *clock_class)
{
	return bt_trace_common_add_clock_class(BT_TO_COMMON(trace),
		(void *) clock_class);
}

int64_t bt_ctf_trace_get_clock_class_count(struct bt_ctf_trace *trace)
{
	return bt_trace_common_get_clock_class_count(BT_TO_COMMON(trace));
}

struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_get(bt_trace_common_borrow_clock_class_by_index(
		BT_TO_COMMON(trace), index));
}

static
int map_clock_classes_func(struct bt_stream_class_common *stream_class,
		struct bt_field_type_common *packet_context_type,
		struct bt_field_type_common *event_header_type)
{
	int ret = bt_ctf_stream_class_map_clock_class(
		BT_FROM_COMMON(stream_class),
		BT_FROM_COMMON(packet_context_type),
		BT_FROM_COMMON(event_header_type));

	if (ret) {
		BT_LOGW_STR("Cannot automatically map selected stream class's field types to stream class's clock's class.");
	}

	return ret;
}

int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;
	struct bt_clock_class *expected_clock_class = NULL;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->clock) {
		struct bt_ctf_clock_class *stream_clock_class =
			stream_class->clock->clock_class;

		/*
		 * Make sure this clock was also added to the
		 * trace (potentially through its CTF writer
		 * owner).
		 */
		size_t i;

		for (i = 0; i < trace->common.clock_classes->len; i++) {
			if (trace->common.clock_classes->pdata[i] ==
					stream_clock_class) {
				/* Found! */
				break;
			}
		}

		if (i == trace->common.clock_classes->len) {
			/* Not found */
			BT_LOGW("Stream class's clock's class is not part of the trace: "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				stream_clock_class,
				bt_clock_class_get_name(
					BT_TO_COMMON(stream_clock_class)));
			ret = -1;
			goto end;
		}

		if (stream_class->common.clock_class &&
				stream_class->common.clock_class !=
				BT_TO_COMMON(stream_class->clock->clock_class)) {
			/*
			 * Stream class already has an expected clock
			 * class, but it does not match its clock's
			 * class.
			 */
			BT_LOGW("Invalid parameter: stream class's clock's "
				"class does not match stream class's "
				"expected clock class: "
				"stream-class-addr=%p, "
				"stream-class-id=%" PRId64 ", "
				"stream-class-name=\"%s\", "
				"expected-clock-class-addr=%p, "
				"expected-clock-class-name=\"%s\"",
				stream_class,
				bt_ctf_stream_class_get_id(stream_class),
				bt_ctf_stream_class_get_name(stream_class),
				stream_class->common.clock_class,
				bt_clock_class_get_name(stream_class->common.clock_class));
		} else if (!stream_class->common.clock_class) {
			/*
			 * Set expected clock class to stream class's
			 * clock's class.
			 */
			expected_clock_class =
				BT_TO_COMMON(stream_class->clock->clock_class);
		}
	}


	ret = bt_trace_common_add_stream_class(BT_TO_COMMON(trace),
		BT_TO_COMMON(stream_class),
		(bt_validation_flag_copy_field_type_func) bt_ctf_field_type_copy,
		expected_clock_class, map_clock_classes_func,
		false);

end:
	return ret;
}

int64_t bt_ctf_trace_get_stream_count(struct bt_ctf_trace *trace)
{
	return bt_trace_common_get_stream_count(BT_TO_COMMON(trace));
}

struct bt_ctf_stream *bt_ctf_trace_get_stream_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_get(bt_trace_common_borrow_stream_by_index(
		BT_TO_COMMON(trace), index));
}

int64_t bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace)
{
	return bt_trace_common_get_stream_class_count(BT_TO_COMMON(trace));
}

struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_get(bt_trace_common_borrow_stream_class_by_index(
		BT_TO_COMMON(trace), index));
}

struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace, uint64_t id)
{
	return bt_get(bt_trace_common_borrow_stream_class_by_id(
		BT_TO_COMMON(trace), id));
}

struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_name(
		struct bt_ctf_trace *trace, const char *name)
{
	return bt_get(
		bt_trace_common_borrow_clock_class_by_name(BT_TO_COMMON(trace),
			name));
}

static
int append_trace_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	unsigned char *uuid = trace->common.uuid;
	int ret = 0;

	if (trace->common.native_byte_order == BT_BYTE_ORDER_NATIVE ||
			trace->common.native_byte_order == BT_BYTE_ORDER_UNSPECIFIED) {
		BT_LOGW("Invalid parameter: trace's byte order cannot be BT_CTF_BYTE_ORDER_NATIVE or BT_CTF_BYTE_ORDER_UNSPECIFIED at this point; "
			"set it with bt_ctf_trace_set_native_byte_order(): "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_get_name(trace));
		ret = -1;
		goto end;
	}

	g_string_append(context->string, "trace {\n");
	g_string_append(context->string, "\tmajor = 1;\n");
	g_string_append(context->string, "\tminor = 8;\n");
	BT_ASSERT(trace->common.native_byte_order == BT_BYTE_ORDER_LITTLE_ENDIAN ||
		trace->common.native_byte_order == BT_BYTE_ORDER_BIG_ENDIAN ||
		trace->common.native_byte_order == BT_BYTE_ORDER_NETWORK);

	if (trace->common.uuid_set) {
		g_string_append_printf(context->string,
			"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
			uuid[0], uuid[1], uuid[2], uuid[3],
			uuid[4], uuid[5], uuid[6], uuid[7],
			uuid[8], uuid[9], uuid[10], uuid[11],
			uuid[12], uuid[13], uuid[14], uuid[15]);
	}

	g_string_append_printf(context->string, "\tbyte_order = %s;\n",
		get_byte_order_string(trace->common.native_byte_order));

	if (trace->common.packet_header_field_type) {
		g_string_append(context->string, "\tpacket.header := ");
		context->current_indentation_level++;
		g_string_assign(context->field_name, "");
		BT_LOGD_STR("Serializing trace's packet header field type's metadata.");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) trace->common.packet_header_field_type,
			context);
		if (ret) {
			goto end;
		}
		context->current_indentation_level--;
	}

	g_string_append(context->string, ";\n};\n\n");
end:
	return ret;
}

static
void append_env_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	int64_t i;
	int64_t env_size;

	env_size = bt_attributes_get_count(trace->common.environment);
	if (env_size <= 0) {
		return;
	}

	g_string_append(context->string, "env {\n");

	for (i = 0; i < env_size; i++) {
		struct bt_value *env_field_value_obj = NULL;
		const char *entry_name;

		entry_name = bt_attributes_get_field_name(
			trace->common.environment, i);
		env_field_value_obj = bt_attributes_borrow_field_value(
			trace->common.environment, i);

		BT_ASSERT(entry_name);
		BT_ASSERT(env_field_value_obj);

		switch (bt_value_get_type(env_field_value_obj)) {
		case BT_VALUE_TYPE_INTEGER:
		{
			int ret;
			int64_t int_value;

			ret = bt_value_integer_get(env_field_value_obj,
				&int_value);
			BT_ASSERT(ret == 0);
			g_string_append_printf(context->string,
				"\t%s = %" PRId64 ";\n", entry_name,
				int_value);
			break;
		}
		case BT_VALUE_TYPE_STRING:
		{
			int ret;
			const char *str_value;
			char *escaped_str = NULL;

			ret = bt_value_string_get(env_field_value_obj,
				&str_value);
			BT_ASSERT(ret == 0);
			escaped_str = g_strescape(str_value, NULL);
			if (!escaped_str) {
				BT_LOGE("Cannot escape string: string=\"%s\"",
					str_value);
				continue;
			}

			g_string_append_printf(context->string,
				"\t%s = \"%s\";\n", entry_name, escaped_str);
			free(escaped_str);
			break;
		}
		default:
			continue;
		}
	}

	g_string_append(context->string, "};\n\n");
}

char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace)
{
	char *metadata = NULL;
	struct metadata_context *context = NULL;
	int err = 0;
	size_t i;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		goto end;
	}

	context = g_new0(struct metadata_context, 1);
	if (!context) {
		BT_LOGE_STR("Failed to allocate one metadata context.");
		goto end;
	}

	context->field_name = g_string_sized_new(DEFAULT_IDENTIFIER_SIZE);
	context->string = g_string_sized_new(DEFAULT_METADATA_STRING_SIZE);
	g_string_append(context->string, "/* CTF 1.8 */\n\n");
	if (append_trace_metadata(trace, context)) {
		/* append_trace_metadata() logs errors */
		goto error;
	}
	append_env_metadata(trace, context);
	g_ptr_array_foreach(trace->common.clock_classes,
		(GFunc) bt_ctf_clock_class_serialize, context);

	for (i = 0; i < trace->common.stream_classes->len; i++) {
		/* bt_stream_class_serialize() logs details */
		err = bt_ctf_stream_class_serialize(
			trace->common.stream_classes->pdata[i], context);
		if (err) {
			/* bt_stream_class_serialize() logs errors */
			goto error;
		}
	}

	metadata = context->string->str;

error:
	g_string_free(context->string, err ? TRUE : FALSE);
	g_string_free(context->field_name, TRUE);
	g_free(context);

end:
	return metadata;
}

enum bt_ctf_byte_order bt_ctf_trace_get_native_byte_order(
		struct bt_ctf_trace *trace)
{
	return (int) bt_trace_common_get_native_byte_order(BT_TO_COMMON(trace));
}

int bt_ctf_trace_set_native_byte_order(struct bt_ctf_trace *trace,
		enum bt_ctf_byte_order byte_order)
{
	return bt_trace_common_set_native_byte_order(BT_TO_COMMON(trace),
		(int) byte_order, false);
}

struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_field_type(
		struct bt_ctf_trace *trace)
{
	return bt_get(bt_trace_common_borrow_packet_header_field_type(
		BT_TO_COMMON(trace)));
}

int bt_ctf_trace_set_packet_header_field_type(struct bt_ctf_trace *trace,
		struct bt_ctf_field_type *packet_header_type)
{
	return bt_trace_common_set_packet_header_field_type(BT_TO_COMMON(trace),
		(void *) packet_header_type);
}

const char *bt_ctf_trace_get_name(struct bt_ctf_trace *trace)
{
	return bt_trace_common_get_name(BT_TO_COMMON(trace));
}
