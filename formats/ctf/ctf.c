/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * Format registration.
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/babeltrace.h>
#include <inttypes.h>
#include <uuid/uuid.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>

#include "metadata/ctf-scanner.h"
#include "metadata/ctf-parser.h"
#include "metadata/ctf-ast.h"

/*
 * We currently simply map a page to read the packet header and packet
 * context to get the packet length and content length.
 */
#define MAX_PACKET_HEADER_LEN	getpagesize()
#define UUID_LEN 16	/* uuid by value len */

extern int yydebug;

struct trace_descriptor {
	struct ctf_trace ctf_trace;
};

struct trace_descriptor *ctf_open_trace(const char *path, int flags);
void ctf_close_trace(struct trace_descriptor *descriptor);

static struct format ctf_format = {
	.uint_read = ctf_uint_read,
	.int_read = ctf_int_read,
	.uint_write = ctf_uint_write,
	.int_write = ctf_int_write,
	.double_read = ctf_double_read,
	.double_write = ctf_double_write,
	.float_copy = ctf_float_copy,
	.string_copy = ctf_string_copy,
	.string_read = ctf_string_read,
	.string_write = ctf_string_write,
	.string_free_temp = ctf_string_free_temp,
	.enum_read = ctf_enum_read,
	.enum_write = ctf_enum_write,
	.struct_begin = ctf_struct_begin,
	.struct_end = ctf_struct_end,
	.variant_begin = ctf_variant_begin,
	.variant_end = ctf_variant_end,
	.array_begin = ctf_array_begin,
	.array_end = ctf_array_end,
	.sequence_begin = ctf_sequence_begin,
	.sequence_end = ctf_sequence_end,
	.open_trace = ctf_open_trace,
	.close_trace = ctf_close_trace,
};

void move_pos_slow(struct stream_pos *pos, size_t offset)
{
	int ret;

	/*
	 * The caller should never ask for move_pos across packets,
	 * except to get exactly at the beginning of the next packet.
	 */
	assert(pos->offset + offset == pos->content_size);

	if (pos->base) {
		/* unmap old base */
		ret = munmap(pos->base, pos->packet_size);
		if (ret) {
			fprintf(stdout, "Unable to unmap old base: %s.\n",
				strerror(errno));
			assert(0);
		}
	}

	pos->mmap_offset += pos->packet_size / CHAR_BIT;
	/* map new base. Need mapping length from header. */
	pos->base = mmap(NULL, MAX_PACKET_HEADER_LEN, PROT_READ,
			 MAP_PRIVATE, pos->fd, pos->mmap_offset);
	pos->content_size = 0;	/* Unknown at this point */
	pos->packet_size = 0;	/* Unknown at this point */

}

/*
 * TODO: for now, we treat the metadata file as a simple text file
 * (without any header nor packets nor padding).
 */
static
int ctf_open_trace_metadata_read(struct trace_descriptor *td)
{
	struct ctf_scanner *scanner;
	FILE *fp;
	int ret = 0;

	td->ctf_trace.metadata.pos.fd = openat(td->ctf_trace.dirfd,
			"metadata", O_RDONLY);
	if (td->ctf_trace.metadata.pos.fd < 0) {
		fprintf(stdout, "Unable to open metadata.\n");
		return td->ctf_trace.metadata.pos.fd;
	}

	if (babeltrace_debug)
		yydebug = 1;

	fp = fdopen(td->ctf_trace.metadata.pos.fd, "r");
	if (!fp) {
		fprintf(stdout, "Unable to open metadata stream.\n");
		ret = -errno;
		goto end_stream;
	}

	scanner = ctf_scanner_alloc(fp);
	if (!scanner) {
		fprintf(stdout, "Error allocating scanner\n");
		ret = -ENOMEM;
		goto end_scanner_alloc;
	}
	ret = ctf_scanner_append_ast(scanner);
	if (ret) {
		fprintf(stdout, "Error creating AST\n");
		goto end;
	}

	if (babeltrace_debug) {
		ret = ctf_visitor_print_xml(stdout, 0, &scanner->ast->root);
		if (ret) {
			fprintf(stdout, "Error visiting AST for XML output\n");
			goto end;
		}
	}

	ret = ctf_visitor_semantic_check(stdout, 0, &scanner->ast->root);
	if (ret) {
		fprintf(stdout, "Error in CTF semantic validation %d\n", ret);
		goto end;
	}
	ret = ctf_visitor_construct_metadata(stdout, 0, &scanner->ast->root,
			&td->ctf_trace, BYTE_ORDER);
	if (ret) {
		fprintf(stdout, "Error in CTF metadata constructor %d\n", ret);
		goto end;
	}
end:
	ctf_scanner_free(scanner);
end_scanner_alloc:
	fclose(fp);
end_stream:
	close(td->ctf_trace.metadata.pos.fd);
	return ret;
}


static
int create_stream_packet_index(struct trace_descriptor *td,
			       struct ctf_file_stream *file_stream)
{
	struct ctf_stream *stream;
	int len_index;
	struct stream_pos *pos;
	struct stat filestats;
	struct packet_index packet_index;
	int first_packet = 1;
	int ret;

	pos = &file_stream->pos;

	ret = fstat(pos->fd, &filestats);
	if (ret < 0)
		return ret;

	for (pos->mmap_offset = 0; pos->mmap_offset < filestats.st_size; ) {
		uint64_t stream_id = 0;

		if (pos->base) {
			/* unmap old base */
			ret = munmap(pos->base, pos->packet_size);
			if (ret) {
				fprintf(stdout, "Unable to unmap old base: %s.\n",
					strerror(errno));
				return ret;
			}
		}
		/* map new base. Need mapping length from header. */
		pos->base = mmap(NULL, MAX_PACKET_HEADER_LEN, PROT_READ,
				 MAP_PRIVATE, pos->fd, pos->mmap_offset);
		pos->content_size = 0;	/* Unknown at this point */
		pos->packet_size = 0;	/* Unknown at this point */
		pos->offset = 0;	/* Position of the packet header */

		/* read and check header, set stream id (and check) */
		if (td->ctf_trace.packet_header) {
			/* Read packet header */
			td->ctf_trace.packet_header->p.declaration->copy(NULL, NULL,
				pos, &ctf_format, &td->ctf_trace.packet_header->p);

			len_index = struct_declaration_lookup_field_index(td->ctf_trace.packet_header->declaration, g_quark_from_static_string("magic"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct field *field;

				field = struct_definition_get_field_from_index(td->ctf_trace.packet_header, len_index);
				assert(field->definition->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field->definition, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				if (defint->value._unsigned != CTF_MAGIC) {
					fprintf(stdout, "[error] Invalid magic number %" PRIX64 ".\n",
							defint->value._unsigned);
					return -EINVAL;
				}
			}

			/* check uuid */
			len_index = struct_declaration_lookup_field_index(td->ctf_trace.packet_header->declaration, g_quark_from_static_string("trace_uuid"));
			if (len_index >= 0) {
				struct definition_array *defarray;
				struct field *field;
				uint64_t i;
				uint8_t uuidval[UUID_LEN];


				field = struct_definition_get_field_from_index(td->ctf_trace.packet_header, len_index);
				assert(field->definition->declaration->id == CTF_TYPE_ARRAY);
				defarray = container_of(field->definition, struct definition_array, p);
				assert(defarray->declaration->len == UUID_LEN);
				assert(defarray->declaration->elem->id == CTF_TYPE_INTEGER);

				for (i = 0; i < UUID_LEN; i++) {
					struct definition *elem;
					struct definition_integer *defint;

					elem = array_index(defarray, i);
					assert(elem);
					defint = container_of(elem, struct definition_integer, p);
					uuidval[i] = defint->value._unsigned;
				}
				ret = uuid_compare(td->ctf_trace.uuid, uuidval);
				if (ret) {
					fprintf(stdout, "[error] Unique Universal Identifiers do not match.\n");
					return -EINVAL;
				}
			}


			len_index = struct_declaration_lookup_field_index(td->ctf_trace.packet_header->declaration, g_quark_from_static_string("stream_id"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct field *field;

				field = struct_definition_get_field_from_index(td->ctf_trace.packet_header, len_index);
				assert(field->definition->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field->definition, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				stream_id = defint->value._unsigned;
			}
		}

		if (!first_packet && file_stream->stream_id != stream_id) {
			fprintf(stdout, "[error] Stream ID is changing within a stream.\n");
			return -EINVAL;
		}
		if (first_packet) {
			file_stream->stream_id = stream_id;
			if (stream_id >= td->ctf_trace.streams->len) {
				fprintf(stdout, "[error] Stream %" PRIu64 " is not declared in metadata.\n", stream_id);
				return -EINVAL;
			}
			stream = g_ptr_array_index(td->ctf_trace.streams, stream_id);
			if (!stream) {
				fprintf(stdout, "[error] Stream %" PRIu64 " is not declared in metadata.\n", stream_id);
				return -EINVAL;
			}
			file_stream->stream = stream;
		}
		first_packet = 0;

		/* Read packet context */
		stream->packet_context->p.declaration->copy(NULL, NULL,
				pos, &ctf_format, &stream->packet_context->p);

		/* read content size from header */
		len_index = struct_declaration_lookup_field_index(stream->packet_context->declaration, g_quark_from_static_string("content_size"));
		if (len_index >= 0) {
			struct definition_integer *defint;
			struct field *field;

			field = struct_definition_get_field_from_index(stream->packet_context, len_index);
			assert(field->definition->declaration->id == CTF_TYPE_INTEGER);
			defint = container_of(field->definition, struct definition_integer, p);
			assert(defint->declaration->signedness == FALSE);
			pos->content_size = defint->value._unsigned;
		} else {
			/* Use file size for packet size */
			pos->content_size = filestats.st_size * CHAR_BIT;
		}

		/* read packet size from header */
		len_index = struct_declaration_lookup_field_index(stream->packet_context->declaration, g_quark_from_static_string("packet_size"));
		if (len_index >= 0) {
			struct definition_integer *defint;
			struct field *field;

			field = struct_definition_get_field_from_index(stream->packet_context, len_index);
			assert(field->definition->declaration->id == CTF_TYPE_INTEGER);
			defint = container_of(field->definition, struct definition_integer, p);
			assert(defint->declaration->signedness == FALSE);
			pos->packet_size = defint->value._unsigned;
		} else {
			/* Use content size if non-zero, else file size */
			pos->packet_size = pos->content_size ? : filestats.st_size * CHAR_BIT;
		}

		packet_index.offset = pos->mmap_offset;
		packet_index.content_size = pos->content_size;
		packet_index.packet_size = pos->packet_size;
		/* add index to packet array */
		g_array_append_val(file_stream->pos.packet_index, packet_index);

		pos->mmap_offset += pos->packet_size / CHAR_BIT;
	}

	return 0;
}

/*
 * Note: many file streams can inherit from the same stream class
 * description (metadata).
 */
static
int ctf_open_file_stream_read(struct trace_descriptor *td, const char *path, int flags)
{
	int ret;
	struct ctf_file_stream *file_stream;

	ret = openat(td->ctf_trace.dirfd, path, flags);
	if (ret < 0)
		goto error;
	file_stream = g_new0(struct ctf_file_stream, 1);
	file_stream->pos.fd = ret;
	file_stream->pos.packet_index = g_array_new(FALSE, TRUE,
						sizeof(struct packet_index));
	ret = create_stream_packet_index(td, file_stream);
	if (ret)
		goto error_index;
	/* Add stream file to stream class */
	g_ptr_array_add(file_stream->stream->files, file_stream);
	return 0;

error_index:
	(void) g_array_free(file_stream->pos.packet_index, TRUE);
	close(file_stream->pos.fd);
	g_free(file_stream);
error:
	return ret;
}

static
int ctf_open_trace_read(struct trace_descriptor *td, const char *path, int flags)
{
	int ret;
	struct dirent *dirent;
	struct dirent *diriter;
	size_t dirent_len;

	td->ctf_trace.flags = flags;

	/* Open trace directory */
	td->ctf_trace.dir = opendir(path);
	if (!td->ctf_trace.dir) {
		fprintf(stdout, "Unable to open trace directory.\n");
		ret = -ENOENT;
		goto error;
	}

	td->ctf_trace.dirfd = open(path, 0);
	if (td->ctf_trace.dirfd < 0) {
		fprintf(stdout, "Unable to open trace directory file descriptor.\n");
		ret = -ENOENT;
		goto error_dirfd;
	}

	td->ctf_trace.streams = g_ptr_array_new();

	/*
	 * Keep the metadata file separate.
	 */

	ret = ctf_open_trace_metadata_read(td);
	if (ret) {
		goto error_metadata;
	}

	/*
	 * Open each stream: for each file, try to open, check magic
	 * number, and get the stream ID to add to the right location in
	 * the stream array.
	 */

	dirent_len = offsetof(struct dirent, d_name) +
			fpathconf(td->ctf_trace.dirfd, _PC_NAME_MAX) + 1;

	dirent = malloc(dirent_len);

	for (;;) {
		ret = readdir_r(td->ctf_trace.dir, dirent, &diriter);
		if (ret) {
			fprintf(stdout, "Readdir error.\n");
			goto readdir_error;
		}
		if (!diriter)
			break;
		if (!strcmp(diriter->d_name, ".")
				|| !strcmp(diriter->d_name, "..")
				|| !strcmp(diriter->d_name, "metadata"))
			continue;
		/* TODO: open file stream */
	}

	free(dirent);
	return 0;

readdir_error:
	free(dirent);
error_metadata:
	g_ptr_array_free(td->ctf_trace.streams, TRUE);
	close(td->ctf_trace.dirfd);
error_dirfd:
	closedir(td->ctf_trace.dir);
error:
	return ret;
}

static
int ctf_open_trace_write(struct trace_descriptor *td, const char *path, int flags)
{
	int ret;

	ret = mkdir(path, S_IRWXU|S_IRWXG);
	if (ret)
		return ret;

	/* Open trace directory */
	td->ctf_trace.dir = opendir(path);
	if (!td->ctf_trace.dir) {
		fprintf(stdout, "Unable to open trace directory.\n");
		ret = -ENOENT;
		goto error;
	}
	

	return 0;

error:
	return ret;
}

struct trace_descriptor *ctf_open_trace(const char *path, int flags)
{
	struct trace_descriptor *td;
	int ret;

	td = g_new0(struct trace_descriptor, 1);

	switch (flags) {
	case O_RDONLY:
		ret = ctf_open_trace_read(td, path, flags);
		if (ret)
			goto error;
		break;
	case O_WRONLY:
		ret = ctf_open_trace_write(td, path, flags);
		if (ret)
			goto error;
		break;
	default:
		fprintf(stdout, "Incorrect open flags.\n");
		goto error;
	}

	return td;
error:
	g_free(td);
	return NULL;
}

static
void ctf_close_file_stream(struct ctf_file_stream *file_stream)
{
	(void) g_array_free(file_stream->pos.packet_index, TRUE);
	close(file_stream->pos.fd);
}

void ctf_close_trace(struct trace_descriptor *td)
{
	int i;

	if (td->ctf_trace.streams) {
		for (i = 0; i < td->ctf_trace.streams->len; i++) {
			struct ctf_stream *stream;
			int j;

			stream = g_ptr_array_index(td->ctf_trace.streams, i);
			for (j = 0; j < stream->files->len; j++) {
				struct ctf_file_stream *file_stream;
				file_stream = g_ptr_array_index(td->ctf_trace.streams, j);
				ctf_close_file_stream(file_stream);
			}

		}
		g_ptr_array_free(td->ctf_trace.streams, TRUE);
	}
	closedir(td->ctf_trace.dir);
	g_free(td);
}

void __attribute__((constructor)) ctf_init(void)
{
	int ret;

	ctf_format.name = g_quark_from_static_string("ctf");
	ret = bt_register_format(&ctf_format);
	assert(!ret);
}

/* TODO: finalize */
