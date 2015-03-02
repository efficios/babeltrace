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
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/compiler.h>

#define DEFAULT_IDENTIFIER_SIZE 128
#define DEFAULT_METADATA_STRING_SIZE 4096

static
void environment_variable_destroy(struct environment_variable *var);
static
void bt_ctf_trace_destroy(struct bt_ctf_ref *ref);
static
int init_trace_packet_header(struct bt_ctf_trace *trace);

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

struct bt_ctf_trace *bt_ctf_trace_create(void)
{
	struct bt_ctf_trace *trace = NULL;

	trace = g_new0(struct bt_ctf_trace, 1);
	if (!trace) {
		goto error;
	}

	bt_ctf_trace_set_byte_order(trace, BT_CTF_BYTE_ORDER_NATIVE);
	bt_ctf_ref_init(&trace->ref_count);
	trace->environment = g_ptr_array_new_with_free_func(
		(GDestroyNotify)environment_variable_destroy);
	trace->clocks = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_clock_put);
	trace->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_stream_put);
	trace->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_stream_class_put);
	if (!trace->environment || !trace->clocks ||
		!trace->stream_classes || !trace->streams) {
		goto error_destroy;
	}

	/* Generate a trace UUID */
	uuid_generate(trace->uuid);
	if (init_trace_packet_header(trace)) {
		goto error_destroy;
	}

	return trace;

error_destroy:
	bt_ctf_trace_destroy(&trace->ref_count);
	trace = NULL;
error:
	return trace;
}

void bt_ctf_trace_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_trace *trace;

	if (!ref) {
		return;
	}

	trace = container_of(ref, struct bt_ctf_trace, ref_count);
	if (trace->environment) {
		g_ptr_array_free(trace->environment, TRUE);
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

	bt_ctf_field_type_put(trace->packet_header_type);
	g_free(trace);
}

struct bt_ctf_stream *bt_ctf_trace_create_stream(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class)
{
	int ret;
	int stream_class_found = 0;
	size_t i;
	struct bt_ctf_stream *stream = NULL;

	if (!trace || !stream_class) {
		goto error;
	}

	stream = bt_ctf_stream_create(stream_class, trace);
	if (!stream) {
		goto error;
	}

	for (i = 0; i < trace->stream_classes->len; i++) {
		if (trace->stream_classes->pdata[i] == stream_class) {
			stream_class_found = 1;
		}
	}

	if (!stream_class_found) {
		int64_t stream_id = bt_ctf_stream_class_get_id(stream_class);

		if (stream_id < 0) {
			/* Try to assign a new stream id */
			if (bt_ctf_stream_class_set_id(stream->stream_class,
				trace->next_stream_id++)) {
				goto error;
			}
		}

		for (i = 0; i < trace->stream_classes->len; i++) {
			if (stream_id == bt_ctf_stream_class_get_id(
				    trace->stream_classes->pdata[i])) {
				/* Duplicate stream id found */
				goto error;
			}
		}
		bt_ctf_stream_class_get(stream->stream_class);
		g_ptr_array_add(trace->stream_classes, stream->stream_class);
	}

	bt_ctf_stream_get(stream);
	g_ptr_array_add(trace->streams, stream);

	/*
	 * Freeze the trace and its packet header.
	 *
	 * All field type byte orders set as "native" byte ordering can now be
	 * safely set to trace's own endianness, including the stream class'.
	 */
	bt_ctf_field_type_set_native_byte_order(trace->packet_header_type,
		trace->byte_order);
	ret = bt_ctf_stream_class_set_byte_order(stream_class,
		trace->byte_order == LITTLE_ENDIAN ?
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN : BT_CTF_BYTE_ORDER_BIG_ENDIAN);
	if (ret) {
		goto error;
	}

	bt_ctf_stream_class_freeze(stream_class);
	trace->frozen = 1;
	return stream;
error:
	bt_ctf_stream_put(stream);
	return NULL;
}

int bt_ctf_trace_add_environment_field(struct bt_ctf_trace *trace,
		const char *name,
		const char *value)
{
	struct environment_variable *var = NULL;
	char *escaped_value = NULL;
	int ret = 0;

	if (!trace || !name || !value || bt_ctf_validate_identifier(name)) {
		ret = -1;
		goto error;
	}

	if (strchr(name, ' ')) {
		ret = -1;
		goto error;
	}

	var = g_new0(struct environment_variable, 1);
	if (!var) {
		ret = -1;
		goto error;
	}

	var->type = BT_ENVIRONMENT_FIELD_TYPE_STRING;
	escaped_value = g_strescape(value, NULL);
	if (!escaped_value) {
		ret = -1;
		goto error;
	}

	var->name = g_string_new(name);
	var->value.string = g_string_new(escaped_value);
	g_free(escaped_value);
	if (!var->name || !var->value.string) {
		ret = -1;
		goto error;
	}

	g_ptr_array_add(trace->environment, var);
	return ret;

error:
	if (var && var->name) {
		g_string_free(var->name, TRUE);
	}

	if (var && var->value.string) {
		g_string_free(var->value.string, TRUE);
	}

	g_free(var);
	return ret;
}

int bt_ctf_trace_add_environment_field_integer(struct bt_ctf_trace *trace,
		const char *name,
	        int64_t value)
{
	struct environment_variable *var = NULL;
	int ret = 0;

	if (!trace || !name) {
		ret = -1;
		goto error;
	}

	var = g_new0(struct environment_variable, 1);
	if (!var) {
		ret = -1;
		goto error;
	}

	var->type = BT_ENVIRONMENT_FIELD_TYPE_INTEGER;
	var->name = g_string_new(name);
	var->value.integer = value;
	if (!var->name) {
		ret = -1;
		goto error;
	}

	g_ptr_array_add(trace->environment, var);
	return ret;

error:
	if (var && var->name) {
		g_string_free(var->name, TRUE);
	}

	g_free(var);
	return ret;
}

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

	bt_ctf_clock_get(clock);
	g_ptr_array_add(trace->clocks, clock);
end:
	return ret;
}

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

struct bt_ctf_clock *bt_ctf_trace_get_clock(struct bt_ctf_trace *trace,
		int index)
{
	struct bt_ctf_clock *clock = NULL;

	if (!trace || index < 0 || index >= trace->clocks->len) {
		goto end;
	}

	clock = g_ptr_array_index(trace->clocks, index);
	bt_ctf_clock_get(clock);
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
void append_env_field_metadata(struct environment_variable *var,
		struct metadata_context *context)
{
	switch (var->type) {
	case BT_ENVIRONMENT_FIELD_TYPE_STRING:
		g_string_append_printf(context->string, "\t%s = \"%s\";\n",
			var->name->str, var->value.string->str);
		break;
	case BT_ENVIRONMENT_FIELD_TYPE_INTEGER:
		g_string_append_printf(context->string, "\t%s = %" PRId64 ";\n",
			var->name->str, var->value.integer);
		break;
	default:
		assert(0);
	}
}

static
void append_env_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	if (trace->environment->len == 0) {
		return;
	}

	g_string_append(context->string, "env {\n");
	g_ptr_array_foreach(trace->environment,
		(GFunc)append_env_field_metadata, context);
	g_string_append(context->string, "};\n\n");
}

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

struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
		struct bt_ctf_trace *trace)
{
	struct bt_ctf_field_type *field_type = NULL;

	if (!trace) {
		goto end;
	}

	bt_ctf_field_type_get(trace->packet_header_type);
	field_type = trace->packet_header_type;
end:
	return field_type;
}

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

	bt_ctf_field_type_get(packet_header_type);
	bt_ctf_field_type_put(trace->packet_header_type);
	trace->packet_header_type = packet_header_type;
end:
	return ret;
}

void bt_ctf_trace_get(struct bt_ctf_trace *trace)
{
	if (!trace) {
		return;
	}

	bt_ctf_ref_get(&trace->ref_count);
}

void bt_ctf_trace_put(struct bt_ctf_trace *trace)
{
	if (!trace) {
		return;
	}

	bt_ctf_ref_put(&trace->ref_count, bt_ctf_trace_destroy);
}

BT_HIDDEN
struct bt_ctf_field_type *get_field_type(enum field_type_alias alias)
{
	unsigned int alignment, size;
	struct bt_ctf_field_type *field_type;

	if (alias >= NR_FIELD_TYPE_ALIAS) {
		return NULL;
	}

	alignment = field_type_aliases_alignments[alias];
	size = field_type_aliases_sizes[alias];
	field_type = bt_ctf_field_type_integer_create(size);
	bt_ctf_field_type_set_alignment(field_type, alignment);
	return field_type;
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
	bt_ctf_field_type_put(uuid_array_type);
	bt_ctf_field_type_put(_uint32_t);
	bt_ctf_field_type_put(_uint8_t);
	bt_ctf_field_put(magic);
	bt_ctf_field_put(uuid_array);
	bt_ctf_field_type_put(trace_packet_header_type);

	return ret;
}

static
void environment_variable_destroy(struct environment_variable *var)
{
	g_string_free(var->name, TRUE);
	if (var->type == BT_ENVIRONMENT_FIELD_TYPE_STRING) {
		g_string_free(var->value.string, TRUE);
	}
	g_free(var);
}
