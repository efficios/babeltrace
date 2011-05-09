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
int convert_stream(struct ctf_text_stream_pos *sout,
		   struct ctf_file_stream *sin)
{
	int ret;

	/* For each event, print header, context, payload */
	/* TODO: order events by timestamps across streams */
	for (;;) {
		ret = sin->pos.parent.event_cb(&sin->pos.parent, &sin->stream);
		if (ret == EOF)
			break;
		else if (ret) {
			fprintf(stdout, "[error] Reading event failed.\n");
			goto error;
		}
		ret = sout->parent.event_cb(&sout->parent, &sin->stream);
		if (ret) {
			fprintf(stdout, "[error] Writing event failed.\n");
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
		struct ctf_stream_class *stream = g_ptr_array_index(tin->streams, stream_id);

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
