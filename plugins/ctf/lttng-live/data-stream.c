/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
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

#define BT_LOG_TAG "PLUGIN-CTF-LTTNG-LIVE-SRC-DS"
#include "logging.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include <inttypes.h>
#include <babeltrace/compat/mman-internal.h>
#include <babeltrace/babeltrace.h>
#include "../common/notif-iter/notif-iter.h"
#include <babeltrace/assert-internal.h>

#include "data-stream.h"

static
enum bt_notif_iter_medium_status medop_request_bytes(
		size_t request_sz, uint8_t **buffer_addr,
		size_t *buffer_sz, void *data)
{
	enum bt_notif_iter_medium_status status =
		BT_NOTIF_ITER_MEDIUM_STATUS_OK;
	struct lttng_live_stream_iterator *stream = data;
	struct lttng_live_trace *trace = stream->trace;
	struct lttng_live_session *session = trace->session;
	struct lttng_live_component *lttng_live = session->lttng_live;
	uint64_t recv_len = 0;
	uint64_t len_left;
	uint64_t read_len;
	//int i;

	len_left = stream->base_offset + stream->len - stream->offset;
	if (!len_left) {
		stream->state = LTTNG_LIVE_STREAM_ACTIVE_NO_DATA;
		status = BT_NOTIF_ITER_MEDIUM_STATUS_AGAIN;
		return status;
	}
	read_len = MIN(request_sz, stream->buflen);
	read_len = MIN(read_len, len_left);
	status = lttng_live_get_stream_bytes(lttng_live,
			stream, stream->buf, stream->offset,
			read_len, &recv_len);
	*buffer_addr = stream->buf;
	*buffer_sz = recv_len;
	stream->offset += recv_len;
	return status;
}

static
struct bt_stream *medop_get_stream(
		struct bt_stream_class *stream_class,
		uint64_t stream_id, void *data)
{
	struct lttng_live_stream_iterator *lttng_live_stream = data;

	if (!lttng_live_stream->stream) {
		int64_t stream_class_id =
			bt_stream_class_get_id(stream_class);

		BT_LOGD("Creating stream %s (ID: %" PRIu64 ") out of stream class %" PRId64,
			lttng_live_stream->name, stream_id, stream_class_id);

		if (stream_id == -1ULL) {
			/* No stream ID */
			lttng_live_stream->stream = bt_stream_create(
				stream_class, lttng_live_stream->name);
		} else {
			lttng_live_stream->stream =
				bt_stream_create_with_id(stream_class,
					lttng_live_stream->name, stream_id);
		}

		if (!lttng_live_stream->stream) {
			BT_LOGE("Cannot create stream %s (stream class %" PRId64 ", stream ID %" PRIu64 ")",
					lttng_live_stream->name,
					stream_class_id, stream_id);
		}
	}

	return lttng_live_stream->stream;
}

static struct bt_notif_iter_medium_ops medops = {
	.request_bytes = medop_request_bytes,
	.get_stream = medop_get_stream,
};

BT_HIDDEN
enum bt_lttng_live_iterator_status lttng_live_lazy_notif_init(
		struct lttng_live_session *session)
{
	struct lttng_live_component *lttng_live = session->lttng_live;
	struct lttng_live_trace *trace;

	if (!session->lazy_stream_notif_init) {
		return BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	}

	bt_list_for_each_entry(trace, &session->traces, node) {
		struct lttng_live_stream_iterator *stream;

		bt_list_for_each_entry(stream, &trace->streams, node) {
			if (stream->notif_iter) {
				continue;
			}
			stream->notif_iter = bt_notif_iter_create(trace->trace,
					lttng_live->max_query_size, medops,
					stream);
			if (!stream->notif_iter) {
				goto error;
			}
		}
	}

	session->lazy_stream_notif_init = false;

	return BT_LTTNG_LIVE_ITERATOR_STATUS_OK;

error:
	return BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
}

BT_HIDDEN
struct lttng_live_stream_iterator *lttng_live_stream_iterator_create(
		struct lttng_live_session *session,
		uint64_t ctf_trace_id,
		uint64_t stream_id)
{
	struct lttng_live_component *lttng_live = session->lttng_live;
	struct lttng_live_stream_iterator *stream =
			g_new0(struct lttng_live_stream_iterator, 1);
	struct lttng_live_trace *trace;
	int ret;

	trace = lttng_live_ref_trace(session, ctf_trace_id);
	if (!trace) {
		goto error;
	}

	stream->p.type = LIVE_STREAM_TYPE_STREAM;
	stream->trace = trace;
	stream->state = LTTNG_LIVE_STREAM_ACTIVE_NO_DATA;
	stream->viewer_stream_id = stream_id;
	stream->ctf_stream_class_id = -1ULL;
	stream->last_returned_inactivity_timestamp = INT64_MIN;

	if (trace->trace) {
		stream->notif_iter = bt_notif_iter_create(trace->trace,
				lttng_live->max_query_size, medops,
				stream);
		if (!stream->notif_iter) {
			goto error;
		}
	}
	stream->buf = g_new0(uint8_t, session->lttng_live->max_query_size);
	stream->buflen = session->lttng_live->max_query_size;

	ret = lttng_live_add_port(lttng_live, stream);
	BT_ASSERT(!ret);

	bt_list_add(&stream->node, &trace->streams);

	goto end;
error:
	/* Do not touch "borrowed" file. */
	lttng_live_stream_iterator_destroy(stream);
	stream = NULL;
end:
	return stream;
}

BT_HIDDEN
void lttng_live_stream_iterator_destroy(struct lttng_live_stream_iterator *stream)
{
	struct lttng_live_component *lttng_live;
	int ret;

	if (!stream) {
		return;
	}

	lttng_live = stream->trace->session->lttng_live;
	ret = lttng_live_remove_port(lttng_live, stream->port);
	BT_ASSERT(!ret);

	if (stream->stream) {
		BT_PUT(stream->stream);
	}

	if (stream->notif_iter) {
		bt_notif_iter_destroy(stream->notif_iter);
	}
	g_free(stream->buf);
	BT_PUT(stream->packet_end_notif_queue);
	bt_list_del(&stream->node);
	/*
	 * Ensure we poke the trace metadata in the future, which is
	 * required to release the metadata reference on the trace.
	 */
	stream->trace->new_metadata_needed = true;
	lttng_live_unref_trace(stream->trace);
	g_free(stream);
}
