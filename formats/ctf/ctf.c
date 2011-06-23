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
#include <stdio.h>
#include <uuid/uuid.h>
#include <sys/mman.h>
#include <errno.h>
#include <endian.h>
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
 * context to get the packet length and content length. (in bits)
 */
#define MAX_PACKET_HEADER_LEN	(getpagesize() * CHAR_BIT)
#define WRITE_PACKET_LEN	(getpagesize() * 8 * CHAR_BIT)
#define UUID_LEN 16	/* uuid by value len */

#ifndef min
#define min(a, b)	(((a) < (b)) ? (a) : (b))
#endif

extern int yydebug;

static
struct trace_descriptor *ctf_open_trace(const char *path, int flags);
static
void ctf_close_trace(struct trace_descriptor *descriptor);

static
rw_dispatch read_dispatch_table[] = {
	[ CTF_TYPE_INTEGER ] = ctf_integer_read,
	[ CTF_TYPE_FLOAT ] = ctf_float_read,
	[ CTF_TYPE_ENUM ] = ctf_enum_read,
	[ CTF_TYPE_STRING ] = ctf_string_read,
	[ CTF_TYPE_STRUCT ] = ctf_struct_rw,
	[ CTF_TYPE_VARIANT ] = ctf_variant_rw,
	[ CTF_TYPE_ARRAY ] = ctf_array_read,
	[ CTF_TYPE_SEQUENCE ] = ctf_sequence_read,
};

static
rw_dispatch write_dispatch_table[] = {
	[ CTF_TYPE_INTEGER ] = ctf_integer_write,
	[ CTF_TYPE_FLOAT ] = ctf_float_write,
	[ CTF_TYPE_ENUM ] = ctf_enum_write,
	[ CTF_TYPE_STRING ] = ctf_string_write,
	[ CTF_TYPE_STRUCT ] = ctf_struct_rw,
	[ CTF_TYPE_VARIANT ] = ctf_variant_rw,
	[ CTF_TYPE_ARRAY ] = ctf_array_write,
	[ CTF_TYPE_SEQUENCE ] = ctf_sequence_write,
};

static
struct format ctf_format = {
	.open_trace = ctf_open_trace,
	.close_trace = ctf_close_trace,
};

static
void ctf_update_timestamp(struct ctf_stream *stream,
			  struct definition_integer *integer_definition)
{
	struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	uint64_t oldval, newval, updateval;

	if (integer_declaration->len == 64) {
		stream->timestamp = integer_definition->value._unsigned;
		return;
	}
	/* keep low bits */
	oldval = stream->timestamp;
	oldval &= (1ULL << integer_declaration->len) - 1;
	newval = integer_definition->value._unsigned;
	/* Test for overflow by comparing low bits */
	if (newval < oldval)
		newval += 1ULL << integer_declaration->len;
	/* updateval contains old high bits, and new low bits (sum) */
	updateval = stream->timestamp;
	updateval &= ~((1ULL << integer_declaration->len) - 1);
	updateval += newval;
	stream->timestamp = updateval;
}

static
int ctf_read_event(struct stream_pos *ppos, struct ctf_stream *stream)
{
	struct ctf_stream_pos *pos =
		container_of(ppos, struct ctf_stream_pos, parent);
	struct ctf_stream_class *stream_class = stream->stream_class;
	struct ctf_stream_event *event;
	uint64_t id = 0;
	int ret;

	if (pos->offset == EOF)
		return EOF;

	/* Read event header */
	if (stream->stream_event_header) {
		struct definition_integer *integer_definition;
		struct definition *variant;

		ret = generic_rw(ppos, &stream->stream_event_header->p);
		if (ret)
			goto error;
		/* lookup event id */
		integer_definition = lookup_integer(&stream->stream_event_header->p, "id", FALSE);
		if (integer_definition) {
			id = integer_definition->value._unsigned;
		} else {
			struct definition_enum *enum_definition;

			enum_definition = lookup_enum(&stream->stream_event_header->p, "id", FALSE);
			if (enum_definition) {
				id = enum_definition->integer->value._unsigned;
			}
		}

		variant = lookup_variant(&stream->stream_event_header->p, "v");
		if (variant) {
			integer_definition = lookup_integer(variant, "id", FALSE);
			if (integer_definition) {
				id = integer_definition->value._unsigned;
			}
		}

		/* lookup timestamp */
		integer_definition = lookup_integer(&stream->stream_event_header->p, "timestamp", FALSE);
		if (integer_definition) {
			ctf_update_timestamp(stream, integer_definition);
		} else {
			if (variant) {
				integer_definition = lookup_integer(variant, "timestamp", FALSE);
				if (integer_definition) {
					ctf_update_timestamp(stream, integer_definition);
				}
			}
		}
	}

	/* Read stream-declared event context */
	if (stream->stream_event_context) {
		ret = generic_rw(ppos, &stream->stream_event_context->p);
		if (ret)
			goto error;
	}

	if (id >= stream_class->events_by_id->len) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -EINVAL;
	}
	event = g_ptr_array_index(stream->events_by_id, id);
	if (!event) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	/* Read event-declared event context */
	if (event->event_context) {
		ret = generic_rw(ppos, &event->event_context->p);
		if (ret)
			goto error;
	}

	/* Read event payload */
	if (event->event_fields) {
		ret = generic_rw(ppos, &event->event_fields->p);
		if (ret)
			goto error;
	}

	return 0;

error:
	fprintf(stdout, "[error] Unexpected end of stream. Either the trace data stream is corrupted or metadata description does not match data layout.\n");
	return ret;
}

static
int ctf_write_event(struct stream_pos *pos, struct ctf_stream *stream)
{
	struct ctf_stream_class *stream_class = stream->stream_class;
	struct ctf_stream_event *event;
	uint64_t id = 0;
	int ret;

	/* print event header */
	if (stream->stream_event_header) {
		struct definition_integer *integer_definition;
		struct definition *variant;

		/* lookup event id */
		integer_definition = lookup_integer(&stream->stream_event_header->p, "id", FALSE);
		if (integer_definition) {
			id = integer_definition->value._unsigned;
		} else {
			struct definition_enum *enum_definition;

			enum_definition = lookup_enum(&stream->stream_event_header->p, "id", FALSE);
			if (enum_definition) {
				id = enum_definition->integer->value._unsigned;
			}
		}

		variant = lookup_variant(&stream->stream_event_header->p, "v");
		if (variant) {
			integer_definition = lookup_integer(variant, "id", FALSE);
			if (integer_definition) {
				id = integer_definition->value._unsigned;
			}
		}

		ret = generic_rw(pos, &stream->stream_event_header->p);
		if (ret)
			goto error;
	}

	/* print stream-declared event context */
	if (stream->stream_event_context) {
		ret = generic_rw(pos, &stream->stream_event_context->p);
		if (ret)
			goto error;
	}

	if (id >= stream_class->events_by_id->len) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -EINVAL;
	}
	event = g_ptr_array_index(stream->events_by_id, id);
	if (!event) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	/* print event-declared event context */
	if (event->event_context) {
		ret = generic_rw(pos, &event->event_context->p);
		if (ret)
			goto error;
	}

	/* Read and print event payload */
	if (event->event_fields) {
		ret = generic_rw(pos, &event->event_fields->p);
		if (ret)
			goto error;
	}

	return 0;

error:
	fprintf(stdout, "[error] Unexpected end of stream. Either the trace data stream is corrupted or metadata description does not match data layout.\n");
	return ret;
}

void ctf_init_pos(struct ctf_stream_pos *pos, int fd, int open_flags)
{
	pos->fd = fd;
	pos->mmap_offset = 0;
	pos->packet_size = 0;
	pos->content_size = 0;
	pos->content_size_loc = NULL;
	pos->base = NULL;
	pos->offset = 0;
	pos->dummy = false;
	pos->cur_index = 0;
	if (fd >= 0)
		pos->packet_index = g_array_new(FALSE, TRUE,
						sizeof(struct packet_index));
	else
		pos->packet_index = NULL;
	switch (open_flags & O_ACCMODE) {
	case O_RDONLY:
		pos->prot = PROT_READ;
		pos->flags = MAP_PRIVATE;
		pos->parent.rw_table = read_dispatch_table;
		pos->parent.event_cb = ctf_read_event;
		break;
	case O_RDWR:
		pos->prot = PROT_WRITE;	/* Write has priority */
		pos->flags = MAP_SHARED;
		pos->parent.rw_table = write_dispatch_table;
		pos->parent.event_cb = ctf_write_event;
		if (fd >= 0)
			ctf_move_pos_slow(pos, 0, SEEK_SET);	/* position for write */
		break;
	default:
		assert(0);
	}
}

void ctf_fini_pos(struct ctf_stream_pos *pos)
{
	int ret;

	if (pos->prot == PROT_WRITE && pos->content_size_loc)
		*pos->content_size_loc = pos->offset;
	if (pos->base) {
		/* unmap old base */
		ret = munmap(pos->base, pos->packet_size / CHAR_BIT);
		if (ret) {
			fprintf(stdout, "[error] Unable to unmap old base: %s.\n",
				strerror(errno));
			assert(0);
		}
	}
	(void) g_array_free(pos->packet_index, TRUE);
}

void ctf_move_pos_slow(struct ctf_stream_pos *pos, size_t offset, int whence)
{
	struct ctf_file_stream *file_stream =
		container_of(pos, struct ctf_file_stream, pos);
	int ret;
	off_t off;
	struct packet_index *index;

	if (pos->prot == PROT_WRITE && pos->content_size_loc)
		*pos->content_size_loc = pos->offset;

	if (pos->base) {
		/* unmap old base */
		ret = munmap(pos->base, pos->packet_size / CHAR_BIT);
		if (ret) {
			fprintf(stdout, "[error] Unable to unmap old base: %s.\n",
				strerror(errno));
			assert(0);
		}
		pos->base = NULL;
	}

	/*
	 * The caller should never ask for ctf_move_pos across packets,
	 * except to get exactly at the beginning of the next packet.
	 */
	if (pos->prot == PROT_WRITE) {
		switch (whence) {
		case SEEK_CUR:
			/* The writer will add padding */
			assert(pos->offset + offset == pos->packet_size);
			pos->mmap_offset += WRITE_PACKET_LEN / CHAR_BIT;
			break;
		case SEEK_SET:
			assert(offset == 0);	/* only seek supported for now */
			pos->cur_index = 0;
			break;
		default:
			assert(0);
		}
		pos->content_size = -1U;	/* Unknown at this point */
		pos->packet_size = WRITE_PACKET_LEN;
		off = posix_fallocate(pos->fd, pos->mmap_offset,
				      pos->packet_size / CHAR_BIT);
		assert(off >= 0);
		pos->offset = 0;
	} else {
		switch (whence) {
		case SEEK_CUR:
			/* The reader will expect us to skip padding */
			assert(pos->offset + offset == pos->content_size);
			++pos->cur_index;
			break;
		case SEEK_SET:
			assert(offset == 0);	/* only seek supported for now */
			pos->cur_index = 0;
			break;
		default:
			assert(0);
		}
		if (pos->cur_index >= pos->packet_index->len) {
			pos->offset = EOF;
			return;
		}
		index = &g_array_index(pos->packet_index, struct packet_index,
				       pos->cur_index);
		pos->mmap_offset = index->offset;

		/* Lookup context/packet size in index */
		file_stream->parent.timestamp = index->timestamp_begin;
		pos->content_size = index->content_size;
		pos->packet_size = index->packet_size;
		if (index->data_offset < index->content_size)
			pos->offset = 0;	/* will read headers */
		else {
			pos->offset = EOF;
			return;
		}
	}
	/* map new base. Need mapping length from header. */
	pos->base = mmap(NULL, pos->packet_size / CHAR_BIT, pos->prot,
			 pos->flags, pos->fd, pos->mmap_offset);
	if (pos->base == MAP_FAILED) {
		fprintf(stdout, "[error] mmap error %s.\n",
			strerror(errno));
		assert(0);
	}

	/* update trace_packet_header and stream_packet_context */
	if (pos->prot != PROT_WRITE && file_stream->parent.trace_packet_header) {
		/* Read packet header */
		ret = generic_rw(&pos->parent, &file_stream->parent.trace_packet_header->p);
		assert(!ret);
	}
	if (pos->prot != PROT_WRITE && file_stream->parent.stream_packet_context) {
		/* Read packet context */
		ret = generic_rw(&pos->parent, &file_stream->parent.stream_packet_context->p);
		assert(!ret);
	}
}

static
int packet_metadata(struct ctf_trace *td, FILE *fp)
{
	uint32_t magic;
	size_t len;
	int ret = 0;

	len = fread(&magic, sizeof(magic), 1, fp);
	if (len != 1) {
		goto end;
	}
	if (magic == TSDL_MAGIC) {
		ret = 1;
		td->byte_order = BYTE_ORDER;
	} else if (magic == GUINT32_SWAP_LE_BE(TSDL_MAGIC)) {
		ret = 1;
		td->byte_order = (BYTE_ORDER == BIG_ENDIAN) ?
					LITTLE_ENDIAN : BIG_ENDIAN;
	}
	CTF_TRACE_SET_FIELD(td, byte_order);
end:
	rewind(fp);
	return ret;
}

static
int ctf_open_trace_metadata_packet_read(struct ctf_trace *td, FILE *in,
					FILE *out)
{
	struct metadata_packet_header header;
	size_t readlen, writelen, toread;
	char buf[4096];
	int ret = 0;

	readlen = fread(&header, header_sizeof(header), 1, in);
	if (readlen < 1)
		return -EINVAL;

	if (td->byte_order != BYTE_ORDER) {
		header.magic = GUINT32_SWAP_LE_BE(header.magic);
		header.checksum = GUINT32_SWAP_LE_BE(header.checksum);
		header.content_size = GUINT32_SWAP_LE_BE(header.content_size);
		header.packet_size = GUINT32_SWAP_LE_BE(header.packet_size);
	}
	if (header.checksum)
		fprintf(stdout, "[warning] checksum verification not supported yet.\n");
	if (header.compression_scheme) {
		fprintf(stdout, "[error] compression (%u) not supported yet.\n",
			header.compression_scheme);
		return -EINVAL;
	}
	if (header.encryption_scheme) {
		fprintf(stdout, "[error] encryption (%u) not supported yet.\n",
			header.encryption_scheme);
		return -EINVAL;
	}
	if (header.checksum_scheme) {
		fprintf(stdout, "[error] checksum (%u) not supported yet.\n",
			header.checksum_scheme);
		return -EINVAL;
	}
	if (!CTF_TRACE_FIELD_IS_SET(td, uuid)) {
		memcpy(td->uuid, header.uuid, sizeof(header.uuid));
		CTF_TRACE_SET_FIELD(td, uuid);
	} else {
		if (uuid_compare(header.uuid, td->uuid))
			return -EINVAL;
	}

	toread = (header.content_size / CHAR_BIT) - header_sizeof(header);

	for (;;) {
		readlen = fread(buf, sizeof(char), min(sizeof(buf), toread), in);
		if (ferror(in)) {
			ret = -EINVAL;
			break;
		}
		if (babeltrace_debug) {
			fprintf(stdout, "[debug] metadata packet read: %s\n",
				buf);
		}

		writelen = fwrite(buf, sizeof(char), readlen, out);
		if (writelen < readlen) {
			ret = -EIO;
			break;
		}
		if (ferror(out)) {
			ret = -EINVAL;
			break;
		}
		toread -= readlen;
		if (!toread) {
			ret = 0;	/* continue reading next packet */
			break;
		}
	}
	return ret;
}

static
int ctf_open_trace_metadata_stream_read(struct ctf_trace *td, FILE **fp,
					char **buf)
{
	FILE *in, *out;
	size_t size;
	int ret;

	in = *fp;
	/*
	 * Using strlen on *buf instead of size of open_memstream
	 * because its size includes garbage at the end (after final
	 * \0). This is the allocated size, not the actual string size.
	 */
	out = open_memstream(buf, &size);
	if (out == NULL)
		return -errno;

	for (;;) {
		ret = ctf_open_trace_metadata_packet_read(td, in, out);
		if (ret) {
			break;
		}
		if (feof(in)) {
			ret = 0;
			break;
		}
	}
	fclose(out);	/* flush the buffer */
	fclose(in);
	/* open for reading */
	*fp = fmemopen(*buf, strlen(*buf), "rb");
	return 0;
}

static
int ctf_open_trace_metadata_read(struct ctf_trace *td)
{
	struct ctf_scanner *scanner;
	struct ctf_file_stream *metadata_stream;
	FILE *fp;
	char *buf = NULL;
	int ret = 0;

	metadata_stream = g_new0(struct ctf_file_stream, 1);
	td->metadata = &metadata_stream->parent;
	metadata_stream->pos.fd = openat(td->dirfd, "metadata", O_RDONLY);
	if (metadata_stream->pos.fd < 0) {
		fprintf(stdout, "Unable to open metadata.\n");
		return metadata_stream->pos.fd;
	}

	if (babeltrace_debug)
		yydebug = 1;

	fp = fdopen(metadata_stream->pos.fd, "r");
	if (!fp) {
		fprintf(stdout, "[error] Unable to open metadata stream.\n");
		ret = -errno;
		goto end_stream;
	}

	if (packet_metadata(td, fp)) {
		ret = ctf_open_trace_metadata_stream_read(td, &fp, &buf);
		if (ret)
			goto end_packet_read;
	}

	scanner = ctf_scanner_alloc(fp);
	if (!scanner) {
		fprintf(stdout, "[error] Error allocating scanner\n");
		ret = -ENOMEM;
		goto end_scanner_alloc;
	}
	ret = ctf_scanner_append_ast(scanner);
	if (ret) {
		fprintf(stdout, "[error] Error creating AST\n");
		goto end;
	}

	if (babeltrace_debug) {
		ret = ctf_visitor_print_xml(stdout, 0, &scanner->ast->root);
		if (ret) {
			fprintf(stdout, "[error] Error visiting AST for XML output\n");
			goto end;
		}
	}

	ret = ctf_visitor_semantic_check(stdout, 0, &scanner->ast->root);
	if (ret) {
		fprintf(stdout, "[error] Error in CTF semantic validation %d\n", ret);
		goto end;
	}
	ret = ctf_visitor_construct_metadata(stdout, 0, &scanner->ast->root,
			td, BYTE_ORDER);
	if (ret) {
		fprintf(stdout, "[error] Error in CTF metadata constructor %d\n", ret);
		goto end;
	}
end:
	ctf_scanner_free(scanner);
end_scanner_alloc:
end_packet_read:
	fclose(fp);
	free(buf);
end_stream:
	close(metadata_stream->pos.fd);
	if (ret)
		g_free(metadata_stream);
	return ret;
}

static
struct ctf_stream_event *create_event_definitions(struct ctf_trace *td,
						  struct ctf_stream *stream,
						  struct ctf_event *event)
{
	struct ctf_stream_event *stream_event = g_new0(struct ctf_stream_event, 1);

	if (event->context_decl) {
		struct definition *definition =
			event->context_decl->p.definition_new(&event->context_decl->p,
				stream->parent_def_scope, 0, 0, "event.context");
		if (!definition) {
			goto error;
		}
		stream_event->event_context = container_of(definition,
					struct definition_struct, p);
		stream->parent_def_scope = stream_event->event_context->p.scope;
	}
	if (event->fields_decl) {
		struct definition *definition =
			event->fields_decl->p.definition_new(&event->fields_decl->p,
				stream->parent_def_scope, 0, 0, "event.fields");
		if (!definition) {
			goto error;
		}
		stream_event->event_fields = container_of(definition,
					struct definition_struct, p);
		stream->parent_def_scope = stream_event->event_fields->p.scope;
	}
	return stream_event;

error:
	if (stream_event->event_fields)
		definition_unref(&stream_event->event_fields->p);
	if (stream_event->event_context)
		definition_unref(&stream_event->event_context->p);
	return NULL;
}

static
int create_stream_definitions(struct ctf_trace *td, struct ctf_stream *stream)
{
	struct ctf_stream_class *stream_class;
	int ret;
	int i;

	if (stream->stream_definitions_created)
		return 0;

	stream_class = stream->stream_class;

	if (stream_class->packet_context_decl) {
		struct definition *definition =
			stream_class->packet_context_decl->p.definition_new(&stream_class->packet_context_decl->p,
				stream->parent_def_scope, 0, 0, "stream.packet.context");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->stream_packet_context = container_of(definition,
						struct definition_struct, p);
		stream->parent_def_scope = stream->stream_packet_context->p.scope;
	}
	if (stream_class->event_header_decl) {
		struct definition *definition =
			stream_class->event_header_decl->p.definition_new(&stream_class->event_header_decl->p,
				stream->parent_def_scope, 0, 0, "stream.event.header");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->stream_event_header =
			container_of(definition, struct definition_struct, p);
		stream->parent_def_scope = stream->stream_event_header->p.scope;
	}
	if (stream_class->event_context_decl) {
		struct definition *definition =
			stream_class->event_context_decl->p.definition_new(&stream_class->event_context_decl->p,
				stream->parent_def_scope, 0, 0, "stream.event.context");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->stream_event_context =
			container_of(definition, struct definition_struct, p);
		stream->parent_def_scope = stream->stream_event_context->p.scope;
	}
	stream->events_by_id = g_ptr_array_new();
	g_ptr_array_set_size(stream->events_by_id, stream_class->events_by_id->len);
	for (i = 0; i < stream->events_by_id->len; i++) {
		struct ctf_event *event = g_ptr_array_index(stream_class->events_by_id, i);
		struct ctf_stream_event *stream_event;

		if (!event)
			continue;
		stream_event = create_event_definitions(td, stream, event);
		if (!stream_event)
			goto error_event;
		g_ptr_array_index(stream->events_by_id, i) = stream_event;
	}
	return 0;

error_event:
	for (i = 0; i < stream->events_by_id->len; i++) {
		struct ctf_stream_event *stream_event = g_ptr_array_index(stream->events_by_id, i);
		if (stream_event)
			g_free(stream_event);
	}
	g_ptr_array_free(stream->events_by_id, TRUE);
error:
	if (stream->stream_event_context)
		definition_unref(&stream->stream_event_context->p);
	if (stream->stream_event_header)
		definition_unref(&stream->stream_event_header->p);
	if (stream->stream_packet_context)
		definition_unref(&stream->stream_packet_context->p);
	return ret;
}


static
int create_stream_packet_index(struct ctf_trace *td,
			       struct ctf_file_stream *file_stream)
{
	struct ctf_stream_class *stream;
	int len_index;
	struct ctf_stream_pos *pos;
	struct stat filestats;
	struct packet_index packet_index;
	int first_packet = 1;
	int ret;

	pos = &file_stream->pos;

	ret = fstat(pos->fd, &filestats);
	if (ret < 0)
		return ret;

	if (filestats.st_size < MAX_PACKET_HEADER_LEN / CHAR_BIT)
		return -EINVAL;

	for (pos->mmap_offset = 0; pos->mmap_offset < filestats.st_size; ) {
		uint64_t stream_id = 0;

		if (pos->base) {
			/* unmap old base */
			ret = munmap(pos->base, pos->packet_size / CHAR_BIT);
			if (ret) {
				fprintf(stdout, "[error] Unable to unmap old base: %s.\n",
					strerror(errno));
				return ret;
			}
			pos->base = NULL;
		}
		/* map new base. Need mapping length from header. */
		pos->base = mmap(NULL, MAX_PACKET_HEADER_LEN / CHAR_BIT, PROT_READ,
				 MAP_PRIVATE, pos->fd, pos->mmap_offset);
		pos->content_size = MAX_PACKET_HEADER_LEN;	/* Unknown at this point */
		pos->packet_size = MAX_PACKET_HEADER_LEN;	/* Unknown at this point */
		pos->offset = 0;	/* Position of the packet header */

		packet_index.offset = pos->mmap_offset;
		packet_index.content_size = 0;
		packet_index.packet_size = 0;
		packet_index.timestamp_begin = 0;
		packet_index.timestamp_end = 0;

		/* read and check header, set stream id (and check) */
		if (file_stream->parent.trace_packet_header) {
			/* Read packet header */
			ret = generic_rw(&pos->parent, &file_stream->parent.trace_packet_header->p);
			if (ret)
				return ret;
			len_index = struct_declaration_lookup_field_index(file_stream->parent.trace_packet_header->declaration, g_quark_from_static_string("magic"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct definition *field;

				field = struct_definition_get_field_from_index(file_stream->parent.trace_packet_header, len_index);
				assert(field->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				if (defint->value._unsigned != CTF_MAGIC) {
					fprintf(stdout, "[error] Invalid magic number 0x%" PRIX64 " at packet %u (file offset %zd).\n",
							defint->value._unsigned,
							file_stream->pos.packet_index->len,
							(ssize_t) pos->mmap_offset);
					return -EINVAL;
				}
			}

			/* check uuid */
			len_index = struct_declaration_lookup_field_index(file_stream->parent.trace_packet_header->declaration, g_quark_from_static_string("uuid"));
			if (len_index >= 0) {
				struct definition_array *defarray;
				struct definition *field;
				uint64_t i;
				uint8_t uuidval[UUID_LEN];

				field = struct_definition_get_field_from_index(file_stream->parent.trace_packet_header, len_index);
				assert(field->declaration->id == CTF_TYPE_ARRAY);
				defarray = container_of(field, struct definition_array, p);
				assert(array_len(defarray) == UUID_LEN);
				assert(defarray->declaration->elem->id == CTF_TYPE_INTEGER);

				for (i = 0; i < UUID_LEN; i++) {
					struct definition *elem;
					struct definition_integer *defint;

					elem = array_index(defarray, i);
					assert(elem);
					defint = container_of(elem, struct definition_integer, p);
					uuidval[i] = defint->value._unsigned;
				}
				ret = uuid_compare(td->uuid, uuidval);
				if (ret) {
					fprintf(stdout, "[error] Unique Universal Identifiers do not match.\n");
					return -EINVAL;
				}
			}


			len_index = struct_declaration_lookup_field_index(file_stream->parent.trace_packet_header->declaration, g_quark_from_static_string("stream_id"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct definition *field;

				field = struct_definition_get_field_from_index(file_stream->parent.trace_packet_header, len_index);
				assert(field->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				stream_id = defint->value._unsigned;
			}
		}

		if (!first_packet && file_stream->parent.stream_id != stream_id) {
			fprintf(stdout, "[error] Stream ID is changing within a stream.\n");
			return -EINVAL;
		}
		if (first_packet) {
			file_stream->parent.stream_id = stream_id;
			if (stream_id >= td->streams->len) {
				fprintf(stdout, "[error] Stream %" PRIu64 " is not declared in metadata.\n", stream_id);
				return -EINVAL;
			}
			stream = g_ptr_array_index(td->streams, stream_id);
			if (!stream) {
				fprintf(stdout, "[error] Stream %" PRIu64 " is not declared in metadata.\n", stream_id);
				return -EINVAL;
			}
			file_stream->parent.stream_class = stream;
		}
		first_packet = 0;

		ret = create_stream_definitions(td, &file_stream->parent);
		if (ret)
			return ret;

		if (file_stream->parent.stream_packet_context) {
			/* Read packet context */
			ret = generic_rw(&pos->parent, &file_stream->parent.stream_packet_context->p);
			if (ret)
				return ret;
			/* read content size from header */
			len_index = struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("content_size"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct definition *field;

				field = struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
				assert(field->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				packet_index.content_size = defint->value._unsigned;
			} else {
				/* Use file size for packet size */
				packet_index.content_size = filestats.st_size * CHAR_BIT;
			}

			/* read packet size from header */
			len_index = struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("packet_size"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct definition *field;

				field = struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
				assert(field->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				packet_index.packet_size = defint->value._unsigned;
			} else {
				/* Use content size if non-zero, else file size */
				packet_index.packet_size = packet_index.content_size ? : filestats.st_size * CHAR_BIT;
			}

			/* read timestamp begin from header */
			len_index = struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("timestamp_begin"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct definition *field;

				field = struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
				assert(field->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				packet_index.timestamp_begin = defint->value._unsigned;
			}

			/* read timestamp end from header */
			len_index = struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("timestamp_end"));
			if (len_index >= 0) {
				struct definition_integer *defint;
				struct definition *field;

				field = struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
				assert(field->declaration->id == CTF_TYPE_INTEGER);
				defint = container_of(field, struct definition_integer, p);
				assert(defint->declaration->signedness == FALSE);
				packet_index.timestamp_end = defint->value._unsigned;
			}
		} else {
			/* Use file size for packet size */
			packet_index.content_size = filestats.st_size * CHAR_BIT;
			/* Use content size if non-zero, else file size */
			packet_index.packet_size = packet_index.content_size ? : filestats.st_size * CHAR_BIT;
		}

		/* Validate content size and packet size values */
		if (packet_index.content_size > packet_index.packet_size) {
			fprintf(stdout, "[error] Content size (%zu bits) is larger than packet size (%zu bits).\n",
				packet_index.content_size, packet_index.packet_size);
			return -EINVAL;
		}

		if (packet_index.packet_size > (filestats.st_size - packet_index.offset) * CHAR_BIT) {
			fprintf(stdout, "[error] Packet size (%zu bits) is larger than remaining file size (%zu bits).\n",
				packet_index.content_size, (filestats.st_size - packet_index.offset) * CHAR_BIT);
			return -EINVAL;
		}

		/* Save position after header and context */
		packet_index.data_offset = pos->offset;

		/* add index to packet array */
		g_array_append_val(file_stream->pos.packet_index, packet_index);

		pos->mmap_offset += packet_index.packet_size / CHAR_BIT;
	}

	/* Move pos back to beginning of file */
	ctf_move_pos_slow(pos, 0, SEEK_SET);	/* position for write */

	return 0;
}

static
int create_trace_definitions(struct ctf_trace *td, struct ctf_stream *stream)
{
	int ret;

	if (td->packet_header_decl) {
		struct definition *definition =
			td->packet_header_decl->p.definition_new(&td->packet_header_decl->p,
				stream->parent_def_scope, 0, 0, "trace.packet.header");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->trace_packet_header = 
			container_of(definition, struct definition_struct, p);
		stream->parent_def_scope = stream->trace_packet_header->p.scope;
	}

	return 0;

error:
	return ret;
}

/*
 * Note: many file streams can inherit from the same stream class
 * description (metadata).
 */
static
int ctf_open_file_stream_read(struct ctf_trace *td, const char *path, int flags)
{
	int ret;
	struct ctf_file_stream *file_stream;

	ret = openat(td->dirfd, path, flags);
	if (ret < 0)
		goto error;
	file_stream = g_new0(struct ctf_file_stream, 1);
	ctf_init_pos(&file_stream->pos, ret, flags);
	ret = create_trace_definitions(td, &file_stream->parent);
	if (ret)
		goto error_def;
	ret = create_stream_packet_index(td, file_stream);
	if (ret)
		goto error_index;
	/* Add stream file to stream class */
	g_ptr_array_add(file_stream->parent.stream_class->streams,
			&file_stream->parent);
	return 0;

error_index:
	if (file_stream->parent.trace_packet_header)
		definition_unref(&file_stream->parent.trace_packet_header->p);
error_def:
	ctf_fini_pos(&file_stream->pos);
	close(file_stream->pos.fd);
	g_free(file_stream);
error:
	return ret;
}

static
int ctf_open_trace_read(struct ctf_trace *td, const char *path, int flags)
{
	int ret;
	struct dirent *dirent;
	struct dirent *diriter;
	size_t dirent_len;

	td->flags = flags;

	/* Open trace directory */
	td->dir = opendir(path);
	if (!td->dir) {
		fprintf(stdout, "[error] Unable to open trace directory.\n");
		ret = -ENOENT;
		goto error;
	}

	td->dirfd = open(path, 0);
	if (td->dirfd < 0) {
		fprintf(stdout, "[error] Unable to open trace directory file descriptor.\n");
		ret = -ENOENT;
		goto error_dirfd;
	}

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
			fpathconf(td->dirfd, _PC_NAME_MAX) + 1;

	dirent = malloc(dirent_len);

	for (;;) {
		ret = readdir_r(td->dir, dirent, &diriter);
		if (ret) {
			fprintf(stdout, "[error] Readdir error.\n");
			goto readdir_error;
		}
		if (!diriter)
			break;
		/* Ignore hidden files, ., .. and metadata. */
		if (!strncmp(diriter->d_name, ".", 1)
				|| !strcmp(diriter->d_name, "..")
				|| !strcmp(diriter->d_name, "metadata"))
			continue;
		ret = ctf_open_file_stream_read(td, diriter->d_name, flags);
		if (ret) {
			fprintf(stdout, "[error] Open file stream error.\n");
			goto readdir_error;
		}
	}

	free(dirent);
	return 0;

readdir_error:
	free(dirent);
error_metadata:
	close(td->dirfd);
error_dirfd:
	closedir(td->dir);
error:
	return ret;
}

static
struct trace_descriptor *ctf_open_trace(const char *path, int flags)
{
	struct ctf_trace *td;
	int ret;

	td = g_new0(struct ctf_trace, 1);

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		if (!path) {
			fprintf(stdout, "[error] Path missing for input CTF trace.\n");
			goto error;
		}
		ret = ctf_open_trace_read(td, path, flags);
		if (ret)
			goto error;
		break;
	case O_RDWR:
		fprintf(stdout, "[error] Opening CTF traces for output is not supported yet.\n");
		goto error;
	default:
		fprintf(stdout, "[error] Incorrect open flags.\n");
		goto error;
	}

	return &td->parent;
error:
	g_free(td);
	return NULL;
}

static
void ctf_close_file_stream(struct ctf_file_stream *file_stream)
{
	ctf_fini_pos(&file_stream->pos);
	close(file_stream->pos.fd);
}

static
void ctf_close_trace(struct trace_descriptor *tdp)
{
	struct ctf_trace *td = container_of(tdp, struct ctf_trace, parent);
	int i;

	if (td->streams) {
		for (i = 0; i < td->streams->len; i++) {
			struct ctf_stream_class *stream;
			int j;

			stream = g_ptr_array_index(td->streams, i);
			if (!stream)
				continue;
			for (j = 0; j < stream->streams->len; j++) {
				struct ctf_file_stream *file_stream;
				file_stream = container_of(g_ptr_array_index(stream->streams, j), struct ctf_file_stream, parent);
				ctf_close_file_stream(file_stream);
			}

		}
		g_ptr_array_free(td->streams, TRUE);
	}
	closedir(td->dir);
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
