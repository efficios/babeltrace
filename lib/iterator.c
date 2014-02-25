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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
	uint64_t current_real_timestamp;
	uint64_t current_cycles_timestamp;
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
	else if (ret == EAGAIN)
		/* Stream is inactive for now (live reading). */
		return EAGAIN;
	else if (ret) {
		fprintf(stderr, "[error] Reading event failed.\n");
		return ret;
	}
	return 0;
}

/*
 * Return true if a < b, false otherwise.
 * If time stamps are exactly the same, compare by stream path. This
 * ensures we get the same result between runs on the same trace
 * collection on different environments.
 * The result will be random for memory-mapped traces since there is no
 * fixed path leading to those (they have empty path string).
 */
static int stream_compare(void *a, void *b)
{
	struct ctf_file_stream *s_a = a, *s_b = b;

	if (s_a->parent.real_timestamp < s_b->parent.real_timestamp) {
		return 1;
	} else if (likely(s_a->parent.real_timestamp > s_b->parent.real_timestamp)) {
		return 0;
	} else {
		return strcmp(s_a->parent.path, s_b->parent.path);
	}
}

void bt_iter_free_pos(struct bt_iter_pos *iter_pos)
{
	if (!iter_pos)
		return;

	if (iter_pos->type == BT_SEEK_RESTORE && iter_pos->u.restore) {
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
 * Return 0 if the seek succeded, EOF if we didn't find any packet
 * containing the timestamp, or a positive integer for error.
 *
 * TODO: this should be turned into a binary search! It is currently
 * doing a linear search in the packets. This is a O(n) operation on a
 * very frequent code path.
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
		if (index->ts_real.timestamp_end < timestamp)
			continue;

		stream_pos->packet_seek(&stream_pos->parent, i, SEEK_SET);
		do {
			ret = stream_read_event(cfs);
		} while (cfs->parent.real_timestamp < timestamp && ret == 0);

		/* Can return either EOF, 0, or error (> 0). */
		return ret;
	}
	/*
	 * Cannot find the timestamp within the stream packets, return
	 * EOF.
	 */
	return EOF;
}

/*
 * seek_ctf_trace_by_timestamp : for each file stream, seek to the event with
 * the corresponding timestamp
 *
 * Return 0 on success.
 * If the timestamp is not part of any file stream, return EOF to inform the
 * user the timestamp is out of the scope.
 * On other errors, return positive value.
 */
static int seek_ctf_trace_by_timestamp(struct ctf_trace *tin,
		uint64_t timestamp, struct ptr_heap *stream_heap)
{
	int i, j, ret;
	int found = 0;

	/* for each stream_class */
	for (i = 0; i < tin->streams->len; i++) {
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(tin->streams, i);
		if (!stream_class)
			continue;
		/* for each file_stream */
		for (j = 0; j < stream_class->streams->len; j++) {
			struct ctf_stream_definition *stream;
			struct ctf_file_stream *cfs;

			stream = g_ptr_array_index(stream_class->streams, j);
			if (!stream)
				continue;
			cfs = container_of(stream, struct ctf_file_stream,
					parent);
			ret = seek_file_stream_by_timestamp(cfs, timestamp);
			if (ret == 0) {
				/* Add to heap */
				ret = bt_heap_insert(stream_heap, cfs);
				if (ret) {
					/* Return positive error. */
					return -ret;
				}
				found = 1;
			} else if (ret > 0) {
				/*
				 * Error in seek (not EOF), failure.
				 */
				return ret;
			}
			/* on EOF just do not put stream into heap. */
		}
	}

	return found ? 0 : EOF;
}

/*
 * Find timestamp of last event in the stream.
 *
 * Return value: 0 if OK, positive error value on error, EOF if no
 * events were found.
 */
static int find_max_timestamp_ctf_file_stream(struct ctf_file_stream *cfs,
		uint64_t *timestamp_end)
{
	int ret, count = 0, i;
	uint64_t timestamp = 0;
	struct ctf_stream_pos *stream_pos;

	stream_pos = &cfs->pos;
	/*
	 * We start by the last packet, and iterate backwards until we
	 * either find at least one event, or we reach the first packet
	 * (some packets can be empty).
	 */
	for (i = stream_pos->packet_index->len - 1; i >= 0; i--) {
		stream_pos->packet_seek(&stream_pos->parent, i, SEEK_SET);
		count = 0;
		/* read each event until we reach the end of the stream */
		do {
			ret = stream_read_event(cfs);
			if (ret == 0) {
				count++;
				timestamp = cfs->parent.real_timestamp;
			}
		} while (ret == 0);

		/* Error */
		if (ret > 0)
			goto end;
		assert(ret == EOF);
		if (count)
			break;
	}

	if (count) {
		*timestamp_end = timestamp;
		ret = 0;
	} else {
		/* Return EOF if no events were found */
		ret = EOF;
	}
end:
	return ret;
}

/*
 * Find the stream within a stream class that contains the event with
 * the largest timestamp, and save that timestamp.
 *
 * Return 0 if OK, EOF if no events were found in the streams, or
 * positive value on error.
 */
static int find_max_timestamp_ctf_stream_class(
		struct ctf_stream_declaration *stream_class,
		struct ctf_file_stream **cfsp,
		uint64_t *max_timestamp)
{
	int ret = EOF, i, found = 0;

	for (i = 0; i < stream_class->streams->len; i++) {
		struct ctf_stream_definition *stream;
		struct ctf_file_stream *cfs;
		uint64_t current_max_ts = 0;

		stream = g_ptr_array_index(stream_class->streams, i);
		if (!stream)
			continue;
		cfs = container_of(stream, struct ctf_file_stream, parent);
		ret = find_max_timestamp_ctf_file_stream(cfs, &current_max_ts);
		if (ret == EOF)
			continue;
		if (ret != 0)
			break;
		if (current_max_ts >= *max_timestamp) {
			*max_timestamp = current_max_ts;
			*cfsp = cfs;
			found = 1;
		}
	}
	assert(ret >= 0 || ret == EOF);
	if (found) {
		return 0;
	}
	return ret;
}

/*
 * seek_last_ctf_trace_collection: seek trace collection to last event.
 *
 * Return 0 if OK, EOF if no events were found, or positive error value
 * on error.
 */
static int seek_last_ctf_trace_collection(struct trace_collection *tc,
		struct ctf_file_stream **cfsp)
{
	int i, j, ret;
	int found = 0;
	uint64_t max_timestamp = 0;

	if (!tc)
		return 1;

	/* For each trace in the trace_collection */
	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct bt_trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		if (!td_read)
			continue;
		tin = container_of(td_read, struct ctf_trace, parent);
		/* For each stream_class in the trace */
		for (j = 0; j < tin->streams->len; j++) {
			struct ctf_stream_declaration *stream_class;

			stream_class = g_ptr_array_index(tin->streams, j);
			if (!stream_class)
				continue;
			ret = find_max_timestamp_ctf_stream_class(stream_class,
					cfsp, &max_timestamp);
			if (ret > 0)
				goto end;
			if (ret == 0)
				found = 1;
			assert(ret == EOF || ret == 0);
		}
	}
	/*
	 * Now we know in which file stream the last event is located,
	 * and we know its timestamp.
	 */
	if (!found) {
		ret = EOF;
	} else {
		ret = seek_file_stream_by_timestamp(*cfsp, max_timestamp);
		assert(ret == 0);
	}
end:
	return ret;
}

int bt_iter_set_pos(struct bt_iter *iter, const struct bt_iter_pos *iter_pos)
{
	struct trace_collection *tc;
	int i, ret;

	if (!iter || !iter_pos)
		return -EINVAL;

	switch (iter_pos->type) {
	case BT_SEEK_RESTORE:
		if (!iter_pos->u.restore)
			return -EINVAL;

		bt_heap_free(iter->stream_heap);
		ret = bt_heap_init(iter->stream_heap, 0, stream_compare);
		if (ret < 0)
			goto error_heap_init;

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
			stream->real_timestamp = saved_pos->current_real_timestamp;
			stream->cycles_timestamp = saved_pos->current_cycles_timestamp;
			stream_pos->offset = saved_pos->offset;
			stream_pos->last_offset = LAST_OFFSET_POISON;

			stream->current.real.begin = 0;
			stream->current.real.end = 0;
			stream->current.cycles.begin = 0;
			stream->current.cycles.end = 0;

			stream->prev.real.begin = 0;
			stream->prev.real.end = 0;
			stream->prev.cycles.begin = 0;
			stream->prev.cycles.end = 0;

			printf_debug("restored to cur_index = %" PRId64 " and "
				"offset = %" PRId64 ", timestamp = %" PRIu64 "\n",
				stream_pos->cur_index,
				stream_pos->offset, stream->real_timestamp);

			ret = stream_read_event(saved_pos->file_stream);
			if (ret != 0) {
				goto error;
			}

			/* Add to heap */
			ret = bt_heap_insert(iter->stream_heap,
					saved_pos->file_stream);
			if (ret)
				goto error;
		}
		return 0;
	case BT_SEEK_TIME:
		tc = iter->ctx->tc;

		bt_heap_free(iter->stream_heap);
		ret = bt_heap_init(iter->stream_heap, 0, stream_compare);
		if (ret < 0)
			goto error_heap_init;

		/* for each trace in the trace_collection */
		for (i = 0; i < tc->array->len; i++) {
			struct ctf_trace *tin;
			struct bt_trace_descriptor *td_read;

			td_read = g_ptr_array_index(tc->array, i);
			if (!td_read)
				continue;
			tin = container_of(td_read, struct ctf_trace, parent);

			ret = seek_ctf_trace_by_timestamp(tin,
					iter_pos->u.seek_time,
					iter->stream_heap);
			/*
			 * Positive errors are failure. Negative value
			 * is EOF (for which we continue with other
			 * traces). 0 is success. Note: on EOF, it just
			 * means that no stream has been added to the
			 * iterator for that trace, which is fine.
			 */
			if (ret != 0 && ret != EOF)
				goto error;
		}
		return 0;
	case BT_SEEK_BEGIN:
		tc = iter->ctx->tc;
		bt_heap_free(iter->stream_heap);
		ret = bt_heap_init(iter->stream_heap, 0, stream_compare);
		if (ret < 0)
			goto error_heap_init;

		for (i = 0; i < tc->array->len; i++) {
			struct ctf_trace *tin;
			struct bt_trace_descriptor *td_read;
			int stream_id;

			td_read = g_ptr_array_index(tc->array, i);
			if (!td_read)
				continue;
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
					if (!file_stream)
						continue;
					ret = babeltrace_filestream_seek(
							file_stream, iter_pos,
							stream_id);
					if (ret != 0 && ret != EOF) {
						goto error;
					}
					if (ret == EOF) {
						/* Do not add EOF streams */
						continue;
					}
					ret = bt_heap_insert(iter->stream_heap, file_stream);
					if (ret)
						goto error;
				}
			}
		}
		break;
	case BT_SEEK_LAST:
	{
		struct ctf_file_stream *cfs = NULL;

		tc = iter->ctx->tc;
		ret = seek_last_ctf_trace_collection(tc, &cfs);
		if (ret != 0 || !cfs)
			goto error;
		/* remove all streams from the heap */
		bt_heap_free(iter->stream_heap);
		/* Create a new empty heap */
		ret = bt_heap_init(iter->stream_heap, 0, stream_compare);
		if (ret < 0)
			goto error;
		/* Insert the stream that contains the last event */
		ret = bt_heap_insert(iter->stream_heap, cfs);
		if (ret)
			goto error;
		break;
	}
	default:
		/* not implemented */
		return -EINVAL;
	}

	return 0;

error:
	bt_heap_free(iter->stream_heap);
error_heap_init:
	if (bt_heap_init(iter->stream_heap, 0, stream_compare) < 0) {
		bt_heap_free(iter->stream_heap);
		g_free(iter->stream_heap);
		iter->stream_heap = NULL;
		ret = -ENOMEM;
	}

	return ret;
}

struct bt_iter_pos *bt_iter_get_pos(struct bt_iter *iter)
{
	struct bt_iter_pos *pos;
	struct trace_collection *tc;
	struct ctf_file_stream *file_stream = NULL, *removed;
	struct ptr_heap iter_heap_copy;
	int ret;

	if (!iter)
		return NULL;

	tc = iter->ctx->tc;
	pos = g_new0(struct bt_iter_pos, 1);
	pos->type = BT_SEEK_RESTORE;
	pos->u.restore = g_new0(struct bt_saved_pos, 1);
	pos->u.restore->tc = tc;
	pos->u.restore->stream_saved_pos = g_array_new(FALSE, TRUE,
			sizeof(struct stream_saved_pos));
	if (!pos->u.restore->stream_saved_pos)
		goto error;

	ret = bt_heap_copy(&iter_heap_copy, iter->stream_heap);
	if (ret < 0)
		goto error_heap;

	/* iterate over each stream in the heap */
	file_stream = bt_heap_maximum(&iter_heap_copy);
	while (file_stream != NULL) {
		struct stream_saved_pos saved_pos;

		assert(file_stream->pos.last_offset != LAST_OFFSET_POISON);
		saved_pos.offset = file_stream->pos.last_offset;
		saved_pos.file_stream = file_stream;
		saved_pos.cur_index = file_stream->pos.cur_index;

		saved_pos.current_real_timestamp = file_stream->parent.real_timestamp;
		saved_pos.current_cycles_timestamp = file_stream->parent.cycles_timestamp;

		g_array_append_val(
				pos->u.restore->stream_saved_pos,
				saved_pos);

		printf_debug("stream : %" PRIu64 ", cur_index : %zd, "
				"offset : %zd, "
				"timestamp = %" PRIu64 "\n",
				file_stream->parent.stream_id,
				saved_pos.cur_index, saved_pos.offset,
				saved_pos.current_real_timestamp);

		/* remove the stream from the heap copy */
		removed = bt_heap_remove(&iter_heap_copy);
		assert(removed == file_stream);

		file_stream = bt_heap_maximum(&iter_heap_copy);
	}
	bt_heap_free(&iter_heap_copy);
	return pos;

error_heap:
	g_array_free(pos->u.restore->stream_saved_pos, TRUE);
error:
	g_free(pos);
	return NULL;
}

struct bt_iter_pos *bt_iter_create_time_pos(struct bt_iter *iter,
		uint64_t timestamp)
{
	struct bt_iter_pos *pos;

	if (!iter)
		return NULL;

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

	if (!file_stream || !begin_pos)
		return -EINVAL;

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
	default:
		assert(0); /* Not yet defined */
	}

	return ret;
}

int bt_iter_add_trace(struct bt_iter *iter,
		struct bt_trace_descriptor *td_read)
{
	struct ctf_trace *tin;
	int stream_id, ret = 0;

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
			struct bt_iter_pos pos;

			file_stream = g_ptr_array_index(stream->streams,
					filenr);
			if (!file_stream)
				continue;

			pos.type = BT_SEEK_BEGIN;
			ret = babeltrace_filestream_seek(file_stream,
					&pos, stream_id);

			if (ret == EOF) {
				ret = 0;
				continue;
			} else if (ret != 0 && ret != EAGAIN) {
				goto error;
			}
			/* Add to heap */
			ret = bt_heap_insert(iter->stream_heap, file_stream);
			if (ret)
				goto error;
		}
	}

error:
	return ret;
}

int bt_iter_init(struct bt_iter *iter,
		struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos)
{
	int i;
	int ret = 0;

	if (!iter || !ctx)
		return -EINVAL;

	if (ctx->current_iterator) {
		ret = -1;
		goto error_ctx;
	}

	iter->stream_heap = g_new(struct ptr_heap, 1);
	iter->end_pos = end_pos;
	bt_context_get(ctx);
	iter->ctx = ctx;

	ret = bt_heap_init(iter->stream_heap, 0, stream_compare);
	if (ret < 0)
		goto error_heap_init;

	for (i = 0; i < ctx->tc->array->len; i++) {
		struct bt_trace_descriptor *td_read;

		td_read = g_ptr_array_index(ctx->tc->array, i);
		if (!td_read)
			continue;
		ret = bt_iter_add_trace(iter, td_read);
		if (ret < 0)
			goto error;
	}

	ctx->current_iterator = iter;
	if (begin_pos && begin_pos->type != BT_SEEK_BEGIN) {
		ret = bt_iter_set_pos(iter, begin_pos);
	}

	return ret;

error:
	bt_heap_free(iter->stream_heap);
error_heap_init:
	g_free(iter->stream_heap);
	iter->stream_heap = NULL;
error_ctx:
	return ret;
}

struct bt_iter *bt_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos)
{
	struct bt_iter *iter;
	int ret;

	if (!ctx)
		return NULL;

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
	assert(iter);
	if (iter->stream_heap) {
		bt_heap_free(iter->stream_heap);
		g_free(iter->stream_heap);
	}
	iter->ctx->current_iterator = NULL;
	bt_context_put(iter->ctx);
}

void bt_iter_destroy(struct bt_iter *iter)
{
	assert(iter);
	bt_iter_fini(iter);
	g_free(iter);
}

int bt_iter_next(struct bt_iter *iter)
{
	struct ctf_file_stream *file_stream, *removed;
	int ret;

	if (!iter)
		return -EINVAL;

	file_stream = bt_heap_maximum(iter->stream_heap);
	if (!file_stream) {
		/* end of file for all streams */
		ret = 0;
		goto end;
	}

	ret = stream_read_event(file_stream);
	if (ret == EOF) {
		removed = bt_heap_remove(iter->stream_heap);
		assert(removed == file_stream);
		ret = 0;
		goto end;
	} else if (ret == EAGAIN) {
		/*
		 * Live streaming: the stream is inactive for now, we
		 * just updated the timestamp_end to skip over this
		 * stream up to a certain point in time.
		 *
		 * Since we can't guarantee that a stream will ever have
		 * any activity, we can't rely on the fact that
		 * bt_iter_next will be called for each stream and deal
		 * with inactive streams. So instead, we return 0 here
		 * to the caller and let the read API handle the
		 * retry case.
		 */
		ret = 0;
		goto reinsert;
	} else if (ret) {
		goto end;
	}

reinsert:
	/* Reinsert the file stream into the heap, and rebalance. */
	removed = bt_heap_replace_max(iter->stream_heap, file_stream);
	assert(removed == file_stream);
end:
	return ret;
}
