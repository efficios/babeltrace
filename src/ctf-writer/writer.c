/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF Writer
 */

#define BT_LOG_TAG "CTF-WRITER"
#include "logging.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <babeltrace2-ctf-writer/object.h>

#include "common/assert.h"
#include "compat/compiler.h"
#include "compat/endian.h"
#include "common/uuid.h"

#include "clock.h"
#include "fields.h"
#include "field-types.h"
#include "functor.h"
#include "stream-class.h"
#include "stream.h"
#include "trace.h"
#include "writer.h"

static
void bt_ctf_writer_destroy(struct bt_ctf_object *obj);

static
int init_trace_packet_header(struct bt_ctf_trace *trace)
{
	int ret = 0;
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

	ret = bt_ctf_trace_set_packet_header_field_type(trace,
		trace_packet_header_type);
	if (ret) {
		goto end;
	}
end:
	bt_ctf_object_put_ref(uuid_array_type);
	bt_ctf_object_put_ref(_uint32_t);
	bt_ctf_object_put_ref(_uint8_t);
	bt_ctf_object_put_ref(trace_packet_header_type);
	return ret;
}

BT_EXPORT
struct bt_ctf_writer *bt_ctf_writer_create(const char *path)
{
	int ret;
	struct bt_ctf_writer *writer = NULL;
	bt_uuid_t uuid;
	char *metadata_path = NULL;

	if (!path) {
		goto error;
	}

	writer = g_new0(struct bt_ctf_writer, 1);
	if (!writer) {
		goto error;
	}

	metadata_path = g_build_filename(path, "metadata", NULL);

	bt_ctf_object_init_shared(&writer->base, bt_ctf_writer_destroy);
	writer->path = g_string_new(path);
	if (!writer->path) {
		goto error_destroy;
	}

	writer->trace = bt_ctf_trace_create();
	if (!writer->trace) {
		goto error_destroy;
	}

	ret = init_trace_packet_header(writer->trace);
	if (ret) {
		goto error_destroy;
	}

	/* Generate a UUID for this writer's trace */
	bt_uuid_generate(uuid);

	ret = bt_ctf_trace_set_uuid(writer->trace, uuid);
	if (ret) {
		goto error_destroy;
	}

	bt_ctf_object_set_parent(&writer->trace->common.base, &writer->base);
	bt_ctf_object_put_ref(writer->trace);

	/* Default to little-endian */
	ret = bt_ctf_writer_set_byte_order(writer, BT_CTF_BYTE_ORDER_NATIVE);
	BT_ASSERT_DBG(ret == 0);

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
	BT_CTF_OBJECT_PUT_REF_AND_RESET(writer);
error:
	g_free(metadata_path);
	return writer;
}

BT_EXPORT
void bt_ctf_writer_destroy(struct bt_ctf_object *obj)
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

	bt_ctf_object_try_spec_release(&writer->trace->common.base);
	g_free(writer);
}

BT_EXPORT
struct bt_ctf_trace *bt_ctf_writer_get_trace(struct bt_ctf_writer *writer)
{
	struct bt_ctf_trace *trace = NULL;

	if (!writer) {
		goto end;
	}

	trace = writer->trace;
	bt_ctf_object_get_ref(trace);
end:
	return trace;
}

BT_EXPORT
struct bt_ctf_stream *bt_ctf_writer_create_stream(struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream *stream = NULL;
	int stream_class_count;
	bt_ctf_bool stream_class_found = BT_CTF_FALSE;
	int i;

	if (!writer || !stream_class) {
		goto error;
	}

	/* Make sure the stream class is part of the writer's trace */
	stream_class_count = bt_ctf_trace_get_stream_class_count(writer->trace);
	if (stream_class_count < 0) {
		goto error;
	}

	for (i = 0; i < stream_class_count; i++) {
		struct bt_ctf_stream_class *existing_stream_class =
			bt_ctf_trace_get_stream_class_by_index(
				writer->trace, i);

		if (existing_stream_class == stream_class) {
			stream_class_found = BT_CTF_TRUE;
		}

		BT_CTF_OBJECT_PUT_REF_AND_RESET(existing_stream_class);

		if (stream_class_found) {
			break;
		}
	}

	if (!stream_class_found) {
		int ret = bt_ctf_trace_add_stream_class(writer->trace,
			stream_class);

		if (ret) {
			goto error;
		}
	}

	stream = bt_ctf_stream_create_with_id(stream_class, NULL, -1ULL);
	if (!stream) {
		goto error;
	}

	return stream;

error:
        BT_CTF_OBJECT_PUT_REF_AND_RESET(stream);
	return stream;
}

BT_EXPORT
int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer,
		const char *name,
		const char *value)
{
	int ret = -1;

	if (!writer || !name || !value) {
		goto end;
	}

	ret = bt_ctf_trace_set_environment_field_string(writer->trace,
		name, value);
end:
	return ret;
}

BT_EXPORT
int bt_ctf_writer_add_environment_field_int64(struct bt_ctf_writer *writer,
		const char *name, int64_t value)
{
	int ret = -1;

	if (!writer || !name) {
		goto end;
	}

	ret = bt_ctf_trace_set_environment_field_integer(writer->trace, name,
		value);
end:
	return ret;
}

BT_EXPORT
int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock)
{
	int ret = -1;

	if (!writer || !clock) {
		goto end;
	}

	ret = bt_ctf_trace_add_clock_class(writer->trace, clock->clock_class);
end:
	return ret;
}

BT_EXPORT
char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer)
{
	char *metadata_string = NULL;

	if (!writer) {
		goto end;
	}

	metadata_string = bt_ctf_trace_get_metadata_string(
		writer->trace);
end:
	return metadata_string;
}

BT_EXPORT
void bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer)
{
	int ret;
	char *metadata_string = NULL;

	if (!writer) {
		goto end;
	}

	metadata_string = bt_ctf_trace_get_metadata_string(
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

BT_EXPORT
int bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;

	if (!writer || writer->frozen) {
		ret = -1;
		goto end;
	}

	if (byte_order == BT_CTF_BYTE_ORDER_NATIVE) {
		if (BYTE_ORDER == LITTLE_ENDIAN) {
			byte_order = BT_CTF_BYTE_ORDER_LITTLE_ENDIAN;
		} else {
			byte_order = BT_CTF_BYTE_ORDER_BIG_ENDIAN;
		}
	}

	ret = bt_ctf_trace_set_native_byte_order(writer->trace,
		byte_order);
end:
	return ret;
}

void bt_ctf_writer_freeze(struct bt_ctf_writer *writer)
{
	writer->frozen = 1;
}

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
		BT_CTF_OBJECT_PUT_REF_AND_RESET(field_type);
	}
end:
	return field_type;
}

const char *bt_ctf_get_byte_order_string(enum bt_ctf_byte_order byte_order)
{
	const char *string;

	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_LITTLE_ENDIAN:
		string = "le";
		break;
	case BT_CTF_BYTE_ORDER_BIG_ENDIAN:
		string = "be";
		break;
	case BT_CTF_BYTE_ORDER_NATIVE:
		string = "native";
		break;
	default:
		bt_common_abort();
	}

	return string;
}
