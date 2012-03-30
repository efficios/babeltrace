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
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/iterator.h>
#include <babeltrace/prio_heap.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/ctf/events.h>
#include <inttypes.h>

static int babeltrace_filestream_seek(struct ctf_file_stream *file_stream,
		const struct bt_iter_pos *begin_pos,
		unsigned long stream_id);

struct stream_saved_pos {
	/*
	 * Use file_stream pointer to check if the trace collection we
	 * restore to match the one we saved from, for each stream.
	 */
	struct ctf_file_stream *file_stream;
	size_t cur_index;	/* current index in packet index */
	ssize_t offset;		/* offset from base, in bits. EOF for end of file. */
	uint64_t current_timestamp;
};

struct bt_saved_pos {
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
		fprintf(stderr, "[error] Reading event failed.\n");
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

void bt_iter_free_pos(struct bt_iter_pos *iter_pos)
{
	if (!iter_pos)
		return;

	if (iter_pos->u.restore) {
		if (iter_pos->u.restore->stream_saved_pos) {
			g_array_free(
				iter_pos->u.restore->stream_saved_pos,
				TRUE);
		}
		g_free(iter_pos->u.restore);
	}
	g_free(iter_pos);
}

/*
 * seek_file_stream_by_timestamp
 *
 * Browse a filestream by index, if an index contains the timestamp passed in
 * argument, seek inside the corresponding packet it until we find the event we
 * are looking for (either the exact timestamp or the event just after the
 * timestamp).
 *
 * Return 0 if the seek succeded and EOF if we didn't find any packet
 * containing the timestamp.
 */
static int seek_file_stream_by_timestamp(struct ctf_file_stream *cfs,
		uint64_t timestamp)
{
	struct ctf_stream_pos *stream_pos;
	struct packet_index *index;
	int i, ret;

	stream_pos = &cfs->pos;
	for (i = 0; i < stream_pos->packet_index->len; i++) {
		index = &g_array_index(stream_pos->packet_index,
				struct packet_index, i);
		if (index->timestamp_end <= timestamp)
			continue;

		stream_pos->packet_seek(&stream_pos->parent, i, SEEK_SET);
		while (cfs->parent.timestamp < timestamp) {
			ret = stream_read_event(cfs);
			if (ret < 0)
				break;
		}
		return 0;
	}
	return EOF;
}

/*
 * seek_ctf_trace_by_timestamp : for each file stream, seek to the event with
 * the corresponding timestamp
 *
 * Return 0 on success.
 * If the timestamp is not part of any file stream, return EOF to inform the
 * user the timestamp is out of the scope
 */
static int seek_ctf_trace_by_timestamp(struct ctf_trace *tin,
		uint64_t timestamp, struct ptr_heap *stream_heap)
{
	int i, j, ret;
	int found = EOF;

	/* for each stream_class */
	for (i = 0; i < tin->streams->len; i++) {
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(tin->streams, i);
		/* for each file_stream */
		for (j = 0; j < stream_class->streams->len; j++) {
			struct ctf_stream_definition *stream;
			struct ctf_file_stream *cfs;

			stream = g_ptr_array_index(stream_class->streams, j);
			cfs = container_of(stream, struct ctf_file_stream,
					parent);
			ret = seek_file_stream_by_timestamp(cfs, timestamp);
			if (ret == 0) {
				/* Add to heap */
				ret = heap_insert(stream_heap, cfs);
				if (ret)
					goto error;
				found = 0;
			}
		}
	}

	return found;

error:
	return -2;
}

int bt_iter_set_pos(struct bt_iter *iter, const struct bt_iter_pos *iter_pos)
{
	struct trace_collection *tc;
	int i, ret;

	switch (iter_pos->type) {
	case BT_SEEK_RESTORE:
		if (!iter_pos->u.restore)
			return -EINVAL;

		heap_free(iter->stream_heap);
		ret = heap_init(iter->stream_heap, 0, stream_compare);
		if (ret < 0)
			goto error;

		for (i = 0; i < iter_pos->u.restore->stream_saved_pos->len;
				i++) {
			struct stream_saved_pos *saved_pos;
			struct ctf_stream_pos *stream_pos;
			struct ctf_stream_definition *stream;

			saved_pos = &g_array_index(
					iter_pos->u.restore->stream_saved_pos,
					struct stream_saved_pos, i);
			stream = &saved_pos->file_stream->parent;
			stream_pos = &saved_pos->file_stream->pos;

			stream_pos->packet_seek(&stream_pos->parent,
					saved_pos->cur_index, SEEK_SET);

			/*
			 * the timestamp needs to be restored after
			 * packet_seek, because this function resets
			 * the timestamp to the beginning of the packet
			 */
			stream->timestamp = saved_pos->current_timestamp;
			stream_pos->offset = saved_pos->offset;
			stream_pos->last_offset = saved_pos->offset;

			stream->prev_timestamp = 0;
			stream->prev_timestamp_end = 0;
			stream->consumed = 0;

			printf_debug("restored to cur_index = %zd and "
				"offset = %zd, timestamp = %" PRIu64 "\n",
				stream_pos->cur_index,
				stream_pos->offset, stream->timestamp);

			stream_read_event(saved_pos->file_stream);

			/* Add to heap */
			ret = heap_insert(iter->stream_heap,
					saved_pos->file_stream);
			if (ret)
				goto error;
		}
	case BT_SEEK_TIME:
		tc = iter->ctx->tc;

		heap_free(iter->stream_heap);
		ret = heap_init(iter->stream_heap, 0, stream_compare);
		if (ret < 0)
			goto error;

		/* for each trace in the trace_collection */
		for (i = 0; i < tc->array->len; i++) {
			struct ctf_trace *tin;
			struct trace_descriptor *td_read;

			td_read = g_ptr_array_index(tc->array, i);
			tin = container_of(td_read, struct ctf_trace, parent);

			ret = seek_ctf_trace_by_timestamp(tin,
					iter_pos->u.seek_time,
					iter->stream_heap);
			if (ret < 0)
				goto error;
		}
		return 0;
	case BT_SEEK_BEGIN:
		tc = iter->ctx->tc;
		for (i = 0; i < tc->array->len; i++) {
			struct ctf_trace *tin;
			struct trace_descriptor *td_read;
			int stream_id;

			td_read = g_ptr_array_index(tc->array, i);
			tin = container_of(td_read, struct ctf_trace, parent);

			/* Populate heap with each stream */
			for (stream_id = 0; stream_id < tin->streams->len;
					stream_id++) {
				struct ctf_stream_declaration *stream;
				int filenr;

				stream = g_ptr_array_index(tin->streams,
						stream_id);
				if (!stream)
					continue;
				for (filenr = 0; filenr < stream->streams->len;
						filenr++) {
					struct ctf_file_stream *file_stream;
					file_stream = g_ptr_array_index(
							stream->streams,
							filenr);
					ret = babeltrace_filestream_seek(
							file_stream, iter_pos,
							stream_id);
				}
			}
		}
		break;
	default:
		/* not implemented */
		return -EINVAL;
	}

	return 0;

error:
	heap_free(iter->stream_heap);
	if (heap_init(iter->stream_heap, 0, stream_compare) < 0) {
		heap_free(iter->stream_heap);
		g_free(iter->stream_heap);
		iter->stream_heap = NULL;
		ret = -ENOMEM;
	}
	return ret;
}

struct bt_iter_pos *bt_iter_get_pos(struct bt_iter *iter)
{
	struct bt_iter_pos *pos;
	struct trace_collection *tc = iter->ctx->tc;
	int i, stream_class_id, stream_id;

	pos = g_new0(struct bt_iter_pos, 1);
	pos->u.restore = g_new0(struct bt_saved_pos, 1);
	pos->u.restore->tc = tc;
	pos->u.restore->stream_saved_pos = g_array_new(FALSE, TRUE,
			sizeof(struct stream_saved_pos));
	if (!pos->u.restore->stream_saved_pos)
		goto error;

	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		tin = container_of(td_read, struct ctf_trace, parent);

		for (stream_class_id = 0; stream_class_id < tin->streams->len;
				stream_class_id++) {
			struct ctf_stream_declaration *stream_class;

			stream_class = g_ptr_array_index(tin->streams,
					stream_class_id);
			for (stream_id = 0;
					stream_id < stream_class->streams->len;
					stream_id++) {
				struct ctf_stream_definition *stream;
				struct ctf_file_stream *cfs;
				struct stream_saved_pos saved_pos;

				stream = g_ptr_array_index(
						stream_class->streams,
						stream_id);
				cfs = container_of(stream,
						struct ctf_file_stream,
						parent);

				saved_pos.file_stream = cfs;
				saved_pos.cur_index = cfs->pos.cur_index;

				/*
				 * It is possible that an event was read during
				 * the last restore, never consumed and its
				 * position saved again.  For this case, we
				 * need to check if the event really was
				 * consumed by the caller otherwise it is lost.
				 */
				if (stream->consumed)
					saved_pos.offset = cfs->pos.offset;
				else
					saved_pos.offset = cfs->pos.last_offset;

				saved_pos.current_timestamp = stream->timestamp;

				g_array_append_val(
					pos->u.restore->stream_saved_pos,
					saved_pos);

				printf_debug("stream : %" PRIu64 ", cur_index : %zd, "
					"offset : %zd, "
					"timestamp = %" PRIu64 "\n",
					stream->stream_id, saved_pos.cur_index,
					saved_pos.offset,
					saved_pos.current_timestamp);
			}
		}
	}

	return pos;

error:
	return NULL;
}

struct bt_iter_pos *bt_iter_create_time_pos(struct bt_iter *iter,
		uint64_t timestamp)
{
	struct bt_iter_pos *pos;

	pos = g_new0(struct bt_iter_pos, 1);
	pos->type = BT_SEEK_TIME;
	pos->u.seek_time = timestamp;
	return pos;
}

/*
 * babeltrace_filestream_seek: seek a filestream to given position.
 *
 * The stream_id parameter is only useful for BT_SEEK_RESTORE.
 */
static int babeltrace_filestream_seek(struct ctf_file_stream *file_stream,
		const struct bt_iter_pos *begin_pos,
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
		file_stream->pos.packet_seek(&file_stream->pos.parent,
				0, SEEK_SET);
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
 * bt_iter_seek: seek iterator to given position.
 */
int bt_iter_seek(struct bt_iter *iter,
		const struct bt_iter_pos *begin_pos)
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
			struct ctf_stream_declaration *stream;
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

int bt_iter_init(struct bt_iter *iter,
		struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos)
{
	int i, stream_id;
	int ret = 0;

	if (ctx->current_iterator) {
		ret = -1;
		goto error_ctx;
	}

	iter->stream_heap = g_new(struct ptr_heap, 1);
	iter->end_pos = end_pos;
	bt_context_get(ctx);
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
			struct ctf_stream_declaration *stream;
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
					ret = babeltrace_filestream_seek(
							file_stream,
							begin_pos,
							stream_id);
				} else {
					struct bt_iter_pos pos;
					pos.type = BT_SEEK_BEGIN;
					ret = babeltrace_filestream_seek(
							file_stream, &pos,
							stream_id);
				}
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

	ctx->current_iterator = iter;
	return 0;

error:
	heap_free(iter->stream_heap);
error_heap_init:
	g_free(iter->stream_heap);
error_ctx:
	return ret;
}

struct bt_iter *bt_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos)
{
	struct bt_iter *iter;
	int ret;

	iter = g_new0(struct bt_iter, 1);
	ret = bt_iter_init(iter, ctx, begin_pos, end_pos);
	if (ret) {
		g_free(iter);
		return NULL;
	}
	return iter;
}

void bt_iter_fini(struct bt_iter *iter)
{
	if (iter->stream_heap) {
		heap_free(iter->stream_heap);
		g_free(iter->stream_heap);
	}
	iter->ctx->current_iterator = NULL;
	bt_context_put(iter->ctx);
}

void bt_iter_destroy(struct bt_iter *iter)
{
	bt_iter_fini(iter);
	g_free(iter);
}

int bt_iter_next(struct bt_iter *iter)
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
