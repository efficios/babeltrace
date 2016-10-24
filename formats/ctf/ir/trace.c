/*
 * trace.c
 *
 * Babeltrace CTF IR - Trace
 *
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/compiler.h>
#include <babeltrace/values.h>
#include <babeltrace/ref.h>
#include <babeltrace/endian.h>

#define DEFAULT_IDENTIFIER_SIZE 128
#define DEFAULT_METADATA_STRING_SIZE 4096

static
void bt_ctf_trace_destroy(struct bt_object *obj);
static
int init_trace_packet_header(struct bt_ctf_trace *trace);
static
void bt_ctf_trace_freeze(struct bt_ctf_trace *trace);

static
const unsigned int field_type_aliases_alignments[] = {
	[FIELD_TYPE_ALIAS_UINT5_T] = 1,
	[FIELD_TYPE_ALIAS_UINT8_T ... FIELD_TYPE_ALIAS_UINT16_T] = 8,
	[FIELD_TYPE_ALIAS_UINT27_T] = 1,
	[FIELD_TYPE_ALIAS_UINT32_T ... FIELD_TYPE_ALIAS_UINT64_T] = 8,
};

static
const unsigned int field_type_aliases_sizes[] = {
	[FIELD_TYPE_ALIAS_UINT5_T] = 5,
	[FIELD_TYPE_ALIAS_UINT8_T] = 8,
	[FIELD_TYPE_ALIAS_UINT16_T] = 16,
	[FIELD_TYPE_ALIAS_UINT27_T] = 27,
	[FIELD_TYPE_ALIAS_UINT32_T] = 32,
	[FIELD_TYPE_ALIAS_UINT64_T] = 64,
};

BT_HIDDEN
struct bt_ctf_trace *bt_ctf_trace_create(void)
{
	struct bt_ctf_trace *trace = NULL;

	trace = g_new0(struct bt_ctf_trace, 1);
	if (!trace) {
		goto error;
	}

	bt_ctf_trace_set_byte_order(trace, BT_CTF_BYTE_ORDER_NATIVE);
	bt_object_init(trace, bt_ctf_trace_destroy);
	trace->clocks = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	trace->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_release);
	trace->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_release);
	if (!trace->clocks || !trace->stream_classes || !trace->streams) {
		goto error;
	}

	/* Generate a trace UUID */
	uuid_generate(trace->uuid);
	if (init_trace_packet_header(trace)) {
		goto error;
	}

	/* Create the environment array object */
	trace->environment = bt_ctf_attributes_create();
	if (!trace->environment) {
		goto error;
	}

	return trace;

error:
	BT_PUT(trace);
	return trace;
}

BT_HIDDEN
void bt_ctf_trace_destroy(struct bt_object *obj)
{
	struct bt_ctf_trace *trace;

	trace = container_of(obj, struct bt_ctf_trace, base);
	if (trace->environment) {
		bt_ctf_attributes_destroy(trace->environment);
	}

	if (trace->clocks) {
		g_ptr_array_free(trace->clocks, TRUE);
	}

	if (trace->streams) {
		g_ptr_array_free(trace->streams, TRUE);
	}

	if (trace->stream_classes) {
		g_ptr_array_free(trace->stream_classes, TRUE);
	}

	bt_put(trace->packet_header_type);
	g_free(trace);
}

BT_HIDDEN
int bt_ctf_trace_set_environment_field(struct bt_ctf_trace *trace,
		const char *name, struct bt_value *value)
{
	int ret = 0;

	if (!trace || !name || !value ||
		bt_ctf_validate_identifier(name) ||
		!(bt_value_is_integer(value) || bt_value_is_string(value))) {
		ret = -1;
		goto end;
	}

	if (strchr(name, ' ')) {
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		/*
		 * New environment fields may be added to a frozen trace,
		 * but existing fields may not be changed.
		 *
		 * The object passed is frozen like all other attributes.
		 */
		struct bt_value *attribute =
			bt_ctf_attributes_get_field_value_by_name(
				trace->environment, name);

		if (attribute) {
			BT_PUT(attribute);
			ret = -1;
			goto end;
		}

		bt_value_freeze(value);
	}

	ret = bt_ctf_attributes_set_field_value(trace->environment, name,
		value);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_set_environment_field_string(struct bt_ctf_trace *trace,
		const char *name, const char *value)
{
	int ret = 0;
	struct bt_value *env_value_string_obj = NULL;

	if (!trace || !name || !value) {
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		/*
		 * New environment fields may be added to a frozen trace,
		 * but existing fields may not be changed.
		 */
		struct bt_value *attribute =
			bt_ctf_attributes_get_field_value_by_name(
				trace->environment, name);

		if (attribute) {
			BT_PUT(attribute);
			ret = -1;
			goto end;
		}
	}

	env_value_string_obj = bt_value_string_create_init(value);

	if (!env_value_string_obj) {
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		bt_value_freeze(env_value_string_obj);
	}
	ret = bt_ctf_trace_set_environment_field(trace, name,
		env_value_string_obj);

end:
	BT_PUT(env_value_string_obj);
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_set_environment_field_integer(struct bt_ctf_trace *trace,
		const char *name, int64_t value)
{
	int ret = 0;
	struct bt_value *env_value_integer_obj = NULL;

	if (!trace || !name) {
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		/*
		 * New environment fields may be added to a frozen trace,
		 * but existing fields may not be changed.
		 */
		struct bt_value *attribute =
			bt_ctf_attributes_get_field_value_by_name(
				trace->environment, name);

		if (attribute) {
			BT_PUT(attribute);
			ret = -1;
			goto end;
		}
	}

	env_value_integer_obj = bt_value_integer_create_init(value);
	if (!env_value_integer_obj) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_trace_set_environment_field(trace, name,
		env_value_integer_obj);
	if (trace->frozen) {
		bt_value_freeze(env_value_integer_obj);
	}
end:
	BT_PUT(env_value_integer_obj);
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_get_environment_field_count(struct bt_ctf_trace *trace)
{
	int ret = 0;

	if (!trace) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_attributes_get_count(trace->environment);

end:
	return ret;
}

BT_HIDDEN
const char *
bt_ctf_trace_get_environment_field_name(struct bt_ctf_trace *trace,
		int index)
{
	const char *ret = NULL;

	if (!trace) {
		goto end;
	}

	ret = bt_ctf_attributes_get_field_name(trace->environment, index);

end:
	return ret;
}

BT_HIDDEN
struct bt_value *bt_ctf_trace_get_environment_field_value(
		struct bt_ctf_trace *trace, int index)
{
	struct bt_value *ret = NULL;

	if (!trace) {
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value(trace->environment, index);

end:
	return ret;
}

BT_HIDDEN
struct bt_value *bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace, const char *name)
{
	struct bt_value *ret = NULL;

	if (!trace || !name) {
		goto end;
	}

	ret = bt_ctf_attributes_get_field_value_by_name(trace->environment,
		name);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_add_clock(struct bt_ctf_trace *trace,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	struct search_query query = { .value = clock, .found = 0 };

	if (!trace || !clock) {
		ret = -1;
		goto end;
	}

	/* Check for duplicate clocks */
	g_ptr_array_foreach(trace->clocks, value_exists, &query);
	if (query.found) {
		ret = -1;
		goto end;
	}

	bt_get(clock);
	g_ptr_array_add(trace->clocks, clock);

	if (!trace->is_created_by_writer) {
		/*
		 * Non-writer mode trace: disable clock value functions
		 * because clock values are per-stream in that
		 * situation.
		 */
		clock->has_value = 0;
	}

	if (trace->frozen) {
		bt_ctf_clock_freeze(clock);
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_get_clock_count(struct bt_ctf_trace *trace)
{
	int ret = -1;

	if (!trace) {
		goto end;
	}

	ret = trace->clocks->len;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_clock *bt_ctf_trace_get_clock(struct bt_ctf_trace *trace,
		int index)
{
	struct bt_ctf_clock *clock = NULL;

	if (!trace || index < 0 || index >= trace->clocks->len) {
		goto end;
	}

	clock = g_ptr_array_index(trace->clocks, index);
	bt_get(clock);
end:
	return clock;
}

BT_HIDDEN
int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class)
{
	int ret, i;
	int64_t stream_id;
	struct bt_ctf_validation_output trace_sc_validation_output = { 0 };
	struct bt_ctf_validation_output *ec_validation_outputs = NULL;
	const enum bt_ctf_validation_flag trace_sc_validation_flags =
		BT_CTF_VALIDATION_FLAG_TRACE |
		BT_CTF_VALIDATION_FLAG_STREAM;
	const enum bt_ctf_validation_flag ec_validation_flags =
		BT_CTF_VALIDATION_FLAG_EVENT;
	struct bt_ctf_field_type *packet_header_type = NULL;
	struct bt_ctf_field_type *packet_context_type = NULL;
	struct bt_ctf_field_type *event_header_type = NULL;
	struct bt_ctf_field_type *stream_event_ctx_type = NULL;
	int event_class_count;
	struct bt_ctf_clock *clock_to_add_to_trace = NULL;

	if (!trace || !stream_class) {
		ret = -1;
		goto end;
	}

	event_class_count =
		bt_ctf_stream_class_get_event_class_count(stream_class);
	assert(event_class_count >= 0);

	/* Check for duplicate stream classes */
	for (i = 0; i < trace->stream_classes->len; i++) {
		if (trace->stream_classes->pdata[i] == stream_class) {
			/* Stream class already registered to the trace */
			ret = -1;
			goto end;
		}
	}

	/*
	 * If the stream class has a clock, register this clock to this
	 * trace if not already done.
	 */
	if (stream_class->clock) {
		const char *clock_name =
			bt_ctf_clock_get_name(stream_class->clock);
		struct bt_ctf_clock *trace_clock;

		assert(clock_name);
		trace_clock = bt_ctf_trace_get_clock_by_name(trace, clock_name);
		bt_put(trace_clock);
		if (trace_clock) {
			if (trace_clock != stream_class->clock) {
				/*
				 * Error: two different clocks in the
				 * trace would share the same name.
				 */
				ret = -1;
				goto end;
			}
		} else {
			clock_to_add_to_trace = bt_get(stream_class->clock);
		}
	}

	/*
	 * We're about to freeze both the trace and the stream class.
	 * Also, each event class contained in this stream class are
	 * already frozen.
	 *
	 * This trace, this stream class, and all its event classes
	 * should be valid at this point.
	 *
	 * Validate trace and stream class first, then each event
	 * class of this stream class can be validated individually.
	 */
	packet_header_type =
		bt_ctf_trace_get_packet_header_type(trace);
	packet_context_type =
		bt_ctf_stream_class_get_packet_context_type(stream_class);
	event_header_type =
		bt_ctf_stream_class_get_event_header_type(stream_class);
	stream_event_ctx_type =
		bt_ctf_stream_class_get_event_context_type(stream_class);
	ret = bt_ctf_validate_class_types(trace->environment,
		packet_header_type, packet_context_type, event_header_type,
		stream_event_ctx_type, NULL, NULL, trace->valid,
		stream_class->valid, 1, &trace_sc_validation_output,
		trace_sc_validation_flags);
	BT_PUT(packet_header_type);
	BT_PUT(packet_context_type);
	BT_PUT(event_header_type);
	BT_PUT(stream_event_ctx_type);

	if (ret) {
		/*
		 * This means something went wrong during the validation
		 * process, not that the objects are invalid.
		 */
		goto end;
	}

	if ((trace_sc_validation_output.valid_flags &
			trace_sc_validation_flags) !=
			trace_sc_validation_flags) {
		/* Invalid trace/stream class */
		ret = -1;
		goto end;
	}

	if (event_class_count > 0) {
		ec_validation_outputs = g_new0(struct bt_ctf_validation_output,
			event_class_count);
		if (!ec_validation_outputs) {
			ret = -1;
			goto end;
		}
	}

	/* Validate each event class individually */
	for (i = 0; i < event_class_count; i++) {
		struct bt_ctf_event_class *event_class =
			bt_ctf_stream_class_get_event_class(stream_class, i);
		struct bt_ctf_field_type *event_context_type = NULL;
		struct bt_ctf_field_type *event_payload_type = NULL;

		event_context_type =
			bt_ctf_event_class_get_context_type(event_class);
		event_payload_type =
			bt_ctf_event_class_get_payload_type(event_class);

		/*
		 * It is important to use the field types returned by
		 * the previous trace and stream class validation here
		 * because copies could have been made.
		 */
		ret = bt_ctf_validate_class_types(trace->environment,
			trace_sc_validation_output.packet_header_type,
			trace_sc_validation_output.packet_context_type,
			trace_sc_validation_output.event_header_type,
			trace_sc_validation_output.stream_event_ctx_type,
			event_context_type, event_payload_type,
			1, 1, event_class->valid, &ec_validation_outputs[i],
			ec_validation_flags);
		BT_PUT(event_context_type);
		BT_PUT(event_payload_type);
		BT_PUT(event_class);

		if (ret) {
			goto end;
		}

		if ((ec_validation_outputs[i].valid_flags &
				ec_validation_flags) != ec_validation_flags) {
			/* Invalid event class */
			ret = -1;
			goto end;
		}
	}

	stream_id = bt_ctf_stream_class_get_id(stream_class);
	if (stream_id < 0) {
		stream_id = trace->next_stream_id++;

		/* Try to assign a new stream id */
		for (i = 0; i < trace->stream_classes->len; i++) {
			if (stream_id == bt_ctf_stream_class_get_id(
				trace->stream_classes->pdata[i])) {
				/* Duplicate stream id found */
				ret = -1;
				goto end;
			}
		}

		if (bt_ctf_stream_class_set_id_no_check(stream_class,
			stream_id)) {
			/* TODO Should retry with a different stream id */
			ret = -1;
			goto end;
		}
	}

	bt_object_set_parent(stream_class, trace);
	g_ptr_array_add(trace->stream_classes, stream_class);

	/*
	 * At this point we know that the function will be successful.
	 * Therefore we can replace the trace and stream class field
	 * types with what's in their validation output structure and
	 * mark them as valid. We can also replace the field types of
	 * all the event classes of the stream class and mark them as
	 * valid.
	 */
	bt_ctf_validation_replace_types(trace, stream_class, NULL,
		&trace_sc_validation_output, trace_sc_validation_flags);
	trace->valid = 1;
	stream_class->valid = 1;

	/*
	 * Put what was not moved in bt_ctf_validation_replace_types().
	 */
	bt_ctf_validation_output_put_types(&trace_sc_validation_output);

	for (i = 0; i < event_class_count; i++) {
		struct bt_ctf_event_class *event_class =
			bt_ctf_stream_class_get_event_class(stream_class, i);

		bt_ctf_validation_replace_types(NULL, NULL, event_class,
			&ec_validation_outputs[i], ec_validation_flags);
		event_class->valid = 1;
		BT_PUT(event_class);

		/*
		 * Put what was not moved in
		 * bt_ctf_validation_replace_types().
		 */
		bt_ctf_validation_output_put_types(&ec_validation_outputs[i]);
	}

	/*
	 * All field type byte orders set as "native" byte ordering can now be
	 * safely set to trace's own endianness, including the stream class'.
	 */
	bt_ctf_field_type_set_native_byte_order(trace->packet_header_type,
		trace->byte_order);
	bt_ctf_stream_class_set_byte_order(stream_class, trace->byte_order);

	/* Add stream class's clock if it exists */
	if (clock_to_add_to_trace) {
		int add_clock_ret =
			bt_ctf_trace_add_clock(trace, clock_to_add_to_trace);
		assert(add_clock_ret == 0);
	}

	/*
	 * Freeze the trace and the stream class.
	 */
	bt_ctf_stream_class_freeze(stream_class);
	bt_ctf_trace_freeze(trace);

end:
	if (ret) {
		bt_object_set_parent(stream_class, NULL);

		if (ec_validation_outputs) {
			for (i = 0; i < event_class_count; i++) {
				bt_ctf_validation_output_put_types(
					&ec_validation_outputs[i]);
			}
		}
	}

	g_free(ec_validation_outputs);
	bt_ctf_validation_output_put_types(&trace_sc_validation_output);
	BT_PUT(clock_to_add_to_trace);
	assert(!packet_header_type);
	assert(!packet_context_type);
	assert(!event_header_type);
	assert(!stream_event_ctx_type);

	return ret;
}

BT_HIDDEN
int bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace)
{
	int ret;

	if (!trace) {
		ret = -1;
		goto end;
	}

	ret = trace->stream_classes->len;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class(
		struct bt_ctf_trace *trace, int index)
{
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!trace || index < 0 || index >= trace->stream_classes->len) {
		goto end;
	}

	stream_class = g_ptr_array_index(trace->stream_classes, index);
	bt_get(stream_class);
end:
	return stream_class;
}

BT_HIDDEN
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace, uint32_t id)
{
	int i;
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!trace) {
		goto end;
	}

	for (i = 0; i < trace->stream_classes->len; i++) {
		struct bt_ctf_stream_class *stream_class_candidate;

		stream_class_candidate =
			g_ptr_array_index(trace->stream_classes, i);

		if (bt_ctf_stream_class_get_id(stream_class_candidate) ==
				(int64_t) id) {
			stream_class = stream_class_candidate;
			bt_get(stream_class);
			goto end;
		}
	}

end:
	return stream_class;
}

BT_HIDDEN
struct bt_ctf_clock *bt_ctf_trace_get_clock_by_name(
		struct bt_ctf_trace *trace, const char *name)
{
	size_t i;
	struct bt_ctf_clock *clock = NULL;

	if (!trace || !name) {
		goto end;
	}

	for (i = 0; i < trace->clocks->len; i++) {
		struct bt_ctf_clock *cur_clk =
			g_ptr_array_index(trace->clocks, i);
		const char *cur_clk_name = bt_ctf_clock_get_name(cur_clk);

		if (!cur_clk_name) {
			goto end;
		}

		if (!strcmp(cur_clk_name, name)) {
			clock = cur_clk;
			bt_get(clock);
			goto end;
		}
	}

end:
	return clock;
}

BT_HIDDEN
const char *get_byte_order_string(int byte_order)
{
	const char *string;

	switch (byte_order) {
	case LITTLE_ENDIAN:
		string = "le";
		break;
	case BIG_ENDIAN:
		string = "be";
		break;
	default:
		string = "unknown";
		break;
	}

	return string;
}

static
int append_trace_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	unsigned char *uuid = trace->uuid;
	int ret;

	g_string_append(context->string, "trace {\n");

	g_string_append(context->string, "\tmajor = 1;\n");
	g_string_append(context->string, "\tminor = 8;\n");

	g_string_append_printf(context->string,
		"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15]);
	g_string_append_printf(context->string, "\tbyte_order = %s;\n",
		get_byte_order_string(trace->byte_order));

	g_string_append(context->string, "\tpacket.header := ");
	context->current_indentation_level++;
	g_string_assign(context->field_name, "");
	ret = bt_ctf_field_type_serialize(trace->packet_header_type,
		context);
	if (ret) {
		goto end;
	}
	context->current_indentation_level--;

	g_string_append(context->string, ";\n};\n\n");
end:
	return ret;
}

static
void append_env_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	int i;
	int env_size;

	env_size = bt_ctf_attributes_get_count(trace->environment);

	if (env_size <= 0) {
		return;
	}

	g_string_append(context->string, "env {\n");

	for (i = 0; i < env_size; i++) {
		struct bt_value *env_field_value_obj = NULL;
		const char *entry_name;

		entry_name = bt_ctf_attributes_get_field_name(
			trace->environment, i);
		env_field_value_obj = bt_ctf_attributes_get_field_value(
			trace->environment, i);

		if (!entry_name || !env_field_value_obj) {
			goto loop_next;
		}

		switch (bt_value_get_type(env_field_value_obj)) {
		case BT_VALUE_TYPE_INTEGER:
		{
			int ret;
			int64_t int_value;

			ret = bt_value_integer_get(env_field_value_obj,
				&int_value);

			if (ret) {
				goto loop_next;
			}

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

			if (ret) {
				goto loop_next;
			}

			escaped_str = g_strescape(str_value, NULL);

			if (!escaped_str) {
				goto loop_next;
			}

			g_string_append_printf(context->string,
				"\t%s = \"%s\";\n", entry_name, escaped_str);
			free(escaped_str);
			break;
		}

		default:
			goto loop_next;
		}

loop_next:
		BT_PUT(env_field_value_obj);
	}

	g_string_append(context->string, "};\n\n");
}

BT_HIDDEN
char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace)
{
	char *metadata = NULL;
	struct metadata_context *context = NULL;
	int err = 0;
	size_t i;

	if (!trace) {
		goto end;
	}

	context = g_new0(struct metadata_context, 1);
	if (!context) {
		goto end;
	}

	context->field_name = g_string_sized_new(DEFAULT_IDENTIFIER_SIZE);
	context->string = g_string_sized_new(DEFAULT_METADATA_STRING_SIZE);
	g_string_append(context->string, "/* CTF 1.8 */\n\n");
	if (append_trace_metadata(trace, context)) {
		goto error;
	}
	append_env_metadata(trace, context);
	g_ptr_array_foreach(trace->clocks,
		(GFunc)bt_ctf_clock_serialize, context);

	for (i = 0; i < trace->stream_classes->len; i++) {
		err = bt_ctf_stream_class_serialize(
			trace->stream_classes->pdata[i], context);
		if (err) {
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

BT_HIDDEN
enum bt_ctf_byte_order bt_ctf_trace_get_byte_order(struct bt_ctf_trace *trace)
{
	enum bt_ctf_byte_order ret = BT_CTF_BYTE_ORDER_UNKNOWN;

	if (!trace) {
		goto end;
	}

	switch (trace->byte_order) {
	case BIG_ENDIAN:
		ret = BT_CTF_BYTE_ORDER_BIG_ENDIAN;
		break;
	case LITTLE_ENDIAN:
		ret = BT_CTF_BYTE_ORDER_LITTLE_ENDIAN;
		break;
	default:
		break;
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_set_byte_order(struct bt_ctf_trace *trace,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;
	int internal_byte_order;

	if (!trace || trace->frozen) {
		ret = -1;
		goto end;
	}

	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_NATIVE:
		/*
		 * This doesn't make sense since the CTF specification defines
		 * the "native" byte order as "the byte order described in the
		 * trace description". However, this behavior had been
		 * implemented as part of v1.2 and is kept to maintain
		 * compatibility.
		 *
		 * This may be changed on a major version bump only.
		 */
		internal_byte_order = (G_BYTE_ORDER == G_LITTLE_ENDIAN) ?
			LITTLE_ENDIAN : BIG_ENDIAN;
		break;
	case BT_CTF_BYTE_ORDER_LITTLE_ENDIAN:
		internal_byte_order = LITTLE_ENDIAN;
		break;
	case BT_CTF_BYTE_ORDER_BIG_ENDIAN:
	case BT_CTF_BYTE_ORDER_NETWORK:
		internal_byte_order = BIG_ENDIAN;
		break;
	default:
		ret = -1;
		goto end;
	}

	trace->byte_order = internal_byte_order;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
		struct bt_ctf_trace *trace)
{
	struct bt_ctf_field_type *field_type = NULL;

	if (!trace) {
		goto end;
	}

	bt_get(trace->packet_header_type);
	field_type = trace->packet_header_type;
end:
	return field_type;
}

BT_HIDDEN
int bt_ctf_trace_set_packet_header_type(struct bt_ctf_trace *trace,
		struct bt_ctf_field_type *packet_header_type)
{
	int ret = 0;

	if (!trace || !packet_header_type || trace->frozen) {
		ret = -1;
		goto end;
	}

	/* packet_header_type must be a structure */
	if (bt_ctf_field_type_get_type_id(packet_header_type) !=
		CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	bt_get(packet_header_type);
	bt_put(trace->packet_header_type);
	trace->packet_header_type = packet_header_type;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *get_field_type(enum field_type_alias alias)
{
	int ret;
	unsigned int alignment, size;
	struct bt_ctf_field_type *field_type = NULL;

	if (alias >= NR_FIELD_TYPE_ALIAS) {
		goto end;
	}

	alignment = field_type_aliases_alignments[alias];
	size = field_type_aliases_sizes[alias];
	field_type = bt_ctf_field_type_integer_create(size);
	ret = bt_ctf_field_type_set_alignment(field_type, alignment);
	if (ret) {
		BT_PUT(field_type);
	}
end:
	return field_type;
}

static
void bt_ctf_trace_freeze(struct bt_ctf_trace *trace)
{
	int i;

	bt_ctf_field_type_freeze(trace->packet_header_type);
	bt_ctf_attributes_freeze(trace->environment);

	for (i = 0; i < trace->clocks->len; i++) {
		struct bt_ctf_clock *clock =
			g_ptr_array_index(trace->clocks, i);

		bt_ctf_clock_freeze(clock);
	}

	trace->frozen = 1;
}

static
int init_trace_packet_header(struct bt_ctf_trace *trace)
{
	int ret = 0;
	struct bt_ctf_field *magic = NULL, *uuid_array = NULL;
	struct bt_ctf_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_ctf_field_type *_uint8_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT8_T);
	struct bt_ctf_field_type *trace_packet_header_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *uuid_array_type =
		bt_ctf_field_type_array_create(_uint8_t, 16);

	if (!trace_packet_header_type || !uuid_array_type) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(trace_packet_header_type,
		_uint32_t, "magic");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(trace_packet_header_type,
		uuid_array_type, "uuid");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(trace_packet_header_type,
		_uint32_t, "stream_id");
	if (ret) {
		goto end;
	}

	ret = bt_ctf_trace_set_packet_header_type(trace,
		trace_packet_header_type);
	if (ret) {
		goto end;
	}
end:
	bt_put(uuid_array_type);
	bt_put(_uint32_t);
	bt_put(_uint8_t);
	bt_put(magic);
	bt_put(uuid_array);
	bt_put(trace_packet_header_type);
	return ret;
}
