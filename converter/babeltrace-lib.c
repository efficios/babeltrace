/*
 * babeltrace-lib.c
 *
 * Babeltrace Trace Converter Library
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/ctf-text/types.h>

static
int convert_event(struct ctf_text_stream_pos *sout,
		  struct ctf_file_stream *sin)
{
	struct ctf_stream *stream_class = sin->stream;
	struct ctf_event *event_class;
	uint64_t id = 0;
	int len_index;

	if (sin->pos.offset == -EOF)
		return -EOF;

	/* Hide event payload struct brackets */
	sout->depth = -1;

	/* Read and print event header */
	if (stream_class->event_header) {
		generic_rw(&sin->pos.parent, &stream_class->event_header->p);

		/* lookup event id */
		len_index = struct_declaration_lookup_field_index(stream_class->event_header_decl,
				g_quark_from_static_string("id"));
		if (len_index >= 0) {
			struct definition_integer *defint;
			struct field *field;

			field = struct_definition_get_field_from_index(stream_class->event_header, len_index);
			assert(field->definition->declaration->id == CTF_TYPE_INTEGER);
			defint = container_of(field->definition, struct definition_integer, p);
			assert(defint->declaration->signedness == FALSE);
			id = defint->value._unsigned;	/* set id */
		}

		generic_rw(&sout->parent, &stream_class->event_header->p);
	}

	/* Read and print stream-declared event context */
	if (stream_class->event_context) {
		generic_rw(&sin->pos.parent, &stream_class->event_context->p);
		generic_rw(&sout->parent, &stream_class->event_context->p);
	}

	if (id >= stream_class->events_by_id->len) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -EINVAL;
	}
	event_class = g_ptr_array_index(stream_class->events_by_id, id);
	if (!event_class) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	/* Read and print event-declared event context */
	if (event_class->context) {
		generic_rw(&sin->pos.parent, &event_class->context->p);
		generic_rw(&sout->parent, &event_class->context->p);
	}

	/* Read and print event payload */
	if (event_class->fields) {
		generic_rw(&sin->pos.parent, &event_class->fields->p);
		generic_rw(&sout->parent, &event_class->fields->p);
	}

	return 0;
}

static
int convert_stream(struct ctf_text_stream_pos *sout,
		   struct ctf_file_stream *sin)
{
	int ret;

	/* For each event, print header, context, payload */
	/* TODO: order events by timestamps across streams */
	for (;;) {
		ret = convert_event(sout, sin);
		if (ret == -EOF)
			break;
		else if (ret) {
			fprintf(stdout, "[error] Printing event failed.\n");
			goto error;
		}
	}

	return 0;

error:
	return ret;
}

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_descriptor *td_read)
{
	struct ctf_trace *tin = container_of(td_read, struct ctf_trace, parent);
	struct ctf_text_stream_pos *sout =
		container_of(td_write, struct ctf_text_stream_pos, trace_descriptor);
	int stream_id, filenr;
	int ret;

	/* For each stream (TODO: order events by timestamp) */
	for (stream_id = 0; stream_id < tin->streams->len; stream_id++) {
		struct ctf_stream *stream = g_ptr_array_index(tin->streams, stream_id);

		if (!stream)
			continue;
		for (filenr = 0; filenr < stream->files->len; filenr++) {
			struct ctf_file_stream *file_stream = g_ptr_array_index(stream->files, filenr);
			ret = convert_stream(sout, file_stream);
			if (ret) {
				fprintf(stdout, "[error] Printing stream %d failed.\n", stream_id);
				goto error;
			}
		}
	}

	return 0;

error:
	return ret;
}
