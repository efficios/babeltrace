/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * CTF Text Format registration.
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

static
int ctf_text_write_event(struct stream_pos *ppos,
			 struct ctf_stream *stream)
{
	struct ctf_text_stream_pos *pos =
		container_of(ppos, struct ctf_text_stream_pos, parent);
	struct ctf_stream_class *stream_class = stream->stream_class;
	struct ctf_event *event_class;
	uint64_t id = 0;
	int len_index;
	int ret;
	int field_nr = 0;

	/* print event header */
	if (stream_class->event_header) {
		/* lookup event id */
		len_index = struct_declaration_lookup_field_index(stream_class->event_header_decl,
				g_quark_from_static_string("id"));
		if (len_index >= 0) {
			struct definition_integer *defint;
			struct definition *field;

			field = struct_definition_get_field_from_index(stream_class->event_header, len_index);
			assert(field->declaration->id == CTF_TYPE_INTEGER);
			defint = container_of(field, struct definition_integer, p);
			assert(defint->declaration->signedness == FALSE);
			id = defint->value._unsigned;	/* set id */
		}
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
	if (pos->print_names)
		fprintf(pos->fp, "name = ");
	fprintf(pos->fp, "%s", g_quark_to_string(event_class->name));
	if (pos->print_names)
		field_nr++;
	else
		fprintf(pos->fp, ": ");

	/* Only show the event header in verbose mode */
	if (babeltrace_verbose && stream_class->event_header) {
		if (field_nr++ != 0)
			fprintf(pos->fp, ", ");
		if (pos->print_names)
			fprintf(pos->fp, "stream.event.header = ");
		ret = generic_rw(ppos, &stream_class->event_header->p);
		if (ret)
			goto error;
	}

	/* print stream-declared event context */
	if (stream_class->event_context) {
		if (field_nr++ != 0)
			fprintf(pos->fp, ", ");
		if (pos->print_names)
			fprintf(pos->fp, "stream.event.context = ");
		ret = generic_rw(ppos, &stream_class->event_context->p);
		if (ret)
			goto error;
	}

	/* print event-declared event context */
	if (event_class->context) {
		if (field_nr++ != 0)
			fprintf(pos->fp, ", ");
		if (pos->print_names)
			fprintf(pos->fp, "event.context = ");
		ret = generic_rw(ppos, &event_class->context->p);
		if (ret)
			goto error;
	}

	/* Read and print event payload */
	if (event_class->fields) {
		if (field_nr++ != 0)
			fprintf(pos->fp, ", ");
		if (pos->print_names)
			fprintf(pos->fp, "event.fields = ");
		ret = generic_rw(ppos, &event_class->fields->p);
		if (ret)
			goto error;
	}
	fprintf(pos->fp, "\n");

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
