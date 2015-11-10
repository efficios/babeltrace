/*
 * writer.c
 *
 * Babeltrace CTF Writer
 *
 * Copyright 2013 EfficiOS Inc.
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

#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/compiler.h>
#include <babeltrace/endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#define DEFAULT_IDENTIFIER_SIZE 128
#define DEFAULT_METADATA_STRING_SIZE 4096

static
void environment_variable_destroy(struct environment_variable *var);
static
void bt_ctf_writer_destroy(struct bt_ctf_ref *ref);
static
int init_trace_packet_header(struct bt_ctf_writer *writer);
static
int create_stream_file(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream);
static
void stream_flush_cb(struct bt_ctf_stream *stream,
		struct bt_ctf_writer *writer);

static
const char * const reserved_keywords_str[] = {"align", "callsite",
	"const", "char", "clock", "double", "enum", "env", "event",
	"floating_point", "float", "integer", "int", "long", "short", "signed",
	"stream", "string", "struct", "trace", "typealias", "typedef",
	"unsigned", "variant", "void" "_Bool", "_Complex", "_Imaginary"};

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

static GHashTable *reserved_keywords_set;
static int init_done;
static int global_data_refcount;

struct bt_ctf_writer *bt_ctf_writer_create(const char *path)
{
	struct bt_ctf_writer *writer = NULL;

	if (!path) {
		goto error;
	}

	writer = g_new0(struct bt_ctf_writer, 1);
	if (!writer) {
		goto error;
	}

	bt_ctf_writer_set_byte_order(writer, BT_CTF_BYTE_ORDER_NATIVE);
	bt_ctf_ref_init(&writer->ref_count);
	writer->path = g_string_new(path);
	if (!writer->path) {
		goto error_destroy;
	}

	/* Create trace directory if necessary and open a metadata file */
	if (g_mkdir_with_parents(path, S_IRWXU | S_IRWXG)) {
		perror("g_mkdir_with_parents");
		goto error_destroy;
	}

	writer->trace_dir_fd = open(path, O_RDONLY, S_IRWXU | S_IRWXG);
	if (writer->trace_dir_fd < 0) {
		perror("open");
		goto error_destroy;
	}

	writer->metadata_fd = openat(writer->trace_dir_fd, "metadata",
		O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	writer->environment = g_ptr_array_new_with_free_func(
		(GDestroyNotify)environment_variable_destroy);
	writer->clocks = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_clock_put);
	writer->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_stream_put);
	writer->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_stream_class_put);
	if (!writer->environment || !writer->clocks ||
		!writer->stream_classes || !writer->streams) {
		goto error_destroy;
	}

	/* Generate a trace UUID */
	uuid_generate(writer->uuid);
	if (init_trace_packet_header(writer)) {
		goto error_destroy;
	}

	return writer;
error_destroy:
	unlinkat(writer->trace_dir_fd, "metadata", 0);
	bt_ctf_writer_destroy(&writer->ref_count);
	writer = NULL;
error:
	return writer;
}

void bt_ctf_writer_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_writer *writer;

	if (!ref) {
		return;
	}

	writer = container_of(ref, struct bt_ctf_writer, ref_count);
	bt_ctf_writer_flush_metadata(writer);
	if (writer->path) {
		g_string_free(writer->path, TRUE);
	}

	if (writer->trace_dir_fd > 0) {
		if (close(writer->trace_dir_fd)) {
			perror("close");
			abort();
		}
	}

	if (writer->metadata_fd > 0) {
		if (close(writer->metadata_fd)) {
			perror("close");
			abort();
		}
	}

	if (writer->environment) {
		g_ptr_array_free(writer->environment, TRUE);
	}

	if (writer->clocks) {
		g_ptr_array_free(writer->clocks, TRUE);
	}

	if (writer->streams) {
		g_ptr_array_free(writer->streams, TRUE);
	}

	if (writer->stream_classes) {
		g_ptr_array_free(writer->stream_classes, TRUE);
	}

	bt_ctf_field_type_put(writer->trace_packet_header_type);
	bt_ctf_field_put(writer->trace_packet_header);
	g_free(writer);
}

struct bt_ctf_stream *bt_ctf_writer_create_stream(struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *stream_class)
{
	int ret;
	int stream_class_found = 0;
	size_t i;
	int stream_fd;
	struct bt_ctf_stream *stream = NULL;

	if (!writer || !stream_class) {
		goto error;
	}

	stream = bt_ctf_stream_create(stream_class);
	if (!stream) {
		goto error;
	}

	stream_fd = create_stream_file(writer, stream);
	if (stream_fd < 0 || bt_ctf_stream_set_fd(stream, stream_fd)) {
		goto error;
	}

	bt_ctf_stream_set_flush_callback(stream, (flush_func)stream_flush_cb,
		writer);
	ret = bt_ctf_stream_class_set_byte_order(stream->stream_class,
		writer->byte_order == LITTLE_ENDIAN ?
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN : BT_CTF_BYTE_ORDER_BIG_ENDIAN);
	if (ret) {
		goto error;
	}

	for (i = 0; i < writer->stream_classes->len; i++) {
		if (writer->stream_classes->pdata[i] == stream->stream_class) {
			stream_class_found = 1;
		}
	}

	if (!stream_class_found) {
		if (bt_ctf_stream_class_set_id(stream->stream_class,
			writer->next_stream_id++)) {
			goto error;
		}

		bt_ctf_stream_class_get(stream->stream_class);
		g_ptr_array_add(writer->stream_classes, stream->stream_class);
	}

	bt_ctf_stream_get(stream);
	g_ptr_array_add(writer->streams, stream);
	writer->frozen = 1;
	return stream;
error:
	bt_ctf_stream_put(stream);
	return NULL;
}

int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer,
		const char *name,
		const char *value)
{
	struct environment_variable *var = NULL;
	char *escaped_value = NULL;
	int ret = 0;

	if (!writer || !name || !value || validate_identifier(name)) {
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

	escaped_value = g_strescape(value, NULL);
	if (!escaped_value) {
		ret = -1;
		goto error;
	}

	var->name = g_string_new(name);
	var->value = g_string_new(escaped_value);
	g_free(escaped_value);
	if (!var->name || !var->value) {
		ret = -1;
		goto error;
	}

	g_ptr_array_add(writer->environment, var);
	return ret;

error:
	if (var && var->name) {
		g_string_free(var->name, TRUE);
	}

	if (var && var->value) {
		g_string_free(var->value, TRUE);
	}

	g_free(var);
	return ret;
}

int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	struct search_query query = { .value = clock, .found = 0 };

	if (!writer || !clock) {
		ret = -1;
		goto end;
	}

	/* Check for duplicate clocks */
	g_ptr_array_foreach(writer->clocks, value_exists, &query);
	if (query.found) {
		ret = -1;
		goto end;
	}

	bt_ctf_clock_get(clock);
	g_ptr_array_add(writer->clocks, clock);
end:
	return ret;
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
int append_trace_metadata(struct bt_ctf_writer *writer,
		struct metadata_context *context)
{
	unsigned char *uuid = writer->uuid;
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
		get_byte_order_string(writer->byte_order));

	g_string_append(context->string, "\tpacket.header := ");
	context->current_indentation_level++;
	g_string_assign(context->field_name, "");
	ret = bt_ctf_field_type_serialize(writer->trace_packet_header_type,
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
	g_string_append_printf(context->string, "\t%s = \"%s\";\n",
		var->name->str, var->value->str);
}

static
void append_env_metadata(struct bt_ctf_writer *writer,
		struct metadata_context *context)
{
	if (writer->environment->len == 0) {
		return;
	}

	g_string_append(context->string, "env {\n");
	g_ptr_array_foreach(writer->environment,
		(GFunc)append_env_field_metadata, context);
	g_string_append(context->string, "};\n\n");
}

char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer)
{
	char *metadata = NULL;
	struct metadata_context *context = NULL;
	int err = 0;
	size_t i;

	if (!writer) {
		goto end;
	}

	context = g_new0(struct metadata_context, 1);
	if (!context) {
		goto end;
	}

	context->field_name = g_string_sized_new(DEFAULT_IDENTIFIER_SIZE);
	context->string = g_string_sized_new(DEFAULT_METADATA_STRING_SIZE);
	g_string_append(context->string, "/* CTF 1.8 */\n\n");
	if (append_trace_metadata(writer, context)) {
		goto error;
	}
	append_env_metadata(writer, context);
	g_ptr_array_foreach(writer->clocks,
		(GFunc)bt_ctf_clock_serialize, context);

	for (i = 0; i < writer->stream_classes->len; i++) {
		err = bt_ctf_stream_class_serialize(
			writer->stream_classes->pdata[i], context);
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

void bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer)
{
	int ret;
	char *metadata_string = NULL;

	if (!writer) {
		goto end;
	}

	metadata_string = bt_ctf_writer_get_metadata_string(writer);
	if (!metadata_string) {
		goto end;
	}

	if (lseek(writer->metadata_fd, 0, SEEK_SET) == (off_t)-1) {
		perror("lseek");
		goto end;
	}

	if (ftruncate(writer->metadata_fd, 0)) {
		perror("ftruncate");
		goto end;
	}

	ret = write(writer->metadata_fd, metadata_string,
		strlen(metadata_string));
	if (ret < 0) {
		perror("write");
		goto end;
	}
end:
	g_free(metadata_string);
}

int bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;
	int internal_byte_order;

	if (!writer || writer->frozen) {
		ret = -1;
		goto end;
	}

	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_NATIVE:
		internal_byte_order =  (G_BYTE_ORDER == G_LITTLE_ENDIAN) ?
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

	writer->byte_order = internal_byte_order;
	if (writer->trace_packet_header_type ||
		writer->trace_packet_header) {
		init_trace_packet_header(writer);
	}
end:
	return ret;
}

void bt_ctf_writer_get(struct bt_ctf_writer *writer)
{
	if (!writer) {
		return;
	}

	bt_ctf_ref_get(&writer->ref_count);
}

void bt_ctf_writer_put(struct bt_ctf_writer *writer)
{
	if (!writer) {
		return;
	}

	bt_ctf_ref_put(&writer->ref_count, bt_ctf_writer_destroy);
}

BT_HIDDEN
int validate_identifier(const char *input_string)
{
	int ret = 0;
	char *string = NULL;
	char *save_ptr, *token;

	if (!input_string || input_string[0] == '\0') {
		ret = -1;
		goto end;
	}

	string = strdup(input_string);
	if (!string) {
		ret = -1;
		goto end;
	}

	token = strtok_r(string, " ", &save_ptr);
	while (token) {
		if (g_hash_table_lookup_extended(reserved_keywords_set,
			GINT_TO_POINTER(g_quark_from_string(token)),
			NULL, NULL)) {
			ret = -1;
			goto end;
		}

		token = strtok_r(NULL, " ", &save_ptr);
	}
end:
	free(string);
	return ret;
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
int init_trace_packet_header(struct bt_ctf_writer *writer)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field *trace_packet_header = NULL,
		*magic = NULL, *uuid_array = NULL;
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

	ret = bt_ctf_field_type_set_byte_order(_uint32_t,
		(writer->byte_order == LITTLE_ENDIAN ?
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN :
		BT_CTF_BYTE_ORDER_BIG_ENDIAN));
	if (ret) {
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

	trace_packet_header = bt_ctf_field_create(trace_packet_header_type);
	if (!trace_packet_header) {
		ret = -1;
		goto end;
	}

	magic = bt_ctf_field_structure_get_field(trace_packet_header, "magic");
	ret = bt_ctf_field_unsigned_integer_set_value(magic, 0xC1FC1FC1);
	if (ret) {
		goto end;
	}

	uuid_array = bt_ctf_field_structure_get_field(trace_packet_header,
		"uuid");
	for (i = 0; i < 16; i++) {
		struct bt_ctf_field *uuid_element =
			bt_ctf_field_array_get_field(uuid_array, i);
		ret = bt_ctf_field_unsigned_integer_set_value(uuid_element,
			writer->uuid[i]);
		bt_ctf_field_put(uuid_element);
		if (ret) {
			goto end;
		}
	}

	bt_ctf_field_type_put(writer->trace_packet_header_type);
	bt_ctf_field_put(writer->trace_packet_header);
	writer->trace_packet_header_type = trace_packet_header_type;
	writer->trace_packet_header = trace_packet_header;
end:
	bt_ctf_field_type_put(uuid_array_type);
	bt_ctf_field_type_put(_uint32_t);
	bt_ctf_field_type_put(_uint8_t);
	bt_ctf_field_put(magic);
	bt_ctf_field_put(uuid_array);
	if (ret) {
		bt_ctf_field_type_put(trace_packet_header_type);
		bt_ctf_field_put(trace_packet_header);
	}

	return ret;
}

static
void environment_variable_destroy(struct environment_variable *var)
{
	g_string_free(var->name, TRUE);
	g_string_free(var->value, TRUE);
	g_free(var);
}

static
int create_stream_file(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream)
{
	int fd;
	GString *filename = g_string_new(stream->stream_class->name->str);

	g_string_append_printf(filename, "_%" PRIu32, stream->id);
	fd = openat(writer->trace_dir_fd, filename->str,
		O_RDWR | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	g_string_free(filename, TRUE);
	return fd;
}

static
void stream_flush_cb(struct bt_ctf_stream *stream, struct bt_ctf_writer *writer)
{
	struct bt_ctf_field *stream_id;

	/* Start a new packet in the stream */
	if (stream->flushed_packet_count) {
		/* ctf_init_pos has already initialized the first packet */
		ctf_packet_seek(&stream->pos.parent, 0, SEEK_CUR);
	}

	stream_id = bt_ctf_field_structure_get_field(
		writer->trace_packet_header, "stream_id");
	bt_ctf_field_unsigned_integer_set_value(stream_id,
		stream->stream_class->id);
	bt_ctf_field_put(stream_id);

	/* Write the trace_packet_header */
	bt_ctf_field_serialize(writer->trace_packet_header, &stream->pos);
}

static __attribute__((constructor))
void writer_init(void)
{
	size_t i;
	const size_t reserved_keywords_count =
		sizeof(reserved_keywords_str) / sizeof(char *);

	global_data_refcount++;
	if (init_done) {
		return;
	}

	reserved_keywords_set = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (i = 0; i < reserved_keywords_count; i++) {
		gpointer quark = GINT_TO_POINTER(g_quark_from_string(
			reserved_keywords_str[i]));

		g_hash_table_insert(reserved_keywords_set, quark, quark);
	}

	init_done = 1;
}

static __attribute__((destructor))
void writer_finalize(void)
{
	if (--global_data_refcount == 0) {
		g_hash_table_destroy(reserved_keywords_set);
	}
}
