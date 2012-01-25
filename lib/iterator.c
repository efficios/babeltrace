/*
 * iterator.c
 *
 * Babeltrace Library
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
#include <babeltrace/babeltrace.h>
#include <babeltrace/callbacks-internal.h>
#include <babeltrace/context.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/iterator.h>
#include <babeltrace/prio_heap.h>

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

/*
 * babeltrace_filestream_seek: seek a filestream to given position.
 *
 * The stream_id parameter is only useful for BT_SEEK_RESTORE.
 */
static int babeltrace_filestream_seek(struct ctf_file_stream *file_stream,
		const struct trace_collection_pos *begin_pos,
		unsigned long stream_id)
{
	int ret = 0;

	switch (begin_pos->type) {
	case BT_SEEK_CUR:
		/*
		 * just insert into the heap we should already know
		 * the timestamps
		 */
		break;
	case BT_SEEK_BEGIN:
		file_stream->pos.move_pos_slow(&file_stream->pos, 0, SEEK_SET);
		ret = stream_read_event(file_stream);
		break;
	case BT_SEEK_TIME:
	case BT_SEEK_RESTORE:
	case BT_SEEK_END:
	default:
		assert(0); /* Not yet defined */
	}

	return ret;
}

/*
 * babeltrace_iter_seek: seek iterator to given position.
 */
int babeltrace_iter_seek(struct babeltrace_iter *iter,
		const struct trace_collection_pos *begin_pos)
{
	int i, stream_id;
	int ret = 0;
	struct trace_collection *tc = iter->ctx->tc;

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
			for (filenr = 0; filenr < stream->streams->len;
					filenr++) {
				struct ctf_file_stream *file_stream;

				file_stream = g_ptr_array_index(stream->streams,
						filenr);
				ret = babeltrace_filestream_seek(file_stream, begin_pos,
						stream_id);
				if (ret < 0)
					goto end;
			}
		}
	}
end:
	return ret;
}

struct babeltrace_iter *babeltrace_iter_create(struct bt_context *ctx,
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
	iter->end_pos = end_pos;
	iter->callbacks = g_array_new(0, 1, sizeof(struct bt_stream_callbacks));
	iter->recalculate_dep_graph = 0;
	iter->main_callbacks.callback = NULL;
	iter->dep_gc = g_ptr_array_new();
	if (bt_context_get(ctx) != 0)
		goto error_ctx;
	iter->ctx = ctx;

	ret = heap_init(iter->stream_heap, 0, stream_compare);
	if (ret < 0)
		goto error_heap_init;

	for (i = 0; i < ctx->tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(ctx->tc->array, i);
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

				if (begin_pos) {
				    ret = babeltrace_filestream_seek(file_stream, begin_pos,
							stream_id);
				    if (ret == EOF) {
					   ret = 0;
					   continue;
				    } else if (ret) {
					   goto error;
				    }
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
error_ctx:
	free(iter);
error_malloc:
	return NULL;
}

void babeltrace_iter_destroy(struct babeltrace_iter *iter)
{
	struct bt_stream_callbacks *bt_stream_cb;
	struct bt_callback_chain *bt_chain;
	int i, j;

	heap_free(iter->stream_heap);
	g_free(iter->stream_heap);

	/* free all events callbacks */
	if (iter->main_callbacks.callback)
		g_array_free(iter->main_callbacks.callback, TRUE);

	/* free per-event callbacks */
	for (i = 0; i < iter->callbacks->len; i++) {
		bt_stream_cb = &g_array_index(iter->callbacks,
				struct bt_stream_callbacks, i);
		if (!bt_stream_cb || !bt_stream_cb->per_id_callbacks)
			continue;
		for (j = 0; j < bt_stream_cb->per_id_callbacks->len; j++) {
			bt_chain = &g_array_index(bt_stream_cb->per_id_callbacks,
					struct bt_callback_chain, j);
			if (bt_chain->callback) {
				g_array_free(bt_chain->callback, TRUE);
			}
		}
		g_array_free(bt_stream_cb->per_id_callbacks, TRUE);
	}

	bt_context_put(iter->ctx);

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

	if ((*stream)->stream_id > iter->callbacks->len)
		goto end;

	process_callbacks(iter, *stream);

end:
	return ret;
}
