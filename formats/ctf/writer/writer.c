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

#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-fields-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/compiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

static
void bt_ctf_writer_destroy(struct bt_ctf_ref *ref);
static
int create_stream_file(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream);

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

	bt_ctf_ref_init(&writer->ref_count);
	writer->path = g_string_new(path);
	if (!writer->path) {
		goto error_destroy;
	}

	writer->trace = bt_ctf_trace_create();
	if (!writer->trace) {
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
		}
	}

	if (writer->metadata_fd > 0) {
		if (close(writer->metadata_fd)) {
			perror("close");
		}
	}

	bt_ctf_trace_put(writer->trace);
	g_free(writer);
}

struct bt_ctf_trace *bt_ctf_writer_get_trace(struct bt_ctf_writer *writer)
{
	struct bt_ctf_trace *trace = NULL;

	if (!writer) {
		goto end;
	}

	trace = writer->trace;
	bt_ctf_trace_get(trace);
end:
	return trace;
}

struct bt_ctf_stream *bt_ctf_writer_create_stream(struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *stream_class)
{
	int stream_fd;
	struct bt_ctf_stream *stream = NULL;

	if (!writer || !stream_class) {
		goto error;
	}

	stream = bt_ctf_trace_create_stream(writer->trace, stream_class);
	if (!stream) {
		goto error;
	}

	stream_fd = create_stream_file(writer, stream);
	if (stream_fd < 0 || bt_ctf_stream_set_fd(stream, stream_fd)) {
		goto error;
	}

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
	int ret = -1;

	if (!writer || !name || !value) {
		goto end;
	}

	ret = bt_ctf_trace_set_environment_field_string(writer->trace,
		name, value);
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

	ret = bt_ctf_trace_add_clock(writer->trace, clock);
end:
	return ret;
}

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

int bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;

	if (!writer || writer->frozen) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_trace_set_byte_order(writer->trace,
		byte_order);
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

static
int create_stream_file(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream)
{
	int fd;
	GString *filename = g_string_new(stream->stream_class->name->str);

	if (stream->stream_class->name->len == 0) {
		int64_t ret;

		ret = bt_ctf_stream_class_get_id(stream->stream_class);
		if (ret < 0) {
			fd = -1;
			goto error;
		}

		g_string_printf(filename, "stream_%" PRId64, ret);
	}

	g_string_append_printf(filename, "_%" PRIu32, stream->id);
	fd = openat(writer->trace_dir_fd, filename->str,
		O_RDWR | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
error:
	g_string_free(filename, TRUE);
	return fd;
}
