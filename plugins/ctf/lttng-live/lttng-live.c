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

#define BT_LOG_TAG "PLUGIN-CTF-LTTNG-LIVE-SRC"
#include "logging.h"

#include <glib.h>
#include <inttypes.h>
#include <unistd.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <plugins-common.h>

#include "data-stream.h"
#include "metadata.h"
#include "lttng-live.h"

#define MAX_QUERY_SIZE			    (256*1024)
#define URL_PARAM			    "url"
#define SESS_NOT_FOUND_ACTION_PARAM	    "session-not-found-action"
#define SESS_NOT_FOUND_ACTION_CONTINUE_STR  "continue"
#define SESS_NOT_FOUND_ACTION_FAIL_STR	    "fail"
#define SESS_NOT_FOUND_ACTION_END_STR	    "end"

#define print_dbg(fmt, ...)	BT_LOGD(fmt, ## __VA_ARGS__)

static
const char *print_live_iterator_status(enum lttng_live_iterator_status status)
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
		abort();
	}
}

static
const char *print_state(struct lttng_live_stream_iterator *s)
{
	switch (s->state) {
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

#define print_stream_state(live_stream_iter) \
	do { \
		BT_LOGD("stream state %s last_inact_ts %" PRId64  \
			", curr_inact_ts %" PRId64, \
			print_state(live_stream_iter), \
			live_stream_iter->last_inactivity_ts, \
			live_stream_iter->current_inactivity_ts); \
	} while (0);

BT_HIDDEN
bool lttng_live_is_canceled(struct lttng_live_component *lttng_live)
{
	const bt_component *component;
	bool ret;

	if (!lttng_live) {
		ret = false;
		goto end;
	}

	component = bt_component_source_as_component_const(
		bt_self_component_source_as_component_source(
		lttng_live->self_comp));

	ret = bt_component_graph_is_canceled(component);

end:
	return ret;
}

static
struct lttng_live_trace *lttng_live_find_trace(struct lttng_live_session *session,
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
	BT_LOGD("Destroy lttng_live_trace");

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

	trace = g_new0(struct lttng_live_trace, 1);
	if (!trace) {
		goto error;
	}
	trace->session = session;
	trace->id = trace_id;
	trace->trace_class = NULL;
	trace->trace = NULL;
	trace->stream_iterators = g_ptr_array_new_with_free_func(
			(GDestroyNotify) lttng_live_stream_iterator_destroy);
	BT_ASSERT(trace->stream_iterators);
	trace->new_metadata_needed = true;
	g_ptr_array_add(session->traces, trace);

	BT_LOGI("Create trace");
	goto end;
error:
	g_free(trace);
	trace = NULL;
end:
	return trace;
}

BT_HIDDEN
struct lttng_live_trace *lttng_live_borrow_trace(
		struct lttng_live_session *session, uint64_t trace_id)
{
	struct lttng_live_trace *trace;

	trace = lttng_live_find_trace(session, trace_id);
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

	session = g_new0(struct lttng_live_session, 1);
	if (!session) {
		goto error;
	}

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

	BT_LOGI("Reading from session: %" PRIu64 " hostname: %s session_name: %s",
		session->id, hostname, session_name);
	g_ptr_array_add(lttng_live_msg_iter->sessions, session);
	goto end;
error:
	BT_LOGE("Error adding session");
	g_free(session);
	ret = -1;
end:
	return ret;
}

static
void lttng_live_destroy_session(struct lttng_live_session *session)
{
	struct lttng_live_component *live_comp;

	if (!session) {
		goto end;
	}

	BT_LOGD("Destroy lttng live session");
	if (session->id != -1ULL) {
		if (lttng_live_detach_session(session)) {
			live_comp = session->lttng_live_msg_iter->lttng_live_comp;
			if (session->lttng_live_msg_iter &&
					!lttng_live_is_canceled(live_comp)) {
				/* Old relayd cannot detach sessions. */
				BT_LOGD("Unable to detach lttng live session %" PRIu64,
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

	BT_OBJECT_PUT_REF_AND_RESET(lttng_live_msg_iter->viewer_connection);
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
	switch (lttng_live_stream->state) {
	case LTTNG_LIVE_STREAM_QUIESCENT:
	case LTTNG_LIVE_STREAM_ACTIVE_DATA:
		break;
	case LTTNG_LIVE_STREAM_ACTIVE_NO_DATA:
		/* Invalid state. */
		BT_LOGF("Unexpected stream state \"ACTIVE_NO_DATA\"");
		abort();
	case LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA:
		/* Invalid state. */
		BT_LOGF("Unexpected stream state \"QUIESCENT_NO_DATA\"");
		abort();
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
	enum lttng_live_iterator_status ret =
			LTTNG_LIVE_ITERATOR_STATUS_OK;
	struct packet_index index;
	enum lttng_live_stream_state orig_state = lttng_live_stream->state;

	if (lttng_live_stream->trace->new_metadata_needed) {
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
	BT_ASSERT(lttng_live_stream->state != LTTNG_LIVE_STREAM_EOF);
	if (lttng_live_stream->state == LTTNG_LIVE_STREAM_QUIESCENT) {
		uint64_t last_inact_ts = lttng_live_stream->last_inactivity_ts,
			 curr_inact_ts = lttng_live_stream->current_inactivity_ts;

		if (orig_state == LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA &&
				last_inact_ts == curr_inact_ts) {
			ret = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			print_stream_state(lttng_live_stream);
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
	enum lttng_live_iterator_status status;
	uint64_t trace_idx;
	int ret = 0;

	if (!session->attached) {
		ret = lttng_live_attach_session(session);
		if (ret) {
			if (lttng_live_msg_iter && lttng_live_is_canceled(
						lttng_live_msg_iter->lttng_live_comp)) {
				status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			} else {
				status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
			}
			goto end;
		}
	}

	status = lttng_live_get_new_streams(session);
	if (status != LTTNG_LIVE_ITERATOR_STATUS_OK &&
			status != LTTNG_LIVE_ITERATOR_STATUS_END) {
		goto end;
	}
	for (trace_idx = 0; trace_idx < session->traces->len; trace_idx++) {
		struct lttng_live_trace *trace =
			g_ptr_array_index(session->traces, trace_idx);

		status = lttng_live_metadata_update(trace);
		if (status != LTTNG_LIVE_ITERATOR_STATUS_OK &&
				status != LTTNG_LIVE_ITERATOR_STATUS_END) {
			goto end;
		}
	}
	status = lttng_live_lazy_msg_init(session);

end:
	return status;
}

BT_HIDDEN
void lttng_live_need_new_streams(struct lttng_live_msg_iter *lttng_live_msg_iter)
{
	uint64_t session_idx;

	for (session_idx = 0; session_idx < lttng_live_msg_iter->sessions->len;
			session_idx++) {
		struct lttng_live_session *session =
			g_ptr_array_index(lttng_live_msg_iter->sessions, session_idx);
		session->new_streams_needed = true;
	}
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
			trace->new_metadata_needed = true;
		}
	}
}

static
enum lttng_live_iterator_status
lttng_live_iterator_handle_new_streams_and_metadata(
		struct lttng_live_msg_iter *lttng_live_msg_iter)
{
	enum lttng_live_iterator_status ret =
			LTTNG_LIVE_ITERATOR_STATUS_OK;
	uint64_t session_idx = 0, nr_sessions_opened = 0;
	struct lttng_live_session *session;
	enum session_not_found_action sess_not_found_act =
		lttng_live_msg_iter->lttng_live_comp->params.sess_not_found_act;

	/*
	 * In a remotely distant future, we could add a "new
	 * session" flag to the protocol, which would tell us that we
	 * need to query for new sessions even though we have sessions
	 * currently ongoing.
	 */
	if (lttng_live_msg_iter->sessions->len == 0) {
		if (sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE) {
			ret = LTTNG_LIVE_ITERATOR_STATUS_END;
			goto end;
		} else {
			/*
			 * Retry to create a viewer session for the requested
			 * session name.
			 */
			if (lttng_live_create_viewer_session(lttng_live_msg_iter)) {
				ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
				goto end;
			}
		}
	}

	for (session_idx = 0; session_idx < lttng_live_msg_iter->sessions->len;
			session_idx++) {
		session = g_ptr_array_index(lttng_live_msg_iter->sessions,
				session_idx);
		ret = lttng_live_get_session(lttng_live_msg_iter, session);
		switch (ret) {
		case LTTNG_LIVE_ITERATOR_STATUS_OK:
			break;
		case LTTNG_LIVE_ITERATOR_STATUS_END:
			ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
			break;
		default:
			goto end;
		}
		if (!session->closed) {
			nr_sessions_opened++;
		}
	}
end:
	if (ret == LTTNG_LIVE_ITERATOR_STATUS_OK &&
			sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE &&
			nr_sessions_opened == 0) {
		ret = LTTNG_LIVE_ITERATOR_STATUS_END;
	}
	return ret;
}

static
enum lttng_live_iterator_status emit_inactivity_message(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream_iter,
		bt_message **message, uint64_t timestamp)
{
	enum lttng_live_iterator_status ret =
			LTTNG_LIVE_ITERATOR_STATUS_OK;
	bt_message *msg = NULL;

	BT_ASSERT(stream_iter->trace->clock_class);

	msg = bt_message_message_iterator_inactivity_create(
			lttng_live_msg_iter->self_msg_iter,
			stream_iter->trace->clock_class,
			timestamp);
	if (!msg) {
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
		bt_message **message)
{
	enum lttng_live_iterator_status ret =
			LTTNG_LIVE_ITERATOR_STATUS_OK;

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
	bt_clock_snapshot_state cs_state = BT_CLOCK_SNAPSHOT_STATE_KNOWN;
	bt_message_stream_activity_clock_snapshot_state sa_cs_state;

	BT_ASSERT(msg);
	BT_ASSERT(ts_ns);

	BT_LOGV("Getting message's timestamp: iter-data-addr=%p, msg-addr=%p, "
		"last-msg-ts=%" PRId64, lttng_live_msg_iter, msg,
		last_msg_ts_ns);

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		clock_class =
			bt_message_event_borrow_stream_class_default_clock_class_const(
				msg);
		BT_ASSERT(clock_class);

		cs_state = bt_message_event_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		clock_class =
			bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		cs_state = bt_message_packet_beginning_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		clock_class =
			bt_message_packet_end_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		cs_state = bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		clock_class =
			bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		cs_state = bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		clock_class =
			bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		cs_state = bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
		clock_class =
			bt_message_stream_activity_beginning_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		sa_cs_state = bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		if (sa_cs_state != BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
		clock_class =
			bt_message_stream_activity_end_borrow_stream_class_default_clock_class_const(
			msg);
		BT_ASSERT(clock_class);

		sa_cs_state = bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		if (sa_cs_state != BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		cs_state =
			bt_message_message_iterator_inactivity_borrow_default_clock_snapshot_const(
				msg, &clock_snapshot);
		break;
	default:
		/* All the other messages have a higher priority */
		BT_LOGV_STR("Message has no timestamp: using the last message timestamp.");
		*ts_ns = last_msg_ts_ns;
		goto end;
	}

	if (cs_state != BT_CLOCK_SNAPSHOT_STATE_KNOWN) {
		BT_LOGE_STR("Unsupported unknown clock snapshot.");
		ret = -1;
		goto end;
	}

	clock_class = bt_clock_snapshot_borrow_clock_class_const(clock_snapshot);
	BT_ASSERT(clock_class);

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot, ts_ns);
	if (ret) {
		BT_LOGE("Cannot get nanoseconds from Epoch of clock snapshot: "
			"clock-snapshot-addr=%p", clock_snapshot);
		goto error;
	}

	goto end;

no_clock_snapshot:
	BT_LOGV_STR("Message's default clock snapshot is missing: "
		"using the last message timestamp.");
	*ts_ns = last_msg_ts_ns;
	goto end;

error:
	ret = -1;

end:
	if (ret == 0) {
		BT_LOGV("Found message's timestamp: "
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
		bt_message **message)
{
	enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
	enum bt_msg_iter_status status;
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
			if (trace->new_metadata_needed) {
				ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
				goto end;
			}
		}
	}

	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_DATA) {
		ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	status = bt_msg_iter_get_next_message(lttng_live_stream->msg_iter,
		lttng_live_msg_iter->self_msg_iter, message);
	switch (status) {
	case BT_MSG_ITER_STATUS_EOF:
		ret = LTTNG_LIVE_ITERATOR_STATUS_END;
		break;
	case BT_MSG_ITER_STATUS_OK:
		ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
		break;
	case BT_MSG_ITER_STATUS_AGAIN:
		/*
		 * Continue immediately (end of packet). The next
		 * get_index may return AGAIN to delay the following
		 * attempt.
		 */
		ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		break;
	case BT_MSG_ITER_STATUS_INVAL:
		/* No argument provided by the user, so don't return INVAL. */
	case BT_MSG_ITER_STATUS_ERROR:
	default:
		ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		BT_LOGW("CTF msg iterator return an error or failed msg_iter=%p",
			lttng_live_stream->msg_iter);
		break;
	}

end:
	return ret;
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
enum lttng_live_iterator_status lttng_live_iterator_next_on_stream(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream_iter,
		bt_message **curr_msg)
{
	enum lttng_live_iterator_status live_status;

retry:
	print_stream_state(stream_iter);
	live_status = lttng_live_iterator_handle_new_streams_and_metadata(
			lttng_live_msg_iter);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	live_status = lttng_live_iterator_next_handle_one_no_data_stream(
			lttng_live_msg_iter, stream_iter);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	live_status = lttng_live_iterator_next_handle_one_quiescent_stream(
			lttng_live_msg_iter, stream_iter, curr_msg);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		BT_ASSERT(*curr_msg == NULL);
		goto end;
	}
	if (*curr_msg) {
		goto end;
	}
	live_status = lttng_live_iterator_next_handle_one_active_data_stream(
			lttng_live_msg_iter, stream_iter, curr_msg);
	if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
		BT_ASSERT(*curr_msg == NULL);
	}

end:
	if (live_status == LTTNG_LIVE_ITERATOR_STATUS_CONTINUE) {
		goto retry;
	}

	return live_status;
}

static
enum lttng_live_iterator_status next_stream_iterator_for_trace(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_trace *live_trace,
		struct lttng_live_stream_iterator **candidate_stream_iter)
{
	struct lttng_live_stream_iterator *curr_candidate_stream_iter = NULL;
	enum lttng_live_iterator_status stream_iter_status;;
	int64_t curr_candidate_msg_ts = INT64_MAX;
	uint64_t stream_iter_idx;

	BT_ASSERT(live_trace);
	BT_ASSERT(live_trace->stream_iterators);
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
			bt_message *msg = NULL;
			int64_t curr_msg_ts_ns = INT64_MAX;
			stream_iter_status = lttng_live_iterator_next_on_stream(
					lttng_live_msg_iter, stream_iter, &msg);

			BT_LOGD("live stream iterator returned status :%s",
					print_live_iterator_status(stream_iter_status));
			if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
				stream_iter_is_ended = true;
				break;
			}

			if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
				goto end;
			}

			BT_ASSERT(msg);

			/*
			 * Get the timestamp in nanoseconds from origin of this
			 * messsage.
			 */
			live_get_msg_ts_ns(stream_iter, lttng_live_msg_iter,
				msg, lttng_live_msg_iter->last_msg_ts_ns,
				&curr_msg_ts_ns);

			/*
			 * Check if the message of the current live stream
			 * iterator occured at the exact same time or after the
			 * last message returned by this component's message
			 * iterator. If not, we return an error.
			 */
			if (curr_msg_ts_ns >= lttng_live_msg_iter->last_msg_ts_ns) {
				stream_iter->current_msg = msg;
				stream_iter->current_msg_ts_ns = curr_msg_ts_ns;
			} else {
				/*
				 * We received a message in the past. To ensure
				 * monotonicity, we can't send it forward.
				 */
				BT_LOGE("Message's timestamp is less than "
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

		if (!stream_iter_is_ended &&
				stream_iter->current_msg_ts_ns <= curr_candidate_msg_ts) {
			/*
			 * Update the current best candidate message for the
			 * stream iterator of thise live trace to be forwarded
			 * downstream.
			 */
			curr_candidate_msg_ts = stream_iter->current_msg_ts_ns;
			curr_candidate_stream_iter = stream_iter;
		}

		if (stream_iter_is_ended) {
			/*
			 * The live stream iterator is ENDed. We remove that
			 * iterator from the list and we restart the iteration
			 * at the beginning of the live stream iterator array
			 * to because the removal will shuffle the array.
			 */
			g_ptr_array_remove_index_fast(live_trace->stream_iterators,
				stream_iter_idx);
			stream_iter_idx = 0;
		} else {
			stream_iter_idx++;
		}
	}

	if (curr_candidate_stream_iter) {
		*candidate_stream_iter = curr_candidate_stream_iter;
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
		struct lttng_live_stream_iterator **candidate_session_stream_iter)
{
	enum lttng_live_iterator_status stream_iter_status;
	uint64_t trace_idx = 0;
	int64_t curr_candidate_msg_ts = INT64_MAX;
	struct lttng_live_stream_iterator *curr_candidate_stream_iter = NULL;

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

	BT_ASSERT(session->traces);

	/*
	 * Use while loops here rather then for loops so we can restart the
	 * iteration if an element is removed from the array during the
	 * looping.
	 */
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
			BT_ASSERT(stream_iter);

			if (stream_iter->current_msg_ts_ns <= curr_candidate_msg_ts) {
				curr_candidate_msg_ts = stream_iter->current_msg_ts_ns;
				curr_candidate_stream_iter = stream_iter;
			}
			trace_idx++;
		} else {
			g_ptr_array_remove_index_fast(session->traces, trace_idx);
			trace_idx = 0;
		}
	}
	if (curr_candidate_stream_iter) {
		*candidate_session_stream_iter = curr_candidate_stream_iter;
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
bt_self_message_iterator_status lttng_live_msg_iter_next(
		bt_self_message_iterator *self_msg_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_self_message_iterator_status status;
	struct lttng_live_msg_iter *lttng_live_msg_iter =
		bt_self_message_iterator_get_data(self_msg_it);
	struct lttng_live_component *lttng_live =
		lttng_live_msg_iter->lttng_live_comp;
	enum lttng_live_iterator_status stream_iter_status;
	uint64_t session_idx;

	*count = 0;

	BT_ASSERT(lttng_live_msg_iter);

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
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
			goto no_session;
		} else {
			/*
			 * The are no more active session for this session
			 * name. Retry to create a viewer session for the
			 * requested session name.
			 */
			if (lttng_live_create_viewer_session(lttng_live_msg_iter)) {
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
				goto no_session;
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
		struct lttng_live_stream_iterator *next_stream_iter = NULL,
						  *candidate_stream_iter = NULL;
		int64_t next_msg_ts_ns = INT64_MAX;

		BT_ASSERT(lttng_live_msg_iter->sessions);
		session_idx = 0;
		/*
		 * Use a while loop instead of a for loop so we can restart the
		 * iteration if we remove an element. We can safely call
		 * next_stream_iterator_for_session() multiple times on the
		 * same session as we only fetch a new message if there is no
		 * current next message for each live stream iterator.
		 * If all live stream iterator of that session already have a
		 * current next message, the function will simply exit return
		 * the same candidate live stream iterator every time.
		 */
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
					 * Remove the session from the list and restart the
					 * iteration at the beginning of the array since the
					 * removal shuffle the elements of the array.
					 */
					g_ptr_array_remove_index_fast(
						lttng_live_msg_iter->sessions,
						session_idx);
					session_idx = 0;
				} else {
					session_idx++;
				}
				continue;
			}

			if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
				goto end;
			}

			if (candidate_stream_iter->current_msg_ts_ns <= next_msg_ts_ns) {
				next_msg_ts_ns = candidate_stream_iter->current_msg_ts_ns;
				next_stream_iter = candidate_stream_iter;
			}

			session_idx++;
		}

		if (!next_stream_iter) {
			stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			goto end;
		}

		BT_ASSERT(next_stream_iter->current_msg);
		/* Ensure monotonicity. */
		BT_ASSERT(lttng_live_msg_iter->last_msg_ts_ns <=
			next_stream_iter->current_msg_ts_ns);

		/*
		 * Insert the next message to the message batch. This will set
		 * stream iterator current messsage to NULL so that next time
		 * we fetch the next message of that stream iterator
		 */
		BT_MESSAGE_MOVE_REF(msgs[*count], next_stream_iter->current_msg);
		(*count)++;

		/* Update the last timestamp in nanoseconds sent downstream. */
		lttng_live_msg_iter->last_msg_ts_ns = next_msg_ts_ns;
		next_stream_iter->current_msg_ts_ns = INT64_MAX;

		stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_OK;
	}
end:
	switch (stream_iter_status) {
	case LTTNG_LIVE_ITERATOR_STATUS_OK:
	case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
		if (*count > 0) {
			/*
			 * We received a again status but we have some messages
			 * to send downstream. We send them and return OK for
			 * now. On the next call we return again if there are
			 * still no new message to send.
			 */
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
		} else {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_AGAIN;
		}
		break;
	case LTTNG_LIVE_ITERATOR_STATUS_END:
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
		break;
	case LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		break;
	case LTTNG_LIVE_ITERATOR_STATUS_ERROR:
	case LTTNG_LIVE_ITERATOR_STATUS_INVAL:
	case LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		/* Put all existing messages on error. */
		put_messages(msgs, *count);
		break;
	default:
		abort();
	}

no_session:
	return status;
}

BT_HIDDEN
bt_self_message_iterator_status lttng_live_msg_iter_init(
		bt_self_message_iterator *self_msg_it,
		bt_self_component_source *self_comp_src,
		bt_self_component_port_output *self_port)
{
	bt_self_message_iterator_status ret =
			BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	bt_self_component *self_comp =
			bt_self_component_source_as_self_component(self_comp_src);
	struct lttng_live_component *lttng_live;
	struct lttng_live_msg_iter *lttng_live_msg_iter;

	BT_ASSERT(self_msg_it);

	lttng_live = bt_self_component_get_data(self_comp);

	/* There can be only one downstream iterator at the same time. */
	BT_ASSERT(!lttng_live->has_msg_iter);
	lttng_live->has_msg_iter = true;

	lttng_live_msg_iter = g_new0(struct lttng_live_msg_iter, 1);
	if (!lttng_live_msg_iter) {
		ret = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	lttng_live_msg_iter->lttng_live_comp = lttng_live;
	lttng_live_msg_iter->self_msg_iter = self_msg_it;

	lttng_live_msg_iter->active_stream_iter = 0;
	lttng_live_msg_iter->last_msg_ts_ns = INT64_MIN;
	lttng_live_msg_iter->sessions = g_ptr_array_new_with_free_func(
		(GDestroyNotify) lttng_live_destroy_session);
	BT_ASSERT(lttng_live_msg_iter->sessions);

	lttng_live_msg_iter->viewer_connection =
		live_viewer_connection_create(lttng_live->params.url->str, false,
			lttng_live_msg_iter);
	if (!lttng_live_msg_iter->viewer_connection) {
		goto error;
	}

	if (lttng_live_create_viewer_session(lttng_live_msg_iter)) {
		goto error;
	}
	if (lttng_live_msg_iter->sessions->len == 0) {
		switch (lttng_live->params.sess_not_found_act) {
		case SESSION_NOT_FOUND_ACTION_CONTINUE:
			BT_LOGI("Unable to connect to the requested live viewer "
				"session. Keep trying to connect because of "
				"%s=\"%s\" component parameter: url=\"%s\"",
				SESS_NOT_FOUND_ACTION_PARAM,
				SESS_NOT_FOUND_ACTION_CONTINUE_STR,
				lttng_live->params.url->str);
			break;
		case SESSION_NOT_FOUND_ACTION_FAIL:
			BT_LOGE("Unable to connect to the requested live viewer "
				"session. Fail the message iterator"
				"initialization because of %s=\"%s\" "
				"component parameter: url =\"%s\"",
				SESS_NOT_FOUND_ACTION_PARAM,
				SESS_NOT_FOUND_ACTION_FAIL_STR,
				lttng_live->params.url->str);
			goto error;
		case SESSION_NOT_FOUND_ACTION_END:
			BT_LOGI("Unable to connect to the requested live viewer "
				"session. End gracefully at the first _next() "
				"call because of %s=\"%s\" component parameter: "
				"url=\"%s\"", SESS_NOT_FOUND_ACTION_PARAM,
				SESS_NOT_FOUND_ACTION_END_STR,
				lttng_live->params.url->str);
			break;
		}
	}

	bt_self_message_iterator_set_data(self_msg_it, lttng_live_msg_iter);

	goto end;
error:
	ret = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
	lttng_live_msg_iter_destroy(lttng_live_msg_iter);
end:
	return ret;
}

static
bt_query_status lttng_live_query_list_sessions(const bt_value *params,
		const bt_value **result)
{
	bt_query_status status = BT_QUERY_STATUS_OK;
	const bt_value *url_value = NULL;
	const char *url;
	struct live_viewer_connection *viewer_connection = NULL;

	url_value = bt_value_map_borrow_entry_value_const(params, URL_PARAM);
	if (!url_value) {
		BT_LOGW("Mandatory `%s` parameter missing", URL_PARAM);
		status = BT_QUERY_STATUS_INVALID_PARAMS;
		goto error;
	}

	if (!bt_value_is_string(url_value)) {
		BT_LOGW("`%s` parameter is required to be a string value",
			URL_PARAM);
		status = BT_QUERY_STATUS_INVALID_PARAMS;
		goto error;
	}

	url = bt_value_string_get(url_value);

	viewer_connection = live_viewer_connection_create(url, true, NULL);
	if (!viewer_connection) {
		goto error;
	}

	status = live_viewer_connection_list_sessions(viewer_connection,
			result);
	if (status != BT_QUERY_STATUS_OK) {
		goto error;
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(*result);

	if (status >= 0) {
		status = BT_QUERY_STATUS_ERROR;
	}

end:
	if (viewer_connection) {
		live_viewer_connection_destroy(viewer_connection);
	}
	return status;
}

BT_HIDDEN
bt_query_status lttng_live_query(bt_self_component_class_source *comp_class,
		const bt_query_executor *query_exec,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	bt_query_status status = BT_QUERY_STATUS_OK;

	if (strcmp(object, "sessions") == 0) {
		status = lttng_live_query_list_sessions(params, result);
	} else {
		BT_LOGW("Unknown query object `%s`", object);
		status = BT_QUERY_STATUS_INVALID_OBJECT;
		goto end;
	}

end:
	return status;
}

static
void lttng_live_component_destroy_data(struct lttng_live_component *lttng_live)
{
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
	const char *no_session_act_str;
	no_session_act_str = bt_value_string_get(no_session_param);
	if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_CONTINUE_STR) == 0) {
		action = SESSION_NOT_FOUND_ACTION_CONTINUE;
	} else if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_FAIL_STR) == 0) {
		action = SESSION_NOT_FOUND_ACTION_FAIL;
	} else if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_END_STR) == 0) {
		action = SESSION_NOT_FOUND_ACTION_END;
	} else {
		action = -1;
	}

	return action;
}

struct lttng_live_component *lttng_live_component_create(const bt_value *params)
{
	struct lttng_live_component *lttng_live;
	const bt_value *value = NULL;
	const char *url;

	lttng_live = g_new0(struct lttng_live_component, 1);
	if (!lttng_live) {
		goto end;
	}
	lttng_live->max_query_size = MAX_QUERY_SIZE;
	lttng_live->has_msg_iter = false;

	value = bt_value_map_borrow_entry_value_const(params, URL_PARAM);
	if (!value || !bt_value_is_string(value)) {
		BT_LOGW("Mandatory `%s` parameter missing or not a string",
			URL_PARAM);
		goto error;
	}
	url = bt_value_string_get(value);
	lttng_live->params.url = g_string_new(url);
	if (!lttng_live->params.url) {
		goto error;
	}

	value = bt_value_map_borrow_entry_value_const(params,
		SESS_NOT_FOUND_ACTION_PARAM);

	if (value && bt_value_is_string(value)) {
		lttng_live->params.sess_not_found_act =
			parse_session_not_found_action_param(value);
		if (lttng_live->params.sess_not_found_act == -1) {
			BT_LOGE("Unexpected value for `%s` parameter: "
				"value=\"%s\"", SESS_NOT_FOUND_ACTION_PARAM,
				bt_value_string_get(value));
			goto error;
		}
	} else {
		BT_LOGW("Optional `%s` parameter is missing or "
			"not a string value. Defaulting to %s=\"%s\".",
			SESS_NOT_FOUND_ACTION_PARAM,
			SESS_NOT_FOUND_ACTION_PARAM,
			SESS_NOT_FOUND_ACTION_CONTINUE_STR);
		lttng_live->params.sess_not_found_act =
			SESSION_NOT_FOUND_ACTION_CONTINUE;
	}

	goto end;

error:
	lttng_live_component_destroy_data(lttng_live);
	lttng_live = NULL;
end:
	return lttng_live;
}

BT_HIDDEN
bt_self_component_status lttng_live_component_init(
		bt_self_component_source *self_comp,
		const bt_value *params, UNUSED_VAR void *init_method_data)
{
	struct lttng_live_component *lttng_live;
	bt_self_component_status ret = BT_SELF_COMPONENT_STATUS_OK;

	lttng_live = lttng_live_component_create(params);
	if (!lttng_live) {
		ret = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}
	lttng_live->self_comp = self_comp;

	if (lttng_live_is_canceled(lttng_live)) {
		goto end;
	}

	ret = bt_self_component_source_add_output_port(
				lttng_live->self_comp, "out",
				NULL, NULL);
	if (ret != BT_SELF_COMPONENT_STATUS_OK) {
		goto end;
	}

	bt_self_component_set_data(
			bt_self_component_source_as_self_component(self_comp),
			lttng_live);

end:
	return ret;
}
