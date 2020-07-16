/*
 * lttng-live.c
 *
 * Babeltrace CTF LTTng-live Client Component
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#define BT_LOG_TAG "PLUGIN/SRC.CTF.LTTNG-LIVE"
#include "logging/comp-logging.h"

#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

#include <glib.h>

#include "common/assert.h"
#include <babeltrace2/babeltrace.h>
#include "compat/compiler.h"
#include <babeltrace2/types.h>

#include "plugins/common/muxing/muxing.h"
#include "plugins/common/param-validation/param-validation.h"

#include "data-stream.h"
#include "metadata.h"
#include "lttng-live.h"

#define MAX_QUERY_SIZE			    (256*1024)
#define URL_PARAM			    "url"
#define INPUTS_PARAM			    "inputs"
#define SESS_NOT_FOUND_ACTION_PARAM	    "session-not-found-action"
#define SESS_NOT_FOUND_ACTION_CONTINUE_STR  "continue"
#define SESS_NOT_FOUND_ACTION_FAIL_STR	    "fail"
#define SESS_NOT_FOUND_ACTION_END_STR	    "end"

#define print_dbg(fmt, ...)	BT_COMP_LOGD(fmt, ## __VA_ARGS__)

static
const char *lttng_live_iterator_status_string(
		enum lttng_live_iterator_status status)
{
	switch (status) {
	case LTTNG_LIVE_ITERATOR_STATUS_CONTINUE:
		return "LTTNG_LIVE_ITERATOR_STATUS_CONTINUE";
	case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
		return "LTTNG_LIVE_ITERATOR_STATUS_AGAIN";
	case LTTNG_LIVE_ITERATOR_STATUS_END:
		return "LTTNG_LIVE_ITERATOR_STATUS_END";
	case LTTNG_LIVE_ITERATOR_STATUS_OK:
		return "LTTNG_LIVE_ITERATOR_STATUS_OK";
	case LTTNG_LIVE_ITERATOR_STATUS_INVAL:
		return "LTTNG_LIVE_ITERATOR_STATUS_INVAL";
	case LTTNG_LIVE_ITERATOR_STATUS_ERROR:
		return "LTTNG_LIVE_ITERATOR_STATUS_ERROR";
	case LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
		return "LTTNG_LIVE_ITERATOR_STATUS_NOMEM";
	case LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
		return "LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED";
	default:
		bt_common_abort();
	}
}

static
const char *lttng_live_stream_state_string(enum lttng_live_stream_state state)
{
	switch (state) {
	case LTTNG_LIVE_STREAM_ACTIVE_NO_DATA:
		return "ACTIVE_NO_DATA";
	case LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA:
		return "QUIESCENT_NO_DATA";
	case LTTNG_LIVE_STREAM_QUIESCENT:
		return "QUIESCENT";
	case LTTNG_LIVE_STREAM_ACTIVE_DATA:
		return "ACTIVE_DATA";
	case LTTNG_LIVE_STREAM_EOF:
		return "EOF";
	default:
		return "ERROR";
	}
}

#define LTTNG_LIVE_LOGD_STREAM_ITER(live_stream_iter) \
	do { \
		BT_COMP_LOGD("Live stream iterator state=%s, last-inact-ts=%" PRId64  \
			", curr-inact-ts %" PRId64, \
			lttng_live_stream_state_string(live_stream_iter->state), \
			live_stream_iter->last_inactivity_ts, \
			live_stream_iter->current_inactivity_ts); \
	} while (0);

BT_HIDDEN
bool lttng_live_graph_is_canceled(struct lttng_live_msg_iter *msg_iter)
{
	bool ret;

	if (!msg_iter) {
		ret = false;
		goto end;
	}

	ret = bt_self_message_iterator_is_interrupted(
		msg_iter->self_msg_iter);

end:
	return ret;
}

static
struct lttng_live_trace *lttng_live_session_borrow_trace_by_id(struct lttng_live_session *session,
		uint64_t trace_id)
{
	uint64_t trace_idx;
	struct lttng_live_trace *ret_trace = NULL;

	for (trace_idx = 0; trace_idx < session->traces->len; trace_idx++) {
		struct lttng_live_trace *trace =
			g_ptr_array_index(session->traces, trace_idx);
		if (trace->id == trace_id) {
			ret_trace = trace;
			goto end;
		}
	}

end:
	return ret_trace;
}

static
void lttng_live_destroy_trace(struct lttng_live_trace *trace)
{
	bt_logging_level log_level = trace->log_level;
	bt_self_component *self_comp = trace->self_comp;

	BT_COMP_LOGD("Destroying live trace: trace-id=%"PRIu64, trace->id);

	BT_ASSERT(trace->stream_iterators);
	g_ptr_array_free(trace->stream_iterators, TRUE);

	BT_TRACE_PUT_REF_AND_RESET(trace->trace);
	BT_TRACE_CLASS_PUT_REF_AND_RESET(trace->trace_class);

	lttng_live_metadata_fini(trace);
	g_free(trace);
}

static
struct lttng_live_trace *lttng_live_create_trace(struct lttng_live_session *session,
		uint64_t trace_id)
{
	struct lttng_live_trace *trace = NULL;
	bt_logging_level log_level = session->log_level;
	bt_self_component *self_comp = session->self_comp;

	BT_COMP_LOGD("Creating live trace: "
		"session-id=%"PRIu64", trace-id=%"PRIu64,
		session->id, trace_id);
	trace = g_new0(struct lttng_live_trace, 1);
	if (!trace) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to allocate live trace");
		goto error;
	}
	trace->log_level = session->log_level;
	trace->self_comp = session->self_comp;
	trace->session = session;
	trace->id = trace_id;
	trace->trace_class = NULL;
	trace->trace = NULL;
	trace->stream_iterators = g_ptr_array_new_with_free_func(
		(GDestroyNotify) lttng_live_stream_iterator_destroy);
	BT_ASSERT(trace->stream_iterators);
	trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED;
	g_ptr_array_add(session->traces, trace);

	goto end;
error:
	g_free(trace);
	trace = NULL;
end:
	return trace;
}

BT_HIDDEN
struct lttng_live_trace *lttng_live_session_borrow_or_create_trace_by_id(
		struct lttng_live_session *session, uint64_t trace_id)
{
	struct lttng_live_trace *trace;

	trace = lttng_live_session_borrow_trace_by_id(session, trace_id);
	if (trace) {
		goto end;
	}

	/* The session is the owner of the newly created trace. */
	trace = lttng_live_create_trace(session, trace_id);

end:
	return trace;
}

BT_HIDDEN
int lttng_live_add_session(struct lttng_live_msg_iter *lttng_live_msg_iter,
		uint64_t session_id, const char *hostname,
		const char *session_name)
{
	int ret = 0;
	struct lttng_live_session *session;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;

	BT_COMP_LOGD("Adding live session: "
		"session-id=%" PRIu64 ", hostname=\"%s\" session-name=\"%s\"",
		session_id, hostname, session_name);

	session = g_new0(struct lttng_live_session, 1);
	if (!session) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to allocate live session");
		goto error;
	}

	session->log_level = lttng_live_msg_iter->log_level;
	session->self_comp = lttng_live_msg_iter->self_comp;
	session->id = session_id;
	session->traces = g_ptr_array_new_with_free_func(
		(GDestroyNotify) lttng_live_destroy_trace);
	BT_ASSERT(session->traces);
	session->lttng_live_msg_iter = lttng_live_msg_iter;
	session->new_streams_needed = true;
	session->hostname = g_string_new(hostname);
	BT_ASSERT(session->hostname);

	session->session_name = g_string_new(session_name);
	BT_ASSERT(session->session_name);

	g_ptr_array_add(lttng_live_msg_iter->sessions, session);
	goto end;
error:
	g_free(session);
	ret = -1;
end:
	return ret;
}

static
void lttng_live_destroy_session(struct lttng_live_session *session)
{
	bt_logging_level log_level;
	bt_self_component *self_comp;

	if (!session) {
		goto end;
	}

	log_level = session->log_level;
	self_comp = session->self_comp;
	BT_COMP_LOGD("Destroying live session: "
		"session-id=%"PRIu64", session-name=\"%s\"",
		session->id, session->session_name->str);
	if (session->id != -1ULL) {
		if (lttng_live_session_detach(session)) {
			if (!lttng_live_graph_is_canceled(
					session->lttng_live_msg_iter)) {
				/* Old relayd cannot detach sessions. */
				BT_COMP_LOGD("Unable to detach lttng live session %" PRIu64,
					session->id);
			}
		}
		session->id = -1ULL;
	}

	if (session->traces) {
		g_ptr_array_free(session->traces, TRUE);
	}

	if (session->hostname) {
		g_string_free(session->hostname, TRUE);
	}
	if (session->session_name) {
		g_string_free(session->session_name, TRUE);
	}
	g_free(session);

end:
	return;
}

static
void lttng_live_msg_iter_destroy(struct lttng_live_msg_iter *lttng_live_msg_iter)
{
	if (!lttng_live_msg_iter) {
		goto end;
	}

	if (lttng_live_msg_iter->sessions) {
		g_ptr_array_free(lttng_live_msg_iter->sessions, TRUE);
	}

	if (lttng_live_msg_iter->viewer_connection) {
		live_viewer_connection_destroy(lttng_live_msg_iter->viewer_connection);
	}
	BT_ASSERT(lttng_live_msg_iter->lttng_live_comp);
	BT_ASSERT(lttng_live_msg_iter->lttng_live_comp->has_msg_iter);

	/* All stream iterators must be destroyed at this point. */
	BT_ASSERT(lttng_live_msg_iter->active_stream_iter == 0);
	lttng_live_msg_iter->lttng_live_comp->has_msg_iter = false;

	g_free(lttng_live_msg_iter);

end:
	return;
}

BT_HIDDEN
void lttng_live_msg_iter_finalize(bt_self_message_iterator *self_msg_iter)
{
	struct lttng_live_msg_iter *lttng_live_msg_iter;

	BT_ASSERT(self_msg_iter);

	lttng_live_msg_iter = bt_self_message_iterator_get_data(self_msg_iter);
	BT_ASSERT(lttng_live_msg_iter);
	lttng_live_msg_iter_destroy(lttng_live_msg_iter);
}

static
enum lttng_live_iterator_status lttng_live_iterator_next_check_stream_state(
		struct lttng_live_stream_iterator *lttng_live_stream)
{
	bt_logging_level log_level = lttng_live_stream->log_level;
	bt_self_component *self_comp = lttng_live_stream->self_comp;

	switch (lttng_live_stream->state) {
	case LTTNG_LIVE_STREAM_QUIESCENT:
	case LTTNG_LIVE_STREAM_ACTIVE_DATA:
		break;
	case LTTNG_LIVE_STREAM_ACTIVE_NO_DATA:
		/* Invalid state. */
		BT_COMP_LOGF("Unexpected stream state \"ACTIVE_NO_DATA\"");
		bt_common_abort();
	case LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA:
		/* Invalid state. */
		BT_COMP_LOGF("Unexpected stream state \"QUIESCENT_NO_DATA\"");
		bt_common_abort();
	case LTTNG_LIVE_STREAM_EOF:
		break;
	}
	return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

/*
 * For active no data stream, fetch next data. It can be either:
 * - quiescent: need to put it in the prio heap at quiescent end
 *   timestamp,
 * - have data: need to wire up first event into the prio heap,
 * - have no data on this stream at this point: need to retry (AGAIN) or
 *   return EOF.
 */
static
enum lttng_live_iterator_status lttng_live_iterator_next_handle_one_no_data_stream(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *lttng_live_stream)
{
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
	enum lttng_live_stream_state orig_state = lttng_live_stream->state;
	struct packet_index index;

	if (lttng_live_stream->trace->metadata_stream_state ==
			LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED) {
		ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		goto end;
	}
	if (lttng_live_stream->trace->session->new_streams_needed) {
		ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		goto end;
	}
	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_NO_DATA &&
			lttng_live_stream->state != LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA) {
		goto end;
	}
	ret = lttng_live_get_next_index(lttng_live_msg_iter, lttng_live_stream,
		&index);
	if (ret != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	BT_ASSERT_DBG(lttng_live_stream->state != LTTNG_LIVE_STREAM_EOF);
	if (lttng_live_stream->state == LTTNG_LIVE_STREAM_QUIESCENT) {
		uint64_t last_inact_ts = lttng_live_stream->last_inactivity_ts,
			 curr_inact_ts = lttng_live_stream->current_inactivity_ts;

		if (orig_state == LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA &&
				last_inact_ts == curr_inact_ts) {
			ret = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			LTTNG_LIVE_LOGD_STREAM_ITER(lttng_live_stream);
		} else {
			ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		}
		goto end;
	}
	lttng_live_stream->base_offset = index.offset;
	lttng_live_stream->offset = index.offset;
	lttng_live_stream->len = index.packet_size / CHAR_BIT;
end:
	if (ret == LTTNG_LIVE_ITERATOR_STATUS_OK) {
		ret = lttng_live_iterator_next_check_stream_state(lttng_live_stream);
	}
	return ret;
}

/*
 * Creation of the message requires the ctf trace class to be created
 * beforehand, but the live protocol gives us all streams (including metadata)
 * at once. So we split it in three steps: getting streams, getting metadata
 * (which creates the ctf trace class), and then creating the per-stream
 * messages.
 */
static
enum lttng_live_iterator_status lttng_live_get_session(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_session *session)
{
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	enum lttng_live_iterator_status status;
	uint64_t trace_idx;

	BT_COMP_LOGD("Updating all streams for session: "
		"session-id=%"PRIu64", session-name=\"%s\"",
		session->id, session->session_name->str);

	if (!session->attached) {
		enum lttng_live_viewer_status attach_status =
			lttng_live_session_attach(session,
				lttng_live_msg_iter->self_msg_iter);
		if (attach_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
			if (lttng_live_graph_is_canceled(lttng_live_msg_iter)) {
				/*
				 * Clear any causes appended in
				 * `lttng_live_attach_session()` as we want to
				 * return gracefully since the graph was
				 * cancelled.
				 */
				bt_current_thread_clear_error();
				status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			} else {
				status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
				BT_COMP_LOGE_APPEND_CAUSE(self_comp,
					"Error attaching to LTTng live session");
			}
			goto end;
		}
	}

	status = lttng_live_session_get_new_streams(session,
		lttng_live_msg_iter->self_msg_iter);
	if (status != LTTNG_LIVE_ITERATOR_STATUS_OK &&
			status != LTTNG_LIVE_ITERATOR_STATUS_END) {
		goto end;
	}
	trace_idx = 0;
	while (trace_idx < session->traces->len) {
		struct lttng_live_trace *trace =
			g_ptr_array_index(session->traces, trace_idx);

		status = lttng_live_metadata_update(trace);
		switch (status) {
		case LTTNG_LIVE_ITERATOR_STATUS_END:
		case LTTNG_LIVE_ITERATOR_STATUS_OK:
			trace_idx++;
			break;
		case LTTNG_LIVE_ITERATOR_STATUS_CONTINUE:
		case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
			goto end;
		default:
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error updating trace metadata: "
				"stream-iter-status=%s, trace-id=%"PRIu64,
				lttng_live_iterator_status_string(status),
				trace->id);
			goto end;
		}
	}
	status = lttng_live_lazy_msg_init(session,
		lttng_live_msg_iter->self_msg_iter);

end:
	return status;
}

static
void lttng_live_force_new_streams_and_metadata(struct lttng_live_msg_iter *lttng_live_msg_iter)
{
	uint64_t session_idx, trace_idx;

	for (session_idx = 0; session_idx < lttng_live_msg_iter->sessions->len;
			session_idx++) {
		struct lttng_live_session *session =
			g_ptr_array_index(lttng_live_msg_iter->sessions, session_idx);
		session->new_streams_needed = true;
		for (trace_idx = 0; trace_idx < session->traces->len;
				trace_idx++) {
			struct lttng_live_trace *trace =
				g_ptr_array_index(session->traces, trace_idx);

			BT_ASSERT(trace->metadata_stream_state !=
				LTTNG_LIVE_METADATA_STREAM_STATE_CLOSED);

			trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED;
		}
	}
}

static
enum lttng_live_iterator_status
lttng_live_iterator_handle_new_streams_and_metadata(
		struct lttng_live_msg_iter *lttng_live_msg_iter)
{
	enum lttng_live_iterator_status status;
	enum lttng_live_viewer_status viewer_status;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	uint64_t session_idx = 0, nr_sessions_opened = 0;
	struct lttng_live_session *session;
	enum session_not_found_action sess_not_found_act =
		lttng_live_msg_iter->lttng_live_comp->params.sess_not_found_act;

	BT_COMP_LOGD("Update data and metadata of all sessions"
		"live-msg-iter-addr=%p", lttng_live_msg_iter);
	/*
	 * In a remotely distant future, we could add a "new
	 * session" flag to the protocol, which would tell us that we
	 * need to query for new sessions even though we have sessions
	 * currently ongoing.
	 */
	if (lttng_live_msg_iter->sessions->len == 0) {
		if (sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE) {
			BT_COMP_LOGD("No session found. Exiting in accordance with the `session-not-found-action` parameter");
			status = LTTNG_LIVE_ITERATOR_STATUS_END;
			goto end;
		} else {
			BT_COMP_LOGD("No session found. Try creating a new one in accordance with the `session-not-found-action` parameter");
			/*
			 * Retry to create a viewer session for the requested
			 * session name.
			 */
			viewer_status = lttng_live_create_viewer_session(lttng_live_msg_iter);
			if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
				if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
					status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
					BT_COMP_LOGE_APPEND_CAUSE(self_comp,
						"Error creating LTTng live viewer session");
				} else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
					status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
				} else {
					bt_common_abort();
				}
				goto end;
			}
		}
	}

	for (session_idx = 0; session_idx < lttng_live_msg_iter->sessions->len;
			session_idx++) {
		session = g_ptr_array_index(lttng_live_msg_iter->sessions,
				session_idx);
		status = lttng_live_get_session(lttng_live_msg_iter, session);
		switch (status) {
		case LTTNG_LIVE_ITERATOR_STATUS_OK:
			break;
		case LTTNG_LIVE_ITERATOR_STATUS_END:
			status = LTTNG_LIVE_ITERATOR_STATUS_OK;
			break;
		default:
			goto end;
		}
		if (!session->closed) {
			nr_sessions_opened++;
		}
	}

	if (sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE &&
			nr_sessions_opened == 0) {
		status = LTTNG_LIVE_ITERATOR_STATUS_END;
	} else {
		status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	}

end:
	return status;
}

static
enum lttng_live_iterator_status emit_inactivity_message(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream_iter,
		const bt_message **message, uint64_t timestamp)
{
	enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	bt_message *msg = NULL;

	BT_ASSERT(stream_iter->trace->clock_class);

	msg = bt_message_message_iterator_inactivity_create(
		lttng_live_msg_iter->self_msg_iter,
		stream_iter->trace->clock_class, timestamp);
	if (!msg) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error emitting message iterator inactivity message");
		goto error;
	}

	*message = msg;
end:
	return ret;

error:
	ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
	bt_message_put_ref(msg);
	goto end;
}

static
enum lttng_live_iterator_status lttng_live_iterator_next_handle_one_quiescent_stream(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *lttng_live_stream,
		const bt_message **message)
{
	enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;

	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_QUIESCENT) {
		return LTTNG_LIVE_ITERATOR_STATUS_OK;
	}

	if (lttng_live_stream->current_inactivity_ts ==
			lttng_live_stream->last_inactivity_ts) {
		lttng_live_stream->state = LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA;
		ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		goto end;
	}

	ret = emit_inactivity_message(lttng_live_msg_iter, lttng_live_stream,
		message, lttng_live_stream->current_inactivity_ts);

	lttng_live_stream->last_inactivity_ts =
		lttng_live_stream->current_inactivity_ts;
end:
	return ret;
}

static
int live_get_msg_ts_ns(struct lttng_live_stream_iterator *stream_iter,
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		const bt_message *msg, int64_t last_msg_ts_ns,
		int64_t *ts_ns)
{
	const bt_clock_class *clock_class = NULL;
	const bt_clock_snapshot *clock_snapshot = NULL;
	int ret = 0;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;

	BT_ASSERT_DBG(msg);
	BT_ASSERT_DBG(ts_ns);

	BT_COMP_LOGD("Getting message's timestamp: iter-data-addr=%p, msg-addr=%p, "
		"last-msg-ts=%" PRId64, lttng_live_msg_iter, msg,
		last_msg_ts_ns);

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		clock_class = bt_message_event_borrow_stream_class_default_clock_class_const(
				msg);
		BT_ASSERT_DBG(clock_class);

		clock_snapshot = bt_message_event_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		clock_class = bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		clock_snapshot = bt_message_packet_beginning_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		clock_class = bt_message_packet_end_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		clock_snapshot = bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		clock_class = bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		clock_snapshot = bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		clock_class = bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		clock_snapshot = bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		clock_snapshot = bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
			msg);
		break;
	default:
		/* All the other messages have a higher priority */
		BT_COMP_LOGD_STR("Message has no timestamp: using the last message timestamp.");
		*ts_ns = last_msg_ts_ns;
		goto end;
	}

	clock_class = bt_clock_snapshot_borrow_clock_class_const(clock_snapshot);
	BT_ASSERT_DBG(clock_class);

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot, ts_ns);
	if (ret) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Cannot get nanoseconds from Epoch of clock snapshot: "
			"clock-snapshot-addr=%p", clock_snapshot);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	if (ret == 0) {
		BT_COMP_LOGD("Found message's timestamp: "
			"iter-data-addr=%p, msg-addr=%p, "
			"last-msg-ts=%" PRId64 ", ts=%" PRId64,
			lttng_live_msg_iter, msg, last_msg_ts_ns, *ts_ns);
	}

	return ret;
}

static
enum lttng_live_iterator_status lttng_live_iterator_next_handle_one_active_data_stream(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *lttng_live_stream,
		const bt_message **message)
{
	enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	enum ctf_msg_iter_status status;
	uint64_t session_idx, trace_idx;

	for (session_idx = 0; session_idx < lttng_live_msg_iter->sessions->len;
			session_idx++) {
		struct lttng_live_session *session =
			g_ptr_array_index(lttng_live_msg_iter->sessions, session_idx);

		if (session->new_streams_needed) {
			ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
			goto end;
		}
		for (trace_idx = 0; trace_idx < session->traces->len;
				trace_idx++) {
			struct lttng_live_trace *trace =
				g_ptr_array_index(session->traces, trace_idx);
			if (trace->metadata_stream_state == LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED) {
				ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
				goto end;
			}
		}
	}

	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_DATA) {
		ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Invalid state of live stream iterator"
			"stream-iter-status=%s",
			lttng_live_stream_state_string(lttng_live_stream->state));
		goto end;
	}

	status = ctf_msg_iter_get_next_message(
		lttng_live_stream->msg_iter, message);
	switch (status) {
	case CTF_MSG_ITER_STATUS_EOF:
		ret = LTTNG_LIVE_ITERATOR_STATUS_END;
		break;
	case CTF_MSG_ITER_STATUS_OK:
		ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
		break;
	case CTF_MSG_ITER_STATUS_AGAIN:
		/*
		 * Continue immediately (end of packet). The next
		 * get_index may return AGAIN to delay the following
		 * attempt.
		 */
		ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		break;
	case CTF_MSG_ITER_STATUS_ERROR:
	default:
		ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"CTF message iterator failed to get next message: "
			"msg-iter=%p, msg-iter-status=%s",
			lttng_live_stream->msg_iter,
			ctf_msg_iter_status_string(status));
		break;
	}

end:
	return ret;
}

static
enum lttng_live_iterator_status lttng_live_iterator_close_stream(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream_iter,
		const bt_message **curr_msg)
{
	enum lttng_live_iterator_status live_status =
		LTTNG_LIVE_ITERATOR_STATUS_OK;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	/*
	 * The viewer has hung up on us so we are closing the stream. The
	 * `ctf_msg_iter` should simply realize that it needs to close the
	 * stream properly by emitting the necessary stream end message.
	 */
	enum ctf_msg_iter_status status = ctf_msg_iter_get_next_message(
		stream_iter->msg_iter, curr_msg);

	if (status == CTF_MSG_ITER_STATUS_ERROR) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error getting the next message from CTF message iterator");
		live_status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		goto end;
	} else if (status == CTF_MSG_ITER_STATUS_EOF) {
		BT_COMP_LOGI("Reached the end of the live stream iterator.");
		live_status = LTTNG_LIVE_ITERATOR_STATUS_END;
		goto end;
	}

	BT_ASSERT(status == CTF_MSG_ITER_STATUS_OK);

end:
	return live_status;
}

/*
 * helper function:
 *            handle_no_data_streams()
 *              retry:
 *                - for each ACTIVE_NO_DATA stream:
 *                  - query relayd for stream data, or quiescence info.
 *                    - if need metadata, get metadata, goto retry.
 *                    - if new stream, get new stream as ACTIVE_NO_DATA, goto retry
 *                  - if quiescent, move to QUIESCENT streams
 *                  - if fetched data, move to ACTIVE_DATA streams
 *                (at this point each stream either has data, or is quiescent)
 *
 *
 * iterator_next:
 *            handle_new_streams_and_metadata()
 *                  - query relayd for known streams, add them as ACTIVE_NO_DATA
 *                  - query relayd for metadata
 *
 *            call handle_active_no_data_streams()
 *
 *            handle_quiescent_streams()
 *                - if at least one stream is ACTIVE_DATA:
 *                  - peek stream event with lowest timestamp -> next_ts
 *                  - for each quiescent stream
 *                    - if next_ts >= quiescent end
 *                      - set state to ACTIVE_NO_DATA
 *                - else
 *                  - for each quiescent stream
 *                      - set state to ACTIVE_NO_DATA
 *
 *            call handle_active_no_data_streams()
 *
 *            handle_active_data_streams()
 *                - if at least one stream is ACTIVE_DATA:
 *                    - get stream event with lowest timestamp from heap
 *                    - make that stream event the current message.
 *                    - move this stream heap position to its next event
 *                      - if we need to fetch data from relayd, move
 *                        stream to ACTIVE_NO_DATA.
 *                    - return OK
 *                - return AGAIN
 *
 * end criterion: ctrl-c on client. If relayd exits or the session
 * closes on the relay daemon side, we keep on waiting for streams.
 * Eventually handle --end timestamp (also an end criterion).
 *
 * When disconnected from relayd: try to re-connect endlessly.
 */
static
enum lttng_live_iterator_status lttng_live_iterator_next_msg_on_stream(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream_iter,
		const bt_message **curr_msg)
{
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	enum lttng_live_iterator_status live_status;

	BT_COMP_LOGD("Finding the next message for stream iterator: "
		"stream-name=\"%s\"", stream_iter->name->str);

	if (stream_iter->has_stream_hung_up) {
		/*
		 * The stream has hung up and the stream was properly closed
		 * during the last call to the current function. Return _END
		 * status now so that this stream iterator is removed for the
		 * stream iterator list.
		 */
		live_status = LTTNG_LIVE_ITERATOR_STATUS_END;
		goto end;
	}

retry:
	LTTNG_LIVE_LOGD_STREAM_ITER(stream_iter);
	live_status = lttng_live_iterator_handle_new_streams_and_metadata(
		lttng_live_msg_iter);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	live_status = lttng_live_iterator_next_handle_one_no_data_stream(
		lttng_live_msg_iter, stream_iter);

	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		if (live_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
			/*
			 * We overwrite `live_status` since `curr_msg` is
			 * likely set to a valid message in this function.
			 */
			live_status = lttng_live_iterator_close_stream(
				lttng_live_msg_iter, stream_iter, curr_msg);
		}
		goto end;
	}
	live_status = lttng_live_iterator_next_handle_one_quiescent_stream(
		lttng_live_msg_iter, stream_iter, curr_msg);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		BT_ASSERT(!*curr_msg);
		goto end;
	}
	if (*curr_msg) {
		goto end;
	}
	live_status = lttng_live_iterator_next_handle_one_active_data_stream(
		lttng_live_msg_iter, stream_iter, curr_msg);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		BT_ASSERT(!*curr_msg);
	}

end:
	if (live_status == LTTNG_LIVE_ITERATOR_STATUS_CONTINUE) {
		goto retry;
	}

	return live_status;
}

static
bool is_discarded_packet_or_event_message(const bt_message *msg)
{
	const enum bt_message_type msg_type = bt_message_get_type(msg);

	return msg_type == BT_MESSAGE_TYPE_DISCARDED_EVENTS ||
			msg_type == BT_MESSAGE_TYPE_DISCARDED_PACKETS;
}

static
enum lttng_live_iterator_status adjust_discarded_packets_message(
		bt_self_message_iterator *iter,
		const bt_stream *stream,
		const bt_message *msg_in, bt_message **msg_out,
		uint64_t new_begin_ts)
{
	enum lttng_live_iterator_status status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	enum bt_property_availability availability;
	const bt_clock_snapshot *clock_snapshot;
	uint64_t end_ts;
	uint64_t count;

	clock_snapshot = bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(msg_in);
	end_ts = bt_clock_snapshot_get_value(clock_snapshot);

	availability = bt_message_discarded_packets_get_count(msg_in, &count);
	BT_ASSERT_DBG(availability == BT_PROPERTY_AVAILABILITY_AVAILABLE);

	*msg_out = bt_message_discarded_packets_create_with_default_clock_snapshots(
		iter, stream, new_begin_ts, end_ts);
	if (!*msg_out) {
		status = LTTNG_LIVE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	bt_message_discarded_packets_set_count(*msg_out, count);
end:
	return status;
}

static
enum lttng_live_iterator_status adjust_discarded_events_message(
		bt_self_message_iterator *iter,
		const bt_stream *stream,
		const bt_message *msg_in, bt_message **msg_out,
		uint64_t new_begin_ts)
{
	enum lttng_live_iterator_status status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	enum bt_property_availability availability;
	const bt_clock_snapshot *clock_snapshot;
	uint64_t end_ts;
	uint64_t count;

	clock_snapshot = bt_message_discarded_events_borrow_end_default_clock_snapshot_const(msg_in);
	end_ts = bt_clock_snapshot_get_value(clock_snapshot);

	availability = bt_message_discarded_events_get_count(msg_in, &count);
	BT_ASSERT_DBG(availability == BT_PROPERTY_AVAILABILITY_AVAILABLE);

	*msg_out = bt_message_discarded_events_create_with_default_clock_snapshots(
		iter, stream, new_begin_ts, end_ts);
	if (!*msg_out) {
		status = LTTNG_LIVE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	bt_message_discarded_events_set_count(*msg_out, count);
end:
	return status;
}

static
enum lttng_live_iterator_status next_stream_iterator_for_trace(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_trace *live_trace,
		struct lttng_live_stream_iterator **youngest_trace_stream_iter)
{
	struct lttng_live_stream_iterator *youngest_candidate_stream_iter = NULL;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	enum lttng_live_iterator_status stream_iter_status;;
	int64_t youngest_candidate_msg_ts = INT64_MAX;
	uint64_t stream_iter_idx;

	BT_ASSERT_DBG(live_trace);
	BT_ASSERT_DBG(live_trace->stream_iterators);

	BT_COMP_LOGD("Finding the next stream iterator for trace: "
		"trace-id=%"PRIu64, live_trace->id);
	/*
	 * Update the current message of every stream iterators of this trace.
	 * The current msg of every stream must have a timestamp equal or
	 * larger than the last message returned by this iterator. We must
	 * ensure monotonicity.
	 */
	stream_iter_idx = 0;
	while (stream_iter_idx < live_trace->stream_iterators->len) {
		bool stream_iter_is_ended = false;
		struct lttng_live_stream_iterator *stream_iter =
			g_ptr_array_index(live_trace->stream_iterators,
				stream_iter_idx);

		/*
		 * Find if there is are now current message for this stream
		 * iterator get it.
		 */
		while (!stream_iter->current_msg) {
			const bt_message *msg = NULL;
			int64_t curr_msg_ts_ns = INT64_MAX;
			stream_iter_status = lttng_live_iterator_next_msg_on_stream(
				lttng_live_msg_iter, stream_iter, &msg);

			BT_COMP_LOGD("live stream iterator returned status :%s",
				lttng_live_iterator_status_string(stream_iter_status));
			if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
				stream_iter_is_ended = true;
				break;
			}

			if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
				goto end;
			}

			BT_ASSERT_DBG(msg);

			/*
			 * Get the timestamp in nanoseconds from origin of this
			 * messsage.
			 */
			live_get_msg_ts_ns(stream_iter, lttng_live_msg_iter,
				msg, lttng_live_msg_iter->last_msg_ts_ns,
				&curr_msg_ts_ns);

			/*
			 * Check if the message of the current live stream
			 * iterator occurred at the exact same time or after the
			 * last message returned by this component's message
			 * iterator. If not, we return an error.
			 */
			if (curr_msg_ts_ns >= lttng_live_msg_iter->last_msg_ts_ns) {
				stream_iter->current_msg = msg;
				stream_iter->current_msg_ts_ns = curr_msg_ts_ns;
			} else {
				const bt_clock_class *clock_class;
				const bt_stream_class *stream_class;
				enum bt_clock_class_cycles_to_ns_from_origin_status ts_ns_status;
				int64_t last_inactivity_ts_ns;

				stream_class = bt_stream_borrow_class_const(stream_iter->stream);
				clock_class = bt_stream_class_borrow_default_clock_class_const(stream_class);

				ts_ns_status = bt_clock_class_cycles_to_ns_from_origin(
					clock_class,
					stream_iter->last_inactivity_ts,
					&last_inactivity_ts_ns);
				if (ts_ns_status != BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK) {
					stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
					goto end;
				}

				if (last_inactivity_ts_ns > curr_msg_ts_ns &&
					is_discarded_packet_or_event_message(msg)) {
					/*
					 * The CTF message iterator emits Discarded
					 * Packets and Events with synthesized begin and
					 * end timestamps from the bounds of the last
					 * known packet and the newly decoded packet
					 * header.
					 *
					 * The CTF message iterator is not aware of
					 * stream inactivity beacons. Hence, we have
					 * to adjust the begin timestamp of those types
					 * of messages if a stream signaled its
					 * inactivity up until _after_ the last known
					 * packet's begin timestamp.
					 *
					 * Otherwise, the monotonicity guarantee would
					 * not be preserved.
					 */
					const enum bt_message_type msg_type =
						bt_message_get_type(msg);
					enum lttng_live_iterator_status adjust_status =
						LTTNG_LIVE_ITERATOR_STATUS_OK;
					bt_message *adjusted_message;

					switch (msg_type) {
					case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
						adjust_status = adjust_discarded_events_message(
							lttng_live_msg_iter->self_msg_iter,
							stream_iter->stream,
							msg, &adjusted_message,
							stream_iter->last_inactivity_ts);
						break;
					case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
						adjust_status = adjust_discarded_packets_message(
							lttng_live_msg_iter->self_msg_iter,
							stream_iter->stream,
							msg, &adjusted_message,
							stream_iter->last_inactivity_ts);
						break;
					default:
						bt_common_abort();
					}

					if (adjust_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
						stream_iter_status = adjust_status;
						goto end;
					}

					BT_ASSERT_DBG(adjusted_message);
					stream_iter->current_msg = adjusted_message;
					stream_iter->current_msg_ts_ns =
						last_inactivity_ts_ns;
				} else {
					/*
					 * We received a message in the past. To ensure
					 * monotonicity, we can't send it forward.
					 */
					BT_COMP_LOGE_APPEND_CAUSE(self_comp,
						"Message's timestamp is less than "
						"lttng-live's message iterator's last "
						"returned timestamp: "
						"lttng-live-msg-iter-addr=%p, ts=%" PRId64 ", "
						"last-msg-ts=%" PRId64,
						lttng_live_msg_iter, curr_msg_ts_ns,
						lttng_live_msg_iter->last_msg_ts_ns);
					stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
					goto end;
				}
			}
		}

		BT_ASSERT_DBG(stream_iter != youngest_candidate_stream_iter);

		if (!stream_iter_is_ended) {
			if (G_UNLIKELY(youngest_candidate_stream_iter == NULL) ||
					stream_iter->current_msg_ts_ns < youngest_candidate_msg_ts) {
				/*
				 * Update the current best candidate message
				 * for the stream iterator of this live trace
				 * to be forwarded downstream.
				 */
				youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
				youngest_candidate_stream_iter = stream_iter;
			} else if (stream_iter->current_msg_ts_ns == youngest_candidate_msg_ts) {
				/*
				 * Order the messages in an arbitrary but
				 * deterministic way.
				 */
				BT_ASSERT_DBG(stream_iter != youngest_candidate_stream_iter);
				int ret = common_muxing_compare_messages(
					stream_iter->current_msg,
					youngest_candidate_stream_iter->current_msg);
				if (ret < 0) {
					/*
					 * The `youngest_candidate_stream_iter->current_msg`
					 * should go first. Update the next
					 * iterator and the current timestamp.
					 */
					youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
					youngest_candidate_stream_iter = stream_iter;
				} else if (ret == 0) {
					/*
					 * Unable to pick which one should go
					 * first.
					 */
					BT_COMP_LOGW("Cannot deterministically pick next live stream message iterator because they have identical next messages: "
						"stream-iter-addr=%p"
						"stream-iter-addr=%p",
						stream_iter,
						youngest_candidate_stream_iter);
				}
			}

			stream_iter_idx++;
		} else {
			/*
			 * The live stream iterator has ended. That
			 * iterator is removed from the array, but
			 * there is no need to increment
			 * stream_iter_idx as
			 * g_ptr_array_remove_index_fast replaces the
			 * removed element with the array's last
			 * element.
			 */
			g_ptr_array_remove_index_fast(
				live_trace->stream_iterators,
				stream_iter_idx);
		}
	}

	if (youngest_candidate_stream_iter) {
		*youngest_trace_stream_iter = youngest_candidate_stream_iter;
		stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	} else {
		/*
		 * The only case where we don't have a candidate for this trace
		 * is if we reached the end of all the iterators.
		 */
		BT_ASSERT(live_trace->stream_iterators->len == 0);
		stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_END;
	}

end:
	return stream_iter_status;
}

static
enum lttng_live_iterator_status next_stream_iterator_for_session(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_session *session,
		struct lttng_live_stream_iterator **youngest_session_stream_iter)
{
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	enum lttng_live_iterator_status stream_iter_status;
	uint64_t trace_idx = 0;
	int64_t youngest_candidate_msg_ts = INT64_MAX;
	struct lttng_live_stream_iterator *youngest_candidate_stream_iter = NULL;

	BT_COMP_LOGD("Finding the next stream iterator for session: "
		"session-id=%"PRIu64, session->id);
	/*
	 * Make sure we are attached to the session and look for new streams
	 * and metadata.
	 */
	stream_iter_status = lttng_live_get_session(lttng_live_msg_iter, session);
	if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK &&
			stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_CONTINUE &&
			stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_END) {
		goto end;
	}

	BT_ASSERT_DBG(session->traces);

	while (trace_idx < session->traces->len) {
		bool trace_is_ended = false;
		struct lttng_live_stream_iterator *stream_iter;
		struct lttng_live_trace *trace =
			g_ptr_array_index(session->traces, trace_idx);

		stream_iter_status = next_stream_iterator_for_trace(
			lttng_live_msg_iter, trace, &stream_iter);
		if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
			/*
			 * All the live stream iterators for this trace are
			 * ENDed. Remove the trace from this session.
			 */
			trace_is_ended = true;
		} else if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
			goto end;
		}

		if (!trace_is_ended) {
			BT_ASSERT_DBG(stream_iter);

			if (G_UNLIKELY(youngest_candidate_stream_iter == NULL) ||
					stream_iter->current_msg_ts_ns < youngest_candidate_msg_ts) {
				youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
				youngest_candidate_stream_iter = stream_iter;
			} else if (stream_iter->current_msg_ts_ns == youngest_candidate_msg_ts) {
				/*
				 * Order the messages in an arbitrary but
				 * deterministic way.
				 */
				int ret = common_muxing_compare_messages(
					stream_iter->current_msg,
					youngest_candidate_stream_iter->current_msg);
				if (ret < 0) {
					/*
					 * The `youngest_candidate_stream_iter->current_msg`
					 * should go first. Update the next iterator
					 * and the current timestamp.
					 */
					youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
					youngest_candidate_stream_iter = stream_iter;
				} else if (ret == 0) {
					/* Unable to pick which one should go first. */
					BT_COMP_LOGW("Cannot deterministically pick next live stream message iterator because they have identical next messages: "
						"stream-iter-addr=%p" "stream-iter-addr=%p",
						stream_iter, youngest_candidate_stream_iter);
				}
			}
			trace_idx++;
		} else {
			/*
			 * trace_idx is not incremented since
			 * g_ptr_array_remove_index_fast replaces the
			 * element at trace_idx with the array's last element.
			 */
			g_ptr_array_remove_index_fast(session->traces,
				trace_idx);
		}
	}
	if (youngest_candidate_stream_iter) {
		*youngest_session_stream_iter = youngest_candidate_stream_iter;
		stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	} else {
		/*
		 * The only cases where we don't have a candidate for this
		 * trace is:
		 *  1. if we reached the end of all the iterators of all the
		 *  traces of this session,
		 *  2. if we never had live stream iterator in the first place.
		 *
		 * In either cases, we return END.
		 */
		BT_ASSERT(session->traces->len == 0);
		stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_END;
	}
end:
	return stream_iter_status;
}

static inline
void put_messages(bt_message_array_const msgs, uint64_t count)
{
	uint64_t i;

	for (i = 0; i < count; i++) {
		BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);
	}
}

BT_HIDDEN
bt_message_iterator_class_next_method_status lttng_live_msg_iter_next(
		bt_self_message_iterator *self_msg_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status;
	enum lttng_live_viewer_status viewer_status;
	struct lttng_live_msg_iter *lttng_live_msg_iter =
		bt_self_message_iterator_get_data(self_msg_it);
	struct lttng_live_component *lttng_live =
		lttng_live_msg_iter->lttng_live_comp;
	bt_self_component *self_comp = lttng_live_msg_iter->self_comp;
	bt_logging_level log_level = lttng_live_msg_iter->log_level;
	enum lttng_live_iterator_status stream_iter_status;
	uint64_t session_idx;

	*count = 0;

	BT_ASSERT_DBG(lttng_live_msg_iter);

	if (G_UNLIKELY(lttng_live_msg_iter->was_interrupted)) {
		/*
		 * The iterator was interrupted in a previous call to the
		 * `_next()` method. We currently do not support generating
		 * messages after such event. The babeltrace2 CLI should never
		 * be running the graph after being interrupted. So this check
		 * is to prevent other graph users from using this live
		 * iterator in an messed up internal state.
		 */
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Message iterator was interrupted during a previous call to the `next()` and currently does not support continuing after such event.");
		goto end;
	}

	/*
	 * Clear all the invalid message reference that might be left over in
	 * the output array.
	 */
	memset(msgs, 0, capacity * sizeof(*msgs));

	/*
	 * If no session are exposed on the relay found at the url provided by
	 * the user, session count will be 0. In this case, we return status
	 * end to return gracefully.
	 */
	if (lttng_live_msg_iter->sessions->len == 0) {
		if (lttng_live->params.sess_not_found_act !=
				SESSION_NOT_FOUND_ACTION_CONTINUE) {
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
			goto end;
		} else {
			/*
			 * The are no more active session for this session
			 * name. Retry to create a viewer session for the
			 * requested session name.
			 */
			viewer_status = lttng_live_create_viewer_session(lttng_live_msg_iter);
			if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
				if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
					status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
					BT_COMP_LOGE_APPEND_CAUSE(self_comp,
						"Error creating LTTng live viewer session");
				} else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
					status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
				} else {
					bt_common_abort();
				}
				goto end;
			}
		}
	}

	if (lttng_live_msg_iter->active_stream_iter == 0) {
		lttng_live_force_new_streams_and_metadata(lttng_live_msg_iter);
	}

	/*
	 * Here the muxing of message is done.
	 *
	 * We need to iterate over all the streams of all the traces of all the
	 * viewer sessions in order to get the message with the smallest
	 * timestamp. In this case, a session is a viewer session and there is
	 * one viewer session per consumer daemon. (UST 32bit, UST 64bit and/or
	 * kernel). Each viewer session can have multiple traces, for example,
	 * 64bit UST viewer sessions could have multiple per-pid traces.
	 *
	 * We iterate over the streams of each traces to update and see what is
	 * their next message's timestamp. From those timestamps, we select the
	 * message with the smallest timestamp as the best candidate message
	 * for that trace and do the same thing across all the sessions.
	 *
	 * We then compare the timestamp of best candidate message of all the
	 * sessions to pick the message with the smallest timestamp and we
	 * return it.
	 */
	while (*count < capacity) {
		struct lttng_live_stream_iterator *youngest_stream_iter = NULL,
			  *candidate_stream_iter = NULL;
		int64_t youngest_msg_ts_ns = INT64_MAX;

		BT_ASSERT_DBG(lttng_live_msg_iter->sessions);
		session_idx = 0;
		while (session_idx < lttng_live_msg_iter->sessions->len) {
			struct lttng_live_session *session =
				g_ptr_array_index(lttng_live_msg_iter->sessions,
					session_idx);

			/* Find the best candidate message to send downstream. */
			stream_iter_status = next_stream_iterator_for_session(
				lttng_live_msg_iter, session,
				&candidate_stream_iter);

			/* If we receive an END status, it means that either:
			 * - Those traces never had active streams (UST with no
			 *   data produced yet),
			 * - All live stream iterators have ENDed.*/
			if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
				if (session->closed && session->traces->len == 0) {
					/*
					 * Remove the session from the list.
					 * session_idx is not modified since
					 * g_ptr_array_remove_index_fast
					 * replaces the the removed element with
					 * the array's last element.
					 */
					g_ptr_array_remove_index_fast(
						lttng_live_msg_iter->sessions,
						session_idx);
				} else {
					session_idx++;
				}
				continue;
			}

			if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
				goto return_status;
			}

			if (G_UNLIKELY(youngest_stream_iter == NULL) ||
					candidate_stream_iter->current_msg_ts_ns < youngest_msg_ts_ns) {
				youngest_msg_ts_ns = candidate_stream_iter->current_msg_ts_ns;
				youngest_stream_iter = candidate_stream_iter;
			} else if (candidate_stream_iter->current_msg_ts_ns == youngest_msg_ts_ns) {
				/*
				 * The currently selected message to be sent
				 * downstream next has the exact same timestamp
				 * that of the current candidate message. We
				 * must break the tie in a predictable manner.
				 */
				BT_COMP_LOGD_STR("Two of the next message candidates have the same timestamps, pick one deterministically.");
				/*
				 * Order the messages in an arbitrary but
				 * deterministic way.
				 */
				int ret = common_muxing_compare_messages(
					candidate_stream_iter->current_msg,
					youngest_stream_iter->current_msg);
				if (ret < 0) {
					/*
					 * The `candidate_stream_iter->current_msg`
					 * should go first. Update the next
					 * iterator and the current timestamp.
					 */
					youngest_msg_ts_ns = candidate_stream_iter->current_msg_ts_ns;
					youngest_stream_iter = candidate_stream_iter;
				} else if (ret == 0) {
					/* Unable to pick which one should go first. */
					BT_COMP_LOGW("Cannot deterministically pick next live stream message iterator because they have identical next messages: "
						"next-stream-iter-addr=%p" "candidate-stream-iter-addr=%p",
						youngest_stream_iter, candidate_stream_iter);
				}
			}

			session_idx++;
		}

		if (!youngest_stream_iter) {
			stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			goto return_status;
		}

		BT_ASSERT_DBG(youngest_stream_iter->current_msg);
		/* Ensure monotonicity. */
		BT_ASSERT_DBG(lttng_live_msg_iter->last_msg_ts_ns <=
			youngest_stream_iter->current_msg_ts_ns);

		/*
		 * Insert the next message to the message batch. This will set
		 * stream iterator current messsage to NULL so that next time
		 * we fetch the next message of that stream iterator
		 */
		BT_MESSAGE_MOVE_REF(msgs[*count], youngest_stream_iter->current_msg);
		(*count)++;

		/* Update the last timestamp in nanoseconds sent downstream. */
		lttng_live_msg_iter->last_msg_ts_ns = youngest_msg_ts_ns;
		youngest_stream_iter->current_msg_ts_ns = INT64_MAX;

		stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	}

return_status:
	switch (stream_iter_status) {
	case LTTNG_LIVE_ITERATOR_STATUS_OK:
	case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
		/*
		 * If we gathered messages, return _OK even if the graph was
		 * interrupted. This allows for the components downstream to at
		 * least get the thoses messages. If the graph was indeed
		 * interrupted there should not be another _next() call as the
		 * application will tear down the graph. This component class
		 * doesn't support restarting after an interruption.
		 */
		if (*count > 0) {
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
		} else {
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
		}
		break;
	case LTTNG_LIVE_ITERATOR_STATUS_END:
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
		break;
	case LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Memory error preparing the next batch of messages: "
			"live-iter-status=%s",
			lttng_live_iterator_status_string(stream_iter_status));
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
		break;
	case LTTNG_LIVE_ITERATOR_STATUS_ERROR:
	case LTTNG_LIVE_ITERATOR_STATUS_INVAL:
	case LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error preparing the next batch of messages: "
			"live-iter-status=%s",
			lttng_live_iterator_status_string(stream_iter_status));

		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		/* Put all existing messages on error. */
		put_messages(msgs, *count);
		break;
	default:
		bt_common_abort();
	}

end:
	return status;
}

static
struct lttng_live_msg_iter *lttng_live_msg_iter_create(
		struct lttng_live_component *lttng_live_comp,
		bt_self_message_iterator *self_msg_it)
{
	bt_self_component *self_comp = lttng_live_comp->self_comp;
	bt_logging_level log_level = lttng_live_comp->log_level;

	struct lttng_live_msg_iter *lttng_live_msg_iter =
		g_new0(struct lttng_live_msg_iter, 1);
	if (!lttng_live_msg_iter) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to allocate lttng_live_msg_iter");
		goto end;
	}

	lttng_live_msg_iter->log_level = lttng_live_comp->log_level;
	lttng_live_msg_iter->self_comp = lttng_live_comp->self_comp;
	lttng_live_msg_iter->lttng_live_comp = lttng_live_comp;
	lttng_live_msg_iter->self_msg_iter = self_msg_it;

	lttng_live_msg_iter->active_stream_iter = 0;
	lttng_live_msg_iter->last_msg_ts_ns = INT64_MIN;
	lttng_live_msg_iter->was_interrupted = false;

	lttng_live_msg_iter->sessions = g_ptr_array_new_with_free_func(
		(GDestroyNotify) lttng_live_destroy_session);
	BT_ASSERT(lttng_live_msg_iter->sessions);

end:
	return lttng_live_msg_iter;

}

BT_HIDDEN
bt_message_iterator_class_initialize_method_status lttng_live_msg_iter_init(
		bt_self_message_iterator *self_msg_it,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_port)
{
	bt_message_iterator_class_initialize_method_status status;
	struct lttng_live_component *lttng_live;
	struct lttng_live_msg_iter *lttng_live_msg_iter;
	enum lttng_live_viewer_status viewer_status;
	bt_logging_level log_level;
	bt_self_component *self_comp =
		bt_self_message_iterator_borrow_component(self_msg_it);

	lttng_live = bt_self_component_get_data(self_comp);
	log_level = lttng_live->log_level;
	self_comp = lttng_live->self_comp;


	/* There can be only one downstream iterator at the same time. */
	BT_ASSERT(!lttng_live->has_msg_iter);
	lttng_live->has_msg_iter = true;

	lttng_live_msg_iter = lttng_live_msg_iter_create(lttng_live,
		self_msg_it);
	if (!lttng_live_msg_iter) {
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to create lttng_live_msg_iter");
		goto error;
	}

	 viewer_status = live_viewer_connection_create(self_comp, NULL,
		log_level, lttng_live->params.url->str, false,
		lttng_live_msg_iter, &lttng_live_msg_iter->viewer_connection);
	if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
		if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create viewer connection");
		} else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
			/*
			 * Interruption in the _iter_init() method is not
			 * supported. Return an error.
			 */
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Interrupted while creating viewer connection");
		}
		goto error;
	}

	viewer_status = lttng_live_create_viewer_session(lttng_live_msg_iter);
	if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
		if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create viewer session");
		} else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
			/*
			 * Interruption in the _iter_init() method is not
			 * supported. Return an error.
			 */
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Interrupted when creating viewer session");
		}
		goto error;
	}

	if (lttng_live_msg_iter->sessions->len == 0) {
		switch (lttng_live->params.sess_not_found_act) {
		case SESSION_NOT_FOUND_ACTION_CONTINUE:
			BT_COMP_LOGI("Unable to connect to the requested live viewer session. Keep trying to connect because of "
				"%s=\"%s\" component parameter: url=\"%s\"",
				SESS_NOT_FOUND_ACTION_PARAM,
				SESS_NOT_FOUND_ACTION_CONTINUE_STR,
				lttng_live->params.url->str);
			break;
		case SESSION_NOT_FOUND_ACTION_FAIL:
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Unable to connect to the requested live viewer session. Fail the message iterator initialization because of %s=\"%s\" "
				"component parameter: url =\"%s\"",
				SESS_NOT_FOUND_ACTION_PARAM,
				SESS_NOT_FOUND_ACTION_FAIL_STR,
				lttng_live->params.url->str);
			goto error;
		case SESSION_NOT_FOUND_ACTION_END:
			BT_COMP_LOGI("Unable to connect to the requested live viewer session. End gracefully at the first _next() "
				"call because of %s=\"%s\" component parameter: "
				"url=\"%s\"", SESS_NOT_FOUND_ACTION_PARAM,
				SESS_NOT_FOUND_ACTION_END_STR,
				lttng_live->params.url->str);
			break;
		default:
			bt_common_abort();
		}
	}

	bt_self_message_iterator_set_data(self_msg_it, lttng_live_msg_iter);
	status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
	lttng_live_msg_iter_destroy(lttng_live_msg_iter);
end:
	return status;
}

static struct bt_param_validation_map_value_entry_descr list_sessions_params[] = {
	{ URL_PARAM, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_STRING } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

static
bt_component_class_query_method_status lttng_live_query_list_sessions(
		const bt_value *params, const bt_value **result,
		bt_self_component_class *self_comp_class,
		bt_logging_level log_level)
{
	bt_component_class_query_method_status status;
	const bt_value *url_value = NULL;
	const char *url;
	struct live_viewer_connection *viewer_connection = NULL;
	enum lttng_live_viewer_status viewer_status;
	enum bt_param_validation_status validation_status;
	gchar *validate_error = NULL;

	validation_status = bt_param_validation_validate(params,
		list_sessions_params, &validate_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class, "%s",
			validate_error);
		goto error;
	}

	url_value = bt_value_map_borrow_entry_value_const(params, URL_PARAM);
	url = bt_value_string_get(url_value);

	viewer_status = live_viewer_connection_create(NULL, self_comp_class,
		log_level, url, true, NULL, &viewer_connection);
	if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
		if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
			BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
				"Failed to create viewer connection");
			status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		} else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
			status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN;
		} else {
			bt_common_abort();
		}
		goto error;
	}

	status = live_viewer_connection_list_sessions(viewer_connection,
		result);
	if (status != BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Failed to list viewer sessions");
		goto error;
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(*result);

	if (status >= 0) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
	}

end:
	if (viewer_connection) {
		live_viewer_connection_destroy(viewer_connection);
	}

	g_free(validate_error);

	return status;
}

static
bt_component_class_query_method_status lttng_live_query_support_info(
		const bt_value *params, const bt_value **result,
		bt_self_component_class *self_comp_class,
		bt_logging_level log_level)
{
	bt_component_class_query_method_status status =
		BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
	const bt_value *input_type_value;
	const bt_value *input_value;
	double weight = 0;
	struct bt_common_lttng_live_url_parts parts = { 0 };

	/* Used by the logging macros */
	__attribute__((unused)) bt_self_component *self_comp = NULL;

	*result = NULL;
	input_type_value = bt_value_map_borrow_entry_value_const(params,
		"type");
	if (!input_type_value) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Missing expected `type` parameter.");
		goto error;
	}

	if (!bt_value_is_string(input_type_value)) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"`type` parameter is not a string value.");
		goto error;
	}

	if (strcmp(bt_value_string_get(input_type_value), "string") != 0) {
		/* We don't handle file system paths */
		goto create_result;
	}

	input_value = bt_value_map_borrow_entry_value_const(params, "input");
	if (!input_value) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Missing expected `input` parameter.");
		goto error;
	}

	if (!bt_value_is_string(input_value)) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"`input` parameter is not a string value.");
		goto error;
	}

	parts = bt_common_parse_lttng_live_url(bt_value_string_get(input_value),
		NULL, 0);
	if (parts.session_name) {
		/*
		 * Looks pretty much like an LTTng live URL: we got the
		 * session name part, which forms a complete URL.
		 */
		weight = .75;
	}

create_result:
	*result = bt_value_real_create_init(weight);
	if (!*result) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	goto end;

error:
	if (status >= 0) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
	}

	BT_ASSERT(!*result);

end:
	bt_common_destroy_lttng_live_url_parts(&parts);
	return status;
}

BT_HIDDEN
bt_component_class_query_method_status lttng_live_query(
		bt_self_component_class_source *comp_class,
		bt_private_query_executor *priv_query_exec,
		const char *object, const bt_value *params,
		__attribute__((unused)) void *method_data,
		const bt_value **result)
{
	bt_component_class_query_method_status status =
		BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
	bt_self_component *self_comp = NULL;
	bt_self_component_class *self_comp_class =
		bt_self_component_class_source_as_self_component_class(comp_class);
	bt_logging_level log_level = bt_query_executor_get_logging_level(
		bt_private_query_executor_as_query_executor_const(
			priv_query_exec));

	if (strcmp(object, "sessions") == 0) {
		status = lttng_live_query_list_sessions(params, result,
			self_comp_class, log_level);
	} else if (strcmp(object, "babeltrace.support-info") == 0) {
		status = lttng_live_query_support_info(params, result,
			self_comp_class, log_level);
	} else {
		BT_COMP_LOGI("Unknown query object `%s`", object);
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT;
		goto end;
	}

end:
	return status;
}

static
void lttng_live_component_destroy_data(struct lttng_live_component *lttng_live)
{
	if (!lttng_live) {
		return;
	}
	if (lttng_live->params.url) {
		g_string_free(lttng_live->params.url, TRUE);
	}
	g_free(lttng_live);
}

BT_HIDDEN
void lttng_live_component_finalize(bt_self_component_source *component)
{
	void *data = bt_self_component_get_data(
		bt_self_component_source_as_self_component(component));

	if (!data) {
		return;
	}
	lttng_live_component_destroy_data(data);
}

static
enum session_not_found_action parse_session_not_found_action_param(
	const bt_value *no_session_param)
{
	enum session_not_found_action action;
	const char *no_session_act_str = bt_value_string_get(no_session_param);

	if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_CONTINUE_STR) == 0) {
		action = SESSION_NOT_FOUND_ACTION_CONTINUE;
	} else if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_FAIL_STR) == 0) {
		action = SESSION_NOT_FOUND_ACTION_FAIL;
	} else {
		BT_ASSERT(strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_END_STR) == 0);
		action = SESSION_NOT_FOUND_ACTION_END;
	}

	return action;
}

static struct bt_param_validation_value_descr inputs_elem_descr = {
	.type = BT_VALUE_TYPE_STRING,
};

static const char *sess_not_found_action_choices[] = {
	SESS_NOT_FOUND_ACTION_CONTINUE_STR,
	SESS_NOT_FOUND_ACTION_FAIL_STR,
	SESS_NOT_FOUND_ACTION_END_STR,
};

static struct bt_param_validation_map_value_entry_descr params_descr[] = {
	{ INPUTS_PARAM, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
		.min_length = 1,
		.max_length = 1,
		.element_type = &inputs_elem_descr,
	} } },
	{ SESS_NOT_FOUND_ACTION_PARAM, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { BT_VALUE_TYPE_STRING, .string = {
		.choices = sess_not_found_action_choices,
	} } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

static
bt_component_class_initialize_method_status lttng_live_component_create(
		const bt_value *params,
		bt_logging_level log_level,
		bt_self_component *self_comp,
		struct lttng_live_component **component)
{
	struct lttng_live_component *lttng_live = NULL;
	const bt_value *inputs_value;
	const bt_value *url_value;
	const bt_value *value;
	const char *url;
	enum bt_param_validation_status validation_status;
	gchar *validation_error = NULL;
	bt_component_class_initialize_method_status status;

	validation_status = bt_param_validation_validate(params, params_descr,
		&validation_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp, "%s", validation_error);
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto error;
	}

	lttng_live = g_new0(struct lttng_live_component, 1);
	if (!lttng_live) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	}
	lttng_live->log_level = log_level;
	lttng_live->self_comp = self_comp;
	lttng_live->max_query_size = MAX_QUERY_SIZE;
	lttng_live->has_msg_iter = false;

	inputs_value =
		bt_value_map_borrow_entry_value_const(params, INPUTS_PARAM);
	url_value =
		bt_value_array_borrow_element_by_index_const(inputs_value, 0);
	url = bt_value_string_get(url_value);

	lttng_live->params.url = g_string_new(url);
	if (!lttng_live->params.url) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	value = bt_value_map_borrow_entry_value_const(params,
		SESS_NOT_FOUND_ACTION_PARAM);
	if (value) {
		lttng_live->params.sess_not_found_act =
			parse_session_not_found_action_param(value);
	} else {
		BT_COMP_LOGI("Optional `%s` parameter is missing: "
			"defaulting to `%s`.",
			SESS_NOT_FOUND_ACTION_PARAM,
			SESS_NOT_FOUND_ACTION_CONTINUE_STR);
		lttng_live->params.sess_not_found_act =
			SESSION_NOT_FOUND_ACTION_CONTINUE;
	}

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	lttng_live_component_destroy_data(lttng_live);
	lttng_live = NULL;
end:
	g_free(validation_error);

	*component = lttng_live;
	return status;
}

BT_HIDDEN
bt_component_class_initialize_method_status lttng_live_component_init(
		bt_self_component_source *self_comp_src,
		bt_self_component_source_configuration *config,
		const bt_value *params,
		__attribute__((unused)) void *init_method_data)
{
	struct lttng_live_component *lttng_live;
	bt_component_class_initialize_method_status ret;
	bt_self_component *self_comp =
		bt_self_component_source_as_self_component(self_comp_src);
	bt_logging_level log_level = bt_component_get_logging_level(
		bt_self_component_as_component(self_comp));
	bt_self_component_add_port_status add_port_status;

	ret = lttng_live_component_create(params, log_level, self_comp, &lttng_live);
	if (ret != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
		goto error;
	}

	add_port_status = bt_self_component_source_add_output_port(
		self_comp_src, "out", NULL, NULL);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		ret = (int) add_port_status;
		goto end;
	}

	bt_self_component_set_data(self_comp, lttng_live);
	goto end;

error:
	lttng_live_component_destroy_data(lttng_live);
	lttng_live = NULL;
end:
	return ret;
}
