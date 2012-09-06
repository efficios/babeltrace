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
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/events-internal.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>

#define NSEC_PER_SEC 1000000000ULL

int opt_all_field_names,
	opt_scope_field_names,
	opt_header_field_names,
	opt_context_field_names,
	opt_payload_field_names,
	opt_all_fields,
	opt_trace_field,
	opt_trace_domain_field,
	opt_trace_procname_field,
	opt_trace_vpid_field,
	opt_trace_hostname_field,
	opt_trace_default_fields = 1,
	opt_loglevel_field,
	opt_delta_field = 1;

enum field_item {
	ITEM_SCOPE,
	ITEM_HEADER,
	ITEM_CONTEXT,
	ITEM_PAYLOAD,
};

enum bt_loglevel {
        BT_LOGLEVEL_EMERG                  = 0,
        BT_LOGLEVEL_ALERT                  = 1,
        BT_LOGLEVEL_CRIT                   = 2,
        BT_LOGLEVEL_ERR                    = 3,
        BT_LOGLEVEL_WARNING                = 4,
        BT_LOGLEVEL_NOTICE                 = 5,
        BT_LOGLEVEL_INFO                   = 6,
        BT_LOGLEVEL_DEBUG_SYSTEM           = 7,
        BT_LOGLEVEL_DEBUG_PROGRAM          = 8,
        BT_LOGLEVEL_DEBUG_PROCESS          = 9,
        BT_LOGLEVEL_DEBUG_MODULE           = 10,
        BT_LOGLEVEL_DEBUG_UNIT             = 11,
        BT_LOGLEVEL_DEBUG_FUNCTION         = 12,
        BT_LOGLEVEL_DEBUG_LINE             = 13,
        BT_LOGLEVEL_DEBUG                  = 14,
};


struct trace_descriptor *ctf_text_open_trace(const char *path, int flags,
		void (*packet_seek)(struct stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp);
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
void set_field_names_print(struct ctf_text_stream_pos *pos, enum field_item item)
{
	switch (item) {
	case ITEM_SCOPE:
		if (opt_all_field_names || opt_scope_field_names)
			pos->print_names = 1;
		else
			pos->print_names = 0;
		break;
	case ITEM_HEADER:
		if (opt_all_field_names || opt_header_field_names)
			pos->print_names = 1;
		else
			pos->print_names = 0;
		break;
	case ITEM_CONTEXT:
		if (opt_all_field_names || opt_context_field_names)
			pos->print_names = 1;
		else
			pos->print_names = 0;
		break;
	case ITEM_PAYLOAD:
		if (opt_all_field_names || opt_payload_field_names)
			pos->print_names = 1;
		else
			pos->print_names = 0;

		break;
	default:
		assert(0);
	}
}

static
const char *print_loglevel(int value)
{
	switch (value) {
	case -1:
		return "";
	case BT_LOGLEVEL_EMERG:
		return "TRACE_EMERG";
	case BT_LOGLEVEL_ALERT:
		return "TRACE_ALERT";
	case BT_LOGLEVEL_CRIT:
		return "TRACE_CRIT";
	case BT_LOGLEVEL_ERR:
		return "TRACE_ERR";
	case BT_LOGLEVEL_WARNING:
		return "TRACE_WARNING";
	case BT_LOGLEVEL_NOTICE:
		return "TRACE_NOTICE";
	case BT_LOGLEVEL_INFO:
		return "TRACE_INFO";
	case BT_LOGLEVEL_DEBUG_SYSTEM:
		return "TRACE_DEBUG_SYSTEM";
	case BT_LOGLEVEL_DEBUG_PROGRAM:
		return "TRACE_DEBUG_PROGRAM";
	case BT_LOGLEVEL_DEBUG_PROCESS:
		return "TRACE_DEBUG_PROCESS";
	case BT_LOGLEVEL_DEBUG_MODULE:
		return "TRACE_DEBUG_MODULE";
	case BT_LOGLEVEL_DEBUG_UNIT:
		return "TRACE_DEBUG_UNIT";
	case BT_LOGLEVEL_DEBUG_FUNCTION:
		return "TRACE_DEBUG_FUNCTION";
	case BT_LOGLEVEL_DEBUG_LINE:
		return "TRACE_DEBUG_LINE";
	case BT_LOGLEVEL_DEBUG:
		return "TRACE_DEBUG";
	default:
		return "<<UNKNOWN>>";
	}
}

static
int ctf_text_write_event(struct stream_pos *ppos, struct ctf_stream_definition *stream)
			 
{
	struct ctf_text_stream_pos *pos =
		container_of(ppos, struct ctf_text_stream_pos, parent);
	struct ctf_stream_declaration *stream_class = stream->stream_class;
	int field_nr_saved;
	struct ctf_event_declaration *event_class;
	struct ctf_event_definition *event;
	uint64_t id;
	int ret;
	int dom_print = 0;

	id = stream->event_id;

	if (id >= stream_class->events_by_id->len) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -EINVAL;
	}
	event = g_ptr_array_index(stream->events_by_id, id);
	if (!event) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}
	event_class = g_ptr_array_index(stream_class->events_by_id, id);
	if (!event_class) {
		fprintf(stderr, "[error] Event class id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	/* Print events discarded */
	if (stream->events_discarded) {
		fflush(pos->fp);
		fprintf(stderr, "[warning] Tracer discarded %" PRIu64 " events between [",
			stream->events_discarded);
		if (opt_clock_cycles) {
			ctf_print_timestamp(stderr, stream,
					stream->prev_cycles_timestamp);
			fprintf(stderr, "] and [");
			ctf_print_timestamp(stderr, stream,
					stream->prev_cycles_timestamp_end);
		} else {
			ctf_print_timestamp(stderr, stream,
					stream->prev_real_timestamp);
			fprintf(stderr, "] and [");
			ctf_print_timestamp(stderr, stream,
					stream->prev_real_timestamp_end);
		}
		fprintf(stderr, "]. You should consider recording a new trace with larger buffers or with fewer events enabled.\n");
		fflush(stderr);
		stream->events_discarded = 0;
	}

	if (stream->has_timestamp) {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names)
			fprintf(pos->fp, "timestamp = ");
		else
			fprintf(pos->fp, "[");
		if (opt_clock_cycles) {
			ctf_print_timestamp(pos->fp, stream, stream->cycles_timestamp);
		} else {
			ctf_print_timestamp(pos->fp, stream, stream->real_timestamp);
		}
		if (!pos->print_names)
			fprintf(pos->fp, "]");

		if (pos->print_names)
			fprintf(pos->fp, ", ");
		else
			fprintf(pos->fp, " ");
	}
	if ((opt_delta_field || opt_all_fields) && stream->has_timestamp) {
		uint64_t delta, delta_sec, delta_nsec;

		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names)
			fprintf(pos->fp, "delta = ");
		else
			fprintf(pos->fp, "(");
		if (pos->last_real_timestamp != -1ULL) {
			delta = stream->real_timestamp - pos->last_real_timestamp;
			delta_sec = delta / NSEC_PER_SEC;
			delta_nsec = delta % NSEC_PER_SEC;
			fprintf(pos->fp, "+%" PRIu64 ".%09" PRIu64,
				delta_sec, delta_nsec);
		} else {
			fprintf(pos->fp, "+?.?????????");
		}
		if (!pos->print_names)
			fprintf(pos->fp, ")");

		if (pos->print_names)
			fprintf(pos->fp, ", ");
		else
			fprintf(pos->fp, " ");
		pos->last_real_timestamp = stream->real_timestamp;
		pos->last_cycles_timestamp = stream->cycles_timestamp;
	}

	if ((opt_trace_field || opt_all_fields) && stream_class->trace->path[0] != '\0') {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names) {
			fprintf(pos->fp, "trace = ");
		}
		fprintf(pos->fp, "%s", stream_class->trace->path);
		if (pos->print_names)
			fprintf(pos->fp, ", ");
		else
			fprintf(pos->fp, " ");
	}
	if ((opt_trace_hostname_field || opt_all_fields || opt_trace_default_fields)
			&& stream_class->trace->env.hostname[0] != '\0') {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names) {
			fprintf(pos->fp, "trace:hostname = ");
		}
		fprintf(pos->fp, "%s", stream_class->trace->env.hostname);
		if (pos->print_names)
			fprintf(pos->fp, ", ");
		dom_print = 1;
	}
	if ((opt_trace_domain_field || opt_all_fields) && stream_class->trace->env.domain[0] != '\0') {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names) {
			fprintf(pos->fp, "trace:domain = ");
		}
		fprintf(pos->fp, "%s", stream_class->trace->env.domain);
		if (pos->print_names)
			fprintf(pos->fp, ", ");
		dom_print = 1;
	}
	if ((opt_trace_procname_field || opt_all_fields || opt_trace_default_fields)
			&& stream_class->trace->env.procname[0] != '\0') {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names) {
			fprintf(pos->fp, "trace:procname = ");
		} else if (dom_print) {
			fprintf(pos->fp, ":");
		}
		fprintf(pos->fp, "%s", stream_class->trace->env.procname);
		if (pos->print_names)
			fprintf(pos->fp, ", ");
		dom_print = 1;
	}
	if ((opt_trace_vpid_field || opt_all_fields || opt_trace_default_fields)
			&& stream_class->trace->env.vpid != -1) {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names) {
			fprintf(pos->fp, "trace:vpid = ");
		} else if (dom_print) {
			fprintf(pos->fp, ":");
		}
		fprintf(pos->fp, "%d", stream_class->trace->env.vpid);
		if (pos->print_names)
			fprintf(pos->fp, ", ");
		dom_print = 1;
	}
	if ((opt_loglevel_field || opt_all_fields) && event_class->loglevel != -1) {
		set_field_names_print(pos, ITEM_HEADER);
		if (pos->print_names) {
			fprintf(pos->fp, "loglevel = ");
		} else if (dom_print) {
			fprintf(pos->fp, ":");
		}
		fprintf(pos->fp, "%s (%d)",
			print_loglevel(event_class->loglevel),
			event_class->loglevel);
		if (pos->print_names)
			fprintf(pos->fp, ", ");
		dom_print = 1;
	}
	if (dom_print && !pos->print_names)
		fprintf(pos->fp, " ");
	set_field_names_print(pos, ITEM_HEADER);
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
		set_field_names_print(pos, ITEM_SCOPE);
		if (pos->print_names)
			fprintf(pos->fp, " stream.packet.context =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		set_field_names_print(pos, ITEM_CONTEXT);
		ret = generic_rw(ppos, &stream->stream_packet_context->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* Only show the event header in verbose mode */
	if (babeltrace_verbose && stream->stream_event_header) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		set_field_names_print(pos, ITEM_SCOPE);
		if (pos->print_names)
			fprintf(pos->fp, " stream.event.header =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		set_field_names_print(pos, ITEM_CONTEXT);
		ret = generic_rw(ppos, &stream->stream_event_header->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* print stream-declared event context */
	if (stream->stream_event_context) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		set_field_names_print(pos, ITEM_SCOPE);
		if (pos->print_names)
			fprintf(pos->fp, " stream.event.context =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		set_field_names_print(pos, ITEM_CONTEXT);
		ret = generic_rw(ppos, &stream->stream_event_context->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* print event-declared event context */
	if (event->event_context) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		set_field_names_print(pos, ITEM_SCOPE);
		if (pos->print_names)
			fprintf(pos->fp, " event.context =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		set_field_names_print(pos, ITEM_CONTEXT);
		ret = generic_rw(ppos, &event->event_context->p);
		if (ret)
			goto error;
		pos->field_nr = field_nr_saved;
	}

	/* Read and print event payload */
	if (event->event_fields) {
		if (pos->field_nr++ != 0)
			fprintf(pos->fp, ",");
		set_field_names_print(pos, ITEM_SCOPE);
		if (pos->print_names)
			fprintf(pos->fp, " event.fields =");
		field_nr_saved = pos->field_nr;
		pos->field_nr = 0;
		set_field_names_print(pos, ITEM_PAYLOAD);
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
	fprintf(stderr, "[error] Unexpected end of stream. Either the trace data stream is corrupted or metadata description does not match data layout.\n");
	return ret;
}


struct trace_descriptor *ctf_text_open_trace(const char *path, int flags,
		void (*packet_seek)(struct stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp)
{
	struct ctf_text_stream_pos *pos;
	FILE *fp;

	pos = g_new0(struct ctf_text_stream_pos, 1);

	pos->last_real_timestamp = -1ULL;
	pos->last_cycles_timestamp = -1ULL;
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
		pos->print_names = 0;
		break;
	case O_RDONLY:
	default:
		fprintf(stderr, "[error] Incorrect open flags.\n");
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
