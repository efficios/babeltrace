/*
 * writer.c
 *
 * Babeltrace CTF Writer
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

#define BT_LOG_TAG "CTF-WRITER"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

static
void bt_ctf_writer_destroy(struct bt_object *obj);

static
int init_trace_packet_header(struct bt_trace *trace)
{
	int ret = 0;
	struct bt_field *magic = NULL, *uuid_array = NULL;
	struct bt_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_field_type *_uint8_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT8_T);
	struct bt_field_type *trace_packet_header_type =
		bt_field_type_structure_create();
	struct bt_field_type *uuid_array_type =
		bt_field_type_array_create(_uint8_t, 16);

	if (!trace_packet_header_type || !uuid_array_type) {
		ret = -1;
		goto end;
	}

	ret = bt_field_type_structure_add_field(trace_packet_header_type,
		_uint32_t, "magic");
	if (ret) {
		goto end;
	}

	ret = bt_field_type_structure_add_field(trace_packet_header_type,
		uuid_array_type, "uuid");
	if (ret) {
		goto end;
	}

	ret = bt_field_type_structure_add_field(trace_packet_header_type,
		_uint32_t, "stream_id");
	if (ret) {
		goto end;
	}

	ret = bt_trace_set_packet_header_type(trace,
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

struct bt_ctf_writer *bt_ctf_writer_create(const char *path)
{
	int ret;
	struct bt_ctf_writer *writer = NULL;
	unsigned char uuid[16];
	char *metadata_path = NULL;

	if (!path) {
		goto error;
	}

	writer = g_new0(struct bt_ctf_writer, 1);
	if (!writer) {
		goto error;
	}

	metadata_path = g_build_filename(path, "metadata", NULL);

	bt_object_init(writer, bt_ctf_writer_destroy);
	writer->path = g_string_new(path);
	if (!writer->path) {
		goto error_destroy;
	}

	writer->trace = bt_trace_create();
	if (!writer->trace) {
		goto error_destroy;
	}

	ret = init_trace_packet_header(writer->trace);
	if (ret) {
		goto error_destroy;
	}

	/* Generate a UUID for this writer's trace */
	ret = bt_uuid_generate(uuid);
	if (ret) {
		BT_LOGE_STR("Cannot generate UUID for CTF writer's trace.");
		goto error_destroy;
	}

	ret = bt_trace_set_uuid(writer->trace, uuid);
	if (ret) {
		goto error_destroy;
	}

	writer->trace->is_created_by_writer = 1;
	bt_object_set_parent(writer->trace, writer);
	bt_put(writer->trace);

	/* Default to little-endian */
	ret = bt_ctf_writer_set_byte_order(writer, BT_BYTE_ORDER_NATIVE);
	BT_ASSERT(ret == 0);

	/* Create trace directory if necessary and open a metadata file */
	if (g_mkdir_with_parents(path, S_IRWXU | S_IRWXG)) {
		perror("g_mkdir_with_parents");
		goto error_destroy;
	}

	writer->metadata_fd = open(metadata_path,
		O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (writer->metadata_fd < 0) {
		perror("open");
		goto error_destroy;
	}

	g_free(metadata_path);
	return writer;

error_destroy:
	BT_PUT(writer);
error:
	g_free(metadata_path);
	return writer;
}

void bt_ctf_writer_destroy(struct bt_object *obj)
{
	struct bt_ctf_writer *writer;

	writer = container_of(obj, struct bt_ctf_writer, base);
	bt_ctf_writer_flush_metadata(writer);
	if (writer->path) {
		g_string_free(writer->path, TRUE);
	}

	if (writer->metadata_fd > 0) {
		if (close(writer->metadata_fd)) {
			perror("close");
		}
	}

	bt_object_release(writer->trace);
	g_free(writer);
}

struct bt_trace *bt_ctf_writer_get_trace(struct bt_ctf_writer *writer)
{
	struct bt_trace *trace = NULL;

	if (!writer) {
		goto end;
	}

	trace = writer->trace;
	bt_get(trace);
end:
	return trace;
}

struct bt_stream *bt_ctf_writer_create_stream(struct bt_ctf_writer *writer,
		struct bt_stream_class *stream_class)
{
	struct bt_stream *stream = NULL;
	int stream_class_count;
	bt_bool stream_class_found = BT_FALSE;
	int i;

	if (!writer || !stream_class) {
		goto error;
	}

	/* Make sure the stream class is part of the writer's trace */
	stream_class_count = bt_trace_get_stream_class_count(writer->trace);
	if (stream_class_count < 0) {
		goto error;
	}

	for (i = 0; i < stream_class_count; i++) {
		struct bt_stream_class *existing_stream_class =
			bt_trace_get_stream_class_by_index(
				writer->trace, i);

		if (existing_stream_class == stream_class) {
			stream_class_found = BT_TRUE;
		}

		BT_PUT(existing_stream_class);

		if (stream_class_found) {
			break;
		}
	}

	if (!stream_class_found) {
		int ret = bt_trace_add_stream_class(writer->trace,
			stream_class);

		if (ret) {
			goto error;
		}
	}

	stream = bt_stream_create(stream_class, NULL);
	if (!stream) {
		goto error;
	}

	return stream;

error:
        BT_PUT(stream);
	return stream;
}

int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer,
		const char *name,
		const char *value)
{
	int ret = -1;

	if (!writer || !name || !value) {
		goto end;
	}

	ret = bt_trace_set_environment_field_string(writer->trace,
		name, value);
end:
	return ret;
}

int bt_ctf_writer_add_environment_field_int64(struct bt_ctf_writer *writer,
		const char *name, int64_t value)
{
	int ret = -1;

	if (!writer || !name) {
		goto end;
	}

	ret = bt_trace_set_environment_field_integer(writer->trace, name,
		value);
end:
	return ret;
}

int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock)
{
	int ret = -1;

	if (!writer || !clock) {
		goto end;
	}

	ret = bt_trace_add_clock_class(writer->trace, clock->clock_class);
end:
	return ret;
}

char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer)
{
	char *metadata_string = NULL;

	if (!writer) {
		goto end;
	}

	metadata_string = bt_trace_get_metadata_string(
		writer->trace);
end:
	return metadata_string;
}

void bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer)
{
	int ret;
	char *metadata_string = NULL;

	if (!writer) {
		goto end;
	}

	metadata_string = bt_trace_get_metadata_string(
		writer->trace);
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
		enum bt_byte_order byte_order)
{
	int ret = 0;

	if (!writer || writer->frozen) {
		ret = -1;
		goto end;
	}

	if (byte_order == BT_BYTE_ORDER_NATIVE) {
		byte_order = BT_MY_BYTE_ORDER;
	}

	ret = bt_trace_set_native_byte_order(writer->trace,
		byte_order);
end:
	return ret;
}

void bt_ctf_writer_get(struct bt_ctf_writer *writer)
{
	bt_get(writer);
}

void bt_ctf_writer_put(struct bt_ctf_writer *writer)
{
	bt_put(writer);
}

BT_HIDDEN
void bt_ctf_writer_freeze(struct bt_ctf_writer *writer)
{
	writer->frozen = 1;
}
