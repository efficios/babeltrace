/*
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/SRC.CTF.LTTNG-LIVE/DS"
#include "logging/comp-logging.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "../common/msg-iter/msg-iter.h"
#include "common/assert.h"
#include "compat/mman.h"
#include "data-stream.h"

#define STREAM_NAME_PREFIX	"stream-"

static
enum ctf_msg_iter_medium_status medop_request_bytes(
		size_t request_sz, uint8_t **buffer_addr,
		size_t *buffer_sz, void *data)
{
	enum ctf_msg_iter_medium_status status =
		CTF_MSG_ITER_MEDIUM_STATUS_OK;
	struct lttng_live_stream_iterator *stream = data;
	struct lttng_live_trace *trace = stream->trace;
	struct lttng_live_session *session = trace->session;
	struct lttng_live_msg_iter *live_msg_iter = session->lttng_live_msg_iter;
	uint64_t recv_len = 0;
	uint64_t len_left;
	uint64_t read_len;

	BT_ASSERT(request_sz);

	if (stream->has_stream_hung_up) {
		status = CTF_MSG_ITER_MEDIUM_STATUS_EOF;
		goto end;
	}

	len_left = stream->base_offset + stream->len - stream->offset;
	if (!len_left) {
		stream->state = LTTNG_LIVE_STREAM_ACTIVE_NO_DATA;
		status = CTF_MSG_ITER_MEDIUM_STATUS_AGAIN;
		goto end;
	}

	read_len = MIN(request_sz, stream->buflen);
	read_len = MIN(read_len, len_left);
	status = lttng_live_get_stream_bytes(live_msg_iter,
			stream, stream->buf, stream->offset,
			read_len, &recv_len);
	*buffer_addr = stream->buf;
	*buffer_sz = recv_len;
	stream->offset += recv_len;
end:
	return status;
}

static
bt_stream *medop_borrow_stream(bt_stream_class *stream_class,
		int64_t stream_id, void *data)
{
	struct lttng_live_stream_iterator *lttng_live_stream = data;
	bt_logging_level log_level = lttng_live_stream->log_level;
	bt_self_component *self_comp = lttng_live_stream->self_comp;

	if (!lttng_live_stream->stream) {
		uint64_t stream_class_id = bt_stream_class_get_id(stream_class);

		BT_COMP_LOGI("Creating stream %s (ID: %" PRIu64 ") out of stream "
			"class %" PRId64, lttng_live_stream->name->str,
			stream_id, stream_class_id);

		if (stream_id < 0) {
			/*
			 * No stream instance ID in the stream. It's possible
			 * to encounter this situation with older version of
			 * LTTng. In these cases, use the viewer_stream_id that
			 * is unique for a live viewer session.
			 */
			lttng_live_stream->stream = bt_stream_create_with_id(
				stream_class, lttng_live_stream->trace->trace,
				lttng_live_stream->viewer_stream_id);
		} else {
			lttng_live_stream->stream = bt_stream_create_with_id(
				stream_class, lttng_live_stream->trace->trace,
				(uint64_t) stream_id);
		}

		if (!lttng_live_stream->stream) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Cannot create stream %s (stream class ID "
				"%" PRId64 ", stream ID %" PRIu64 ")",
				lttng_live_stream->name->str,
				stream_class_id, stream_id);
			goto end;
		}

		bt_stream_set_name(lttng_live_stream->stream,
			lttng_live_stream->name->str);
	}

end:
	return lttng_live_stream->stream;
}

static struct ctf_msg_iter_medium_ops medops = {
	.request_bytes = medop_request_bytes,
	.seek = NULL,
	.borrow_stream = medop_borrow_stream,
};

BT_HIDDEN
enum lttng_live_iterator_status lttng_live_lazy_msg_init(
		struct lttng_live_session *session,
		bt_self_message_iterator *self_msg_iter)
{
	struct lttng_live_component *lttng_live =
		session->lttng_live_msg_iter->lttng_live_comp;
	uint64_t trace_idx, stream_iter_idx;
	bt_logging_level log_level = session->log_level;
	bt_self_component *self_comp = session->self_comp;

	if (!session->lazy_stream_msg_init) {
		return LTTNG_LIVE_ITERATOR_STATUS_OK;
	}

	BT_COMP_LOGD("Lazily initializing self message iterator for live session: "
		"session-id=%"PRIu64", self-msg-iter-addr=%p", session->id,
		self_msg_iter);

	for (trace_idx = 0; trace_idx < session->traces->len; trace_idx++) {
		struct lttng_live_trace *trace =
			g_ptr_array_index(session->traces, trace_idx);

		for (stream_iter_idx = 0;
				stream_iter_idx < trace->stream_iterators->len;
				stream_iter_idx++) {
			struct ctf_trace_class *ctf_tc;
			struct lttng_live_stream_iterator *stream_iter =
				g_ptr_array_index(trace->stream_iterators,
					stream_iter_idx);

			if (stream_iter->msg_iter) {
				continue;
			}
			ctf_tc = ctf_metadata_decoder_borrow_ctf_trace_class(
				trace->metadata->decoder);
			BT_COMP_LOGD("Creating CTF message iterator: "
				"session-id=%"PRIu64", ctf-tc-addr=%p, "
				"stream-iter-name=%s, self-msg-iter-addr=%p",
				session->id, ctf_tc, stream_iter->name->str, self_msg_iter);
			stream_iter->msg_iter = ctf_msg_iter_create(ctf_tc,
				lttng_live->max_query_size, medops, stream_iter,
				log_level, self_comp, self_msg_iter);
			if (!stream_iter->msg_iter) {
				BT_COMP_LOGE_APPEND_CAUSE(self_comp,
					"Failed to create CTF message iterator");
				goto error;
			}
		}
	}

	session->lazy_stream_msg_init = false;

	return LTTNG_LIVE_ITERATOR_STATUS_OK;

error:
	return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
}

BT_HIDDEN
struct lttng_live_stream_iterator *lttng_live_stream_iterator_create(
		struct lttng_live_session *session,
		uint64_t ctf_trace_id,
		uint64_t stream_id,
		bt_self_message_iterator *self_msg_iter)
{
	struct lttng_live_stream_iterator *stream_iter;
	struct lttng_live_component *lttng_live;
	struct lttng_live_trace *trace;
	bt_logging_level log_level;
	bt_self_component *self_comp;

	BT_ASSERT(session);
	BT_ASSERT(session->lttng_live_msg_iter);
	BT_ASSERT(session->lttng_live_msg_iter->lttng_live_comp);
	log_level = session->log_level;
	self_comp = session->self_comp;

	lttng_live = session->lttng_live_msg_iter->lttng_live_comp;

	stream_iter = g_new0(struct lttng_live_stream_iterator, 1);
	if (!stream_iter) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to allocate struct lttng_live_stream_iterator");
		goto error;
	}

	stream_iter->log_level = log_level;
	stream_iter->self_comp = self_comp;
	trace = lttng_live_session_borrow_or_create_trace_by_id(session, ctf_trace_id);
	if (!trace) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to borrow CTF trace.");
		goto error;
	}

	stream_iter->trace = trace;
	stream_iter->state = LTTNG_LIVE_STREAM_ACTIVE_NO_DATA;
	stream_iter->viewer_stream_id = stream_id;
	stream_iter->ctf_stream_class_id = -1ULL;
	stream_iter->last_inactivity_ts = INT64_MIN;

	if (trace->trace) {
		struct ctf_trace_class *ctf_tc =
			ctf_metadata_decoder_borrow_ctf_trace_class(
				trace->metadata->decoder);
		BT_ASSERT(!stream_iter->msg_iter);
		stream_iter->msg_iter = ctf_msg_iter_create(ctf_tc,
			lttng_live->max_query_size, medops, stream_iter,
			log_level, self_comp, self_msg_iter);
		if (!stream_iter->msg_iter) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create CTF message iterator");
			goto error;
		}
	}
	stream_iter->buf = g_new0(uint8_t, lttng_live->max_query_size);
	if (!stream_iter->buf) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to allocate live stream iterator buffer");
		goto error;
	}

	stream_iter->buflen = lttng_live->max_query_size;
	stream_iter->name = g_string_new(NULL);
	if (!stream_iter->name) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to allocate live stream iterator name buffer");
		goto error;
	}

	g_string_printf(stream_iter->name, STREAM_NAME_PREFIX "%" PRIu64,
		stream_iter->viewer_stream_id);
	g_ptr_array_add(trace->stream_iterators, stream_iter);

	/* Track the number of active stream iterator. */
	session->lttng_live_msg_iter->active_stream_iter++;

	goto end;
error:
	lttng_live_stream_iterator_destroy(stream_iter);
	stream_iter = NULL;
end:
	return stream_iter;
}

BT_HIDDEN
void lttng_live_stream_iterator_destroy(
		struct lttng_live_stream_iterator *stream_iter)
{
	if (!stream_iter) {
		return;
	}

	if (stream_iter->stream) {
		BT_STREAM_PUT_REF_AND_RESET(stream_iter->stream);
	}

	if (stream_iter->msg_iter) {
		ctf_msg_iter_destroy(stream_iter->msg_iter);
	}
	g_free(stream_iter->buf);
	if (stream_iter->name) {
		g_string_free(stream_iter->name, TRUE);
	}

	bt_message_put_ref(stream_iter->current_msg);

	/* Track the number of active stream iterator. */
	stream_iter->trace->session->lttng_live_msg_iter->active_stream_iter--;

	g_free(stream_iter);
}
