/*
 * ctf/iterator.c
 *
 * Babeltrace Library
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/prio_heap.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf/metadata.h>
#include <glib.h>

#include "events-private.h"

struct bt_ctf_iter *bt_ctf_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos)
{
	struct bt_ctf_iter *iter;
	int ret;

	if (!ctx)
		return NULL;

	iter = g_new0(struct bt_ctf_iter, 1);
	ret = bt_iter_init(&iter->parent, ctx, begin_pos, end_pos);
	if (ret) {
		g_free(iter);
		return NULL;
	}
	iter->callbacks = g_array_new(FALSE, TRUE,
			sizeof(struct bt_stream_callbacks));
	iter->recalculate_dep_graph = 0;
	iter->main_callbacks.callback = NULL;
	iter->dep_gc = g_ptr_array_new();
	return iter;
}

struct bt_ctf_iter *bt_ctf_iter_create_intersect(struct bt_context *ctx,
		struct bt_iter_pos **inter_begin_pos,
		struct bt_iter_pos **inter_end_pos)
{
	int ret;
	uint64_t begin, end;

	/*
	 * The iterator's range is the union of each trace's intersection of
	 * streams. This means that we determine the "active" region of each
	 * trace (that is the region where all of its streams are active), and
	 * use the TraceCollection to merge all of these active regions.
	 *
	 * This results in a union of the traces' active regions.
	 */
	ret = ctf_find_tc_stream_packet_intersection_union(ctx, &begin, &end);
	if (ret == 1) {
		fprintf(stderr, "[error] No intersection found between trace files.\n");
		goto error;
	} else if (ret != 0) {
		goto error;
	}
	*inter_begin_pos = bt_iter_create_time_pos(NULL, begin);
	if (!(*inter_begin_pos)) {
		goto error;
	}
	*inter_end_pos = bt_iter_create_time_pos(NULL, end);
	if (!(*inter_end_pos)) {
		goto error;
	}

	ret = ctf_tc_set_stream_intersection_mode(ctx);
	if (ret) {
		goto error;
	}

	/*
	 * bt_ctf_iter does not take ownership of begin and end positions,
	 * so we return them to the caller who must still assume their ownership
	 * until the iterator is destroyed.
	 */
	return bt_ctf_iter_create(ctx, *inter_begin_pos,
			*inter_end_pos);
error:
	return NULL;
}

void bt_ctf_iter_destroy(struct bt_ctf_iter *iter)
{
	struct bt_stream_callbacks *bt_stream_cb;
	struct bt_callback_chain *bt_chain;
	int i, j;

	assert(iter);

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
	g_array_free(iter->callbacks, TRUE);
	g_ptr_array_free(iter->dep_gc, TRUE);

	bt_iter_fini(&iter->parent);
	g_free(iter);
}

struct bt_iter *bt_ctf_get_iter(struct bt_ctf_iter *iter)
{
	if (!iter)
		return NULL;

	return &iter->parent;
}

struct bt_ctf_event *bt_ctf_iter_read_event_flags(struct bt_ctf_iter *iter,
		int *flags)
{
	struct ctf_file_stream *file_stream;
	struct bt_ctf_event *ret;
	struct ctf_stream_definition *stream;
	struct packet_index *packet_index;

	/*
	 * We do not want to fail for any other reason than end of
	 * trace, hence the assert.
	 */
	assert(iter);

	if (flags)
		*flags = 0;

	ret = &iter->current_ctf_event;
	file_stream = bt_heap_maximum(iter->parent.stream_heap);
	if (!file_stream) {
		/* end of file for all streams */
		goto stop;
	}

	/*
	 * If the packet is empty (contains only headers or is of size 0), the
	 * caller has to know that we can't read the current event and we need
	 * to do a bt_iter_next.
	 */
	if (file_stream->pos.data_offset == file_stream->pos.content_size
			|| file_stream->pos.content_size == 0) {
		/* More events may come. */
		ret = NULL;
		if (flags)
			*flags |= BT_ITER_FLAG_RETRY;
		goto end;
	}

	stream = &file_stream->parent;
	if (iter->parent.end_pos &&
		iter->parent.end_pos->type == BT_SEEK_TIME &&
		stream->real_timestamp > iter->parent.end_pos->u.seek_time) {
		goto stop;
	}
	ret->parent = g_ptr_array_index(stream->events_by_id,
			stream->event_id);

	if (!file_stream->pos.packet_index)
		packet_index = NULL;
	else
		packet_index = &g_array_index(file_stream->pos.packet_index,
				struct packet_index, file_stream->pos.cur_index);
	iter->events_lost = 0;
	if (packet_index && packet_index->events_discarded >
			file_stream->pos.last_events_discarded) {
		if (flags)
			*flags |= BT_ITER_FLAG_LOST_EVENTS;
		iter->events_lost += packet_index->events_discarded -
			file_stream->pos.last_events_discarded;
		file_stream->pos.last_events_discarded =
			packet_index->events_discarded;
	}

	if (ret->parent->stream->stream_id > iter->callbacks->len)
		goto end;

	process_callbacks(iter, ret->parent->stream);

end:
	return ret;
stop:
	return NULL;
}

struct bt_ctf_event *bt_ctf_iter_read_event(struct bt_ctf_iter *iter)
{
	return bt_ctf_iter_read_event_flags(iter, NULL);
}

uint64_t bt_ctf_get_lost_events_count(struct bt_ctf_iter *iter)
{
	if (!iter)
		return -1ULL;

	return iter->events_lost;
}
