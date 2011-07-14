/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * CTF Text Format registration.
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <babeltrace/ctf-text/types.h>
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

struct trace_descriptor *ctf_text_open_trace(const char *path, int flags);
void ctf_text_close_trace(struct trace_descriptor *descriptor);

static
rw_dispatch write_dispatch_table[] = {
	[ CTF_TYPE_INTEGER ] = ctf_text_integer_write,
	[ CTF_TYPE_FLOAT ] = ctf_text_float_write,
	[ CTF_TYPE_ENUM ] = ctf_text_enum_write,
	[ CTF_TYPE_STRING ] = ctf_text_string_write,
	[ CTF_TYPE_STRUCT ] = ctf_text_struct_write,
	[ CTF_TYPE_VARIANT ] = ctf_text_variant_write,
	[ CTF_TYPE_ARRAY ] = ctf_text_array_write,
	[ CTF_TYPE_SEQUENCE ] = ctf_text_sequence_write,
};

static
struct format ctf_text_format = {
	.open_trace = ctf_text_open_trace,
	.close_trace = ctf_text_close_trace,
};

static GQuark Q_STREAM_PACKET_CONTEXT_TIMESTAMP_BEGIN,
	Q_STREAM_PACKET_CONTEXT_TIMESTAMP_END,
	Q_STREAM_PACKET_CONTEXT_EVENTS_DISCARDED,
	Q_STREAM_PACKET_CONTEXT_CONTENT_SIZE,
	Q_STREAM_PACKET_CONTEXT_PACKET_SIZE;

static
void __attribute__((constructor)) init_quarks(void)
{
	Q_STREAM_PACKET_CONTEXT_TIMESTAMP_BEGIN = g_quark_from_static_string("stream.packet.context.timestamp_begin");
	Q_STREAM_PACKET_CONTEXT_TIMESTAMP_END = g_quark_from_static_string("stream.packet.context.timestamp_end");
	Q_STREAM_PACKET_CONTEXT_EVENTS_DISCARDED = g_quark_from_static_string("stream.packet.context.events_discarded");
	Q_STREAM_PACKET_CONTEXT_CONTENT_SIZE = g_quark_from_static_string("stream.packet.context.content_size");
	Q_STREAM_PACKET_CONTEXT_PACKET_SIZE = g_quark_from_static_string("stream.packet.context.packet_size");
}

int print_field(struct definition *definition)
{
	/* Print all fields in verbose mode */
	if (babeltrace_verbose)
		return 1;

	/* Filter out part of the packet context */
	if (definition->path == Q_STREAM_PACKET_CONTEXT_TIMESTAMP_BEGIN)
		return 0;
	if (definition->path == Q_STREAM_PACKET_CONTEXT_TIMESTAMP_END)
		return 0;
	if (definition->path == Q_STREAM_PACKET_CONTEXT_EVENTS_DISCARDED)
		return 0;
	if (definition->path == Q_STREAM_PACKET_CONTEXT_CONTENT_SIZE)
		return 0;
	if (definition->path == Q_STREAM_PACKET_CONTEXT_PACKET_SIZE)
		return 0;

	return 1;
}

static
int ctf_text_write_event(struct stream_pos *ppos,
			 struct ctf_stream *stream)
{
	struct ctf_text_stream_pos *pos =
		container_of(ppos, struct ctf_text_stream_pos, parent);
	struct ctf_stream_class *stream_class = stream->stream_class;
	int field_nr_saved;
	struct ctf_event *event_class;
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
	event_class = g_ptr_array_index(stream_class->events_by_id, id);
	if (!event) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	if (stream->has_timestamp) {
		if (pos->print_names)
			fprintf(pos->fp, "timestamp = ");
		else
			fprintf(pos->fp, "[");
		fprintf(pos->fp, "%12" PRIu64, stream->timestamp);
		if (!pos->print_names)
			fprintf(pos->fp, "]");

		if (pos->print_names)
			fprintf(pos->fp, ", ");
		else
			fprintf(pos->fp, " ");
	}
	if (pos->print_names)
		fprintf(pos->fp, "name = ");
	fprintf(pos->fp, "%s", g_quark_to_string(event_class->name));
	if (pos->print_names)
		pos->field_nr++;
	else
		fprintf(pos->fp, ":");

	/* print cpuid field from packet context */
	if (stream->stream_packet_context) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		if (pos->print_names)
			fprintf(pos->fp, " stream.packet.context =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		ret = generic_rw(ppos, &stream->stream_packet_context->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* Only show the event header in verbose mode */
	if (babeltrace_verbose && stream->stream_event_header) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		if (pos->print_names)
			fprintf(pos->fp, " stream.event.header =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		ret = generic_rw(ppos, &stream->stream_event_header->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* print stream-declared event context */
	if (stream->stream_event_context) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		if (pos->print_names)
			fprintf(pos->fp, " stream.event.context =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		ret = generic_rw(ppos, &stream->stream_event_context->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* print event-declared event context */
	if (event->event_context) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		if (pos->print_names)
			fprintf(pos->fp, " event.context =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		ret = generic_rw(ppos, &event->event_context->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* Read and print event payload */
	if (event->event_fields) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		if (pos->print_names)
			fprintf(pos->fp, " event.fields =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		ret = generic_rw(ppos, &event->event_fields->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}
	/* newline */
	fprintf(pos->fp, "\n");
	pos->field_nr = 0;

	return 0;

error:
	fprintf(stdout, "[error] Unexpected end of stream. Either the trace data stream is corrupted or metadata description does not match data layout.\n");
	return ret;
}


struct trace_descriptor *ctf_text_open_trace(const char *path, int flags)
{
	struct ctf_text_stream_pos *pos;
	FILE *fp;

	pos = g_new0(struct ctf_text_stream_pos, 1);

	switch (flags & O_ACCMODE) {
	case O_RDWR:
		if (!path)
			fp = stdout;
		else
			fp = fopen(path, "w");
		if (!fp)
			goto error;
		pos->fp = fp;
		pos->parent.rw_table = write_dispatch_table;
		pos->parent.event_cb = ctf_text_write_event;
		pos->print_names = opt_field_names;
		break;
	case O_RDONLY:
	default:
		fprintf(stdout, "[error] Incorrect open flags.\n");
		goto error;
	}

	return &pos->trace_descriptor;
error:
	g_free(pos);
	return NULL;
}

void ctf_text_close_trace(struct trace_descriptor *td)
{
	struct ctf_text_stream_pos *pos =
		container_of(td, struct ctf_text_stream_pos, trace_descriptor);
	fclose(pos->fp);
	g_free(pos);
}

void __attribute__((constructor)) ctf_text_init(void)
{
	int ret;

	ctf_text_format.name = g_quark_from_static_string("text");
	ret = bt_register_format(&ctf_text_format);
	assert(!ret);
}

/* TODO: finalize */
