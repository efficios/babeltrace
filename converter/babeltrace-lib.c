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
#include <babeltrace/prio_heap.h>

static int read_event(struct ctf_file_stream *sin)
{
	int ret;

	ret = sin->pos.parent.event_cb(&sin->pos.parent, &sin->parent);
	if (ret == EOF)
		return EOF;
	else if (ret) {
		fprintf(stdout, "[error] Reading event failed.\n");
		return ret;
	}
	return 0;
}

/*
 * returns true if a < b, false otherwise.
 */
int stream_compare(void *a, void *b)
{
	struct ctf_file_stream *s_a = a, *s_b = b;

	if (s_a->parent.timestamp < s_b->parent.timestamp)
		return 1;
	else
		return 0;
}

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_descriptor *td_read)
{
	struct ctf_trace *tin = container_of(td_read, struct ctf_trace, parent);
	struct ctf_text_stream_pos *sout =
		container_of(td_write, struct ctf_text_stream_pos, trace_descriptor);
	int stream_id;
	int ret = 0;

	tin->stream_heap = g_new(struct ptr_heap, 1);
	heap_init(tin->stream_heap, 0, stream_compare);

	/* Populate heap with each stream */
	for (stream_id = 0; stream_id < tin->streams->len; stream_id++) {
		struct ctf_stream_class *stream = g_ptr_array_index(tin->streams, stream_id);
		int filenr;

		if (!stream)
			continue;
		for (filenr = 0; filenr < stream->streams->len; filenr++) {
			struct ctf_file_stream *file_stream = g_ptr_array_index(stream->streams, filenr);
			ret = read_event(file_stream);
			if (ret == EOF) {
				ret = 0;
				continue;
			} else if (ret)
				goto end;
			/* Add to heap */
			ret = heap_insert(tin->stream_heap, file_stream);
			if (ret) {
				fprintf(stdout, "[error] Out of memory.\n");
				goto end;
			}
		}
	}

	/* Replace heap entries until EOF for each stream (heap empty) */
	for (;;) {
		struct ctf_file_stream *file_stream, *removed;

		file_stream = heap_maximum(tin->stream_heap);
		if (!file_stream) {
			/* end of file for all streams */
			ret = 0;
			break;
		}
		ret = sout->parent.event_cb(&sout->parent, &file_stream->parent);
		if (ret) {
			fprintf(stdout, "[error] Writing event failed.\n");
			goto end;
		}
		ret = read_event(file_stream);
		if (ret == EOF) {
			removed = heap_remove(tin->stream_heap);
			assert(removed == file_stream);
			ret = 0;
			continue;
		} else if (ret)
			goto end;
		/* Reinsert the file stream into the heap, and rebalance. */
		removed = heap_replace_max(tin->stream_heap, file_stream);
		assert(removed == file_stream);
	}

end:
	heap_free(tin->stream_heap);
	g_free(tin->stream_heap);
	return ret;
}
