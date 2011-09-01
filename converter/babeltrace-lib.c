/*
 * babeltrace-lib.c
 *
 * Babeltrace Trace Converter Library
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

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/prio_heap.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <stdarg.h>

struct stream_saved_pos {
	/*
	 * Use file_stream pointer to check if the trace collection we
	 * restore to match the one we saved from, for each stream.
	 */
	struct ctf_file_stream *file_stream;
	size_t cur_index;	/* current index in packet index */
	ssize_t offset;		/* offset from base, in bits. EOF for end of file. */
};

struct babeltrace_saved_pos {
	struct trace_collection *tc;
	GArray *stream_saved_pos;	/* Contains struct stream_saved_pos */
};

struct bt_callback {
	int prio;		/* Callback order priority. Lower first. Dynamically assigned from dependency graph. */
	void *private_data;
	enum bt_cb_ret (*callback)(void *private_data, void *caller_data);
};

struct bt_callback_chain {
	GArray *callback;	/* Array of struct bt_callback, ordered by priority */
};

/*
 * per id callbacks need to be per stream class because event ID vs
 * event name mapping can vary from stream to stream.
 */
struct bt_stream_callbacks {
	GArray *per_id_callbacks;	/* Array of struct bt_callback_chain */
};

/*
 * struct babeltrace_iter: data structure representing an iterator on a trace
 * collection.
 */
struct babeltrace_iter {
	struct ptr_heap *stream_heap;
	struct trace_collection *tc;
	struct trace_collection_pos *end_pos;
	GArray *callbacks;				/* Array of struct bt_stream_hooks */
	struct bt_callback_chain main_callbacks;	/* For all events */
	/*
	 * Flag indicating if dependency graph needs to be recalculated.
	 * Set by babeltrace_iter_add_callback(), and checked (and
	 * cleared) by upon entry into babeltrace_iter_read_event().
	 * babeltrace_iter_read_event() is responsible for calling dep
	 * graph calculation if it sees this flag set.
	 */
	int recalculate_dep_graph;
	/*
	 * Array of pointers to struct bt_dependencies, for garbage
	 * collection. We're not using a linked list here because each
	 * struct bt_dependencies can belong to more than one
	 * babeltrace_iter.
	 */
	GPtrArray *dep_gc;
};

struct bt_dependencies {
	GArray *deps;			/* Array of GQuarks */
	int refcount;			/* free when decremented to 0 */
};

static
struct bt_dependencies *_babeltrace_dependencies_create(const char *first,
							va_list ap)
{
	const char *iter;
	struct bt_dependencies *dep;

	dep = g_new0(struct bt_dependencies, 1);
	dep->refcount = 1;
	dep->deps = g_array_new(FALSE, TRUE, sizeof(GQuark));
	iter = first;
	while (iter) {
		GQuark q = g_quark_from_string(iter);
		g_array_append_val(dep->deps, q);
		iter = va_arg(ap, const char *);
	}
	return dep;
}

struct bt_dependencies *babeltrace_dependencies_create(const char *first, ...)
{
	va_list ap;
	struct bt_dependencies *deps;

	va_start(ap, first);
	deps = _babeltrace_dependencies_create(first, ap);
	va_end(ap);
	return deps;
}

static int stream_read_event(struct ctf_file_stream *sin)
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

/* TODO: use begin_pos/end_pos */
struct babeltrace_iter *babeltrace_iter_create(struct trace_collection *tc,
		struct trace_collection_pos *begin_pos,
		struct trace_collection_pos *end_pos)
{
	int i, stream_id;
	int ret = 0;
	struct babeltrace_iter *iter;

	iter = malloc(sizeof(struct babeltrace_iter));
	if (!iter)
		goto error_malloc;
	iter->stream_heap = g_new(struct ptr_heap, 1);
	iter->tc = tc;

	ret = heap_init(iter->stream_heap, 0, stream_compare);
	if (ret < 0)
		goto error_heap_init;

	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		tin = container_of(td_read, struct ctf_trace, parent);

		/* Populate heap with each stream */
		for (stream_id = 0; stream_id < tin->streams->len;
				stream_id++) {
			struct ctf_stream_class *stream;
			int filenr;

			stream = g_ptr_array_index(tin->streams, stream_id);
			if (!stream)
				continue;
			for (filenr = 0; filenr < stream->streams->len;
					filenr++) {
				struct ctf_file_stream *file_stream;

				file_stream = g_ptr_array_index(stream->streams,
						filenr);

				ret = stream_read_event(file_stream);
				if (ret == EOF) {
					ret = 0;
					continue;
				} else if (ret) {
					goto error;
				}
				/* Add to heap */
				ret = heap_insert(iter->stream_heap, file_stream);
				if (ret)
					goto error;
			}
		}
	}

	return iter;

error:
	heap_free(iter->stream_heap);
error_heap_init:
	g_free(iter->stream_heap);
	free(iter);
error_malloc:
	return NULL;
}

void babeltrace_iter_destroy(struct babeltrace_iter *iter)
{
	heap_free(iter->stream_heap);
	g_free(iter->stream_heap);
	free(iter);
}

int babeltrace_iter_next(struct babeltrace_iter *iter)
{
	struct ctf_file_stream *file_stream, *removed;
	int ret;

	file_stream = heap_maximum(iter->stream_heap);
	if (!file_stream) {
		/* end of file for all streams */
		ret = 0;
		goto end;
	}

	ret = stream_read_event(file_stream);
	if (ret == EOF) {
		removed = heap_remove(iter->stream_heap);
		assert(removed == file_stream);
		ret = 0;
		goto end;
	} else if (ret) {
		goto end;
	}
	/* Reinsert the file stream into the heap, and rebalance. */
	removed = heap_replace_max(iter->stream_heap, file_stream);
	assert(removed == file_stream);

end:
	return ret;
}

int babeltrace_iter_read_event(struct babeltrace_iter *iter,
		struct ctf_stream **stream,
		struct ctf_stream_event **event)
{
	struct ctf_file_stream *file_stream;
	int ret = 0;

	file_stream = heap_maximum(iter->stream_heap);
	if (!file_stream) {
		/* end of file for all streams */
		ret = EOF;
		goto end;
	}
	*stream = &file_stream->parent;
	*event = g_ptr_array_index((*stream)->events_by_id, (*stream)->event_id);
end:
	return ret;
}

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_collection *trace_collection_read)
{
	struct babeltrace_iter *iter;
	struct ctf_stream *stream;
	struct ctf_stream_event *event;
	struct ctf_text_stream_pos *sout;
	int ret = 0;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	iter = babeltrace_iter_create(trace_collection_read, NULL, NULL);
	while (babeltrace_iter_read_event(iter, &stream, &event) == 0) {
		ret = sout->parent.event_cb(&sout->parent, stream);
		if (ret) {
			fprintf(stdout, "[error] Writing event failed.\n");
			goto end;
		}
		ret = babeltrace_iter_next(iter);
		if (ret < 0)
			goto end;
	}
end:
	babeltrace_iter_destroy(iter);
	return ret;
}
