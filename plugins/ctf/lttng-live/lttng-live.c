/*
 * lttng-live.c
 *
 * Babeltrace CTF LTTng-live Client Component
 *
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <inttypes.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>
#include <unistd.h>
#include <plugins-common.h>

#include "data-stream.h"
#include "metadata.h"
#include "lttng-live-internal.h"

#define MAX_QUERY_SIZE		(256*1024)

#define print_dbg(fmt, ...)	BT_LOGD(fmt, ## __VA_ARGS__)

static const char *print_state(struct lttng_live_stream_iterator *s)
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

static
void print_stream_state(struct lttng_live_stream_iterator *stream)
{
	const bt_port *port;

	port = bt_port_from_private(stream->port);
	print_dbg("stream %s state %s last_inact_ts %" PRId64 " cur_inact_ts %" PRId64,
		bt_port_get_name(port),
		print_state(stream),
		stream->last_returned_inactivity_timestamp,
		stream->current_inactivity_timestamp);
	bt_port_put_ref(port);
}

BT_HIDDEN
bt_bool lttng_live_is_canceled(struct lttng_live_component *lttng_live)
{
	bt_component *component;
	const bt_graph *graph;
	bt_bool ret;

	if (!lttng_live) {
		return BT_FALSE;
	}

	component = bt_component_from_private(lttng_live->private_component);
	graph = bt_component_get_graph(component);
	ret = bt_graph_is_canceled(graph);
	bt_graph_put_ref(graph);
	bt_component_put_ref(component);
	return ret;
}

BT_HIDDEN
int lttng_live_add_port(struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *stream_iter)
{
	int ret;
	struct bt_private_port *private_port;
	char name[STREAM_NAME_MAX_LEN];
	enum bt_component_status status;

	ret = sprintf(name, STREAM_NAME_PREFIX "%" PRIu64, stream_iter->viewer_stream_id);
	BT_ASSERT(ret > 0);
	strcpy(stream_iter->name, name);
	if (lttng_live_is_canceled(lttng_live)) {
		return 0;
	}
	status = bt_self_component_source_add_output_port(
			lttng_live->private_component, name, stream_iter,
			&private_port);
	switch (status) {
	case BT_COMPONENT_STATUS_GRAPH_IS_CANCELED:
		return 0;
	case BT_COMPONENT_STATUS_OK:
		break;
	default:
		return -1;
	}
	bt_object_put_ref(private_port);	/* weak */
	BT_LOGI("Added port %s", name);

	if (lttng_live->no_stream_port) {
		bt_object_get_ref(lttng_live->no_stream_port);
		ret = bt_private_port_remove_from_component(lttng_live->no_stream_port);
		bt_object_put_ref(lttng_live->no_stream_port);
		if (ret) {
			return -1;
		}
		lttng_live->no_stream_port = NULL;
		lttng_live->no_stream_iter->port = NULL;
	}
	stream_iter->port = private_port;
	return 0;
}

BT_HIDDEN
int lttng_live_remove_port(struct lttng_live_component *lttng_live,
		struct bt_private_port *port)
{
	bt_component *component;
	int64_t nr_ports;
	int ret;

	component = bt_component_from_private(lttng_live->private_component);
	nr_ports = bt_component_source_get_output_port_count(component);
	if (nr_ports < 0) {
		return -1;
	}
	BT_COMPONENT_PUT_REF_AND_RESET(component);
	if (nr_ports == 1) {
		enum bt_component_status status;

		BT_ASSERT(!lttng_live->no_stream_port);

		if (lttng_live_is_canceled(lttng_live)) {
			return 0;
		}
		status = bt_self_component_source_add_output_port(lttng_live->private_component,
				"no-stream", lttng_live->no_stream_iter,
				&lttng_live->no_stream_port);
		switch (status) {
		case BT_COMPONENT_STATUS_GRAPH_IS_CANCELED:
			return 0;
		case BT_COMPONENT_STATUS_OK:
			break;
		default:
			return -1;
		}
		bt_object_put_ref(lttng_live->no_stream_port);	/* weak */
		lttng_live->no_stream_iter->port = lttng_live->no_stream_port;
	}
	bt_object_get_ref(port);
	ret = bt_private_port_remove_from_component(port);
	bt_object_put_ref(port);
	if (ret) {
		return -1;
	}
	return 0;
}

static
struct lttng_live_trace *lttng_live_find_trace(struct lttng_live_session *session,
		uint64_t trace_id)
{
	struct lttng_live_trace *trace;

	bt_list_for_each_entry(trace, &session->traces, node) {
		if (trace->id == trace_id) {
			return trace;
		}
	}
	return NULL;
}

static
void lttng_live_destroy_trace(bt_object *obj)
{
	struct lttng_live_trace *trace = container_of(obj, struct lttng_live_trace, obj);

	BT_LOGI("Destroy trace");
	BT_ASSERT(bt_list_empty(&trace->streams));
	bt_list_del(&trace->node);

	if (trace->trace) {
		int retval;

		retval = bt_trace_set_is_static(trace->trace);
		BT_ASSERT(!retval);
		BT_TRACE_PUT_REF_AND_RESET(trace->trace);
	}
	lttng_live_metadata_fini(trace);
	BT_OBJECT_PUT_REF_AND_RESET(trace->cc_prio_map);
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
	BT_INIT_LIST_HEAD(&trace->streams);
	trace->new_metadata_needed = true;
	bt_list_add(&trace->node, &session->traces);
	bt_object_init(&trace->obj, lttng_live_destroy_trace);
	BT_LOGI("Create trace");
	goto end;
error:
	g_free(trace);
	trace = NULL;
end:
	return trace;
}

BT_HIDDEN
struct lttng_live_trace *lttng_live_ref_trace(struct lttng_live_session *session,
		uint64_t trace_id)
{
	struct lttng_live_trace *trace;

	trace = lttng_live_find_trace(session, trace_id);
	if (trace) {
		bt_object_get_ref(trace);
		return trace;
	}
	return lttng_live_create_trace(session, trace_id);
}

BT_HIDDEN
void lttng_live_unref_trace(struct lttng_live_trace *trace)
{
	bt_object_put_ref(trace);
}

static
void lttng_live_close_trace_streams(struct lttng_live_trace *trace)
{
	struct lttng_live_stream_iterator *stream, *s;

	bt_list_for_each_entry_safe(stream, s, &trace->streams, node) {
		lttng_live_stream_iterator_destroy(stream);
	}
	lttng_live_metadata_fini(trace);
}

BT_HIDDEN
int lttng_live_add_session(struct lttng_live_component *lttng_live,
		uint64_t session_id, const char *hostname,
		const char *session_name)
{
	int ret = 0;
	struct lttng_live_session *s;

	s = g_new0(struct lttng_live_session, 1);
	if (!s) {
		goto error;
	}

	s->id = session_id;
	BT_INIT_LIST_HEAD(&s->traces);
	s->lttng_live = lttng_live;
	s->new_streams_needed = true;
	s->hostname = g_string_new(hostname);
	s->session_name = g_string_new(session_name);

	BT_LOGI("Reading from session: %" PRIu64 " hostname: %s session_name: %s",
		s->id, hostname, session_name);
	bt_list_add(&s->node, &lttng_live->sessions);
	goto end;
error:
	BT_LOGE("Error adding session");
	g_free(s);
	ret = -1;
end:
	return ret;
}

static
void lttng_live_destroy_session(struct lttng_live_session *session)
{
	struct lttng_live_trace *trace, *t;

	BT_LOGI("Destroy session");
	if (session->id != -1ULL) {
		if (lttng_live_detach_session(session)) {
			if (!lttng_live_is_canceled(session->lttng_live)) {
				/* Old relayd cannot detach sessions. */
				BT_LOGD("Unable to detach session %" PRIu64,
					session->id);
			}
		}
		session->id = -1ULL;
	}
	bt_list_for_each_entry_safe(trace, t, &session->traces, node) {
		lttng_live_close_trace_streams(trace);
	}
	bt_list_del(&session->node);
	if (session->hostname) {
		g_string_free(session->hostname, TRUE);
	}
	if (session->session_name) {
		g_string_free(session->session_name, TRUE);
	}
	g_free(session);
}

BT_HIDDEN
void lttng_live_iterator_finalize(bt_self_message_iterator *it)
{
	struct lttng_live_stream_iterator_generic *s =
			bt_self_message_iterator_get_user_data(it);

	switch (s->type) {
	case LIVE_STREAM_TYPE_NO_STREAM:
	{
		/* Leave no_stream_iter in place when port is removed. */
		break;
	}
	case LIVE_STREAM_TYPE_STREAM:
	{
		struct lttng_live_stream_iterator *stream_iter =
			container_of(s, struct lttng_live_stream_iterator, p);

		lttng_live_stream_iterator_destroy(stream_iter);
		break;
	}
	}
}

static
enum bt_lttng_live_iterator_status lttng_live_iterator_next_check_stream_state(
		struct lttng_live_component *lttng_live,
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
	return BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
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
enum bt_lttng_live_iterator_status lttng_live_iterator_next_handle_one_no_data_stream(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *lttng_live_stream)
{
	enum bt_lttng_live_iterator_status ret =
			BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	struct packet_index index;
	enum lttng_live_stream_state orig_state = lttng_live_stream->state;

	if (lttng_live_stream->trace->new_metadata_needed) {
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		goto end;
	}
	if (lttng_live_stream->trace->session->new_streams_needed) {
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		goto end;
	}
	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_NO_DATA
			&& lttng_live_stream->state != LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA) {
		goto end;
	}
	ret = lttng_live_get_next_index(lttng_live, lttng_live_stream, &index);
	if (ret != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	BT_ASSERT(lttng_live_stream->state != LTTNG_LIVE_STREAM_EOF);
	if (lttng_live_stream->state == LTTNG_LIVE_STREAM_QUIESCENT) {
		if (orig_state == LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA
				&& lttng_live_stream->last_returned_inactivity_timestamp ==
					lttng_live_stream->current_inactivity_timestamp) {
			ret = BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			print_stream_state(lttng_live_stream);
		} else {
			ret = BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		}
		goto end;
	}
	lttng_live_stream->base_offset = index.offset;
	lttng_live_stream->offset = index.offset;
	lttng_live_stream->len = index.packet_size / CHAR_BIT;
end:
	if (ret == BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		ret = lttng_live_iterator_next_check_stream_state(
				lttng_live, lttng_live_stream);
	}
	return ret;
}

/*
 * Creation of the message requires the ctf trace to be created
 * beforehand, but the live protocol gives us all streams (including
 * metadata) at once. So we split it in three steps: getting streams,
 * getting metadata (which creates the ctf trace), and then creating the
 * per-stream messages.
 */
static
enum bt_lttng_live_iterator_status lttng_live_get_session(
		struct lttng_live_component *lttng_live,
		struct lttng_live_session *session)
{
	enum bt_lttng_live_iterator_status status;
	struct lttng_live_trace *trace, *t;

	if (lttng_live_attach_session(session)) {
		if (lttng_live_is_canceled(lttng_live)) {
			return BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		} else {
			return BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		}
	}
	status = lttng_live_get_new_streams(session);
	if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK &&
			status != BT_LTTNG_LIVE_ITERATOR_STATUS_END) {
		return status;
	}
	bt_list_for_each_entry_safe(trace, t, &session->traces, node) {
		status = lttng_live_metadata_update(trace);
		if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK &&
				status != BT_LTTNG_LIVE_ITERATOR_STATUS_END) {
			return status;
		}
	}
	return lttng_live_lazy_msg_init(session);
}

BT_HIDDEN
void lttng_live_need_new_streams(struct lttng_live_component *lttng_live)
{
	struct lttng_live_session *session;

	bt_list_for_each_entry(session, &lttng_live->sessions, node) {
		session->new_streams_needed = true;
	}
}

static
void lttng_live_force_new_streams_and_metadata(struct lttng_live_component *lttng_live)
{
	struct lttng_live_session *session;

	bt_list_for_each_entry(session, &lttng_live->sessions, node) {
		struct lttng_live_trace *trace;

		session->new_streams_needed = true;
		bt_list_for_each_entry(trace, &session->traces, node) {
			trace->new_metadata_needed = true;
		}
	}
}

static
enum bt_lttng_live_iterator_status lttng_live_iterator_next_handle_new_streams_and_metadata(
		struct lttng_live_component *lttng_live)
{
	enum bt_lttng_live_iterator_status ret =
			BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	unsigned int nr_sessions_opened = 0;
	struct lttng_live_session *session, *s;

	bt_list_for_each_entry_safe(session, s, &lttng_live->sessions, node) {
		if (session->closed && bt_list_empty(&session->traces)) {
			lttng_live_destroy_session(session);
		}
	}
	/*
	 * Currently, when there are no sessions, we quit immediately.
	 * We may want to add a component parameter to keep trying until
	 * we get data in the future.
	 * Also, in a remotely distant future, we could add a "new
	 * session" flag to the protocol, which would tell us that we
	 * need to query for new sessions even though we have sessions
	 * currently ongoing.
	 */
	if (bt_list_empty(&lttng_live->sessions)) {
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_END;
		goto end;
	}
	bt_list_for_each_entry(session, &lttng_live->sessions, node) {
		ret = lttng_live_get_session(lttng_live, session);
		switch (ret) {
		case BT_LTTNG_LIVE_ITERATOR_STATUS_OK:
			break;
		case BT_LTTNG_LIVE_ITERATOR_STATUS_END:
			ret = BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
			break;
		default:
			goto end;
		}
		if (!session->closed) {
			nr_sessions_opened++;
		}
	}
end:
	if (ret == BT_LTTNG_LIVE_ITERATOR_STATUS_OK && !nr_sessions_opened) {
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_END;
	}
	return ret;
}

static
enum bt_lttng_live_iterator_status emit_inactivity_message(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *lttng_live_stream,
		const bt_message **message,
		uint64_t timestamp)
{
	enum bt_lttng_live_iterator_status ret =
			BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	struct lttng_live_trace *trace;
	const bt_clock_class *clock_class = NULL;
	bt_clock_snapshot *clock_snapshot = NULL;
	const bt_message *msg = NULL;
	int retval;

	trace = lttng_live_stream->trace;
	if (!trace) {
		goto error;
	}
	clock_class = bt_clock_class_priority_map_get_clock_class_by_index(trace->cc_prio_map, 0);
	if (!clock_class) {
		goto error;
	}
	clock_snapshot = bt_clock_snapshot_create(clock_class, timestamp);
	if (!clock_snapshot) {
		goto error;
	}
	msg = bt_message_inactivity_create(trace->cc_prio_map);
	if (!msg) {
		goto error;
	}
	retval = bt_message_inactivity_set_clock_snapshot(msg, clock_snapshot);
	if (retval) {
		goto error;
	}
	*message = msg;
end:
	bt_object_put_ref(clock_snapshot);
	bt_clock_class_put_ref(clock_class);
	return ret;

error:
	ret = BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
	bt_message_put_ref(msg);
	goto end;
}

static
enum bt_lttng_live_iterator_status lttng_live_iterator_next_handle_one_quiescent_stream(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *lttng_live_stream,
		const bt_message **message)
{
	enum bt_lttng_live_iterator_status ret =
			BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	const bt_clock_class *clock_class = NULL;
	bt_clock_snapshot *clock_snapshot = NULL;

	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_QUIESCENT) {
		return BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	}

	if (lttng_live_stream->current_inactivity_timestamp ==
			lttng_live_stream->last_returned_inactivity_timestamp) {
		lttng_live_stream->state = LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA;
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		goto end;
	}

	ret = emit_inactivity_message(lttng_live, lttng_live_stream, message,
			(uint64_t) lttng_live_stream->current_inactivity_timestamp);

	lttng_live_stream->last_returned_inactivity_timestamp =
			lttng_live_stream->current_inactivity_timestamp;
end:
	bt_object_put_ref(clock_snapshot);
	bt_clock_class_put_ref(clock_class);
	return ret;
}

static
enum bt_lttng_live_iterator_status lttng_live_iterator_next_handle_one_active_data_stream(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *lttng_live_stream,
		const bt_message **message)
{
	enum bt_lttng_live_iterator_status ret =
			BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	enum bt_msg_iter_status status;
	struct lttng_live_session *session;

	bt_list_for_each_entry(session, &lttng_live->sessions, node) {
		struct lttng_live_trace *trace;

		if (session->new_streams_needed) {
			return BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		}
		bt_list_for_each_entry(trace, &session->traces, node) {
			if (trace->new_metadata_needed) {
				return BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
			}
		}
	}

	if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_DATA) {
		return BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
	}
	if (lttng_live_stream->packet_end_msg_queue) {
		*message = lttng_live_stream->packet_end_msg_queue;
		lttng_live_stream->packet_end_msg_queue = NULL;
		status = BT_MSG_ITER_STATUS_OK;
	} else {
		status = bt_msg_iter_get_next_message(
				lttng_live_stream->msg_iter,
				lttng_live_stream->trace->cc_prio_map,
				message);
		if (status == BT_MSG_ITER_STATUS_OK) {
			/*
			 * Consider empty packets as inactivity.
			 */
			if (bt_message_get_type(*message) == BT_MESSAGE_TYPE_PACKET_END) {
				lttng_live_stream->packet_end_msg_queue = *message;
				*message = NULL;
				return emit_inactivity_message(lttng_live,
						lttng_live_stream, message,
						lttng_live_stream->current_packet_end_timestamp);
			}
		}
	}
	switch (status) {
	case BT_MSG_ITER_STATUS_EOF:
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_END;
		break;
	case BT_MSG_ITER_STATUS_OK:
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
		break;
	case BT_MSG_ITER_STATUS_AGAIN:
		/*
		 * Continue immediately (end of packet). The next
		 * get_index may return AGAIN to delay the following
		 * attempt.
		 */
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		break;
	case BT_MSG_ITER_STATUS_INVAL:
		/* No argument provided by the user, so don't return INVAL. */
	case BT_MSG_ITER_STATUS_ERROR:
	default:
		ret = BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		break;
	}
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
bt_message_iterator_next_method_return lttng_live_iterator_next_stream(
		bt_self_message_iterator *iterator,
		struct lttng_live_stream_iterator *stream_iter)
{
	enum bt_lttng_live_iterator_status status;
	bt_message_iterator_next_method_return next_return;
	struct lttng_live_component *lttng_live;

	lttng_live = stream_iter->trace->session->lttng_live;
retry:
	print_stream_state(stream_iter);
	next_return.message = NULL;
	status = lttng_live_iterator_next_handle_new_streams_and_metadata(lttng_live);
	if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	status = lttng_live_iterator_next_handle_one_no_data_stream(
			lttng_live, stream_iter);
	if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	status = lttng_live_iterator_next_handle_one_quiescent_stream(
			lttng_live, stream_iter, &next_return.message);
	if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		BT_ASSERT(next_return.message == NULL);
		goto end;
	}
	if (next_return.message) {
		goto end;
	}
	status = lttng_live_iterator_next_handle_one_active_data_stream(lttng_live,
			stream_iter, &next_return.message);
	if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		BT_ASSERT(next_return.message == NULL);
	}

end:
	switch (status) {
	case BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE:
		print_dbg("continue");
		goto retry;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_AGAIN;
		print_dbg("again");
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_END:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_END;
		print_dbg("end");
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_OK:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_OK;
		print_dbg("ok");
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_INVAL:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_INVALID;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_NOMEM;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_UNSUPPORTED;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR:
	default:	/* fall-through */
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		break;
	}
	return next_return;
}

static
bt_message_iterator_next_method_return lttng_live_iterator_next_no_stream(
		bt_self_message_iterator *iterator,
		struct lttng_live_no_stream_iterator *no_stream_iter)
{
	enum bt_lttng_live_iterator_status status;
	bt_message_iterator_next_method_return next_return;
	struct lttng_live_component *lttng_live;

	lttng_live = no_stream_iter->lttng_live;
retry:
	lttng_live_force_new_streams_and_metadata(lttng_live);
	next_return.message = NULL;
	status = lttng_live_iterator_next_handle_new_streams_and_metadata(lttng_live);
	if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
		goto end;
	}
	if (no_stream_iter->port) {
		status = BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
	} else {
		status = BT_LTTNG_LIVE_ITERATOR_STATUS_END;
	}
end:
	switch (status) {
	case BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE:
		goto retry;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_AGAIN;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_END:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_END;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_INVAL:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_INVALID;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_NOMEM;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_UNSUPPORTED;
		break;
	case BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR:
	default:	/* fall-through */
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		break;
	}
	return next_return;
}

BT_HIDDEN
bt_message_iterator_next_method_return lttng_live_iterator_next(
		bt_self_message_iterator *iterator)
{
	struct lttng_live_stream_iterator_generic *s =
			bt_self_message_iterator_get_user_data(iterator);
	bt_message_iterator_next_method_return next_return;

	switch (s->type) {
	case LIVE_STREAM_TYPE_NO_STREAM:
		next_return = lttng_live_iterator_next_no_stream(iterator,
			container_of(s, struct lttng_live_no_stream_iterator, p));
		break;
	case LIVE_STREAM_TYPE_STREAM:
		next_return = lttng_live_iterator_next_stream(iterator,
			container_of(s, struct lttng_live_stream_iterator, p));
		break;
	default:
		next_return.status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		break;
	}
	return next_return;
}

BT_HIDDEN
enum bt_message_iterator_status lttng_live_iterator_init(
		bt_self_message_iterator *it,
		struct bt_private_port *port)
{
	enum bt_message_iterator_status ret =
			BT_MESSAGE_ITERATOR_STATUS_OK;
	struct lttng_live_stream_iterator_generic *s;

	BT_ASSERT(it);

	s = bt_private_port_get_user_data(port);
	BT_ASSERT(s);
	switch (s->type) {
	case LIVE_STREAM_TYPE_NO_STREAM:
	{
		struct lttng_live_no_stream_iterator *no_stream_iter =
			container_of(s, struct lttng_live_no_stream_iterator, p);
		ret = bt_self_message_iterator_set_user_data(it, no_stream_iter);
		if (ret) {
			goto error;
		}
		break;
	}
	case LIVE_STREAM_TYPE_STREAM:
	{
		struct lttng_live_stream_iterator *stream_iter =
			container_of(s, struct lttng_live_stream_iterator, p);
		ret = bt_self_message_iterator_set_user_data(it, stream_iter);
		if (ret) {
			goto error;
		}
		break;
	}
	default:
		ret = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

end:
	return ret;
error:
	if (bt_self_message_iterator_set_user_data(it, NULL)
			!= BT_MESSAGE_ITERATOR_STATUS_OK) {
		BT_LOGE("Error setting private data to NULL");
	}
	goto end;
}

static
bt_component_class_query_method_return lttng_live_query_list_sessions(
		const bt_component_class *comp_class,
		const bt_query_executor *query_exec,
		bt_value *params)
{
	bt_component_class_query_method_return query_ret = {
		.result = NULL,
		.status = BT_QUERY_STATUS_OK,
	};

	bt_value *url_value = NULL;
	const char *url;
	struct bt_live_viewer_connection *viewer_connection = NULL;

	url_value = bt_value_map_get(params, "url");
	if (!url_value || bt_value_is_null(url_value) || !bt_value_is_string(url_value)) {
		BT_LOGW("Mandatory \"url\" parameter missing");
		query_ret.status = BT_QUERY_STATUS_INVALID_PARAMS;
		goto error;
	}

	if (bt_value_string_get(url_value, &url) != BT_VALUE_STATUS_OK) {
		BT_LOGW("\"url\" parameter is required to be a string value");
		query_ret.status = BT_QUERY_STATUS_INVALID_PARAMS;
		goto error;
	}

	viewer_connection = bt_live_viewer_connection_create(url, NULL);
	if (!viewer_connection) {
		goto error;
	}

	query_ret.result =
		bt_live_viewer_connection_list_sessions(viewer_connection);
	if (!query_ret.result) {
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(query_ret.result);

	if (query_ret.status >= 0) {
		query_ret.status = BT_QUERY_STATUS_ERROR;
	}

end:
	if (viewer_connection) {
		bt_live_viewer_connection_destroy(viewer_connection);
	}
	BT_VALUE_PUT_REF_AND_RESET(url_value);
	return query_ret;
}

BT_HIDDEN
bt_component_class_query_method_return lttng_live_query(
		const bt_component_class *comp_class,
		const bt_query_executor *query_exec,
		const char *object, bt_value *params)
{
	bt_component_class_query_method_return ret = {
		.result = NULL,
		.status = BT_QUERY_STATUS_OK,
	};

	if (strcmp(object, "sessions") == 0) {
		return lttng_live_query_list_sessions(comp_class,
			query_exec, params);
	}
	BT_LOGW("Unknown query object `%s`", object);
	ret.status = BT_QUERY_STATUS_INVALID_OBJECT;
	return ret;
}

static
void lttng_live_component_destroy_data(struct lttng_live_component *lttng_live)
{
	int ret;
	struct lttng_live_session *session, *s;

	bt_list_for_each_entry_safe(session, s, &lttng_live->sessions, node) {
		lttng_live_destroy_session(session);
	}
	BT_OBJECT_PUT_REF_AND_RESET(lttng_live->viewer_connection);
	if (lttng_live->url) {
		g_string_free(lttng_live->url, TRUE);
	}
	if (lttng_live->no_stream_port) {
		bt_object_get_ref(lttng_live->no_stream_port);
		ret = bt_private_port_remove_from_component(lttng_live->no_stream_port);
		bt_object_put_ref(lttng_live->no_stream_port);
		BT_ASSERT(!ret);
	}
	if (lttng_live->no_stream_iter) {
		g_free(lttng_live->no_stream_iter);
	}
	g_free(lttng_live);
}

BT_HIDDEN
void lttng_live_component_finalize(bt_self_component *component)
{
	void *data = bt_self_component_get_user_data(component);

	if (!data) {
		return;
	}
	lttng_live_component_destroy_data(data);
}

static
struct lttng_live_component *lttng_live_component_create(bt_value *params,
		bt_self_component *private_component)
{
	struct lttng_live_component *lttng_live;
	bt_value *value = NULL;
	const char *url;
	enum bt_value_status ret;

	lttng_live = g_new0(struct lttng_live_component, 1);
	if (!lttng_live) {
		goto end;
	}
	/* TODO: make this an overridable parameter. */
	lttng_live->max_query_size = MAX_QUERY_SIZE;
	BT_INIT_LIST_HEAD(&lttng_live->sessions);
	value = bt_value_map_get(params, "url");
	if (!value || bt_value_is_null(value) || !bt_value_is_string(value)) {
		BT_LOGW("Mandatory \"url\" parameter missing");
		goto error;
	}
	url = bt_value_string_get(value);
	lttng_live->url = g_string_new(url);
	if (!lttng_live->url) {
		goto error;
	}
	BT_VALUE_PUT_REF_AND_RESET(value);
	lttng_live->viewer_connection =
		bt_live_viewer_connection_create(lttng_live->url->str, lttng_live);
	if (!lttng_live->viewer_connection) {
		goto error;
	}
	if (lttng_live_create_viewer_session(lttng_live)) {
		goto error;
	}
	lttng_live->private_component = private_component;

	goto end;

error:
	lttng_live_component_destroy_data(lttng_live);
	lttng_live = NULL;
end:
	return lttng_live;
}

BT_HIDDEN
enum bt_component_status lttng_live_component_init(
		bt_self_component *private_component,
		bt_value *params, void *init_method_data)
{
	struct lttng_live_component *lttng_live;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	/* Passes ownership of iter ref to lttng_live_component_create. */
	lttng_live = lttng_live_component_create(params, private_component);
	if (!lttng_live) {
		//TODO : we need access to the application cancel state
		//because we are not part of a graph yet.
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	lttng_live->no_stream_iter = g_new0(struct lttng_live_no_stream_iterator, 1);
	lttng_live->no_stream_iter->p.type = LIVE_STREAM_TYPE_NO_STREAM;
	lttng_live->no_stream_iter->lttng_live = lttng_live;
	if (lttng_live_is_canceled(lttng_live)) {
		goto end;
	}
	ret = bt_self_component_source_add_output_port(
				lttng_live->private_component, "no-stream",
				lttng_live->no_stream_iter,
				&lttng_live->no_stream_port);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	bt_object_put_ref(lttng_live->no_stream_port);	/* weak */
	lttng_live->no_stream_iter->port = lttng_live->no_stream_port;

	ret = bt_self_component_set_user_data(private_component, lttng_live);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

end:
	return ret;
error:
	(void) bt_self_component_set_user_data(private_component, NULL);
	lttng_live_component_destroy_data(lttng_live);
	return ret;
}

BT_HIDDEN
enum bt_component_status lttng_live_accept_port_connection(
		bt_self_component *private_component,
		struct bt_private_port *self_private_port,
		const bt_port *other_port)
{
	struct lttng_live_component *lttng_live =
			bt_self_component_get_user_data(private_component);
	bt_component *other_component;
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	const bt_port *self_port = bt_port_from_private(self_private_port);

	other_component = bt_port_get_component(other_port);
	bt_component_put_ref(other_component);	/* weak */

	if (!lttng_live->downstream_component) {
		lttng_live->downstream_component = other_component;
		goto end;
	}

	/*
	 * Compare prior component to ensure we are connected to the
	 * same downstream component as prior ports.
	 */
	if (lttng_live->downstream_component != other_component) {
		BT_LOGW("Cannot connect ctf.lttng-live component port \"%s\" to component \"%s\": already connected to component \"%s\".",
			bt_port_get_name(self_port),
			bt_component_get_name(other_component),
			bt_component_get_name(lttng_live->downstream_component));
		status = BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION;
		goto end;
	}
end:
	bt_port_put_ref(self_port);
	return status;
}
